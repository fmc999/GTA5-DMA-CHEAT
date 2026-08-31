#include "pch.h"

#include "Offsets.h"

#include "OffsetResolver.h"
#include "PatternScanner.h"

#include "Features.h"
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

MemoryBackend& DMA::Memory() noexcept
{
	static MemoryBackend backend;
	return backend;
}


bool DMA::Initialize()
{
	LPCSTR args[] = { "","-device","FPGA" };

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
		PlayerList::OnDMAFrame();
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
		else continue;

		const auto result = OffsetResolver::ResolveOne(
			spec, section->bytes, section->runtimeAddress, section->moduleBase, section->imageSize, fallback);

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

			std::println("[Offsets] {} = 0x{:X} (pattern)", result.name, result.value);
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
