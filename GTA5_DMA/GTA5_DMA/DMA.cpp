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

	DWORD BytesRead = 0;

	VMMDLL_MemReadEx(vmh, PID, LocationAddress, (BYTE*)&LocalPlayerLocation, sizeof(Vec3), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(Vec3))
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

	DWORD BytesRead = 0x0;

	uintptr_t GlobalAddress = BaseAddress + Offsets::GlobalPtr;

	uintptr_t ChunkPtr = GlobalAddress + (ChunkIndex * 0x8);

	uintptr_t ChunkAddress = 0x0;
	VMMDLL_MemReadEx(vmh, PID, ChunkPtr, (BYTE*)&ChunkAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(uintptr_t))
	{
		std::println("Incomplete ChunkPtr read.");
		return 0;
	}

	uintptr_t ElementAddress = ChunkAddress + (ElementIndex * 0x8);

	return ElementAddress;
}

bool DMA::UpdateEssentials()
{
	DWORD BytesRead = 0x0;

	uintptr_t WorldPtr = BaseAddress + Offsets::WorldPtr;
	uintptr_t WorldAddress = 0x0;
	VMMDLL_MemReadEx(vmh, PID, WorldPtr, (BYTE*)&WorldAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
	if (BytesRead != sizeof(uintptr_t)) [[unlikely]]
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
	VMMDLL_MemReadEx(vmh, PID, LocalPlayerPtr, (BYTE*)&LocalPlayerAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
	if (BytesRead != sizeof(uintptr_t)) [[unlikely]]
		throw std::runtime_error("Incomplete LocalPlayerPtr read.");

	if (!LocalPlayerAddress) [[unlikely]]
		throw std::runtime_error("*LocalPlayerPtr is null.");

	uintptr_t CNavigationPtr = LocalPlayerAddress + offsetof(PED, pCNavigation);
	VMMDLL_MemReadEx(vmh, PID, CNavigationPtr, (BYTE*)&NavigationAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
	if (BytesRead != sizeof(uintptr_t)) [[unlikely]]
		throw std::runtime_error("Incomplete CNavigationPtr read.");

	if (!NavigationAddress) [[unlikely]]
		throw std::runtime_error("*CNavigationPtr is null.");

	uintptr_t PlayerInfoPtr = LocalPlayerAddress + offsetof(PED, pPlayerInfo);
	VMMDLL_MemReadEx(vmh, PID, PlayerInfoPtr, (BYTE*)&PlayerInfoAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
	if (BytesRead != sizeof(uintptr_t)) [[unlikely]]
		throw std::runtime_error("Incomplete PlayerInfoPtr read.");

	if (!PlayerInfoAddress) [[unlikely]]
		throw std::runtime_error("*PlayerInfoPtr is null.");

	uintptr_t WeaponInventoryPtr = LocalPlayerAddress + offsetof(PED, pCWeaponInventory);
	VMMDLL_MemReadEx(vmh, PID, WeaponInventoryPtr, (BYTE*)&WeaponInventoryAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
	if (BytesRead != sizeof(uintptr_t)) [[unlikely]]
		throw std::runtime_error("Incomplete WeaponInventoryPtr read.");

	if (!WeaponInventoryAddress) [[unlikely]]
		throw std::runtime_error("*WeaponInventoryPtr is null.");

	uintptr_t WeaponManagerPtr = LocalPlayerAddress + offsetof(PED, pCPedWeaponManager);
	VMMDLL_MemReadEx(vmh, PID, WeaponManagerPtr, (BYTE*)&WeaponManagerAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
	if (BytesRead != sizeof(uintptr_t)) [[unlikely]]
		throw std::runtime_error("Incomplete WeaponManagerPtr read.");

	if (!WeaponManagerAddress) [[unlikely]]
		throw std::runtime_error("*WeaponManagerPtr is null.");

	uintptr_t WeaponInfoPtr = WeaponManagerAddress + offsetof(CPEdWeaponManager, pCWeaponInfo);
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, WeaponInfoPtr, (BYTE*)&WeaponInfoAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
	if (BytesRead != sizeof(uintptr_t)) [[unlikely]]
		throw std::runtime_error("Incomplete WeaponInfoPtr read.");

	UpdateVehicleInformation();

	UpdatePlayerCurrentLocation();

	return 1;
}

bool DMA::Close()
{
	VMMDLL_Close(vmh);
	return 1;
}

bool DMA::UpdateVehicleInformation()
{
	DWORD BytesRead = 0x0;

	uintptr_t VehiclePtr = LocalPlayerAddress + offsetof(PED, pCVehicle);
	VMMDLL_MemReadEx(vmh, PID, VehiclePtr, (BYTE*)&VehicleAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
	if (BytesRead != sizeof(uintptr_t)) [[unlikely]]
	{
		VehicleAddress = 0;
		return 0;
	}

	if (!VehicleAddress) [[unlikely]]
		return 0;

	uintptr_t VehicleNavigationPtr = VehicleAddress + offsetof(CVehicle, pCNavigation);
	VMMDLL_MemReadEx(vmh, PID, VehicleNavigationPtr, (BYTE*)&VehicleNavigationAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
	if (BytesRead != sizeof(uintptr_t)) [[unlikely]]
	{
		VehicleNavigationAddress = 0;
		return 0;
	}

	if (!VehicleNavigationAddress) [[unlikely]]
		return 0;

	return 1;
}