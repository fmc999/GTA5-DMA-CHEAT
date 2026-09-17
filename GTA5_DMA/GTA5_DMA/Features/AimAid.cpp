#include "pch.h"

#include "AimAid.h"

#include "OffsetResolver.h"
#include "PatternScanner.h"
#include "Offsets.h"

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace
{
	// 一处补丁的完整描述：特征码 -> 补丁点位移 -> 要写入的字节
	struct PatchSpec
	{
		AimAid::PatchId id;
		const char* name;
		const char* pattern;
		std::ptrdiff_t siteDelta;        // 相对特征码命中处的位移
		const uint8_t* bytes;            // 要写入的字节
		size_t byteCount;
	};

	// 四处补丁的字节内容（与 YimMenuV2 一致）
	const uint8_t kShouldNotTargetBytes[] = { 0xB0, 0x00, 0xC3 };              // mov al,0 ; ret
	const uint8_t kAssistedAimTypeBytes[] = { 0xBD, 0x01, 0x00, 0x00, 0x00 };  // mov ebp,1
	const uint8_t kLockOnPosBytes[]       = { 0xEB };                          // jmp short
	const uint8_t kDriverLockOnBytes[]    = { 0xB0, 0x01, 0xC3 };              // mov al,1 ; ret

	const PatchSpec kSpecs[] = {
		{ AimAid::PatchId::ShouldNotTarget,
		  "不排除目标 (ShouldNotTargetEntity)",
		  "F6 80 A9 14 00 00 01",
		  -0x53,
		  kShouldNotTargetBytes, sizeof(kShouldNotTargetBytes) },

		{ AimAid::PatchId::AssistedAimType,
		  "辅助瞄准类型 (GetAssistedAimType)",
		  "FF E0 48 8D 86",
		  -0x15,
		  kAssistedAimTypeBytes, sizeof(kAssistedAimTypeBytes) },

		{ AimAid::PatchId::LockOnPos,
		  "锁定头部 (GetLockOnPos)",
		  "0F 29 74 24 ? 48 89 D6 48 89 CF 48 8B 05",
		  0x22,
		  kLockOnPosBytes, sizeof(kLockOnPosBytes) },

		{ AimAid::PatchId::DriverLockOn,
		  "驾驶员锁定 (ShouldAllowDriverLockOn)",
		  "75 ? 45 89 C7 49 89 CE",
		  -0x2C,
		  kDriverLockOnBytes, sizeof(kDriverLockOnBytes) },
	};

	constexpr int kSpecCount = static_cast<int>(sizeof(kSpecs) / sizeof(kSpecs[0]));

	// 运行时状态（DMA 线程写 / UI 线程读，均为单字节级别，用原子量保底）
	std::atomic<bool>      g_resolved[kSpecCount];
	std::atomic<bool>      g_applied[kSpecCount];
	std::atomic<uintptr_t> g_site[kSpecCount];
	std::atomic<int>       g_contended[kSpecCount];   // 写入后被别人改回的次数
	uint8_t                g_original[16 * kSpecCount]{};   // 每处最多保留 16 字节原值
	bool                   g_haveOriginal[kSpecCount]{};
	uintptr_t              g_originalSite[kSpecCount]{};     // 原值对应的补丁地址
	uint32_t               g_lastAttempt[kSpecCount]{};      // 重试节流（写失败才重试）
	bool                   g_loggedApplied[kSpecCount]{};   // 只在状态翻转时打印一次

	// 已解析的目标进程身份。DMA 重连时用它区分「同一进程」和「游戏重启」。
	uint32_t               g_processId = 0;
	uintptr_t              g_processBase = 0;
	bool                   g_hasProcessIdentity = false;

	// .text 段（Resolve 时读入内存，之后即可释放）——只做特征码搜索用
	std::vector<uint8_t>  g_textBytes;
	uintptr_t             g_textRuntime = 0;

	void ClearScanState()
	{
		for (int i = 0; i < kSpecCount; ++i)
		{
			g_resolved[i].store(false);
			g_applied[i].store(false);
			g_site[i].store(0);
			g_contended[i].store(0);
			g_lastAttempt[i] = 0;
			g_loggedApplied[i] = false;
		}
		g_textBytes.clear();
		g_textRuntime = 0;
	}

	bool IsSameProcess() noexcept
	{
		return g_hasProcessIdentity &&
		       g_processId == DMA::PID &&
		       g_processBase == DMA::BaseAddress;
	}

	// 把 pattern 里的通配符转成可直接比较的字节/mask 对
	struct CompiledPattern
	{
		std::vector<uint8_t> bytes;
		std::vector<bool>    mask;
		bool ok = false;
	};

	CompiledPattern Compile(const char* pattern)
	{
		CompiledPattern cp;
		const char* s = pattern;
		while (*s)
		{
			while (*s == ' ') ++s;
			if (!*s) break;

			if (*s == '?')
			{
				cp.bytes.push_back(0);
				cp.mask.push_back(false);
				++s;
				while (*s && *s != ' ') ++s;
			}
			else
			{
				if (!s[0] || !s[1]) return cp;   // 残缺字节
				char b[3] = { s[0], s[1], 0 };
				char* end = nullptr;
				const unsigned long v = std::strtoul(b, &end, 16);
				if (end != b + 2) return cp;
				cp.bytes.push_back(static_cast<uint8_t>(v));
				cp.mask.push_back(true);
				s += 2;
			}
		}
		cp.ok = cp.bytes.size() >= 3;
		return cp;
	}

	// 在已加载的 .text 里找一个「唯一」命中；多命中时返回 false（宁可不改）
	bool FindUniqueInText(const CompiledPattern& cp, size_t& outOffset)
	{
		if (!cp.ok || g_textBytes.empty()) return false;

		const size_t n = cp.bytes.size();
		size_t found = 0;
		size_t first = 0;

		for (size_t i = 0; i + n <= g_textBytes.size(); ++i)
		{
			bool match = true;
			for (size_t k = 0; k < n; ++k)
			{
				if (cp.mask[k] && g_textBytes[i + k] != cp.bytes[k]) { match = false; break; }
			}
			if (match)
			{
				if (found == 0) first = i;
				++found;
				if (found > 1) break;      // 多命中，直接失败
			}
		}

		if (found != 1) return false;
		outOffset = first;
		return true;
	}
}

bool AimAid::Resolve()
{
	if (!DMA::IsReady() || currentGameType != GameType::GTA5_Enhanced)
		return false;

	const bool sameProcess = IsSameProcess();
	if (!sameProcess)
		Reset();
	else
		ClearScanState();

	g_processId = DMA::PID;
	g_processBase = DMA::BaseAddress;
	g_hasProcessIdentity = true;

	// 复用 OffsetResolver 的段加载：一次把 .text 读进内存，四处特征码共用
	VMMDLL_MAP_MODULEENTRY moduleInfo{};
	PVMMDLL_MAP_MODULEENTRY pModuleInfo = nullptr;
	if (!VMMDLL_Map_GetModuleFromNameU(DMA::vmh, DMA::PID, "GTA5_Enhanced.exe", &pModuleInfo, 0) || !pModuleInfo)
		return false;
	moduleInfo = *pModuleInfo;
	VMMDLL_MemFree(pModuleInfo);

	const uint32_t imageSize = moduleInfo.cbImageSize;
	if (imageSize == 0)
		return false;

	const OffsetResolver::MemoryReader reader = [](std::uintptr_t address, void* buffer, std::size_t size, const char* stage) {
		DWORD bytesRead = 0;
		const BOOL ok = VMMDLL_MemReadEx(
			DMA::vmh, DMA::PID, static_cast<ULONG64>(address),
			static_cast<PBYTE>(buffer), static_cast<DWORD>(size), &bytesRead,
			VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
		(void)stage;
		return ok != FALSE;
	};

	std::string diagnostic = "unknown";
	const auto section = OffsetResolver::LoadExecutableSection(reader, DMA::BaseAddress, imageSize, &diagnostic);
	if (!section)
	{
		std::println("[AimAid] .text load failed: {}", diagnostic);
		return false;
	}

	g_textBytes = section->bytes;              // 拷贝一份用于搜索
	g_textRuntime = section->runtimeAddress;

	int resolvedCount = 0;
	for (int i = 0; i < kSpecCount; ++i)
	{
		const PatchSpec& spec = kSpecs[i];
		const CompiledPattern cp = Compile(spec.pattern);
		size_t offset = 0;

		if (!FindUniqueInText(cp, offset))
		{
			std::println("[AimAid] {} : signature not unique/found, patch disabled", spec.name);
			continue;
		}

		const std::uintptr_t siteAddr = static_cast<std::uintptr_t>(
			static_cast<std::int64_t>(g_textRuntime + offset) + static_cast<std::int64_t>(spec.siteDelta));

		if (siteAddr == 0 || siteAddr < g_textRuntime ||
			siteAddr + spec.byteCount > g_textRuntime + g_textBytes.size())
		{
			std::println("[AimAid] {} : patch site out of range, skipped", spec.name);
			continue;
		}

		// 保存原始字节（用于还原）。同一进程重连且补丁仍处于生效状态时，
		// 必须保留上一轮记录，绝不能把补丁字节误学成原始值。
		uint8_t current[16]{};
		if (!DMA::Memory().Read(siteAddr, current, spec.byteCount))
		{
			std::println("[AimAid] {} : failed to read original bytes", spec.name);
			continue;
		}

		const bool currentIsPatch = std::memcmp(current, spec.bytes, spec.byteCount) == 0;
		if (sameProcess && g_haveOriginal[i])
		{
			// 同一地址始终沿用最先保存的真实原值。
			if (g_originalSite[i] != siteAddr && currentIsPatch)
			{
				// 地址变化且当前地址已经是我们写入的补丁形态：无法安全推断新地址的原值。
				std::println("[AimAid] {} : patch site moved while patch bytes are present, skipped", spec.name);
				continue;
			}
			else if (g_originalSite[i] != siteAddr)
			{
				std::memcpy(g_original + i * 16, current, spec.byteCount);
				g_originalSite[i] = siteAddr;
			}
		}
		else
		{
			std::memcpy(g_original + i * 16, current, spec.byteCount);
			g_originalSite[i] = siteAddr;
		}

		g_haveOriginal[i] = true;
		g_site[i].store(siteAddr);
		g_resolved[i].store(true);
		++resolvedCount;

		std::println("[AimAid] {} resolved @ 0x{:X} (base+0x{:X})", spec.name, siteAddr,
			siteAddr - DMA::BaseAddress);
	}

	std::println("[AimAid] {}/{} patch sites resolved.", resolvedCount, kSpecCount);

	// 搜索用的副本不再需要
	g_textBytes.clear();
	g_textBytes.shrink_to_fit();

	return resolvedCount > 0;
}

bool AimAid::RestoreAll()
{
	if (!DMA::IsReady())
		return false;

	bool allRestored = true;
	for (int i = 0; i < kSpecCount; ++i)
	{
		if (!g_resolved[i].load() || !g_haveOriginal[i])
			continue;

		const std::uintptr_t site = g_site[i].load();
		if (!site)
		{
			allRestored = false;
			continue;
		}

		const PatchSpec& spec = kSpecs[i];
		const uint8_t* original = g_original + i * 16;
		uint8_t current[16]{};
		if (!DMA::Memory().Read(site, current, spec.byteCount))
		{
			allRestored = false;
			continue;
		}

		if (std::memcmp(current, original, spec.byteCount) != 0)
		{
			if (!DMA::Memory().Write(site, original, spec.byteCount))
			{
				std::println("[AimAid] {} : restore write failed", spec.name);
				allRestored = false;
				continue;
			}

			uint8_t verify[16]{};
			if (!DMA::Memory().Read(site, verify, spec.byteCount) ||
			    std::memcmp(verify, original, spec.byteCount) != 0)
			{
				g_contended[i].fetch_add(1);
				std::println("[AimAid] {} : restore verify failed", spec.name);
				allRestored = false;
				continue;
			}
		}

		g_applied[i].store(false);
		if (g_loggedApplied[i])
		{
			g_loggedApplied[i] = false;
			std::println("[AimAid] {} restored", spec.name);
		}
	}
	return allRestored;
}

bool AimAid::PrepareForClose()
{
	const bool restored = RestoreAll();
	ClearScanState();
	return restored;
}

void AimAid::OnDMAFrame()
{
	if (!DMA::IsReady())
		return;

	// 每个补丁的目标状态由开关决定
	const bool want[kSpecCount] = {
		bAssistedAim.load(),    // A
		bAssistedAim.load(),    // B
		bAimForHead.load(),     // C
		bDriverLockOn.load(),   // D
	};

	const uint32_t now = GetTickCount();

	for (int i = 0; i < kSpecCount; ++i)
	{
		if (!g_resolved[i].load() || !g_haveOriginal[i])
			continue;

		const std::uintptr_t site = g_site[i].load();
		if (!site)
			continue;

		const PatchSpec& spec = kSpecs[i];
		const uint8_t* expected = want[i] ? spec.bytes : (g_original + i * 16);

		// 先读当前字节：只有与目标不一致时才写。
		// 这样「被别人改回」的情况才会触发重写，稳态下零写流量。
		uint8_t check[16]{};
		if (!DMA::Memory().Read(site, check, spec.byteCount))
			continue;

		if (std::memcmp(check, expected, spec.byteCount) != 0)
		{
			// 节流重试：每处最多 250ms 一次，避免与其它工具互相刷写拖垮 PCIe
			if (g_lastAttempt[i] != 0 && (now - g_lastAttempt[i]) < 250)
				continue;
			g_lastAttempt[i] = now;

			// 对方改过之后可能不是我们记录的原始字节；还原时按实际读到的值处理
			if (!want[i] && std::memcmp(check, spec.bytes, spec.byteCount) != 0)
			{
				// 内容既不是 patch 也不是我们记住的原始值——说明第三方改成了别的值。
				// 此时把实际读到的字节当作新的「原始值」再还原，保证我们不留残迹。
				std::memcpy(g_original + i * 16, check, spec.byteCount);
			}

			if (!DMA::Memory().Write(site, expected, spec.byteCount))
				continue;

			uint8_t verify[16]{};
			if (!DMA::Memory().Read(site, verify, spec.byteCount))
				continue;

			if (std::memcmp(verify, expected, spec.byteCount) != 0)
			{
				// 写入被立刻覆盖：记录争用，供 UI 显示
				g_contended[i].fetch_add(1);
				continue;
			}
		}

		// 到了这里，内存状态已经等于目标状态
		g_applied[i].store(want[i]);

		if (want[i])
		{
			if (!g_loggedApplied[i])
			{
				g_loggedApplied[i] = true;
				std::println("[AimAid] {} applied", spec.name);
			}
		}
		else
		{
			// 关闭态：只有在确实还原成功后才记录一次
			if (g_loggedApplied[i])
			{
				g_loggedApplied[i] = false;
				std::println("[AimAid] {} restored", spec.name);
			}
		}
	}
}

bool AimAid::IsResolved(PatchId id)
{
	const int i = static_cast<int>(id);
	return i >= 0 && i < kSpecCount && g_resolved[i].load();
}

bool AimAid::IsApplied(PatchId id)
{
	const int i = static_cast<int>(id);
	return i >= 0 && i < kSpecCount && g_applied[i].load();
}

uintptr_t AimAid::GetSite(PatchId id)
{
	const int i = static_cast<int>(id);
	return (i >= 0 && i < kSpecCount) ? g_site[i].load() : 0;
}

const char* AimAid::GetName(PatchId id)
{
	const int i = static_cast<int>(id);
	return (i >= 0 && i < kSpecCount) ? kSpecs[i].name : "?";
}

int AimAid::GetContentionCount(PatchId id)
{
	const int i = static_cast<int>(id);
	return (i >= 0 && i < kSpecCount) ? g_contended[i].load() : 0;
}

int AimAid::GetResolvedCount()
{
	int n = 0;
	for (int i = 0; i < kSpecCount; ++i)
		if (g_resolved[i].load()) ++n;
	return n;
}

void AimAid::Reset()
{
	ClearScanState();
	for (int i = 0; i < kSpecCount; ++i)
		g_haveOriginal[i] = false;
	std::memset(g_original, 0, sizeof(g_original));
	std::memset(g_originalSite, 0, sizeof(g_originalSite));
	g_processId = 0;
	g_processBase = 0;
	g_hasProcessIdentity = false;
}
