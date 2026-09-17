#pragma once

// ============================================================================
// PlayerList — 战局玩家列表（在线玩家枚举 + 实时状态 + 玩家详情）
//
// 数据链（参考 YimMenuV2 Players 系统与本仓库 CT 表）：
//   CNetworkPlayerMgrPtr（Offsets，支持特征码动态解析）
//     -> CNetworkPlayerMgr::m_Players[32]        (0x188)
//       -> CNetGamePlayer
//         +0x10  m_RockstarId
//         +0x61  m_PlayerIndex
//         +0xE8  m_PlayerInfo -> PlayerInfo
//                                  +0xFC  Name
//                                  +0x98  m_Ped -> PED
//                                            +0x280 血量 / +0x150C 护甲
//                                            +0x189 无敌 / +0x30  导航(位置)
// ============================================================================

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "VehicleNames.h"

struct SessionPlayer
{
    uint32_t DisplayIndex = 0;   // 显示序号（排序后分配，稳定）
    uint8_t  PlayerIndex = 0;    // 网络 PlayerIndex（mgr 槽位）
    int64_t  RockstarId = 0;
    char     Name[20] = {};
    float    Health = 0.0f;
    float    MaxHealth = 0.0f;
    float    Armor = 0.0f;
    float    Distance = 0.0f;
    bool     GodMode = false;
    bool     InVehicle = false;
    // 第27轮：所在载具情报（只读）—— 模型哈希 / 中文名 / 载具血量
    uint32_t    VehicleModel = 0;         // 载具模型哈希（0 = 不在载具或读不到）
    const VehicleNameEntry* VehicleName = nullptr;   // 查 VehicleNames 表得到的条目（nullptr = 未收录）
    float       VehicleHealth = 0.0f;     // 车身健康（CVehicle+0x280）
    float       VehicleEngineHealth = 0.0f; // 引擎健康（CVehicle+0x910）
    bool     IsLocal = false;
    int32_t  WantedLevel = 0;
    uintptr_t PedAddress = 0;
    uintptr_t NavigationAddress = 0;
    Vec3     Position = {};
};

// 「拉到我这里」（Bring，参考 YimMenuV2 players/teleport/Bring.cpp）的落地报告。
// 方向契约：只写目标玩家的导航位置；本地玩家自己的坐标在本轮里只读不写
// （与既有「传送到此玩家」正好反方向）。
struct BringReport
{
    bool    Sent = false;          // 是否收到过请求
    bool    Ok = false;            // 写入 + 读回校验是否通过
    uint8_t PlayerIndex = 0;       // 目标玩家网络索引
    char    Name[20] = {};         // 目标玩家名称（取自快照）
    float   Me[3] = {};            // 执行时的本地玩家坐标
    float   TargetBefore[3] = {};  // 目标玩家执行前的坐标
    float   Landed[3] = {};        // 实际写入的落点（我旁边错开 2 米）
};

class PlayerList
{
public:
    // DMA 线程：每帧刷新（内部按间隔节流）
    static void OnDMAFrame();

    // 缓存的玩家快照（UI 线程只读）
    static std::vector<SessionPlayer> GetSnapshot();

    // 状态
    static bool IsSessionActive();
    static int GetPlayerCount();

    // 加入/离开日志开关（默认关闭；UI 设置页可切）
    static inline std::atomic<bool> bLogJoinLeave{ false };

    // 对选中玩家执行的操作（DMA 线程消费）
    static void RequestExplode(uint8_t playerIndex);
    static void RequestKill(uint8_t playerIndex);
    static void RequestTeleportTo(uint8_t playerIndex);   // 我 → 目标玩家
    static void RequestBring(uint8_t playerIndex);        // 目标玩家 → 我（错开 2 米防卡模）

    // 最近一次「拉到我这里」的落地报告（线程安全，UI 线程读取用于显示）
    static BringReport GetLastBring();

private:
    enum class PedAction
    {
        Kill,      // 血量清零
        Explode    // 触发爆炸冲击
    };

    static void RefreshPlayers();
    static uintptr_t FindPedByPlayerIndex(uint8_t playerIndex);
    static void TeleportToPlayer(uint8_t playerIndex);
    static void BringPlayerToMe(uint8_t playerIndex);
    static void ApplyPedAction(uint8_t playerIndex, PedAction action);
};
