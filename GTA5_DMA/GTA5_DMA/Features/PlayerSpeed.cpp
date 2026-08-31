#include "pch.h"
#include "PlayerSpeed.h"
#include "DMA.h"
#include "Offsets.h"

void PlayerSpeed::OnDMAFrame()
{
	// Sync UI values to atomic values
	SyncUI();
	
	if (!bEnable.load())
		return;

	UpdateSpeed();
}

void PlayerSpeed::UpdateSpeed()
{
	// Player speed: "GTA5_Enhanced.exe"+043DBC98 -> [8, 10a8, D50]
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
	
	// Now read the PlayerInfo address (offset 0x10a8)
	uintptr_t playerInfoAddress = 0;
	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, playerAddress + 0x10a8, (BYTE*)&playerInfoAddress, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
	
	if (bytesRead != sizeof(uintptr_t) || !playerInfoAddress) {
		return;
	}
	
	// Write the player speed value (offset 0xD50)
	float playerSpeedValue = playerSpeed.load();
	VMMDLL_MemWrite(DMA::vmh, DMA::PID, playerInfoAddress + 0xD50, (BYTE*)&playerSpeedValue, sizeof(float));
}

void PlayerSpeed::SyncUI()
{
	// Sync from UI to atomic values
	bEnable.store(bEnableUI);
	bBeastMode.store(bBeastModeUI);
	
	// 根据野兽模式状态设置速度值
	if (bBeastModeUI) {
		// 野兽模式：设置速度为1.5
		playerSpeedUI = 1.5f;
	}
	// 非野兽模式：保留用户通过滑块设置的值，不再强制设置为1.0
	
	// 同步最终的速度值
	playerSpeed.store(playerSpeedUI);
}