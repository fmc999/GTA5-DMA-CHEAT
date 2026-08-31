#include "pch.h"

#include "Offsets.h"

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
