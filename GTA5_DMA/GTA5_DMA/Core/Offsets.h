#pragma once
#include <cstdint>
#include <string>

// 游戏类型枚举
enum class GameType
{
	GTA5_Enhanced,
	GTA5,
	Unknown
};

// 全局游戏类型
extern GameType currentGameType;

namespace Offsets
{
	// 基础指针偏移量 - 来自 GTA5_Enhanced 最新修复传送报错版.CT
	static const uintptr_t WorldPtr_Enhanced = 0x0434A958;     // 48 8B 0D ? ? ? ? 49 8B 9E
	static const uintptr_t GlobalPtr_Enhanced = 0x4737178;   // 48 8B 0D ? ? ? ? 0F 1F 44 00
	static const uintptr_t BlipPtr_Enhanced = 0x03DEBD70;     // 4C 8D 3D ? ? ? ? 49 8B 34 C7
	static const uintptr_t TimeBasePtr_Enhanced = 0x47CDF70;  // Time structure base address
	
	// GTA5.exe 的偏移量
	static const uintptr_t WorldPtr_Original = 0x2603908;     // 48 8B 0D ? ? ? ? 49 8B 9E
	static const uintptr_t GlobalPtr_Original = 0x2FA8550;     // 48 8B 0D ? ? ? ? 0F 1F 44 00
	static const uintptr_t BlipPtr_Original = 0x206D600;       // 4C 8D 3D ? ? ? ? 49 8B 34 C7
	static const uintptr_t TimeBasePtr_Original = 0x47CDF70;    // Time structure base address
	
	// 玩家管理器偏移量
	static const uintptr_t PlayerMgrPtr_Enhanced = 0x048591D8;
	static const uintptr_t AimCPedPtr_Enhanced = 0x03EDC060;
	static const uintptr_t PlayerMgrPtr_Original = 0x02603908;
	static const uintptr_t AimCPedPtr_Original = 0x0206D600;

	// 当前使用的偏移量
	extern uintptr_t WorldPtr;
	extern uintptr_t GlobalPtr;
	extern uintptr_t BlipPtr;
	extern uintptr_t TimeBasePtr;
	extern uintptr_t PlayerMgrPtr;
	extern uintptr_t AimCPedPtr;
	
	// 根据包名设置偏移量和游戏类型
	inline void SetOffsetsByPackageName(const std::string& packageName)
	{
		if (packageName == "GTA5_Enhanced.exe")
		{
			WorldPtr = WorldPtr_Enhanced;
			GlobalPtr = GlobalPtr_Enhanced;
			BlipPtr = BlipPtr_Enhanced;
			TimeBasePtr = TimeBasePtr_Enhanced;
			PlayerMgrPtr = PlayerMgrPtr_Enhanced;
			AimCPedPtr = AimCPedPtr_Enhanced;
			currentGameType = GameType::GTA5_Enhanced;
		}
		else if (packageName == "GTA5.exe")
		{
			WorldPtr = WorldPtr_Original;
			GlobalPtr = GlobalPtr_Original;
			BlipPtr = BlipPtr_Original;
			TimeBasePtr = TimeBasePtr_Original;
			PlayerMgrPtr = PlayerMgrPtr_Original;
			AimCPedPtr = AimCPedPtr_Original;
			currentGameType = GameType::GTA5;
		}
		else
		{
			currentGameType = GameType::Unknown;
		}
	}
}
