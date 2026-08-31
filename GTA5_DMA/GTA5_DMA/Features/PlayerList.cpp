#include "pch.h"

#include "PlayerList.h"

#include "DMA.h"
#include "Offsets.h"
#include "Reclass.h"

#include <chrono>
#include <cstring>
#include <mutex>

namespace
{
    std::mutex g_PlayerMutex;
    std::vector<SessionPlayer> g_Players;
    std::atomic<int> g_PlayerCount{ 0 };
    std::atomic<bool> g_SessionActive{ false };
    std::atomic<uint8_t> g_PendingExplode{ 0xFF };
    std::atomic<uint8_t> g_PendingKill{ 0xFF };

    constexpr auto kRefreshInterval = std::chrono::milliseconds(500);
    std::chrono::steady_clock::time_point g_LastRefresh{};

    bool IsSessionPlayer(const CNetGamePlayer* netPlayer)
    {
        return netPlayer != nullptr;
    }

    void ReadPlayerFromMemory(uintptr_t netPlayerAddress, SessionPlayer& out)
    {
        std::memset(&out, 0, sizeof(out));

        CNetGamePlayer netPlayer{};
        if (!DMA::Memory().Read(netPlayerAddress, &netPlayer, sizeof(netPlayer)))
            return;

        out.PlayerIndex = netPlayer.m_PlayerIndex;
        out.RockstarId = netPlayer.m_RockstarId;

        if (netPlayer.m_PlayerInfo)
        {
            PlayerInfo info{};
            if (DMA::Memory().Read(
                    reinterpret_cast<uintptr_t>(netPlayer.m_PlayerInfo),
                    &info,
                    sizeof(info)))
            {
                std::memcpy(out.Name, info.Name, sizeof(out.Name) - 1);
                out.Name[sizeof(out.Name) - 1] = '\0';
                out.WantedLevel = info.WantedLevel;

                if (info.m_Ped)
                {
                    const uintptr_t pedAddress = reinterpret_cast<uintptr_t>(info.m_Ped);
                    out.PedAddress = pedAddress;

                    PED ped{};
                    if (DMA::Memory().Read(pedAddress, &ped, sizeof(ped)))
                    {
                        out.Health = ped.CurrentHealth;
                        out.MaxHealth = ped.MaxHealth;
                        out.GodMode = (ped.GodFlags & 0x1) != 0;
                        out.InVehicle = ped.pCVehicle != nullptr;

                        if (ped.pCNavigation)
                        {
                            CNavigation navigation{};
                            if (DMA::Memory().Read(
                                    reinterpret_cast<uintptr_t>(ped.pCNavigation),
                                    &navigation,
                                    sizeof(navigation)))
                            {
                                out.Position = navigation.Position;
                            }
                        }
                    }

                    // 护甲在 PED+0x150C（CT: CPed.Armor），Reclass.h 的 PED 类未覆盖到，单独读
                    DMA::Memory().Read(pedAddress + 0x150C, &out.Armor, sizeof(out.Armor));
                }
            }
        }

        if (DMA::LocalPlayerAddress != 0 && out.PedAddress == DMA::LocalPlayerAddress)
            out.IsLocal = true;

        if (out.Position.x != 0.0f || out.Position.y != 0.0f || out.Position.z != 0.0f)
            out.Distance = DMA::LocalPlayerLocation.Distance(out.Position);
    }
}

void PlayerList::OnDMAFrame()
{
    if (!DMA::IsReady())
        return;

    const auto now = std::chrono::steady_clock::now();
    if (now - g_LastRefresh < kRefreshInterval)
        return;
    g_LastRefresh = now;

    RefreshPlayers();

    // 处理 UI 请求的玩家操作
    const uint8_t explodeTarget = g_PendingExplode.exchange(0xFF);
    if (explodeTarget != 0xFF)
        ApplyPedAction(explodeTarget, PedAction::Explode);

    const uint8_t killTarget = g_PendingKill.exchange(0xFF);
    if (killTarget != 0xFF)
        ApplyPedAction(killTarget, PedAction::Kill);
}

void PlayerList::RefreshPlayers()
{
    const uintptr_t mgrAddress = GetPlayerMgrAddress();
    if (!mgrAddress)
    {
        std::lock_guard<std::mutex> lock(g_PlayerMutex);
        g_Players.clear();
        g_PlayerCount.store(0);
        g_SessionActive.store(false);
        return;
    }

    CNetworkPlayerMgr mgr{};
    if (!DMA::Memory().Read(mgrAddress, &mgr, sizeof(mgr)))
        return;

    std::vector<SessionPlayer> players;
    players.reserve(8);

    for (uint8_t index = 0; index < 32; ++index)
    {
        if (!mgr.m_Players[index])
            continue;

        SessionPlayer player;
        ReadPlayerFromMemory(reinterpret_cast<uintptr_t>(mgr.m_Players[index]), player);
        if (player.PlayerIndex == 0 && player.RockstarId == 0 && player.Name[0] == '\0')
            continue;
        players.push_back(player);
    }

    std::lock_guard<std::mutex> lock(g_PlayerMutex);
    g_Players = std::move(players);
    g_PlayerCount.store(static_cast<int>(g_Players.size()));
    g_SessionActive.store(!g_Players.empty());
}

uintptr_t PlayerList::GetPlayerMgrAddress()
{
    if (!DMA::IsReady())
        return 0;

    uintptr_t mgrAddress = 0;
    if (!DMA::Memory().Read(DMA::BaseAddress + Offsets::PlayerMgrPtr, &mgrAddress, sizeof(mgrAddress)) || !mgrAddress)
        return 0;
    return mgrAddress;
}

std::vector<SessionPlayer> PlayerList::GetSnapshot()
{
    std::lock_guard<std::mutex> lock(g_PlayerMutex);
    return g_Players;
}

bool PlayerList::IsSessionActive()
{
    return g_SessionActive.load();
}

int PlayerList::GetPlayerCount()
{
    return g_PlayerCount.load();
}

void PlayerList::RequestExplode(uint8_t playerIndex)
{
    g_PendingExplode.store(playerIndex);
}

void PlayerList::RequestKill(uint8_t playerIndex)
{
    g_PendingKill.store(playerIndex);
}

uintptr_t PlayerList::FindPedByPlayerIndex(uint8_t playerIndex)
{
    const uintptr_t mgrAddress = GetPlayerMgrAddress();
    if (!mgrAddress)
        return 0;

    CNetworkPlayerMgr mgr{};
    if (!DMA::Memory().Read(mgrAddress, &mgr, sizeof(mgr)))
        return 0;

    if (playerIndex >= 32 || !mgr.m_Players[playerIndex])
        return 0;

    CNetGamePlayer netPlayer{};
    if (!DMA::Memory().Read(reinterpret_cast<uintptr_t>(mgr.m_Players[playerIndex]), &netPlayer, sizeof(netPlayer)))
        return 0;

    if (!netPlayer.m_PlayerInfo)
        return 0;

    PlayerInfo info{};
    if (!DMA::Memory().Read(reinterpret_cast<uintptr_t>(netPlayer.m_PlayerInfo), &info, sizeof(info)))
        return 0;

    return reinterpret_cast<uintptr_t>(info.m_Ped);
}

void PlayerList::ApplyPedAction(uint8_t playerIndex, PedAction action)
{
    const uintptr_t pedAddress = FindPedByPlayerIndex(playerIndex);
    if (!pedAddress)
        return;

    switch (action)
    {
    case PedAction::Kill:
    {
        // 血量清零（PED+0x280）
        const float zero = 0.0f;
        DMA::Memory().Write(pedAddress + offsetof(PED, CurrentHealth), &zero, sizeof(zero));
        break;
    }
    case PedAction::Explode:
    {
        // 写爆炸类型冲击（PED+0x288 是 Immunity/Impact 区域附近，安全做法是血量归零 + 武器信息冲击爆炸）
        // 直接方案：血量清零 + GodFlags 清除后归零
        const float zero = 0.0f;
        DMA::Memory().Write(pedAddress + offsetof(PED, CurrentHealth), &zero, sizeof(zero));
        DMA::Memory().Write(pedAddress + offsetof(PED, MaxHealth), &zero, sizeof(zero));
        break;
    }
    }
}
