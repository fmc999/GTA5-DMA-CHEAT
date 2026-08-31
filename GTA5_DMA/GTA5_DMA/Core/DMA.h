#pragma once

#include "MemoryBackend.h"

class DMA
{
public: /* Interface variables */
	static inline VMM_HANDLE vmh = 0;
	static inline DWORD PID = 0;

	static inline uintptr_t BaseAddress = 0;
	static inline uintptr_t LocalPlayerAddress = 0;
	static inline uintptr_t NavigationAddress = 0;
	static inline uintptr_t PlayerInfoAddress = 0;
	static inline uintptr_t VehicleAddress = 0;
	static inline uintptr_t VehicleNavigationAddress = 0;
	static inline uintptr_t WeaponInventoryAddress = 0;
	static inline uintptr_t WeaponManagerAddress = 0;
	static inline uintptr_t WeaponInfoAddress = 0;
	static inline uint32_t LocalPlayerModelHash = 0;

	static inline Vec3 LocalPlayerLocation = { 0,0,0 };

public: /* DMA Interface function */
	static bool IsReady() noexcept { return vmh != 0 && PID != 0; }
	static bool ResolveRuntimeOffsets();  // 启动时特征码扫描，动态解析 Enhanced 偏移（失败回退静态值）
	static bool IsValidAddress(uintptr_t address) noexcept { return IsReady() && address != 0; }
	static MemoryBackend& Memory() noexcept;
	static bool Initialize();
	static bool DMAThreadEntry();
	static bool UpdatePlayerCurrentLocation();
	static bool UpdateVehicleInformation();

public: /* Globals */
	static uintptr_t GetGlobalAddress(DWORD Index);

	template <typename T>
	static bool GetGlobalValue(DWORD Index, T& Out)
	{
		uintptr_t GlobalAddress = GetGlobalAddress(Index);
		if (!GlobalAddress) return 0;

		T ReturnValue{};
		if (!Memory().Read(GlobalAddress, &ReturnValue, sizeof(ReturnValue)))
			return 0;

		Out = ReturnValue;

		return 1;
	}

	template <typename T>
	static bool SetGlobalValue(DWORD Index, T In)
	{
		uintptr_t GlobalAddress = GetGlobalAddress(Index);
		if (!GlobalAddress) return 0;

		return Memory().Write(GlobalAddress, &In, sizeof(In));
	}

	/* Multi-level pointer functions */
	template <typename T>
	static bool ReadMultiLevelPointer(uintptr_t baseAddress, const std::vector<uintptr_t>& offsets, T& outValue)
	{
		if (!IsValidAddress(baseAddress) || offsets.empty()) return false;


		uintptr_t address = baseAddress;
		// Read through each offset except the last one
		for (size_t i = 0; i < offsets.size() - 1; ++i) {
			if (!Memory().Read(address + offsets[i], &address, sizeof(address)) || !address) {
				return false;
			}
		}

		// Read the final value
		return Memory().Read(address + offsets.back(), &outValue, sizeof(outValue));
	}

	template <typename T>
	static bool WriteMultiLevelPointer(uintptr_t baseAddress, const std::vector<uintptr_t>& offsets, const T& value)
	{
		if (!IsValidAddress(baseAddress) || offsets.empty()) return false;

		uintptr_t address = baseAddress;
		// Read through each offset except the last one
		for (size_t i = 0; i < offsets.size() - 1; ++i) {
			if (!Memory().Read(address + offsets[i], &address, sizeof(address)) || !address) {
				// Debug output
				std::println("WriteMultiLevelPointer failed at offset {}: address={:x}", i, address);
				return false;
			}
		}

		// Write the final value
		return Memory().Write(address + offsets.back(), &value, sizeof(value));
	}

private: /* Private functions */
	static bool UpdateEssentials();
	static bool Close();
};


