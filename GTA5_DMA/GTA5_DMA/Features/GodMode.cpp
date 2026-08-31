#include "pch.h"

#include "GodMode.h"

bool GodMode::OnDMAFrame()
{
	// Always apply god mode status on each frame based on current settings
	// This ensures that the settings persist even after map changes
	PlayerSet(bPlayerGodMode.load());
	
	// Only set vehicle god mode if we're actually in a vehicle
	if (DMA::VehicleAddress) {
		VehicleSet(bVehicleGodMode.load());
	}
	
	
	
	// Reset the request flag if it was set
	if (bRequestedGodmode.load()) {
		bRequestedGodmode.store(false);
	}

	return 1;
}

bool GodMode::PlayerSet(bool GodMode)
{
	uintptr_t GodBitsAddress = DMA::LocalPlayerAddress + offsetof(PED, GodFlags);

	uint32_t OriginalBits = 0x0;

	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, GodBitsAddress, (BYTE*)&OriginalBits, sizeof(uint32_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(uint32_t))
	{
		std::println("{} failed!", __FUNCTION__);
		return 0;
	}

	std::bitset<16>GodBits(OriginalBits);

	// Check if player is already in the desired god mode state
	bool isCurrentlyGod = GodBits.test(4) && GodBits.test(8);
	
	if (GodMode && !isCurrentlyGod)
	{
		GodBits.set(4);
		GodBits.set(8);
	}
	else if (!GodMode && isCurrentlyGod)
	{
		GodBits.reset(4);
		GodBits.reset(8);
	}
	else
	{
		// Already in the correct state, no need to write to memory
		return 1;
	}

	uint32_t Bits = GodBits.to_ulong();

	VMMDLL_MemWrite(DMA::vmh, DMA::PID, GodBitsAddress, (BYTE*)&Bits, sizeof(uint32_t));

	return 1;
}

bool GodMode::VehicleSet(bool GodMode)
{
	uintptr_t GodBitsAddress = DMA::VehicleAddress + offsetof(CVehicle, GodBits);

	uint32_t OriginalBits = 0x0;

	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, GodBitsAddress, (BYTE*)&OriginalBits, sizeof(uint32_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(uint32_t))
	{
		std::println("{} failed!", __FUNCTION__);
		return 0;
	}

	std::bitset<16>GodBits(OriginalBits);

	// Check if vehicle is already in the desired god mode state
	bool isCurrentlyGod = GodBits.test(4) && GodBits.test(8);
	
	if (GodMode && !isCurrentlyGod)
	{
		GodBits.set(4);
		GodBits.set(8);
	}
	else if (!GodMode && isCurrentlyGod)
	{
		GodBits.reset(4);
		GodBits.reset(8);
	}
	else
	{
		// Already in the correct state, no need to write to memory
		return 1;
	}

	uint32_t Bits = GodBits.to_ulong();

	VMMDLL_MemWrite(DMA::vmh, DMA::PID, GodBitsAddress, (BYTE*)&Bits, sizeof(uint32_t));

	return 1;
}

bool GodMode::VehicleEnable()
{
	bVehicleGodMode.store(true);
	bRequestedGodmode.store(true);
	return 1;
}

bool GodMode::VehicleDisable()
{
	bVehicleGodMode.store(false);
	bRequestedGodmode.store(true);
	return 1;
}