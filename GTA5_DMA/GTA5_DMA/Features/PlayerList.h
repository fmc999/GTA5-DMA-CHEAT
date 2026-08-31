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

struct SessionPlayer
{
    uint8_t  PlayerIndex = 0;
    int64_t  RockstarId = 0;
    char     Name[20] = {};
    float    Health = 0.0f;
    float    MaxHealth = 0.0f;
    float    Armor = 0.0f;
    float    Distance = 0.0f;
    bool     GodMode = false;
    bool     InVehicle = false;
    bool     IsLocal = false;
    int32_t  WantedLevel = 0;
    uintptr_t PedAddress = 0;
    Vec3     Position = {};
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

    // 对选中玩家执行的操作（DMA 线程消费）
    static void RequestExplode(uint8_t playerIndex);
    static void RequestKill(uint8_t playerIndex);

private:
    enum class PedAction
    {
        Kill,      // 血量清零
        Explode    // 触发爆炸冲击
    };

    static void RefreshPlayers();
    static uintptr_t GetPlayerMgrAddress();
    static uintptr_t FindPedByPlayerIndex(uint8_t playerIndex);
    static void ApplyPedAction(uint8_t playerIndex, PedAction action);
};
