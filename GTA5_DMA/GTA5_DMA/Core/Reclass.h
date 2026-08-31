#pragma once
#include <cstddef>
#include <cstdint>
// Created with ReClass.NET 1.2 by KN4CK3R

class World
{
public:
	char pad_0000[8]; //0x0000
	class PED* pPlayer; //0x0008
}; //Size: 0x0010
static_assert(sizeof(World) == 0x10);

class PED
{
public:
	char pad_0000[32]; //0x0000
	class CModelInfo* pCModelInfo; //0x0020
	char pad_0028[4]; //0x0028
	int8_t InvisibilityFlag; //0x002C
	char pad_002D[3]; //0x002D
	class CNavigation* pCNavigation; //0x0030
	char pad_0038[336]; //0x0038
	uint32_t GodFlags; //0x0188
	char pad_018C[244]; //0x018C
	float CurrentHealth; //0x0280
	float MaxHealth; //0x0284
	char pad_0288[2696]; //0x0288
	class CVehicle* pCVehicle; //0x0D10
	char pad_0D18[282]; //0x0D18
	uint8_t InVehicleBits; //0x0E32
	char pad_0E33[629]; //0x0E33
	class PlayerInfo* pPlayerInfo; //0x10A8
	class WeaponInventory* pCWeaponInventory; //0x10B0
	class CPEdWeaponManager* pCPedWeaponManager; //0x10B8
	char pad_10C0[72]; //0x10C0
	class PED* pSelf2; //0x1108
	char pad_1110[16]; //0x1110
	class PED* pSelf; //0x1120
	char pad_1128[2048]; //0x1128
}; //Size: 0x1928
static_assert(sizeof(PED) == 0x1928);
static_assert(offsetof(PED, pCModelInfo) == 0x20);

class PlayerInfo
{
public:
	char pad_0000[184]; //0x0000
	int32_t N00000406; //0x00B8
	int32_t N00000409; //0x00BC
	char pad_00C0[292]; //0x00C0
	float WalkSpeed; //0x01E4
	char pad_01E8[1792]; //0x01E8
	int32_t WantedLevel; //0x08E8
	char pad_08EC[1124]; //0x08EC
	float RunSpeed; //0x0D50
	char pad_0D54[916]; //0x0D54
}; //Size: 0x10E8
static_assert(sizeof(PlayerInfo) == 0x10E8);

class WeaponInventory
{
public:
	char pad_0000[120]; //0x0000
	uint8_t AmmoModifier; //0x0078
}; //Size: 0x0079
static_assert(sizeof(WeaponInventory) == 0x79);

class CPEdWeaponManager
{
public:
	char pad_0000[32]; //0x0000
	class WeaponInfo* pCWeaponInfo; //0x0020
}; //Size: 0x0028
static_assert(sizeof(CPEdWeaponManager) == 0x28);

class WeaponInfo
{
public:
	char pad_0000[16]; //0x0000
	uint32_t m_name; //0x0010
	uint32_t m_model; //0x0014
	uint32_t m_audio; //0x0018
	uint32_t m_slot; //0x001C
	int32_t ImpactType; //0x0020
	int32_t ImpactExplosion; //0x0024
	char pad_0028[72]; //0x0028
	uint32_t clip_size; //0x0070
	float WeaponAccuracy; //0x0074
	char pad_0078[8]; //0x0078
	float WeaponMoveAccuracy; //0x0080
	char pad_0084[44]; //0x0084
	float WeaponDamage; //0x00B0
	char pad_00B4[92]; //0x00B4
	float WeaponPenetration; //0x0110
	char pad_0114[32]; //0x0114
	float ReloadMultiplier; //0x0134
	char pad_0138[4]; //0x0138
	float WeaponFireRate; //0x013C
	char pad_0140[272]; //0x0140
	int32_t N000007DD; //0x0250
	char pad_0254[8]; //0x0254
	int32_t N0000082F; //0x025C
	char pad_0260[20]; //0x0260
	int32_t N00000835; //0x0274
	char pad_0278[16]; //0x0278
	float WeaponLockRange; //0x0288
	float WeaponRange; //0x028C
	char pad_0290[8]; //0x0290
	float N000007E6; //0x0298
	float N0000083F; //0x029C
	char pad_02A0[84]; //0x02A0
	float RecoilAmplitude; //0x02F4
}; //Size: 0x02F8
static_assert(sizeof(WeaponInfo) == 0x2F8);
static_assert(offsetof(WeaponInfo, WeaponAccuracy) == 0x74);
static_assert(offsetof(WeaponInfo, WeaponMoveAccuracy) == 0x80);
static_assert(offsetof(WeaponInfo, WeaponLockRange) == 0x288);

class CNavigation
{
public:
	char pad_0000[80]; //0x0000
	Vec3 Position; //0x0050
	char pad_005C[44]; //0x005C
	float CameraZ; //0x0088
	char pad_008C[4]; //0x008C
	float CameraX; //0x0090
	float CameraY; //0x0094
}; //Size: 0x0098
static_assert(sizeof(CNavigation) == 0x98);

class BlipArray
{
public:
	class Blip* pBlips[2000]; //0x0000
}; //Size: 0x3E80
static_assert(sizeof(BlipArray) == 0x3E80);

class Blip
{
public:
	char pad_0000[16]; //0x0000
	Vec3 Position; //0x0010
	char pad_001C[36]; //0x001C
	int32_t ID; //0x0040
}; //Size: 0x0044
static_assert(sizeof(Blip) == 0x44);

class N0000078D
{
public:
	class N000007DA* N0000078E; //0x0000
	char pad_0008[120]; //0x0008
}; //Size: 0x0080
static_assert(sizeof(N0000078D) == 0x80);

class N000007DA
{
public:
	char pad_0000[2192]; //0x0000
}; //Size: 0x0890
static_assert(sizeof(N000007DA) == 0x890);

class CVehicle
{
public:
	char pad_0000[24]; //0x0000
	uint32_t EntityModelHash; //0x0018
	char pad_001C[4]; //0x001C
	class CModelInfo* pCModelInfo; //0x0020
	char pad_0028[8]; //0x0028
	class CNavigation* pCNavigation; //0x0030
	char pad_0038[160]; //0x0038
	uint8_t VehicleState; //0x00D8
	char pad_00D9[175]; //0x00D9
	uint32_t GodBits; //0x0188
	char pad_018C[244]; //0x018C
	float Health; //0x0280
	char pad_0284[1756]; //0x0284
	class CHandlingData* pCHandlingData; //0x0960
	char pad_0968[16]; //0x0968
	uint8_t FreezeFlag; //0x0978
	char pad_0979[2411]; //0x0979
	uint8_t VehicleWeaponAmmo; //0x12E4
}; //Size: 0x12E8
static_assert(sizeof(CVehicle) == 0x12E8);
static_assert(offsetof(CVehicle, EntityModelHash) == 0x18);
static_assert(offsetof(CVehicle, pCModelInfo) == 0x20);
static_assert(offsetof(CVehicle, VehicleState) == 0xD8);
static_assert(offsetof(CVehicle, FreezeFlag) == 0x978);
static_assert(offsetof(CVehicle, VehicleWeaponAmmo) == 0x12E4);

class CHandlingData
{
public:
	char pad_0000[12]; //0x0000
	float Mass; //0x000C
	char pad_0010[60]; //0x0010
	float Acceleration; //0x004C
	char pad_0050[32]; //0x0050
	float BrakeForce; //0x006C
	char pad_0070[672]; //0x0070
	float Thrust; //0x0310
	char pad_0314[1380]; //0x0314
}; //Size: 0x0878
static_assert(sizeof(CHandlingData) == 0x87C);

class CModelInfo
{
public:
	char pad_0000[24]; //0x0000
	uint32_t ModelHash; //0x0018
	char pad_001C[364]; //0x001C
}; //Size: 0x0188
static_assert(sizeof(CModelInfo) == 0x188);
static_assert(offsetof(CModelInfo, ModelHash) == 0x18);
