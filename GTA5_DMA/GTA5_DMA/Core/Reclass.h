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
	char pad_0000[152]; //0x0000
	class PED* m_Ped; //0x0098  所属玩家 Ped
	char pad_00A0[92]; //0x00A0
	char Name[20]; //0x00FC  玩家名称（CT: CPlayerInfo.Name）
	char pad_0110[20]; //0x0110
	int32_t N00000406; //0x0124
	int32_t N00000409; //0x0128
	char pad_012C[180]; //0x012C
	float WalkSpeed; //0x01E4
	char pad_01E8[1792]; //0x01E8
	int32_t WantedLevel; //0x08E8
	char pad_08EC[1124]; //0x08EC
	float RunSpeed; //0x0D50
	char pad_0D54[916]; //0x0D54
}; //Size: 0x10E8
static_assert(sizeof(PlayerInfo) == 0x10E8);
static_assert(offsetof(PlayerInfo, Name) == 0xFC);
static_assert(offsetof(PlayerInfo, m_Ped) == 0x98);
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

// YimMenuV2: rage::fwVehiclePool（非加密，虚表+池地址+位图）
// 池指针为三重间接: VehiclePoolPtr 解析出 fwVehiclePool** (指针表)
class FwVehiclePool
{
public:
	void* vtbl;                    //0x0000 虚表
	void** m_PoolAddress;          //0x0008 实体指针数组
	uint32_t m_Size;               //0x0010 池大小
	char pad_0014[36];             //0x0014
	uint32_t* m_BitArray;          //0x0038 有效位图
	char pad_003C[40];             //0x003C
	uint32_t m_ItemCount;          //0x0064
};

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

// ============================ 网络玩家结构 ============================
// 参考 YimMenuV2 结构定义与本仓库 CT 表（GTA5_Enhanced.CT）
// CNetworkPlayerMgrPTR -> CNetworkPlayerMgr
class CNetworkPlayerMgr
{
public:
    char pad_0000[0x188];              //0x0000  netPlayerMgrBase 头部（虚表/连接管理/本地玩家）
    class CNetGamePlayer* m_Players[32]; //0x0188  战局玩家指针数组
    uint32_t m_MaxPlayers;             //0x0288
    char pad_028C[4];                  //0x028C
    int m_UnloadedPlayerCount;         //0x0290
    int m_LoadedPlayerCount;           //0x0294  当前玩家数量
}; //Size: 0x0298
static_assert(sizeof(CNetworkPlayerMgr) == 0x298);
static_assert(offsetof(CNetworkPlayerMgr, m_Players) == 0x188);
static_assert(offsetof(CNetworkPlayerMgr, m_MaxPlayers) == 0x288);

// rage::netPlayer 基类头部（虚表 + 网络 ID），CNetGamePlayer 继承它
class CNetGamePlayer
{
public:
    char pad_0000[0x08];               //0x0000  vtable
    int m_AccountId;                   //0x0008
    char pad_000C[4];                  //0x000C
    int64_t m_RockstarId;              //0x0010  R*
    char pad_0018[0x40];               //0x0018
    uint32_t m_MessageId;              //0x0058
    char pad_005C[4];                  //0x005C
    uint8_t m_ActiveIndex;             //0x0060
    uint8_t m_PlayerIndex;             //0x0061
    char pad_0062[0x6E];               //0x0062
    uint8_t m_Flags;                   //0x00D0  bit0 = IsLocal（YimMenu netPlayer）
    char pad_00D1[0x0F];               //0x00D1
    void* m_Unk;                       //0x00E0
    class PlayerInfo* m_PlayerInfo;    //0x00E8
    char pad_00F0[0x280];              //0x00F0
}; //Size: 0x0370
static_assert(sizeof(CNetGamePlayer) == 0x370);
static_assert(offsetof(CNetGamePlayer, m_RockstarId) == 0x10);
static_assert(offsetof(CNetGamePlayer, m_PlayerIndex) == 0x61);
static_assert(offsetof(CNetGamePlayer, m_PlayerInfo) == 0xE8);

// ============================ rage 池结构（YimMenuV2 现代实现） ============================
// RDR 移植的加密池指针（Enhanced 反作弊手段），解密后为 fwBasePool
class PoolEncryption
{
public:
    bool m_IsSet;       //0x0000
    char pad_0001[7];   //0x0001
    uint64_t m_First;   //0x0008
    uint64_t m_Second;  //0x0010
}; //Size: 0x0018
static_assert(sizeof(PoolEncryption) == 0x18);

class fwBasePool
{
public:
    char pad_0000[8];           //0x0000  vtable
    uintptr_t m_Entries;        //0x0008  条目数组
    uint8_t* m_Flags;           //0x0010  槽位标志（bit7=空闲）
    uint32_t m_Size;            //0x0018
    uint32_t m_ItemSize;        //0x001C
    uint32_t m_NextSlotIndex;   //0x0020
    uint32_t m_0024;            //0x0024
    uint32_t m_FreeSlotIndex;   //0x0028

    bool IsValid(uint32_t index) const { return !(m_Flags[index] & 0x80); }
}; //Size: 0x0030
static_assert(sizeof(fwBasePool) == 0x30);
