#include "pch.h"
#include "Ragdoll.h"
#include "DMA.h"
#include "Offsets.h"

void Ragdoll::OnDMAFrame()
{
	// 布娃娃功能已固定开启，无需同步UI值
	// 始终保持布娃娃状态为开启
	UpdateRagdoll();
}

void Ragdoll::UpdateRagdoll()
{
	// Ragdoll effect: "GTA5_Enhanced.exe"+043DBC98 -> [8, 1098]
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
	
	// 固定设置为布娃娃开启 (1 = no ragdoll, 32 = ragdoll disabled)
	// 注意：根据注释，1表示无布娃娃效果，32表示布娃娃已禁用
	// 这里我们设置为1来禁用布娃娃效果（保持角色直立）
	BYTE ragdollValue = 1;
	VMMDLL_MemWrite(DMA::vmh, DMA::PID, playerAddress + 0x1098, (BYTE*)&ragdollValue, sizeof(BYTE));
}

void Ragdoll::SyncUI()
{
	// Sync from UI to atomic values
	bRagdoll.store(bRagdollUI);
}