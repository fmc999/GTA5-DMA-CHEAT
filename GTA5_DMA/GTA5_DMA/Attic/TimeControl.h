#pragma once
#include <atomic>

class TimeControl
{
public:
	static inline std::atomic<bool> bEnable = false;
	static inline std::atomic<int> day = 0;
	static inline std::atomic<int> hour = 12;
	static inline std::atomic<int> minute = 0;
	static inline std::atomic<int> second = 0;
	
	// Non-atomic versions for UI
	static inline bool bEnableUI = false;
	static inline int dayUI = 0;
	static inline int hourUI = 12;
	static inline int minuteUI = 0;
	static inline int secondUI = 0;

	static void OnDMAFrame();
	static void UpdateTime();
	static void SyncUI();
};