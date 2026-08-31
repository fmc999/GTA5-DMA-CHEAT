#pragma once
#include <atomic>

class Invisibility
{
public:
	static inline std::atomic<bool> bInvisibility = false;

	static void OnDMAFrame();
	static void UpdateInvisibility();
};
