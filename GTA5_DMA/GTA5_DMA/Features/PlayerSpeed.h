#pragma once
#include <atomic>

class PlayerSpeed
{
public:
	static inline std::atomic<bool> bEnable = false;
	static inline std::atomic<float> playerSpeed = 1.0f;
	static inline std::atomic<bool> bBeastMode = false;
	
	// Non-atomic versions for UI
	static inline bool bEnableUI = false;
	static inline float playerSpeedUI = 1.0f;
	static inline bool bBeastModeUI = false;

	static void OnDMAFrame();
	static void UpdateSpeed();
	static void SyncUI();
};