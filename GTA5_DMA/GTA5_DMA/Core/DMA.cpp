#include "pch.h"

#include "Offsets.h"

#include "OffsetResolver.h"
#include "PatternScanner.h"

#include <cstdio>

#include "Features.h"
#include "OffRadar.h"
#include "Diagnostics.h"
#include "RuntimeTables.h"
#include "VehicleList.h"
#include "ArmorManager.h"
#include "HealthManager.h"
#include "AppRuntime.h"

// 定义全局游戏类型
GameType currentGameType = GameType::Unknown;

// 定义 Offsets 命名空间中的外部变量
uintptr_t Offsets::WorldPtr = Offsets::WorldPtr_Enhanced;
uintptr_t Offsets::GlobalPtr = Offsets::GlobalPtr_Enhanced;
uintptr_t Offsets::BlipPtr = Offsets::BlipPtr_Enhanced;
uintptr_t Offsets::TimeBasePtr = Offsets::TimeBasePtr_Enhanced;
uintptr_t Offsets::PlayerMgrPtr = Offsets::PlayerMgrPtr_Enhanced;
uintptr_t Offsets::AimCPedPtr = Offsets::AimCPedPtr_Enhanced;
uintptr_t Offsets::WaypointPtr = Offsets::WaypointPtr_Enhanced;
uintptr_t Offsets::LocalScriptsPtr = Offsets::LocalScriptsPtr_Enhanced;
uintptr_t Offsets::GTAPlusPtr = Offsets::GTAPlusPtr_Enhanced;
uintptr_t Offsets::PedPoolPtr = Offsets::PedPoolPtr_Enhanced;
uintptr_t Offsets::VehiclePoolPtr = Offsets::VehiclePoolPtr_Enhanced;
uintptr_t Offsets::NetworkTimePtr = Offsets::NetworkTimePtr_Enhanced;    // 第26轮：网络时间全局（雷达隐身）

MemoryBackend& DMA::Memory() noexcept
{
	static MemoryBackend backend;
	return backend;
}


bool DMA::Initialize()
{
	// 第19轮：运行时表要在**解析偏移/tunable 之前**就位 ——
	// 「名字/值 → 索引」和「特征码追加候选」都在这里读进来（外部文件改动立刻生效，不必重编译）。
	RuntimeTables::Initialize();

	LPCSTR args[] = { "", "-device", "FPGA" };

	vmh = VMMDLL_Initialize(3, args);

	if (!vmh)
	{
		std::println("VMMDLL_Initialize failed.");
		return 0;
	}

	if (VMMDLL_PidGetFromName(vmh, "GTA5_Enhanced.exe", &PID))
	{
		if (!PID)
		{
			std::println("GTA5_Enhanced.exe PID is null.");
			return 0;
		}

		BaseAddress = VMMDLL_ProcessGetModuleBaseU(vmh, PID, "GTA5_Enhanced.exe");

		if (!BaseAddress)
		{
			std::println("GTA5_Enhanced.exe BaseAddress is null.");
			return 0;
		}

		std::println("GTA5_Enhanced.exe found @ {0:x}\n", BaseAddress);
		// 设置GTA5_Enhanced.exe的偏移量
		Offsets::SetOffsetsByPackageName("GTA5_Enhanced.exe");
		Memory().Attach(vmh, PID);
		// 特征码动态解析偏移（仅 Enhanced；失败自动回退静态值）
		ResolveRuntimeOffsets();
		// 自瞄补丁点解析（四处特征码；任一失败则该补丁保持禁用）
		AimAid::Resolve();
		// 空闲踢出 tunable 只依赖 GlobalPtr，不依赖本地玩家世界链。
		// 在这里解析可避免玩家尚未进入战局时主循环提前失败而始终无法探测。
		NoIdleKick::Resolve();
		// tunable 块：值锚动态定位 + 默认值体检（进度类功能与踢出保护都依赖它）。
		Tunables::Resolve();

		// 第17轮：脚本线程（只读枚举，为下一轮 locals 写入打底）+ 经济与自动化（全局动作格）
		ScriptThreads::LogRunningScripts(10);
		EconomyFeatures::Resolve();

		// 第18轮：把本次解析结果落盘（设备独占，外部进程拿不到；常驻实例自证）
		Diagnostics::WriteReport();
		return 1;
	}

	// 如果GTA5_Enhanced.exe失败，尝试GTA5.exe
	if (VMMDLL_PidGetFromName(vmh, "GTA5.exe", &PID))
	{
		if (!PID)
		{
			std::println("GTA5.exe PID is null.");
			return 0;
		}

		BaseAddress = VMMDLL_ProcessGetModuleBaseU(vmh, PID, "GTA5.exe");

		if (!BaseAddress)
		{
			std::println("GTA5.exe BaseAddress is null.");
			return 0;
		}

		std::println("GTA5.exe found @ {0:x}\n", BaseAddress);
		// 设置GTA5.exe的偏移量
		Offsets::SetOffsetsByPackageName("GTA5.exe");
		Memory().Attach(vmh, PID);
		return 1;
	}

	std::println("Both GTA5_Enhanced.exe and GTA5.exe not found.");
	return 0;
}

bool DMA::DMAThreadEntry()
{

	while (AppRuntime::IsRunning())
	{
		try
		{
			UpdateEssentials();
		}
		catch (std::runtime_error& e)
		{
			std::println("UpdateEssentials threw exception!\n   {}\n",e.what());
			continue;
		}
		catch (...)
		{
			std::println("Uncaught exception in UpdateEssentials()");
			continue;
		}

		RefreshHealth::OnDMAFrame();
		NoWanted::OnDMAFrame();
		WeaponInspector::OnDMAFrame();
		Teleport::OnDMAFrame();
		GodMode::OnDMAFrame();
		VehicleEditor::OnDMAFrame();
		// DISABLED: TimeControl implementation is retained for later restoration.
		// TimeControl::OnDMAFrame();
		Ragdoll::OnDMAFrame();
		PlayerSpeed::OnDMAFrame();
		Invisibility::OnDMAFrame();
		NoCollision::OnDMAFrame();
		// DISABLED: PlayerChaser and HeistDividend implementations are retained for later restoration.
		// PlayerChaser::OnDMAFrame();
		// HeistDividend::OnDMAFrame();
		ArmorManager::OnDMAFrame();
		HealthManager::OnDMAFrame();
		AimAid::OnDMAFrame();
		NoIdleKick::OnDMAFrame();

		// 进度类 tunable（RP 倍率 / 外貌免冷却 / 免费）：开着的时候每帧复查重写。
		ProgressFeatures::OnDMAFrame();
		EconomyFeatures::OnDMAFrame();   // 内部含 ScriptGlobals::OnFrame()（动作格脉冲还原）
		PlayerList::OnDMAFrame();
		OffRadar::OnDMAFrame();
	VehicleList::OnDMAFrame();

		// 载具修复：只有收到请求时才写，常态零开销。
		VehicleRepair::OnDMAFrame();
	}

	DMA::Close();

	return 1;
}

bool DMA::UpdatePlayerCurrentLocation()
{
	uintptr_t LocationAddress = NavigationAddress + offsetof(CNavigation, Position);
	if (!Memory().Read(LocationAddress, &LocalPlayerLocation, sizeof(LocalPlayerLocation)))
	{
		ZeroMemory(&LocalPlayerLocation, sizeof(Vec3));
		throw std::runtime_error("Incomplete LocalPlayer location read.");
	}

	return 1;
}

uintptr_t DMA::GetGlobalAddress(DWORD Index)
{
	int ChunkIndex = Index >> 0x12 & 0x3F;
	int ElementIndex = Index & 0x3FFFF;

	uintptr_t GlobalAddress = BaseAddress + Offsets::GlobalPtr;

	uintptr_t ChunkPtr = GlobalAddress + (ChunkIndex * 0x8);

	uintptr_t ChunkAddress = 0x0;
	if (!Memory().Read(ChunkPtr, &ChunkAddress, sizeof(ChunkAddress)) || !ChunkAddress)
	{
		std::println("Incomplete ChunkPtr read.");
		return 0;
	}

	uintptr_t ElementAddress = ChunkAddress + (ElementIndex * 0x8);

	return ElementAddress;
}

bool DMA::UpdateEssentials()
{
	uintptr_t WorldPtr = BaseAddress + Offsets::WorldPtr;
	uintptr_t WorldAddress = 0x0;
	if (!Memory().Read(WorldPtr, &WorldAddress, sizeof(WorldAddress))) [[unlikely]]
	{
		std::println("WorldPtr Dereference failed! Reinitializing VMH.");
		Close();
		Initialize();
		return 0;
	}
	if (!WorldAddress) [[unlikely]]
	{
		std::println("*WorldPtr is null! Reinitializing VMH.");
		Close();
		Initialize();
		return 0;
	}

	uintptr_t LocalPlayerPtr = WorldAddress + offsetof(World, pPlayer);
	if (!Memory().Read(LocalPlayerPtr, &LocalPlayerAddress, sizeof(LocalPlayerAddress))) [[unlikely]]
		throw std::runtime_error("Incomplete LocalPlayerPtr read.");

	if (!LocalPlayerAddress) [[unlikely]]
		throw std::runtime_error("*LocalPlayerPtr is null.");

	uintptr_t ModelInfoAddress = 0;
	VehicleAddress = 0;
	NavigationAddress = 0;
	PlayerInfoAddress = 0;
	WeaponInventoryAddress = 0;
	WeaponManagerAddress = 0;

	auto scatter = Memory().BeginScatter();
	if (!scatter.IsValid() ||
		!scatter.PrepareRead(
			LocalPlayerAddress + offsetof(PED, pCModelInfo),
			&ModelInfoAddress,
			sizeof(ModelInfoAddress)) ||
		!scatter.PrepareRead(
			LocalPlayerAddress + offsetof(PED, pCNavigation),
			&NavigationAddress,
			sizeof(NavigationAddress)) ||
		!scatter.PrepareRead(
			LocalPlayerAddress + offsetof(PED, pPlayerInfo),
			&PlayerInfoAddress,
			sizeof(PlayerInfoAddress)) ||
		!scatter.PrepareRead(
			LocalPlayerAddress + offsetof(PED, pCWeaponInventory),
			&WeaponInventoryAddress,
			sizeof(WeaponInventoryAddress)) ||
		!scatter.PrepareRead(
			LocalPlayerAddress + offsetof(PED, pCPedWeaponManager),
			&WeaponManagerAddress,
			sizeof(WeaponManagerAddress)) ||
		!scatter.PrepareRead(
			LocalPlayerAddress + offsetof(PED, pCVehicle),
			&VehicleAddress,
			sizeof(VehicleAddress)) ||
		!scatter.Execute()) [[unlikely]]
	{
		throw std::runtime_error("Incomplete essential pointer Scatter read.");
	}

	if (!NavigationAddress) [[unlikely]]
		throw std::runtime_error("*CNavigationPtr is null.");

	if (!PlayerInfoAddress) [[unlikely]]
		throw std::runtime_error("*PlayerInfoPtr is null.");

	if (!WeaponInventoryAddress) [[unlikely]]
		throw std::runtime_error("*WeaponInventoryPtr is null.");

	if (!WeaponManagerAddress) [[unlikely]]
		throw std::runtime_error("*WeaponManagerPtr is null.");

	uintptr_t WeaponInfoPtr = WeaponManagerAddress + offsetof(CPEdWeaponManager, pCWeaponInfo);
	if (!Memory().Read(WeaponInfoPtr, &WeaponInfoAddress, sizeof(WeaponInfoAddress))) [[unlikely]]
		throw std::runtime_error("Incomplete WeaponInfoPtr read.");

	LocalPlayerModelHash = 0;
	if (ModelInfoAddress)
	{
		Memory().Read(
			ModelInfoAddress + offsetof(CModelInfo, ModelHash),
			&LocalPlayerModelHash,
			sizeof(LocalPlayerModelHash));
	}

	UpdateVehicleInformation();

	UpdatePlayerCurrentLocation();

	return 1;
}

bool DMA::Close()
{
	RuntimeTables::Shutdown();
	// 先写回内存补丁，再断开 VM 和 MemoryBackend，避免退出时把补丁留在游戏进程。
	AimAid::PrepareForClose();
	NoIdleKick::PrepareForClose();
	// tunable 还原：把本会话写过的单元按记录写回原值。
	EconomyFeatures::PrepareForClose();
	ScriptThreads::Reset();
	Tunables::PrepareForClose();
	Memory().Reset();
	const VMM_HANDLE handle = vmh;
	vmh = nullptr;
	PID = 0;
	BaseAddress = 0;
	LocalPlayerAddress = 0;
	NavigationAddress = 0;
	PlayerInfoAddress = 0;
	VehicleAddress = 0;
	VehicleNavigationAddress = 0;
	WeaponInventoryAddress = 0;
	WeaponManagerAddress = 0;
	WeaponInfoAddress = 0;
	LocalPlayerModelHash = 0;
	LocalPlayerLocation = {0, 0, 0};
	if (handle != nullptr)
	{
		VMMDLL_Close(handle);
	}
	return 1;
}

bool DMA::UpdateVehicleInformation()
{
	if (!VehicleAddress) [[unlikely]]
	{
		VehicleNavigationAddress = 0;
		return 0;
	}

	uintptr_t VehicleNavigationPtr = VehicleAddress + offsetof(CVehicle, pCNavigation);
	if (!Memory().Read(
			VehicleNavigationPtr,
			&VehicleNavigationAddress,
			sizeof(VehicleNavigationAddress))) [[unlikely]]
	{
		VehicleNavigationAddress = 0;
		return 0;
	}

	if (!VehicleNavigationAddress) [[unlikely]]
		return 0;

	return 1;
}


// ============================================================================
// 特征码动态偏移解析
// 在进程识别后扫描主模块代码段，用 CT 表验证过的特征码解析 Enhanced 偏移。
// 任何失败（Legacy 版本 / 特征码未命中 / 目标越界）都保持静态回退值。
// ============================================================================
bool DMA::ResolveRuntimeOffsets()
{
	if (currentGameType != GameType::GTA5_Enhanced)
		return false;  // Legacy 无已验证特征码目录，保持静态偏移

	// 通过 VMMDLL 获取模块大小
	VMMDLL_MAP_MODULEENTRY moduleInfo{};
	PVMMDLL_MAP_MODULEENTRY pModuleInfo = nullptr;
	if (!VMMDLL_Map_GetModuleFromNameU(vmh, PID, "GTA5_Enhanced.exe", &pModuleInfo, 0) || !pModuleInfo)
	{
		std::println("[Offsets] module info query failed, keeping static offsets.");
		return false;
	}
	moduleInfo = *pModuleInfo;
	VMMDLL_MemFree(pModuleInfo);
	const uint32_t imageSize = moduleInfo.cbImageSize;
	if (imageSize == 0)
	{
		std::println("[Offsets] invalid module image size, keeping static offsets.");
		return false;
	}

	// 读取可执行段
	// 扫描专用读取：NOCACHE + ZEROPAD_ON_FAIL —— 换出页返回零填充而非失败。
	// 参考 DMA 社区实践（另一工具同特征码在此设备上正常工作，其位移读取
	// 也是失败后 direct-read 重试成功的）。零填充仅影响个别页，特征码
	// 匹配不会命中全零页；唯一风险是某特征码恰好跨页被零填充截断——
	// 由 FindUnique 的 NotFound 兜底，回退静态值。
	const OffsetResolver::MemoryReader reader = [](std::uintptr_t address, void* buffer, std::size_t size, const char* stage) {
		DWORD bytesRead = 0;
		const BOOL ok = VMMDLL_MemReadEx(
			DMA::vmh, DMA::PID, static_cast<ULONG64>(address),
			static_cast<PBYTE>(buffer), static_cast<DWORD>(size), &bytesRead,
			VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
		if (ok != FALSE)
			return true;
		std::println("[Offsets] read failed at {} stage, address 0x{:X} size {} — FPGA 传输失败", stage ? stage : "?", address, size);
		return false;
	};
	std::string diagnostic = "unknown";
	const auto section = OffsetResolver::LoadExecutableSection(reader, BaseAddress, imageSize, &diagnostic);
	if (!section)
	{
		std::println("[Offsets] executable section load failed, keeping static offsets.");
		std::println("[Offsets] 失败原因: {}", diagnostic);
		std::println("[Offsets] 提示: 若频繁失败，检查 FPGA 线缆/带宽；本工具将在下次重启进程时重试。");
		return false;
	}

	std::println("[Offsets] scanning '{}' section ({} bytes) for signatures...", section->name, section->bytes.size());

	// 零填充洞体检：扫描 reader 带 ZEROPAD_ON_FAIL —— 换出/未驻留的页会被填 0，
	// 特征码若落在这些页上就会失配，从而回退备用特征码或静态值（本次 GlobalPtr 事故的入口）。
	// 把洞的数量与首个位置打出来，失配时能立刻分辨是「特征码失效」还是「页没读到」。
	{
		const auto& scanBytes = section->bytes;
		std::size_t zeroPages = 0;
		std::size_t firstZeroPage = 0;
		bool haveFirstZeroPage = false;
		for (std::size_t page = 0; page + 4096 <= scanBytes.size(); page += 4096)
		{
			bool allZero = true;
			for (std::size_t probe = 0; probe < 4096; ++probe)
			{
				if (scanBytes[page + probe] != 0)
				{
					allZero = false;
					break;
				}
			}
			if (allZero)
			{
				++zeroPages;
				if (!haveFirstZeroPage)
				{
					firstZeroPage = page;
					haveFirstZeroPage = true;
				}
			}
		}
		if (zeroPages > 0)
		{
			std::println("[Offsets] 扫描缓冲含 {} 个全零页（换出/未驻留，ZEROPAD 填充），首个在 +0x{:X}；"
			             "落在这些页上的特征码会失配", zeroPages, firstZeroPage);
		}
	}

	// ScriptGlobals（rage::scrGlobal 的 64 分块指针表）候选体检：
	// 真身的分块项要么是 0（该分块尚未分配），要么是**可读、16 字节对齐的用户空间指针**。
	// 实机对照（2026-09-17 同一进程）：
	//   0x3ED15A8（YimMenuV2 ScriptGlobals 特征码解析值，= 静态兜底值）→ 15 个非 0 项全部可读；
	//   0x4737178（老版 GTA5.exe 备用签名算出的值）→ 13 个非 0 项里 4 个不可读 → 必须被拒绝。
	const auto validateScriptGlobals = [](std::uintptr_t candidate, std::string* reason) -> bool {
		constexpr std::size_t kChunkCount = 64;
		constexpr int kMinNonZeroChunks = 8;
		std::uintptr_t chunks[kChunkCount] = {};
		if (!DMA::Memory().Read(candidate, chunks, sizeof(chunks)))
		{
			if (reason)
				*reason = "ScriptGlobals 候选的分块表读不出来";
			return false;
		}

		int nonZero = 0;
		for (std::size_t i = 0; i < kChunkCount; ++i)
		{
			const std::uintptr_t chunk = chunks[i];
			if (chunk == 0)
				continue;
			++nonZero;
			if ((chunk & 0xF) != 0 || chunk >= 0x7FFFFFFFFFFFull)
			{
				char buf[128];
				std::snprintf(buf, sizeof(buf), "分块 %zu 不是 16 字节对齐的用户空间指针 (0x%llX)",
				              i, static_cast<unsigned long long>(chunk));
				if (reason)
					*reason = buf;
				return false;
			}
			std::uint64_t probe = 0;
			if (!DMA::Memory().Read(chunk, &probe, sizeof(probe)))
			{
				char buf[128];
				std::snprintf(buf, sizeof(buf), "分块 %zu 指向的内存不可读 (0x%llX)",
				              i, static_cast<unsigned long long>(chunk));
				if (reason)
					*reason = buf;
				return false;
			}
		}

		if (nonZero < kMinNonZeroChunks)
		{
			char buf[128];
			std::snprintf(buf, sizeof(buf), "分块表只有 %d 个非 0 项（要求 >= %d）", nonZero, kMinNonZeroChunks);
			if (reason)
				*reason = buf;
			return false;
		}
		return true;
	};
	// 逐个解析并应用（成功的立即写入 Offsets::，失败保持静态值）
	int resolved = 0;
	const auto catalog = OffsetResolver::GetCatalog(GameType::GTA5_Enhanced);
	for (const auto& spec : catalog)
	{
		uintptr_t fallback = 0;
		if (spec.name == "WorldPtr") fallback = Offsets::WorldPtr;
		else if (spec.name == "GlobalPtr") fallback = Offsets::GlobalPtr;
		else if (spec.name == "BlipPtr") fallback = Offsets::BlipPtr;
		else if (spec.name == "PlayerMgrPtr") fallback = Offsets::PlayerMgrPtr;
		else if (spec.name == "AimCPedPtr") fallback = Offsets::AimCPedPtr;
		else if (spec.name == "WaypointPtr") fallback = Offsets::WaypointPtr;
		else if (spec.name == "LocalScriptsPtr") fallback = Offsets::LocalScriptsPtr;
		else if (spec.name == "GTAPlusPtr") fallback = Offsets::GTAPlusPtr;
		else if (spec.name == "PedPoolPtr") fallback = Offsets::PedPoolPtr;
    else if (spec.name == "VehiclePoolPtr") fallback = Offsets::VehiclePoolPtr;
    else if (spec.name == "NetworkTimePtr") fallback = Offsets::NetworkTimePtr;
		else continue;

		// 候选校验器：只配给「算错会静默危害功能」的偏移（目前是 GlobalPtr / ScriptGlobals）。
		const OffsetResolver::CandidateValidator validator =
			spec.name == "GlobalPtr" ? OffsetResolver::CandidateValidator(validateScriptGlobals)
			                          : OffsetResolver::CandidateValidator{};
		const auto result = OffsetResolver::ResolveOne(
			spec, section->bytes, section->runtimeAddress, section->moduleBase, section->imageSize, fallback, validator);

		if (result.source == OffsetResolver::OffsetSource::Pattern)
		{
			if (spec.name == "WorldPtr") Offsets::WorldPtr = result.value;
			else if (spec.name == "GlobalPtr") Offsets::GlobalPtr = result.value;
			else if (spec.name == "BlipPtr") Offsets::BlipPtr = result.value;
			else if (spec.name == "PlayerMgrPtr") Offsets::PlayerMgrPtr = result.value;
			else if (spec.name == "AimCPedPtr") Offsets::AimCPedPtr = result.value;
			else if (spec.name == "WaypointPtr") Offsets::WaypointPtr = result.value;
			else if (spec.name == "LocalScriptsPtr") Offsets::LocalScriptsPtr = result.value;
			else if (spec.name == "GTAPlusPtr") Offsets::GTAPlusPtr = result.value;
			else if (spec.name == "PedPoolPtr") Offsets::PedPoolPtr = result.value;
    else if (spec.name == "VehiclePoolPtr") Offsets::VehiclePoolPtr = result.value;
    else if (spec.name == "NetworkTimePtr") Offsets::NetworkTimePtr = result.value;

			std::println("[Offsets] {} = 0x{:X} (pattern{})", result.name, result.value,
			             result.validated ? ", validated" : "");
			++resolved;
		}
		else
		{
			std::println("[Offsets] {} kept static 0x{:X} ({})", result.name, result.value, result.diagnostic);
		}
	}

	std::println("[Offsets] {}/{} offsets resolved from signatures.\n", resolved, catalog.size());
	return resolved > 0;
}


// ============================================================================
// 网络时间深层探针（--netptr-probe）
//   YimMenuV2 的 networkTimePtrn 在当前线上版本失配（实测 NotFound）。
//   这里把特征码放宽成前缀 89 05 ?? ?? ?? ?? 80 3D（mov [rip+disp], eax; cmp byte[rip+disp], imm8），
//   对每个命中解析 RIP 目标地址，再间隔 3 秒采样两次 —— 每秒 +1 的那个就是网络时间。
// ============================================================================
int DMA::NetworkTimeDeepProbe()
{
	std::println("");
	std::println("=== 网络时间深层探针（放宽前缀 + 每秒自增验证）===");

	PVMMDLL_MAP_MODULEENTRY pModuleInfo = nullptr;
	if (!VMMDLL_Map_GetModuleFromNameU(vmh, PID, "GTA5_Enhanced.exe", &pModuleInfo, 0) || !pModuleInfo)
	{
		std::println("  模块查询失败");
		return 1;
	}
	const uint32_t imageSize = pModuleInfo->cbImageSize;
	VMMDLL_MemFree(pModuleInfo);

	const OffsetResolver::MemoryReader reader = [](std::uintptr_t address, void* buffer, std::size_t size, const char* stage) {
		DWORD bytesRead = 0;
		const BOOL ok = VMMDLL_MemReadEx(DMA::vmh, DMA::PID, static_cast<ULONG64>(address),
			static_cast<PBYTE>(buffer), static_cast<DWORD>(size), &bytesRead,
			VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL);
		(void)stage;
		return ok != FALSE;
	};
	std::string diag = "unknown";
	const auto section = OffsetResolver::LoadExecutableSection(reader, BaseAddress, imageSize, &diag);
	if (!section)
	{
		std::println("  可执行段加载失败：{}", diag);
		return 1;
	}
	std::println("  .text {} 字节，开始放宽扫描", section->bytes.size());

	const auto found = PatternScanner::FindAll(section->bytes, "89 05 ?? ?? ?? ?? 80 3D");
	std::println("  放宽前缀命中 {} 处（status={}）", found.offsets.size(), static_cast<int>(found.status));
	if (found.offsets.empty())
		return 1;

	std::vector<std::uintptr_t> targets;
	for (std::size_t off : found.offsets)
	{
		if (const auto t = PatternScanner::ResolveRelativeTarget(section->bytes, off, 2, 6, section->runtimeAddress))
			targets.push_back(*t);
	}
	std::println("  解析出 {} 个候选地址，采样中（间隔 3 秒）...", targets.size());
	std::fflush(stdout);

	std::vector<uint32_t> first(targets.size(), 0), second(targets.size(), 0);
	for (std::size_t i = 0; i < targets.size(); ++i)
		Memory().Read(targets[i], &first[i], sizeof(uint32_t));
	Sleep(3000);
	for (std::size_t i = 0; i < targets.size(); ++i)
		Memory().Read(targets[i], &second[i], sizeof(uint32_t));

	int hits = 0;
	for (std::size_t i = 0; i < targets.size(); ++i)
	{
		const int64_t d = static_cast<int64_t>(second[i]) - static_cast<int64_t>(first[i]);
		const bool like = (d >= 1 && d <= 6 && second[i] > 100000);
		if (like)
			++hits;
		std::println("    候选 {:<2} 地址 0x{:X}  {} → {}  Δ={} {}", i, targets[i],
				first[i], second[i], d, like ? "← 像网络时间 ✓" : "");
	}
	std::println("");
	std::println("  像网络时间的候选：{} 个", hits);
	return hits > 0 ? 0 : 1;
}
