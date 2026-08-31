#include "pch.h"
#include "NoCollision.h"
#include "DMA.h"
#include "Offsets.h"

// Static variable to store original value
static float originalNoCollisionValue = 0.25f;
static bool bOriginalValueStored = false;

void NoCollision::OnDMAFrame()
{
	// Sync UI values to atomic values
	SyncUI();
	
	// Always update no collision state based on UI setting
	UpdateNoCollision();
}

void NoCollision::UpdateNoCollision()
{
	// No collision volume: "GTA5_Enhanced.exe"+043DBC98 -> [8, 30, 20, 10, 70, 0, 2C]
	// This is a multi-level pointer that needs to be resolved
	
	// First, get the base address from WorldPtr
	uintptr_t worldAddress = 0;
	DWORD bytesRead = 0;
	
	// Read the world pointer
	uintptr_t worldPtr = DMA::BaseAddress + Offsets::WorldPtr;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, worldPtr, (BYTE*)&worldAddress, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
	
	if (bytesRead != sizeof(uintptr_t) || !worldAddress) {
		return;
	}
	
	// Now read the player address (offset 0x8)
	uintptr_t playerAddress = 0;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, worldAddress + 0x8, (BYTE*)&playerAddress, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
	
	if (bytesRead != sizeof(uintptr_t) || !playerAddress) {
		return;
	}
	
	// Read the CPlayerInfo pointer (offset 0x30)
	uintptr_t playerInfoAddress = 0;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, playerAddress + 0x30, (BYTE*)&playerInfoAddress, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
	
	if (bytesRead != sizeof(uintptr_t) || !playerInfoAddress) {
		return;
	}
	
	// Read the CPlayerData pointer (offset 0x10)
	uintptr_t playerDataAddress = 0;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, playerInfoAddress + 0x10, (BYTE*)&playerDataAddress, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
	
	if (bytesRead != sizeof(uintptr_t) || !playerDataAddress) {
		return;
	}
	
	// Read the CPlayerSpecialAbility pointer (offset 0x20)
	uintptr_t playerSpecialAbilityAddress = 0;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, playerDataAddress + 0x20, (BYTE*)&playerSpecialAbilityAddress, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
	
	if (bytesRead != sizeof(uintptr_t) || !playerSpecialAbilityAddress) {
		return;
	}
	
	// Read the CPlayerSpecialAbilityData pointer (offset 0x70)
	uintptr_t playerSpecialAbilityDataAddress = 0;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, playerSpecialAbilityAddress + 0x70, (BYTE*)&playerSpecialAbilityDataAddress, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
	
	if (bytesRead != sizeof(uintptr_t) || !playerSpecialAbilityDataAddress) {
		return;
	}
	
	// Read the CPlayerSpecialAbilityDataValue pointer (offset 0x0)
	uintptr_t playerSpecialAbilityDataValueAddress = 0;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, playerSpecialAbilityDataAddress + 0x0, (BYTE*)&playerSpecialAbilityDataValueAddress, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
	
	if (bytesRead != sizeof(uintptr_t) || !playerSpecialAbilityDataValueAddress) {
		return;
	}
	
	// Handle no collision based on UI state
	if (bNoCollision.load()) {
		// Store original value if not already stored
		if (!bOriginalValueStored) {
			VMMDLL_MemReadEx(DMA::vmh, DMA::PID, playerSpecialAbilityDataValueAddress + 0x2C, (BYTE*)&originalNoCollisionValue, sizeof(float), &bytesRead, VMMDLL_FLAG_NOCACHE);
			bOriginalValueStored = true;
		}
		
		// Set no collision value (-1.0f = enable no collision)
		float noCollisionValue = -1.0f;
		VMMDLL_MemWrite(DMA::vmh, DMA::PID, playerSpecialAbilityDataValueAddress + 0x2C, (BYTE*)&noCollisionValue, sizeof(float));
	} else {
		// Restore original value if we have stored it
		if (bOriginalValueStored) {
			VMMDLL_MemWrite(DMA::vmh, DMA::PID, playerSpecialAbilityDataValueAddress + 0x2C, (BYTE*)&originalNoCollisionValue, sizeof(float));
			bOriginalValueStored = false;
		}
	}
}

void NoCollision::SyncUI()
{
	// Sync from UI to atomic values
	bNoCollision.store(bNoCollisionUI);
}