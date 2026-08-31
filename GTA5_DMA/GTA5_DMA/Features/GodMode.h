#pragma once
#include <atomic>

class GodMode {
public:
	static inline std::atomic<bool> bRequestedGodmode = false;
	static inline std::atomic<bool> bPlayerGodMode = false;
	static inline std::atomic<bool> bVehicleGodMode = false;

	static inline DWORD BytesRead = 0x0;

public:
	static bool OnDMAFrame();

	static bool PlayerSet(bool GodMode);
	static bool VehicleSet(bool GodMode);
	static bool VehicleEnable();
	static bool VehicleDisable();

};