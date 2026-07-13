#pragma once

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

	static inline Vec3 LocalPlayerLocation = { 0,0,0 };

public: /* DMA Interface function */
	static bool IsReady() noexcept { return vmh != 0 && PID != 0; }
	static bool IsValidAddress(uintptr_t address) noexcept { return IsReady() && address != 0; }
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

		T ReturnValue = 0;

		DWORD BytesRead = 0x0;

		VMMDLL_MemReadEx(vmh, PID, GlobalAddress, (BYTE*)&ReturnValue, sizeof(T), &BytesRead, VMMDLL_FLAG_NOCACHE);

		if (BytesRead != sizeof(T))
			return 0;

		Out = ReturnValue;

		return 1;
	}

	template <typename T>
	static bool SetGlobalValue(DWORD Index, T In)
	{
		uintptr_t GlobalAddress = GetGlobalAddress(Index);
		if (!GlobalAddress) return 0;

		return VMMDLL_MemWrite(vmh, PID, GlobalAddress, (BYTE*)&In, static_cast<DWORD>(sizeof(T))) != FALSE;
	}

	/* Multi-level pointer functions */
	template <typename T>
	static bool ReadMultiLevelPointer(uintptr_t baseAddress, const std::vector<uintptr_t>& offsets, T& outValue)
	{
		if (!IsValidAddress(baseAddress) || offsets.empty()) return false;


		uintptr_t address = baseAddress;
		DWORD bytesRead = 0;

		// Read through each offset except the last one
		for (size_t i = 0; i < offsets.size() - 1; ++i) {
			VMMDLL_MemReadEx(vmh, PID, address + offsets[i], (BYTE*)&address, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
			if (bytesRead != sizeof(uintptr_t) || !address) {
				return false;
			}
		}

		// Read the final value
		VMMDLL_MemReadEx(vmh, PID, address + offsets.back(), (BYTE*)&outValue, sizeof(T), &bytesRead, VMMDLL_FLAG_NOCACHE);
		return bytesRead == sizeof(T);
	}

	template <typename T>
	static bool WriteMultiLevelPointer(uintptr_t baseAddress, const std::vector<uintptr_t>& offsets, const T& value)
	{
		if (!IsValidAddress(baseAddress) || offsets.empty()) return false;

		uintptr_t address = baseAddress;
		DWORD bytesRead = 0;

		// Read through each offset except the last one
		for (size_t i = 0; i < offsets.size() - 1; ++i) {
			VMMDLL_MemReadEx(vmh, PID, address + offsets[i], (BYTE*)&address, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE);
			if (bytesRead != sizeof(uintptr_t) || !address) {
				// Debug output
				std::println("WriteMultiLevelPointer failed at offset {}: bytesRead={}, address={:x}", i, bytesRead, address);
				return false;
			}
		}

		// Write the final value
		return VMMDLL_MemWrite(vmh, PID, address + offsets.back(), (BYTE*)&value, static_cast<DWORD>(sizeof(T))) != FALSE;
	}

private: /* Private functions */
	static bool UpdateEssentials();
	static bool Close();
};


