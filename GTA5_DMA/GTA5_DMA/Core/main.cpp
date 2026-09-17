#include "pch.h"
#include <print>
#include <string>
#include <thread>

#include "MyImGui.h"
#include "BanCheck.h"
#include "PhoneSilencer.h"
#include "GodMode.h"
#include "AppRuntime.h"
#include "InputManager.h"
#include "DMA.h"
#include "Tunables.h"
#include "ScriptGlobals.h"
#include "ScriptThreads.h"
#include "EconomyFeatures.h"
#include "Diagnostics.h"
#include "HeistSetup.h"
#include "OffRadar.h"
#include "TimeControl.h"
#include <map>
#include <unordered_map>
#include "PlayerList.h"
#include "VehicleNameOverrides.h"
#include "VehicleList.h"
#include "NetTimeScan.h"


int main(int argc, char** argv)
{
	// 无界面自检模式：用本体代码路径做 tunable 写入实证（--tunable-selftest）
	// 存在理由：python + ctypes 探针在设备被上一次强杀占用时会在 vmm 初始化里直接崩，
	//           而本体走的是同一条 DMA 写入路径，且能受控还原。
	if (argc > 1 && std::string(argv[1]) == "--tunable-selftest")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[selftest] DMA 初始化失败（设备被占用或未连接）" << std::endl;
			return 1;
		}
		const int rc = Tunables::SelfTest();
		DMA::Close();
		return rc;
	}

	// 经济动作格写入自检（--economy-selftest）：同一套 ScriptGlobals 写入路径
	if (argc > 1 && std::string(argv[1]) == "--economy-selftest")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[economy-selftest] DMA 初始化失败（设备被占用或未连接）" << std::endl;
			return 1;
		}
		ScriptThreads::LogRunningScripts(8);
		const int rc = EconomyFeatures::SelfTest();
		DMA::Close();
		return rc;
	}

	// 游戏时钟实测（--clock-probe）
	// 写入闸门演练（--gate-drill）：制造真漂移（改成别的值），断言工具自愈后仍能写入
	if (argc > 1 && std::string(argv[1]) == "--gate-drill")
	{
		if (!DMA::Initialize())
		{
			std::printf("DMA 初始化失败\n");
			return 1;
		}
		ScriptGlobals::Resolve();
		const int idx = ScriptGlobals::Find("GTA_PLUS_ENABLED");
		if (idx < 0 || !ScriptGlobals::IsResolved(static_cast<uint32_t>(idx)))
		{
			std::printf("[drill] 全局未解析（不在线上战局？）→ 跳过\n");
			DMA::Close();
			return 2;
		}
		const uintptr_t addr = ScriptGlobals::GetAddress(static_cast<uint32_t>(idx));
		const int32_t baseline = ScriptGlobals::ReadLive(static_cast<uint32_t>(idx), nullptr);
		const int32_t target   = baseline;                 // 我们要维持的值
		const int32_t drifted  = (baseline == 0) ? 1 : 0;  // 模拟目标把它改成别的值
		std::printf("[drill] 槽位=%d 地址=0x%llX 基线=%d 漂移值=%d\n", idx,
		            static_cast<unsigned long long>(addr), baseline, drifted);

		// ① 绕过闸门写入漂移值（模拟目标自己重置/别的工具改了它）
		{
			int32_t d = drifted;
		    DMA::Memory().Write(addr, &d, sizeof(d));
		}
		const int32_t afterDrift = ScriptGlobals::ReadLive(static_cast<uint32_t>(idx), nullptr);

		// ② 走正常写入路径：前若干次会被闸门拒绝（值既非原值也非上次写入），
		//    连续 24 次后应触发「自动重新解析 + 重新基线」，随后写入成功
		bool wrote = false;
		int attempts = 0;
		for (; attempts < 40 && !wrote; ++attempts)
		    wrote = ScriptGlobals::Write(static_cast<uint32_t>(idx), target, "drill");
		const int32_t afterWrite = ScriptGlobals::ReadLive(static_cast<uint32_t>(idx), nullptr);

		// ③ 还原到基线
		{
			int32_t back = baseline;
		    DMA::Memory().Write(addr, &back, sizeof(back));
		}
		const int32_t restored = ScriptGlobals::ReadLive(static_cast<uint32_t>(idx), nullptr);

		const bool pass = wrote && afterWrite == target && restored == baseline;
		std::printf("[drill] 模拟漂移为 %d（读到 %d）→ 正常写入尝试 %d 次 %s，读到 %d → 还原 %d → %s\n",
		            drifted, afterDrift, attempts, wrote ? "成功" : "仍失败", afterWrite, restored,
		            pass ? "PASS" : "FAIL");
		std::printf("[drill] 自动重基线次数=%d 拒绝写入计数=%d\n",
		            ScriptGlobals::GetRebaselineCount(), ScriptGlobals::GetBlockedWriteCount());
		DMA::Close();
		return pass ? 0 : 2;
	}

	// 一键关停写入类开关（--all-off / --god-off）：把无敌相关的写入位清零并读回
	if (argc > 1 && (std::string(argv[1]) == "--all-off" || std::string(argv[1]) == "--god-off"))
	{
		if (!DMA::Initialize())
		{
			std::printf("DMA 初始化失败\n");
			return 1;
		}
		DMA::UpdateEssentials();
		std::printf("[off] 本地玩家 ped=0x%llX 载具=0x%llX\n",
			static_cast<unsigned long long>(DMA::LocalPlayerAddress),
			static_cast<unsigned long long>(DMA::VehicleAddress));
		GodMode::bPlayerGodMode.store(false);
		GodMode::bVehicleGodMode.store(false);
		const bool okP = GodMode::PlayerSet(false);
		std::printf("[off] 玩家无敌清零：%s\n", okP ? "已写入" : "写入失败/不在玩家");
		if (DMA::VehicleAddress)
		{
			const bool okV = GodMode::VehicleSet(false);
			std::printf("[off] 载具无敌清零：%s\n", okV ? "已写入" : "写入失败");
		}
		else
		{
			std::printf("[off] 当前不在载具里，跳过载具无敌\n");
		}
		// 读回校验：玩家 god bits（bit4 + bit8）应为 0
		{
			uint32_t bits = 0xFFFFFFFFu;
			const uintptr_t addr = DMA::LocalPlayerAddress + offsetof(PED, GodFlags);
			if (DMA::LocalPlayerAddress && DMA::Memory().Read(addr, &bits, sizeof(bits)))
			{
				const bool god = ((bits >> 4) & 1u) != 0 && ((bits >> 8) & 1u) != 0;
				std::printf("[off] 读回校验：GodFlags=0x%08X → 无敌位 %s\n", bits, god ? "仍为开 ✗" : "已关 ✓");
			}
			else
			{
				std::printf("[off] 读回失败（读不到玩家）\n");
			}
		}
		DMA::Close();
		return 0;
	}

	// 静音来电探针（--phone-probe）：打印 4 个脚本全局的地址与当前值
	if (argc > 1 && std::string(argv[1]) == "--phone-probe")
	{
		if (!DMA::Initialize())
		{
			std::printf("DMA 初始化失败\n");
			return 1;
		}
		const PhoneSilencer::Snapshot s = PhoneSilencer::Read();
		std::printf("[phone] 状态                    idx 23040 addr=0x%llX value=%d\n",
			static_cast<unsigned long long>(s.addrState), s.state);
		std::printf("[phone] 通话进行中              idx 23046 addr=0x%llX value=%d\n",
			static_cast<unsigned long long>(s.addrProgress), s.progress);
		std::printf("[phone] 有来电                  idx 23050 addr=0x%llX value=%d\n",
			static_cast<unsigned long long>(s.addrIncoming), s.incoming);
		std::printf("[phone] 来电角色                idx  8818 addr=0x%llX value=%d\n",
			static_cast<unsigned long long>(s.addrCaller), s.caller);
		std::printf("[phone] 全部可解析: %s\n", s.ok ? "是" : "否（多半不在线上战局）");
		if (s.ok && argc > 2 && std::string(argv[2]) == "--write-test")
		{
			const int32_t before = s.state;
			const int32_t target = 6;
			int32_t after = 0;
			bool ok = DMA::Memory().Write(s.addrState, &target, sizeof(target));
			DMA::Memory().Read(s.addrState, &after, sizeof(after));
			std::printf("[phone] 写测试：%d → 写 6 → 读回 %d（%s）\n",
				before, after, (ok && after == 6) ? "通过" : "未通过");
			// 还原原值，避免留下副作用
			int32_t back = before;
			DMA::Memory().Write(s.addrState, &back, sizeof(back));
			DMA::Memory().Read(s.addrState, &after, sizeof(after));
			std::printf("[phone] 已还原为 %d（读回 %d）\n", before, after);
		}
		DMA::Close();
		return s.ok ? 0 : 2;
	}

	// BE 封禁查询自检（--ban-check <RID>）：不需要游戏/DMA，纯 BE 服务端库查询
	if (argc > 2 && std::string(argv[1]) == "--ban-check")
	{
		const long long rid = std::atoll(argv[2]);
		std::printf("[BanCheck] 查询 RID %lld ...\n", rid);
		if (!BanCheck::Available())
		{
			std::printf("[BanCheck] 不可用: %s\n", BanCheck::LastError().c_str());
			return 1;
		}
		if (!BanCheck::Start(rid))
		{
			std::printf("[BanCheck] 启动失败: %s\n", BanCheck::LastError().c_str());
			return 1;
		}
		const int st = BanCheck::WaitFor(rid, 20000);   // 并行引擎内部自己 Tick
		switch (st)
		{
		case 2:
			std::printf("[BanCheck] RID %lld -> 已封禁 | 理由: %s\n", rid, BanCheck::Reason().c_str());
			return 0;
		case 3:
			std::printf("[BanCheck] RID %lld -> 未封禁（无 BE 封禁记录）\n", rid);
			return 0;
		default:
			std::printf("[BanCheck] RID %lld -> 查询未完成/失败\n", rid);
			return 2;
		}
	}

	if (argc > 1 && std::string(argv[1]) == "--clock-probe")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[clock] DMA 初始化失败" << std::endl;
			return 1;
		}
		const int rc = TimeControl::Probe();
		DMA::Close();
		return rc;
	}

	// 载具指针偏移定位（--vehicle-hunt）：走载具池拿真实载具地址 → 在本地 ped 里搜指向它们的指针
	if (argc > 1 && std::string(argv[1]) == "--vehicle-hunt")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[hunt] DMA 初始化失败" << std::endl;
			return 1;
		}
		// ---- ① 走载具池（VehiclePoolPtr → fwVehiclePool** → fwVehiclePool）----
		const uintptr_t poolSlot = DMA::BaseAddress + Offsets::VehiclePoolPtr;
		uintptr_t poolIndirect = 0, poolAddr = 0;
		DMA::Memory().Read(poolSlot, &poolIndirect, sizeof(poolIndirect));
		std::println("");
		std::println("=== 载具池链 ===");
		std::println("  poolSlot=0x{:X} → poolIndirect=0x{:X}", poolSlot, poolIndirect);
		if (poolIndirect) DMA::Memory().Read(poolIndirect, &poolAddr, sizeof(poolAddr));
		std::println("  poolAddr=0x{:X}", poolAddr);
		if (!poolAddr) { std::println("  ✗ 池链断了"); DMA::Close(); return 1; }

		// 用仓库里已有的 FwVehiclePool 定义（Reclass.h）：vtbl@0x00 / 池指针@0x08 / 大小@0x10 / 位图@0x38 / 数量@0x64
		FwVehiclePool pool{};
		if (!DMA::Memory().Read(poolAddr, &pool, sizeof(pool)))
		{ std::println("  ✗ 池结构读取失败"); DMA::Close(); return 1; }
		std::println("  m_PoolAddress=0x{:X} m_BitArray=0x{:X} m_Size={} m_ItemCount={}",
			(uintptr_t)pool.m_PoolAddress, (uintptr_t)pool.m_BitArray, pool.m_Size, pool.m_ItemCount);
		if (!pool.m_PoolAddress || !pool.m_BitArray || !pool.m_Size || pool.m_Size > 8192) { std::println("  ✗ 池字段不合理"); DMA::Close(); return 1; }

		const uint32_t flagBytes = ((pool.m_Size + 31) / 32) * 4;
		std::vector<uint8_t> flags(flagBytes);
		if (!DMA::Memory().Read(reinterpret_cast<uintptr_t>(pool.m_BitArray), flags.data(), flagBytes)) { std::println("  ✗ 位图读取失败"); DMA::Close(); return 1; }

		std::vector<uintptr_t> vehAddrs;
		std::vector<uint32_t> vehHashes;
		for (uint32_t i = 0; i < pool.m_Size && vehAddrs.size() < 40; ++i)
		{
			if (!((flags[i >> 3] >> (i & 7)) & 1)) continue;
			uintptr_t a = 0;
			if (!DMA::Memory().Read(reinterpret_cast<uintptr_t>(pool.m_PoolAddress) + i * sizeof(uintptr_t), &a, sizeof(a)) || !a) continue;
			// 第27轮修正：模型哈希 = m_ModelInfo(+0x20) 指向结构的 +0x18
			uint32_t h = 0;
			{
				uintptr_t mi2 = 0;
				if (DMA::Memory().Read(a + 0x20, &mi2, sizeof(mi2)) && mi2)
					DMA::Memory().Read(mi2 + 0x18, &h, sizeof(h));
			}
			vehAddrs.push_back(a);
			vehHashes.push_back(h);
		}
		std::println("");
		// ---- 用全量表统计定位「模型哈希字段」----
		// 判据：模型哈希字段对**所有**车都应能在 921 条全量表里命中；
		//       其他 32 位字段（标志位/坐标/句柄）命中率极低。
		{
			std::println("");
			std::println("=== 模型哈希字段定位（全量表 {} 条）===", kVehicleNameCount);

			const int kMaxVeh = 25;
			const int kVehScan = 0x100;
			const int kMiScan = 0x100;

			std::vector<int> vehHits(kVehScan / 4, 0);
			std::vector<int> miHits(kMiScan / 4, 0);
			std::unordered_map<uintptr_t, int> miSeen;
			int sampled = 0;
			int miSame = 0;

			for (size_t i = 0; i < vehAddrs.size() && sampled < kMaxVeh; ++i)
			{
				std::vector<uint8_t> vb(kVehScan);
				if (!DMA::Memory().Read(vehAddrs[i], vb.data(), vb.size()))
					continue;

				// ① 载具自身结构里直接找
				for (int off = 0; off + 4 <= kVehScan; off += 4)
				{
					uint32_t v = 0;
					std::memcpy(&v, vb.data() + off, 4);
					if (LookupVehicleNameEx(v))
						vehHits[off / 4]++;
				}

				// ② 顺着 m_ModelInfo(+0x20) 进去找（YimMenuV2: fwEntity::m_ModelInfo @ +0x20）
				uintptr_t mi = 0;
				std::memcpy(&mi, vb.data() + 0x20, sizeof(mi));
				if (mi >= 0x10000)
				{
					std::vector<uint8_t> mb(kMiScan);
					if (DMA::Memory().Read(mi, mb.data(), mb.size()))
					{
						const int seen = miSeen[mi]++;
						if (seen > 0)
							++miSame;   // 同一个模型信息被多辆车共用（同型号）
						for (int off = 0; off + 4 <= kMiScan; off += 4)
						{
							uint32_t v = 0;
							std::memcpy(&v, mb.data() + off, 4);
							if (LookupVehicleNameEx(v))
								miHits[off / 4]++;
						}
						++sampled;
					}
				}
			}

			std::println("  采样 {} 辆车；其中 {} 辆的 m_ModelInfo 指针与别的车相同（同型号共用 → 指针链正确）", sampled, miSame);
			std::println("");
			std::println("  载具结构自身命中率（前几名）：");
			for (int k = 0; k < (int)vehHits.size(); ++k)
				if (vehHits[k] > 0)
					std::println("    +0x{:02X}：{}/{}", k * 4, vehHits[k], sampled);
			std::println("  模型信息结构命中率（前几名）：");
			for (int k = 0; k < (int)miHits.size(); ++k)
				if (miHits[k] > 0)
					std::println("    +0x{:02X}：{}/{}", k * 4, miHits[k], sampled);
		}
		// 数据驱动定位模型哈希字段：统计每个偏移在全部车上的「不同取值数」
		{
			std::vector<std::vector<uint8_t>> blobs(vehAddrs.size());
			for (size_t i = 0; i < vehAddrs.size(); ++i)
			{
				blobs[i].resize(0x400);
				DMA::Memory().Read(vehAddrs[i], blobs[i].data(), blobs[i].size());
			}
			std::println("");
			std::println("=== 各偏移的不同取值数（模型哈希字段应约等于车辆数）===");
			for (int off = 0; off + 4 <= 0x400; off += 4)
			{
				std::map<uint32_t, int> distinct;
				for (const auto& b : blobs)
				{
					uint32_t v = 0;
					std::memcpy(&v, b.data() + off, 4);
					distinct[v]++;
				}
				if ((int)distinct.size() < (int)vehAddrs.size() - 4) continue;
				int tableHits = 0;
				for (const auto& kv : distinct) if (LookupVehicleNameEx(kv.first)) ++tableHits;
				std::println("    +0x{:03X}  不同取值 {} / {}   名表命中 {}", off, distinct.size(), vehAddrs.size(), tableHits);
				for (const auto& kv : distinct)
				{
					const VehicleNameEntry* e = LookupVehicleNameEx(kv.first);
					if (e) std::println("        ★ 0x{:08X} → {}（{}）", kv.first, e->cn, e->model);
				}
			}
		}
		std::println("=== 池内真实载具（前 {} 辆）===", vehAddrs.size());
		for (size_t i = 0; i < vehAddrs.size(); ++i)
		{
			const VehicleNameEntry* e = LookupVehicleNameEx(vehHashes[i]);
			float h280 = 0.0f;
			DMA::Memory().Read(vehAddrs[i] + 0x280, &h280, sizeof(h280));
			std::println("  [{:>2}] 0x{:X}  +0x18=0x{:08X}  +0x280={:.0f}  {}", i, vehAddrs[i], vehHashes[i], h280,
				e ? e->cn : "（+0x18 不是名表里的模型哈希）");
		}

		// ---- ② 在本地 ped 里搜「指向上述载具地址的指针」 ----
		if (vehAddrs.empty()) { std::println("  ✗ 池里没载具，无法定位 ped 载具指针"); DMA::Close(); return 1; }
		std::println("");
		std::println("=== 本地 ped 0x{:X} 中指向池内载具的指针 ===", DMA::LocalPlayerAddress);
		std::vector<uint8_t> pedBuf(0x2000);
		if (!DMA::Memory().Read(DMA::LocalPlayerAddress, pedBuf.data(), pedBuf.size()))
		{ std::println("  ✗ ped 读取失败"); DMA::Close(); return 1; }
		int found = 0;
		for (size_t off = 0; off + 8 <= pedBuf.size(); off += 8)
		{
			uintptr_t v = 0;
			std::memcpy(&v, pedBuf.data() + off, 8);
			for (size_t k = 0; k < vehAddrs.size(); ++k)
			{
				if (v == vehAddrs[k])
				{
					const VehicleNameEntry* e = LookupVehicleNameEx(vehHashes[k]);
					std::println("  ★ +0x{:04X} → 载具 0x{:X}（{}）", off, v, e ? e->cn : "未收录");
					++found;
				}
			}
		}
		std::println("  命中 {} 处（这个偏移就是 ped 的载具指针）", found);
		DMA::Close();
		return 0;
	}

	// 载具模型字段定位（--vehicle-dump）：dump 本地玩家载具前 0x100 字节，用名表反查真字段
	if (argc > 1 && std::string(argv[1]) == "--vehicle-dump")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[dump] DMA 初始化失败" << std::endl;
			return 1;
		}
		for (int i = 0; i < 3; ++i) { PlayerList::OnDMAFrame(); Sleep(500); }
		const auto snap = PlayerList::GetSnapshot();
		uintptr_t veh = 0;
		const char* vehOwner = nullptr;
		int localSeen = 0;
		for (const auto& p : snap)
		{
			if (p.IsLocal) ++localSeen;
			uintptr_t cand = 0;
			DMA::Memory().Read(p.PedAddress + 0xD10, &cand, sizeof(cand));
			if (cand != 0 && veh == 0) { veh = cand; vehOwner = p.Name; }
			if (cand != 0 && p.IsLocal) { veh = cand; vehOwner = p.Name; break; }
		}
		std::println("  快照里标记为本地玩家的数量：{}", localSeen);
		std::println("");
		std::println("=== 载具结构 dump（车主 {} 的载具 0x{:X}）===", vehOwner ? vehOwner : "?", veh);
		if (!veh) { std::println("  快照里没有任何玩家在载具中 ✗"); DMA::Close(); return 1; }
		// 在「载具结构区」与「本地玩家 ped 区」各扫 0x1400 字节，用名表反查模型哈希字段
		for (int pass = 0; pass < 2; ++pass)
		{
			uintptr_t base = (pass == 0) ? veh : 0;
			if (pass == 1)
			{
				for (const auto& p : snap)
					if (p.IsLocal) { base = p.PedAddress; break; }
			}
			if (!base) { std::println("  [{}] 无基址，跳过", pass == 0 ? "载具区" : "本地 ped 区"); continue; }
			std::vector<uint8_t> buf(0x1400);
			if (!DMA::Memory().Read(base, buf.data(), buf.size())) { std::println("  [0x{:X}] 读取失败", base); continue; }
			std::println("  —— 扫描 0x{:X}（{}）——", base, pass == 0 ? "载具区" : "本地 ped 区");
			for (size_t off = 0; off + 4 <= buf.size(); off += 4)
			{
				uint32_t v = 0;
				std::memcpy(&v, buf.data() + off, 4);
				const VehicleNameEntry* e = LookupVehicleNameEx(v);
				if (e)
					std::println("    ★ +0x{:03X} = 0x{:08X} → {}（{}）", off, v, e->cn, e->model);
			}
		}
		DMA::Close();
		return 0;
	}

	// 世界载具名表探针（--vehicles-probe）：只读，打印世界载具的模型哈希与解析名
	if (argc > 1 && std::string(argv[1]) == "--vehicles-probe")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[vehicles] DMA 初始化失败" << std::endl;
			return 1;
		}
		for (int i = 0; i < 3; ++i)
		{
			VehicleList::OnDMAFrame();
			Sleep(500);
		}
		const auto vs = VehicleList::GetSnapshot();
		std::println("");
		std::println("=== 世界载具名表实测（只读）===");
		std::println("  载具数 {}", vs.size());
		int named = 0;
		for (const auto& v : vs)
		{
			const VehicleNameEntry* e = LookupVehicleNameEx(v.ModelHash);
			const char* nm = e ? e->model : nullptr;
			if (nm)
				++named;
			std::println("  [{:>2}] 0x{:08X}  {}", v.DisplayIndex, v.ModelHash,
				nm ? (e ? e->cn : nm) : "（未收录）");
		}
		std::println("");
		std::println("  收录取名 {}/{}", named, vs.size());
		DMA::Close();
		return 0;
	}

	// 战局玩家载具情报探针（--players-probe）：只读，打印每人所在载具型号与血量
	if (argc > 1 && std::string(argv[1]) == "--players-probe")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[players] DMA 初始化失败" << std::endl;
			return 1;
		}
		for (int i = 0; i < 3; ++i)
		{
			PlayerList::OnDMAFrame();
			Sleep(600);
		}
		const auto snap = PlayerList::GetSnapshot();
		std::println("");
		std::println("=== 战局玩家载具情报（只读）===");
		std::println("  玩家数 {}", snap.size());
		// 原始字段核对：PED+0xD10（载具指针）与 PED+0xE30（在载具位所在字）
		std::println("");
		std::println("  —— PED 载具字段原始值（只读核对）——");
		int dumpCount = 0;
		for (const auto& p : snap)
		{
			if (dumpCount >= 5) break;
			++dumpCount;
			uintptr_t vptr = 0;
			uint32_t bits = 0;
			DMA::Memory().Read(p.PedAddress + 0xD10, &vptr, sizeof(vptr));
			DMA::Memory().Read(p.PedAddress + 0xE30, &bits, sizeof(bits));
			std::println("    {:<18} ped 0x{:X}  +0xD10=0x{:X}  +0xE30=0x{:08X}（bit0={}）",
				p.Name, p.PedAddress, vptr, bits, bits & 1);
		}

		for (const auto& p : snap)
		{
			std::println("  [{:>2}] {:<18} RID {:<12} 血 {:>5.0f} 甲 {:>5.0f} 距 {:>6.0f}m {}",
				p.DisplayIndex, p.Name, p.RockstarId, p.Health, p.Armor, p.Distance,
				p.IsLocal ? "（本地）" : "");
			if (p.VehicleModel != 0)
			{
				if (p.VehicleName)
					std::println("        载具：{}（{}，模型 {}）车身 {:.0f} 引擎 {:.0f}",
						p.VehicleName->cn, p.VehicleName->model, p.VehicleName->kind,
						p.VehicleHealth, p.VehicleEngineHealth);
				else
					std::println("        载具：未收录模型 0x{:08X} 车身 {:.0f} 引擎 {:.0f}",
						p.VehicleModel, p.VehicleHealth, p.VehicleEngineHealth);
			}
			else
			{
				std::println("        载具：无（{}）", p.InVehicle ? "在载具中但读不到模型" : "步行");
			}
		}
		DMA::Close();
		return 0;
	}

	// 游戏时钟重新定位（--clock-scan）
	if (argc > 1 && std::string(argv[1]) == "--clock-scan")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[clock] DMA 初始化失败" << std::endl;
			return 1;
		}
		const int rc = TimeControl::ClockScan();
		DMA::Close();
		return rc;
	}

	// 游戏时钟写入自检（--clock-selftest）
	if (argc > 1 && std::string(argv[1]) == "--clock-selftest")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[clock] DMA 初始化失败" << std::endl;
			return 1;
		}
		const int rc = TimeControl::SelfTest();
		DMA::Close();
		return rc;
	}

	// 网络时间深层探针（--netptr-probe）
	if (argc > 1 && std::string(argv[1]) == "--netptr-probe")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[netptr] DMA 初始化失败" << std::endl;
			return 1;
		}
		const int rc = DMA::NetworkTimeDeepProbe();
		DMA::Close();
		return rc;
	}

	// 雷达隐身只读体检（--offradar-probe）
	if (argc > 1 && std::string(argv[1]) == "--offradar-probe")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[radar] DMA 初始化失败" << std::endl;
			return 1;
		}
		const int rc = OffRadar::Probe();
		DMA::Close();
		return rc;
	}

	// 网络时间基准扫描（--time-scan）：雷达隐身要写网络时间戳
	if (argc > 1 && std::string(argv[1]) == "--time-scan")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[time] DMA 初始化失败" << std::endl;
			return 1;
		}
		const int rc = NetTimeScan::Scan();
		DMA::Close();
		return rc;
	}

	// 抢劫分账写入自检（--heist-selftest）：写 → 读回 → 还原
	if (argc > 1 && std::string(argv[1]) == "--heist-selftest")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[heist] DMA 初始化失败" << std::endl;
			return 1;
		}
		const int rc = HeistSetup::SelfTest();
		DMA::Close();
		return rc;
	}

	// 抢劫分账相关全局索引实测
	if (argc > 1 && std::string(argv[1]) == "--heist-probe")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[heist] DMA 初始化失败\n";
			return 1;
		}
		const int rc = HeistSetup::Probe();
		DMA::Close();
		return rc;
	}

	// 诊断导出模式（--diag [路径]）：把本次解析结果落盘，供外部阅读（设备独占，外部进程拿不到）
	if (argc > 1 && std::string(argv[1]) == "--diag")
	{
		AppRuntime::Reset();
		if (!DMA::Initialize())
		{
			std::cerr << "[diag] DMA 初始化失败（设备被占用或未连接）" << std::endl;
			return 1;
		}
		const char* path = (argc > 2) ? argv[2] : nullptr;
		const bool ok = Diagnostics::WriteReport(path);
		DMA::Close();
		return ok ? 0 : 1;
	}

	AppRuntime::Reset();
	// Initialize ImGui
	if (!MyImGui::Initialize())
	{
		std::cerr << "Failed to initialize ImGui" << std::endl;
		return -1;
	}

	// Initialize DMA in a separate thread
	std::thread DMAThread([]() {
		if (!DMA::Initialize())
		{
			std::cerr << "Failed to initialize DMA" << std::endl;
			return;
		}

		if (!g_inputManager.InitKeyboard())
		{
			std::cerr << "Failed to initialize target keyboard input; local hotkeys remain available." << std::endl;
		}

		DMA::DMAThreadEntry();
	});

	// Main loop
	while (AppRuntime::IsRunning())
	{
		// Handle exit keys
		if ((GetAsyncKeyState(VK_END) & 1) || g_inputManager.IsKeyPressed(VK_END))
			AppRuntime::RequestStop();

		// BE 封禁查询：驱动 BE 网络循环（无查询时是空操作）
		BanCheck::Tick();

		// Render ImGui frame
		MyImGui::OnFrame();
	}

	// Clean up
	if (DMAThread.joinable())
		DMAThread.join();

	MyImGui::Close();

	return 0;
}
