#include "pch.h"

#include "Tunables.h"

#include "RuntimeTables.h"

#include <cstdlib>

#include "DMA.h"
#include "Offsets.h"

#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace
{
	constexpr uint32_t kChunkShift = 18;
	constexpr uint32_t kChunkMask = 0x3F;
	constexpr uint32_t kElementMask = 0x3FFFF;
	constexpr uint32_t kBlockReadBytes = 512 * 1024;    // tunable 块约 294KB，读 512KB 留余量
	constexpr uint32_t kPageSize = 4096;
	constexpr uint32_t kResolveIntervalMs = 5000;

	struct Slot
	{
		std::atomic<uintptr_t> address{ 0 };
		std::atomic<bool>      resolved{ false };
		int32_t                originalBits = 0;
		int32_t                lastWrittenBits = 0;
		bool                   haveOriginal = false;
		bool                   haveWritten = false;
		const char*            locatedBy = "未定位";
	};

	Slot    g_slots[Tunables::kSlotCount];
	uint32_t g_processId = 0;
	uintptr_t g_processBase = 0;
	bool     g_hasProcessIdentity = false;
	uint32_t g_lastResolveAttempt = 0;
	int      g_anchorElement = -1;
	int      g_blockedWrites = 0;
	std::mutex g_logMutex;
	bool     g_loggedBlockUnavailable = false;

	uint32_t ChunkOf(uint32_t index) { return (index >> kChunkShift) & kChunkMask; }
	uint32_t ElementOf(uint32_t index) { return index & kElementMask; }

	// tunable 块所在分块的基址（chunk 指针，块内第 e 个单元 = base + e*8）
	uintptr_t TunableChunkBase()
	{
		if (!DMA::IsReady())
			return 0;

		const uint32_t chunkIndex = ChunkOf(TunableTable::kTunableBaseAddress);
		const uintptr_t slot = DMA::BaseAddress + Offsets::GlobalPtr + chunkIndex * sizeof(uintptr_t);
		uintptr_t chunk = 0;
		if (!DMA::Memory().Read(slot, &chunk, sizeof(chunk)) || !chunk)
			return 0;
		return chunk;
	}

	int32_t ReadBits(uintptr_t address)
	{
		int32_t value = 0;
		if (!DMA::Memory().Read(address, &value, sizeof(value)))
			return 0;
		return value;
	}

	float BitsToFloat(int32_t bits)
	{
		float f = 0.0f;
		std::memcpy(&f, &bits, sizeof(f));
		return f;
	}

	int32_t FloatToBits(float value)
	{
		int32_t bits = 0;
		std::memcpy(&bits, &value, sizeof(bits));
		return bits;
	}

	// 期望默认值的「位型」：int 条目直接用；float 条目存的就是 IEEE 位型
	int32_t ExpectedBits(const TunableTable::Entry& entry)
	{
		return entry.expectedDefault;
	}

	bool MatchesExpectedBits(const TunableTable::Entry& entry, int32_t bits)
	{
		// 第18轮新增的「金额/额度类」没有固定默认值（游戏会按配置上下浮动）：
		// 只做区间体检 —— 落在 [minValue, maxValue] 内即认为这个格子是这个条目。
		if (entry.kind == TunableTable::Kind::Value)
			return bits >= entry.minValue && bits <= entry.maxValue;

		if (bits == ExpectedBits(entry))
			return true;
		// 第20轮：格子被写成「已禁用」（典型是踢出计时的 INT_MAX）也是**合法状态** ——
		// 否则工具挂着用一段时间后这些条目会一直显示「未定位」，功能看起来像坏了。
		if (entry.altValue != 0 && bits == entry.altValue)
			return true;
		// 已由本工具写过的值同样接受（避免第二次解析把自己的写入当异常）
		return false;
	}

	bool IsSameProcess()
	{
		return g_hasProcessIdentity && g_processId == DMA::PID && g_processBase == DMA::BaseAddress;
	}

	void ClearResolution()
	{
		for (uint32_t i = 0; i < Tunables::kSlotCount; ++i)
		{
			g_slots[i].address.store(0);
			g_slots[i].resolved.store(false);
			g_slots[i].locatedBy = "未定位";
		}
		g_anchorElement = -1;
		g_lastResolveAttempt = 0;
	}

	void ClearAllState()
	{
		ClearResolution();
		for (uint32_t i = 0; i < Tunables::kSlotCount; ++i)
		{
			g_slots[i].originalBits = 0;
			g_slots[i].lastWrittenBits = 0;
			g_slots[i].haveOriginal = false;
			g_slots[i].haveWritten = false;
		}
		g_processId = 0;
		g_processBase = 0;
		g_hasProcessIdentity = false;
		g_loggedBlockUnavailable = false;
	}

	// 分页读 tunable 块（大块读在 FPGA 上遇未驻留页会整块失败，所以按 4KB 页读并统计失败页）
	bool ReadBlock(std::vector<uint8_t>& out, uintptr_t chunkBase, uint32_t& failedPages)
	{
		out.assign(kBlockReadBytes, 0);
		failedPages = 0;
		for (uint32_t off = 0; off < kBlockReadBytes; off += kPageSize)
		{
			const uint32_t n = (kPageSize < kBlockReadBytes - off) ? kPageSize : (kBlockReadBytes - off);
			bool ok = false;
			for (int attempt = 0; attempt < 3 && !ok; ++attempt)
				ok = DMA::Memory().Read(chunkBase + off, out.data() + off, n);
			if (!ok)
				++failedPages;
		}
		return failedPages < (kBlockReadBytes / kPageSize);   // 至少读到一页才算成功
	}

	bool ShapeQuad(const int32_t* v, int32_t& base, double r1, double r2, double r3, double tol)
	{
		if (v[0] < 1000 || v[1] <= v[0] || v[2] <= v[1] || v[3] <= v[2])
			return false;
		const double a = static_cast<double>(v[0]);
		const auto close = [tol](double got, double want) {
			return got > want * (1.0 - tol) && got < want * (1.0 + tol);
		};
		if (!close(v[1] / a, r1) || !close(v[2] / a, r2) || !close(v[3] / a, r3))
			return false;
		base = v[0];
		return true;
	}

	// 值锚：在块内找「四连比例特征」的起点 element
	int FindAnchorElement(const std::vector<uint8_t>& block, int32_t& outBase)
	{
		const uint32_t maxElement = static_cast<uint32_t>(block.size() / 8);
		for (uint32_t e = 1; e + 3 < maxElement; ++e)
		{
			int32_t v[4] = {};
			std::memcpy(v, block.data() + e * 8, sizeof(v));

			// 受限踢出计时：1 : 2 : 3 : 4（实机 30000/60000/90000/120000）
			int32_t base = 0;
			if (ShapeQuad(v, base, 2.0, 3.0, 4.0, 0.02))
			{
				outBase = base;
				return static_cast<int>(e);
			}
		}
		return -1;
	}

	// 逐格直读的四连校验/搜索：大块分页读在未驻留页上会失败，
	// 但 8 字节的单格直读一直可用 —— 用它把「四连锚」真正验证出来（而不是只信表里的候选位置）。
	int VerifyOrFindQuadDirect(const char* firstName, uintptr_t chunkBase)
	{
		const int slot = Tunables::Find(firstName);
		if (slot < 0 || static_cast<uint32_t>(slot) + 3 >= Tunables::kSlotCount)
			return -1;

		int32_t expected[4] = {};
		for (int k = 0; k < 4; ++k)
			expected[k] = ExpectedBits(TunableTable::kEntries[static_cast<uint32_t>(slot) + static_cast<uint32_t>(k)]);

		const int candidate = static_cast<int>(ElementOf(TunableTable::kEntries[static_cast<uint32_t>(slot)].globalIndex));

		const auto matches = [&](int element) {
			if (element < 0)
				return false;
			for (int k = 0; k < 4; ++k)
			{
				if (ReadBits(chunkBase + static_cast<uintptr_t>(element + k) * 8) != expected[k])
					return false;
			}
			return true;
		};

		if (matches(candidate))
			return candidate;
		for (int off = 1; off <= Tunables::kNeighborhood; ++off)
		{
			if (matches(candidate - off))
				return candidate - off;
			if (matches(candidate + off))
				return candidate + off;
		}
		return -1;
	}

	int FindIdleKickAnchorElement(const std::vector<uint8_t>& block)
	{
		const uint32_t maxElement = static_cast<uint32_t>(block.size() / 8);
		for (uint32_t e = 1; e + 3 < maxElement; ++e)
		{
			int32_t v[4] = {};
			std::memcpy(v, block.data() + e * 8, sizeof(v));
			int32_t base = 0;
			// 空闲踢出计时：1 : 2.5 : 5 : 7.5（实机 120000/300000/600000/900000）
			if (ShapeQuad(v, base, 2.5, 5.0, 7.5, 0.02))
				return static_cast<int>(e);
		}
		return -1;
	}
}

bool Tunables::Resolve()
{
	if (!DMA::IsReady())
		return false;

	if (!IsSameProcess())
		ClearAllState();

	// 已全部解析过就不重复干活
	if (g_hasProcessIdentity && GetResolvedCount() == static_cast<int>(kSlotCount))
		return true;

	const uint32_t now = GetTickCount();
	if (g_lastResolveAttempt != 0 && (now - g_lastResolveAttempt) < kResolveIntervalMs)
		return false;
	g_lastResolveAttempt = now;

	g_processId = DMA::PID;
	g_processBase = DMA::BaseAddress;
	g_hasProcessIdentity = true;

	const uintptr_t chunkBase = TunableChunkBase();
	if (!chunkBase)
	{
		if (!g_loggedBlockUnavailable)
		{
			g_loggedBlockUnavailable = true;
			const uint32_t chunkIndex = ChunkOf(TunableTable::kTunableBaseAddress);
			const uintptr_t slot = DMA::BaseAddress + Offsets::GlobalPtr + chunkIndex * sizeof(uintptr_t);
			uintptr_t chunk = 0;
			DMA::Memory().Read(slot, &chunk, sizeof(chunk));
			std::println("[Tunables] tunable 块不可用：chunk {} 的分块指针 = 0x{:X}"
			             "（0 = 未分配：还没进在线战局，或 tunables 脚本未加载）", chunkIndex, chunk);
		}
		return false;
	}

	std::vector<uint8_t> block;
	uint32_t failedPages = 0;
	if (!ReadBlock(block, chunkBase, failedPages))
	{
		std::println("[Tunables] tunable 块分页读失败（chunk 基址 0x{:X}）", chunkBase);
		return false;
	}

	// ---- ① 值锚 ----
	int32_t anchorBase = 0;
	const int constrainedAnchor = FindAnchorElement(block, anchorBase);
	const int idleAnchor = FindIdleKickAnchorElement(block);
	g_anchorElement = constrainedAnchor;

	// 大读受限时改用逐格直读的四连校验/搜索（8 字节单格读一直可用）
	const int constrainedDirect = VerifyOrFindQuadDirect("ConstrainedKick_Warning1", chunkBase);
	const int idleDirect = VerifyOrFindQuadDirect("IDLEKICK_WARNING1", chunkBase);
	const bool constrainedAnchorVerified = constrainedAnchor >= 0 || constrainedDirect >= 0;
	const bool idleAnchorVerified = idleAnchor >= 0 || idleDirect >= 0;
	if (constrainedAnchor < 0 && constrainedDirect >= 0)
		g_anchorElement = constrainedDirect;

	// 锚的 element ↔ 全局索引：由表里对应条目的候选索引算出「锚的候选 element」，再与实扫结果对比
	int anchorTableSlot = Find("ConstrainedKick_Warning1");
	uint32_t anchorCandidateElement = 0;
	// 第19轮：索引先走运行时解析（外部覆盖文件 → tunables.bin → 编译种子）。
	// 名字哈希稳定、索引会随更新漂移，所以每次启动都重新问一遍，不靠编译期表。
	auto IndexOfEntry = [](const TunableTable::Entry& e)
	{
		const char* src = nullptr;
		return RuntimeTables::ResolveTunableIndex(e.name, e.hash, e.globalIndex, &src);
	};

	if (anchorTableSlot >= 0)
		anchorCandidateElement = ElementOf(IndexOfEntry(TunableTable::kEntries[anchorTableSlot]));

	const int anchorElement = constrainedAnchor >= 0
		? constrainedAnchor                                     // 大读扫描到的锚 element
		: (constrainedDirect >= 0 ? constrainedDirect           // 逐格直读验证到的锚 element
		                          : (anchorTableSlot >= 0 ? static_cast<int>(anchorCandidateElement) : -1));

int idleAnchorElement = idleAnchor >= 0 ? idleAnchor : idleDirect;
	if (idleAnchorElement < 0)
	{
		const int idleSlot = Find("IDLEKICK_WARNING1");
		if (idleSlot >= 0)
			idleAnchorElement = static_cast<int>(ElementOf(TunableTable::kEntries[idleSlot].globalIndex));
	}

	std::println("[Tunables] 四连锚：受限踢出 element={}（{}）、空闲踢出 element={}（{}）；大读 {} KB 失败页 {}",
	             anchorElement,
	             constrainedAnchor >= 0 ? "大读扫描命中" : (constrainedDirect >= 0 ? "逐格直读验证命中" : "未命中"),
	             idleAnchorElement,
	             idleAnchor >= 0 ? "大读扫描命中" : (idleDirect >= 0 ? "逐格直读验证命中" : "未命中"),
	             block.size() / 1024, failedPages);

	int resolvedCount = 0;
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		const TunableTable::Entry& entry = TunableTable::kEntries[i];
		const uint32_t candidateElement = ElementOf(IndexOfEntry(entry));
		const int32_t expected = ExpectedBits(entry);

		int element = static_cast<int>(candidateElement);
		const char* locatedBy = "绝对";

		// ---- ② 相对定位：同组条目以各自的锚推算 ----
		const bool isIdleGroup = std::strstr(entry.name, "IDLEKICK_") != nullptr;


		const bool isConstrainedGroup = std::strstr(entry.name, "ConstrainedKick_") != nullptr;
		if (isIdleGroup && idleAnchorElement >= 0)
		{
			const int idleTableSlot = Find("IDLEKICK_WARNING1");
			if (idleTableSlot >= 0)
			{
				const int delta = static_cast<int>(candidateElement) -
					static_cast<int>(ElementOf(TunableTable::kEntries[idleTableSlot].globalIndex));
				element = idleAnchorElement + delta;
				locatedBy = idleAnchorVerified ? "四连锚+相对" : "绝对";
			}
		}
		else if (isConstrainedGroup && anchorElement >= 0)
		{
			if (anchorTableSlot >= 0)
			{
				const int delta = static_cast<int>(candidateElement) -
					static_cast<int>(ElementOf(IndexOfEntry(TunableTable::kEntries[anchorTableSlot])));
				element = anchorElement + delta;
				locatedBy = constrainedAnchorVerified ? "四连锚+相对" : "绝对";
			}
		}

		uintptr_t address = 0;
		int32_t bits = 0;
		if (element >= 0)
		{
			address = chunkBase + static_cast<uintptr_t>(element) * 8;
			bits = ReadBits(address);
		}

		// ---- ③ 已移除「单值邻域搜索」 ----
		//  实机 2026-09-17 观测：tunable 块布局会随会话变化（同一天里空闲踢出组从 element 85 移到 56），
		//  而 120000 / 100000 / 1.0f 这类默认值在块内重复出现几十次 —— 按单值在 ±256 内搜索会**静默定位到别的条目**
		//  （当时 8 条踢出计时里有 4 条被错定位）。现在只认两种可信来源：
		//    ① 表里的候选 element 上「当前值 == 期望默认值」
		//    ② 四连组校验通过（VerifyOrFindQuadDirect：整组比例成立，且四个格子逐一核对）
		//  两者都不成立 → 判「未定位」，绝不写入。

		// 体检：Int/Float 型比对期望默认值；Value 型（金额/额度）只做区间体检
		bool verified = MatchesExpectedBits(entry, bits);

		// ---- ③ 值序列自发现（第19轮）：靠一组已知值的连续出现位置反推索引 ----
		//  游戏更新把索引整段挪走时（tunables.bin 也没有），只要这组值还连续排在一起，
		//  就能把整组索引重新推出来 —— 不需要重新编译 exe。
		if (!verified && entry.runId >= 0 && entry.runId < TunableTable::kRunCount)
		{
			const auto& run = TunableTable::kRuns[entry.runId];
			uint32_t occurrences = 0;
			const int64_t found = RuntimeTables::FindValueRunGlobalIndex(run.values, run.count, &occurrences);
			if (found > 0)
			{
				const int64_t candidate = found + entry.runOffset;
				const int64_t seed = static_cast<int64_t>(IndexOfEntry(entry));
				// 采信条件（满足其一即可）：
				//   ① 整组值在块里**只出现一次** —— 唯一性本身就是强证据，
				//      哪怕索引整体挪走 20 万个元素也能找回（这正是不用重编译的关键）
				//   ② 出现多次时，用位置消歧：要求结果离编译种子不远（≤8192 元素）
				const bool unique = occurrences == 1;
				const bool nearSeed = std::llabs(candidate - seed) <= 8192;
				if (unique || nearSeed)
				{
					const uint32_t retryElement = ElementOf(static_cast<uint32_t>(candidate));
					const int32_t retryBits = ReadBits(chunkBase + static_cast<uintptr_t>(retryElement) * 8);
					if (MatchesExpectedBits(entry, retryBits))
					{
						std::println("[Tunables] {} 值序列自发现命中：全局索引 0x{:X}（出现 {} 次{}，相对种子 {:+d}），已体检通过",
						             entry.name,
						             (ChunkOf(TunableTable::kTunableBaseAddress) << kChunkShift) |
						                 static_cast<uint32_t>(retryElement),
						             occurrences,
						             unique ? "，唯一" : "",
						             static_cast<int>(candidate - seed));
						element = static_cast<int>(retryElement);
						bits = retryBits;
						address = chunkBase + static_cast<uintptr_t>(retryElement) * 8;
						locatedBy = "值序列自发现";
						verified = true;
					}
				}
			}
		}

		if (!verified)
		{
			std::println("[Tunables] {} 体检未通过：element {} 读到 {}{}，本会话跳过该条", entry.name, element,
			             entry.kind == TunableTable::Kind::Float ? std::to_string(BitsToFloat(bits))
			                                                     : std::to_string(bits),
			             entry.kind == TunableTable::Kind::Value
			                 ? ("（Value 型：合法区间 [" + std::to_string(entry.minValue) + ", " +
			                    std::to_string(entry.maxValue) + "]）")
			                 : ("（期望默认 " + (entry.kind == TunableTable::Kind::Float
			                                        ? std::to_string(BitsToFloat(expected))
			                                        : std::to_string(expected)) +
			                    "）"));
			continue;
		}

		if (!g_slots[i].haveOriginal || g_slots[i].address.load() != address)
		{
			g_slots[i].originalBits = bits;
			g_slots[i].haveOriginal = true;
		}
		g_slots[i].address.store(address);
		g_slots[i].resolved.store(true);
		g_slots[i].locatedBy = locatedBy;
		++resolvedCount;

		std::println("[Tunables] {} @ 全局索引 0x{:X} (element {}) = {}  [定位: {}]",
		             entry.name, (ChunkOf(TunableTable::kTunableBaseAddress) << kChunkShift) |
		                 static_cast<uint32_t>(element),
		             element,
		             entry.kind == TunableTable::Kind::Float
		                 ? std::to_string(BitsToFloat(bits)) : std::to_string(bits),
		             locatedBy);
	}

	g_loggedBlockUnavailable = false;
	std::println("[Tunables] 解析完成：{}/{} 条通过体检", resolvedCount, kSlotCount);
	return resolvedCount > 0;
}



void Tunables::Reset()
{
	ClearAllState();
}

bool Tunables::RestoreAll()
{
	if (!DMA::IsReady())
		return false;

	bool allRestored = true;
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		if (!g_slots[i].resolved.load() || !g_slots[i].haveOriginal || !g_slots[i].haveWritten)
			continue;

		const uintptr_t address = g_slots[i].address.load();
		if (!address)
		{
			allRestored = false;
			continue;
		}

		const int32_t current = ReadBits(address);
		if (current != g_slots[i].originalBits)
		{
			int32_t desired = g_slots[i].originalBits;
			if (!DMA::Memory().Write(address, &desired, sizeof(desired)))
			{
				std::println("[Tunables] {} 还原写入失败", TunableTable::kEntries[i].name);
				allRestored = false;
				continue;
			}
			if (ReadBits(address) != desired)
			{
				std::println("[Tunables] {} 还原读回校验失败", TunableTable::kEntries[i].name);
				allRestored = false;
				continue;
			}
		}
		g_slots[i].haveWritten = false;
	}
	return allRestored;
}

bool Tunables::PrepareForClose()
{
	const bool restored = RestoreAll();
	Reset();
	return restored;
}

bool Tunables::Write(uint32_t i, int32_t value, const char* why)
{
	if (i >= kSlotCount || !g_slots[i].resolved.load())
		return false;

	const uintptr_t address = g_slots[i].address.load();
	if (!address)
		return false;

	const int32_t current = ReadBits(address);
	if (current == value)
	{
		g_slots[i].haveWritten = true;
		g_slots[i].lastWrittenBits = value;
		return true;
	}

	// 写入前体检：当前值必须是原始值或本工具上次写入值，否则拒绝（防止写到被别的工具动过的格子里）
	if (current != g_slots[i].originalBits && !(g_slots[i].haveWritten && current == g_slots[i].lastWrittenBits))
	{
		++g_blockedWrites;
		std::println("[Tunables] {} 拒绝写入：当前值 {} 既非原始值 {} 也非本工具上次写入值 {}",
		             TunableTable::kEntries[i].name, current, g_slots[i].originalBits, g_slots[i].lastWrittenBits);
		return false;
	}

	if (!DMA::Memory().Write(address, &value, sizeof(value)))
		return false;

	if (ReadBits(address) != value)
	{
		++g_blockedWrites;
		std::println("[Tunables] {} 写入被立刻覆盖（读回校验失败）", TunableTable::kEntries[i].name);
		return false;
	}

	g_slots[i].haveWritten = true;
	g_slots[i].lastWrittenBits = value;
	std::println("[Tunables] {} = {}{}{}", TunableTable::kEntries[i].name,
	             TunableTable::kEntries[i].kind == TunableTable::Kind::Float
	                 ? std::to_string(BitsToFloat(value)) : std::to_string(value),
	             why ? "  (" : "", why ? why : "");
	return true;
}

bool Tunables::WriteFloat(uint32_t i, float value, const char* why)
{
	return Write(i, FloatToBits(value), why);
}

int Tunables::SelfTest()
{
	if (!Resolve())
	{
		std::println("[selftest] tunable 解析失败：块未分配或体检未通过");
		return 1;
	}

	std::println("[selftest] 已定位 {}/{} 条，值锚 element = {}，拒绝写入计数 = {}",
	             GetResolvedCount(), kSlotCount, GetAnchorElement(), GetBlockedWriteCount());
	int rc = 0;

	// 1) float 条目：XP_MULTIPLIER  1.0f -> 2.0f -> 还原
	const int xp = Find("XP_MULTIPLIER");
	if (xp < 0 || !IsResolved(static_cast<uint32_t>(xp)))
	{
		std::println("[selftest] XP_MULTIPLIER 未定位 -> FAIL");
		rc = 1;
	}
	else
	{
		const int32_t original = GetOriginal(static_cast<uint32_t>(xp));
		std::println("[selftest] XP_MULTIPLIER 写前 = {:.4f}（bits 0x{:08X}，定位 {}）", BitsToFloat(original),
		             static_cast<uint32_t>(original), GetLocatedBy(static_cast<uint32_t>(xp)));

		const float target = 2.0f;
		const bool wrote = WriteFloat(static_cast<uint32_t>(xp), target, "自检");
		const int32_t back = ReadLive(static_cast<uint32_t>(xp));
		std::println("[selftest] 写 2.0f：写入{} -> 读回 {:.4f} -> {}", wrote ? "成功" : "失败", BitsToFloat(back),
		             back == FloatToBits(target) ? "PASS" : "FAIL");
		if (back != FloatToBits(target))
			rc = 1;

		const bool restored = Write(static_cast<uint32_t>(xp), original, "自检还原");
		const int32_t rest = ReadLive(static_cast<uint32_t>(xp));
		std::println("[selftest] 还原：写入{} -> 读回 {:.4f} -> {}", restored ? "成功" : "失败", BitsToFloat(rest),
		             rest == original ? "PASS" : "FAIL");
		if (rest != original)
			rc = 1;
	}

	// 2) int 条目：IDLEKICK_WARNING1  120000 -> INT_MAX -> 还原
	const int kick = Find("IDLEKICK_WARNING1");
	if (kick < 0 || !IsResolved(static_cast<uint32_t>(kick)))
	{
		std::println("[selftest] IDLEKICK_WARNING1 未定位 -> FAIL");
		rc = 1;
	}
	else
	{
		const int32_t original = GetOriginal(static_cast<uint32_t>(kick));
		std::println("[selftest] IDLEKICK_WARNING1 写前 = {}（定位 {}）", original,
		             GetLocatedBy(static_cast<uint32_t>(kick)));

		const bool wrote = Write(static_cast<uint32_t>(kick), INT_MAX, "自检");
		const int32_t back = ReadLive(static_cast<uint32_t>(kick));
		std::println("[selftest] 写 INT_MAX：写入{} -> 读回 {} -> {}", wrote ? "成功" : "失败", back,
		             back == INT_MAX ? "PASS" : "FAIL");
		if (back != INT_MAX)
			rc = 1;

		const bool restored = Write(static_cast<uint32_t>(kick), original, "自检还原");
		const int32_t rest = ReadLive(static_cast<uint32_t>(kick));
		std::println("[selftest] 还原：写入{} -> 读回 {} -> {}", restored ? "成功" : "失败", rest,
		             rest == original ? "PASS" : "FAIL");
		if (rest != original)
			rc = 1;
	}

	// 3) 金额/额度类（第18轮）：挑第一条 Value 型，写「原值 × 2」（clamp 到区间）→ 读回 → 还原
	{
		int valueEntry = -1;
		for (uint32_t i = 0; i < kSlotCount; ++i)
		{
			if (IsValueKind(i) && IsResolved(i))
			{
				valueEntry = static_cast<int>(i);
				break;
			}
		}
		if (valueEntry < 0)
		{
			std::println("[selftest] 没有可用的金额类条目（未定位或本构建未登记）→ 跳过");
		}
		else
		{
			const uint32_t slot = static_cast<uint32_t>(valueEntry);
			const int32_t original = GetOriginal(slot);
			const int32_t lo = GetMinValue(slot);
			const int32_t hi = GetMaxValue(slot);
			int32_t target = original * 2;
			if (target > hi)
				target = hi;
			if (target < lo)
				target = lo;

			std::println("[selftest] {} 写前 = {}（合法区间 [{}, {}]）", GetEntryName(slot), original, lo, hi);

			const bool wrote = Write(slot, target, "自检");
			const int32_t back = ReadLive(slot);
			std::println("[selftest] 金额倍率写 {}：写入{} → 读回 {} → {}", target, wrote ? "成功" : "失败", back,
			             back == target ? "PASS" : "FAIL");
			if (!wrote || back != target)
				rc = 1;

			const bool restored = Write(slot, original, "自检还原");
			const int32_t rest = ReadLive(slot);
			std::println("[selftest] 还原：写入{} → 读回 {} → {}", restored ? "成功" : "失败", rest,
			             rest == original ? "PASS" : "FAIL");
			if (!restored || rest != original)
				rc = 1;
		}
	}

	std::println("[selftest] 结论：{}", rc == 0 ? "tunable 写入路径实证通过（结束时已还原）" : "存在问题（见上）");
	return rc;
}
int Tunables::Find(const char* name)
{
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		if (std::strcmp(TunableTable::kEntries[i].name, name) == 0)
			return static_cast<int>(i);
	}
	return -1;
}

bool Tunables::IsResolved(uint32_t i) { return i < kSlotCount && g_slots[i].resolved.load(); }
uintptr_t Tunables::GetAddress(uint32_t i) { return i < kSlotCount ? g_slots[i].address.load() : 0; }
uint32_t Tunables::GetGlobalIndex(uint32_t i) { return i < kSlotCount ? TunableTable::kEntries[i].globalIndex : 0; }
int32_t Tunables::GetOriginal(uint32_t i) { return i < kSlotCount ? g_slots[i].originalBits : 0; }
int32_t Tunables::GetLastWritten(uint32_t i) { return i < kSlotCount ? g_slots[i].lastWrittenBits : 0; }
bool Tunables::HasWritten(uint32_t i) { return i < kSlotCount && g_slots[i].haveWritten; }
int Tunables::GetBlockedWriteCount() { return g_blockedWrites; }
int Tunables::GetAnchorElement() { return g_anchorElement; }
const char* Tunables::GetLocatedBy(uint32_t i) { return i < kSlotCount ? g_slots[i].locatedBy : "?"; }
const char* Tunables::GetEntryLabel(uint32_t i)
{
	if (i >= kSlotCount)
		return "?";
	return TunableTable::kEntries[i].label;
}

const char* Tunables::GetEntryName(uint32_t i) { return i < kSlotCount ? TunableTable::kEntries[i].name : "?"; }
bool Tunables::IsValueKind(uint32_t i) { return i < kSlotCount && TunableTable::kEntries[i].kind == TunableTable::Kind::Value; }
int32_t Tunables::GetMinValue(uint32_t i) { return i < kSlotCount ? TunableTable::kEntries[i].minValue : 0; }
int32_t Tunables::GetMaxValue(uint32_t i) { return i < kSlotCount ? TunableTable::kEntries[i].maxValue : 0; }

int32_t Tunables::GetExpectedDefault(uint32_t i)
{
	return i < kSlotCount ? TunableTable::kEntries[i].expectedDefault : 0;
}

int32_t Tunables::ReadLive(uint32_t i, bool* ok)
{
	int32_t bits = 0;
	const bool success = i < kSlotCount && g_slots[i].resolved.load() &&
	                     DMA::Memory().Read(g_slots[i].address.load(), &bits, sizeof(bits));
	if (ok)
		*ok = success;
	return bits;
}

int Tunables::GetResolvedCount()
{
	int count = 0;
	for (uint32_t i = 0; i < kSlotCount; ++i)
		if (g_slots[i].resolved.load())
			++count;
	return count;
}

