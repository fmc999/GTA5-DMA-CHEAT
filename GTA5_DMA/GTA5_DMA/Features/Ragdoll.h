#pragma once
#include <atomic>

class Ragdoll
{
public:
	static inline std::atomic<bool> bEnable = true;
	static inline std::atomic<bool> bRagdoll = true;
	
	// Non-atomic versions for UI
	static inline bool bEnableUI = true;
	static inline bool bRagdollUI = true;

	static void OnDMAFrame();
	static void UpdateRagdoll();
	static void SyncUI();
};