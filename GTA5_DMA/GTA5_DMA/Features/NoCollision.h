#pragma once
#include <atomic>

class NoCollision
{
public:
	static inline std::atomic<bool> bEnable = false;
	static inline std::atomic<bool> bNoCollision = false;
	
	// Non-atomic versions for UI
	static inline bool bEnableUI = false;
	static inline bool bNoCollisionUI = false;

	static void OnDMAFrame();
	static void UpdateNoCollision();
	static void SyncUI();
};