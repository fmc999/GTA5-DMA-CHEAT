#pragma once
#include "DMA.h"
enum eImpactType
{
	IT_FIST = 2,
	IT_BULLET = 3,
	IT_EXPLOSION = 5
};

enum eImpactExplosion
{
	IE_GRENADE = 0,
	IE_GRENADELAUNCHER = 1,
	IE_STICKYBOMB = 2,
	IE_MOLOTOV = 3,
	IE_ROCKET = 4,
	IE_TANKSHELL = 5,
	IE_HI_OCTANE = 6,
	IE_CAR = 7,
	IE_PLANE = 8,
	IE_PETROL_PUMP = 9,
	IE_BIKE = 10,
	IE_DIR_STEAM = 11,
	IE_DIR_FLAME = 12,
	IE_DIR_WATER_HYDRANT = 13,
	IE_DIR_GAS_CANISTER = 14,
	IE_BOAT = 15,
	IE_SHIP_DESTROY = 16,
	IE_TRUCK = 17,
	IE_BULLET = 18,
	IE_SMOKEGRENADELAUNCHER = 19,
	IE_SMOKEGRENADE = 20,
	IE_BZGAS = 21,
	IE_FLARE = 22,
	IE_GAS_CANISTER = 23,
	IE_EXTINGUISHER = 24,
	IE_PROGRAMMABLEAR = 25,
	IE_TRAIN = 26,
	IE_BARREL = 27,
	IE_PROPANE = 28,
	IE_BLIMP = 29,
	IE_DIR_FLAME_EXPLODE = 30,
	IE_TANKER = 31,
	IE_PLANE_ROCKET = 32,
	IE_VEHICLE_BULLET = 33,
	IE_GAS_TANK = 34,
	IE_BIRD_CRAP = 35,
	IE_RAILGUN = 36,
	IE_BLIMP2 = 37,
	IE_FIREWORK = 38,
	IE_SNOWBALL = 39,
	IE_PROXMINE = 40,
	IE_VALKYRIE_CANNON = 41,
	IE_AIR_DEFENSE = 42,
	IE_PIPEBOMB = 43,
	IE_VEHICLEMINE = 44,
	IE_EXPLOSIVEAMMO = 45,
	IE_APCSHELL = 46,
	IE_BOMB_CLUSTER = 47,
	IE_BOMB_GAS = 48,
	IE_BOMB_INCENDIARY = 49,
	IE_BOMB_STANDARD = 50,
	IE_TORPEDO = 51,
	IE_TORPEDO_UNDERWATER = 52,
	IE_BOMBUSHKA_CANNON = 53,
	IE_BOMB_CLUSTER_SECONDARY = 54,
	IE_HUNTER_BARRAGE = 55,
	IE_HUNTER_CANNON = 56,
	IE_ROGUE_CANNON = 57,
	IE_MINE_UNDERWATER = 58,
	IE_ORBITAL_CANNON = 59,
	IE_BOMB_STANDARD_WIDE = 60,
	IE_EXPLOSIVEAMMO_SHOTGUN = 61,
	IE_OPPRESSOR2_CANNON = 62,
	IE_MORTAR_KINETIC = 63,
	IE_VEHICLEMINE_KINETIC = 64,
	IE_VEHICLEMINE_EMP = 65,
	IE_VEHICLEMINE_SPIKE = 66,
	IE_VEHICLEMINE_SLICK = 67,
	IE_VEHICLEMINE_TAR = 68,
	IE_SCRIPT_DRONE = 69,
	IE_RAYGUN = 70,
	IE_BURIEDMINE = 71,
	IE_SCRIPT_MISSILE = 72,
	IE_SPOOF_EXPLOSION = 99
};

class WeaponInspector
{
public: /* Interface methods */
	static bool Render();
	static bool RenderContent();
	static bool OnDMAFrame();
	static bool UpdateEssentials();

public: /* Interface variables */
	static inline bool bEnable = true;
	static inline bool bNoReload = false;
	static inline bool bInfiniteAmmo = false;
	static inline bool bApplyOneClickMod = false; // Checkbox for direct application
	static inline bool bDisableOthersWeapons = false; // 禁用其他人的武器
	static inline bool bSetAimTargetHealthToMinusOne = false; // 将瞄准敌人血量修改为-1
	static inline bool bModifyOthersMoveSpeed = false; // 修改其他人移动速度为10
	static inline bool bSetAimTargetArmorToMinusOne = false; // 将瞄准敌人防弹衣修改为-1
	static inline bool bMillionInstantHit = false; // 百万瞬击 - 子弹飞行速度设置为9999
	
	// 冲击力相关变量
	static inline bool bApplyImpactForces = false; // 应用冲击力修改
	static inline float DesiredObjectImpactForce = 1000.0f; // 目标物体冲击力
	static inline float DesiredPedImpactForce = 1000.0f; // 目标行人冲击力
	static inline float DesiredVehicleImpactForce = 1000.0f; // 目标载具冲击力
	static inline float DesiredAircraftImpactForce = 1000.0f; // 目标飞行目标冲击力

private: /* Private functions */
	static bool UpdateCurrentWeapon();
	static bool UpdateWeaponInventory();
	static bool UpdateWeaponInfo();

	// 读取子弹飞行速度
	static bool ReadBulletSpeed(float& outBulletSpeed);
	
	// 写入子弹飞行速度
	static bool WriteBulletSpeed(float bulletSpeed);
	
	// 读取冲击力相关数值
	static bool ReadImpactForce(uintptr_t offset, float& outImpactForce);
	
	// 写入冲击力相关数值
	static bool WriteImpactForce(uintptr_t offset, float impactForce);

	static bool EnableInfAmmo();
	static bool DisableInfAmmo();
	static bool EnableNoReload();
	static bool DisableNoReload();

private: /* 'Essentials' */
	static inline uintptr_t WeaponInvAddress = 0x0;
	static inline WeaponInfo WepInfo;
	static inline WeaponInventory WepInv;

private: /* Remaining private variables */
	static inline WeaponInfo DesiredWepInfo;
	static inline DWORD BytesRead = 0x0;
	static inline bool bNeedsOverwrite = false;
	static inline bool bRequestedCopyToDesired = false;
	static inline bool bPrevNoReload = false;
	static inline bool bPrevInfiniteAmmo = false;
	static inline bool bPrevDisableOthersWeapons = false;
	static inline bool bPrevApplyOneClickMod = false;
};