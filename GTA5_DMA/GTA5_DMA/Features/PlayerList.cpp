#include "pch.h"

#include "PlayerList.h"

#include "DMA.h"
#include "Offsets.h"
#include "Reclass.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>

namespace
{
    uintptr_t GetPlayerMgrAddressImpl();  // 定义于下方匿名命名空间

    std::mutex g_PlayerMutex;
    std::vector<SessionPlayer> g_Players;
    std::atomic<int> g_PlayerCount{ 0 };
    std::atomic<bool> g_SessionActive{ false };
    std::atomic<uint8_t> g_PendingExplode{ 0xFF };
    std::atomic<uint8_t> g_PendingKill{ 0xFF };
    std::atomic<uint8_t> g_PendingTeleportTo{ 0xFF };
    std::atomic<int> g_TotalJoins{ 0 };    // 本进程累计加入人次
    std::atomic<int> g_TotalLeaves{ 0 };   // 本进程累计离开人次

    constexpr auto kRefreshInterval = std::chrono::milliseconds(500);
    std::chrono::steady_clock::time_point g_LastRefresh{};

    // 64 位循环左移（YimMenu 池解密用）
    uint64_t Rotl64(uint64_t value, uint32_t shift)
    {
        shift &= 63;
        return (value << shift) | (value >> (64 - shift));
    }

    // YimMenuV2 池解密: x = rotl64(Second,30); pool = ~rotl64(rotl64(x ^ First,32), (x&0x1F)+2)
    uintptr_t DecryptPedPool(const PoolEncryption& enc)
    {
        if (!enc.m_IsSet)
            return 0;
        const uint64_t x = Rotl64(enc.m_Second, 30);
        return static_cast<uintptr_t>(
            ~Rotl64(Rotl64(x ^ enc.m_First, 32), static_cast<uint32_t>(x & 0x1F) + 2));
    }

    // ============================================================================
    // 数据链：YimMenuV2 现代实现（Enhanced 加密池）
    //   PedPoolPtr(特征码) -> PoolEncryption -> 解密 -> fwBasePool
    //   槽位有效: !(Flags[i] & 0x80)，地址 = Entries + i*ItemSize
    //   玩家判定: PED+0x10A8 有 PlayerInfo；Name 在 PlayerInfo+0xFC
    // ============================================================================

    void ReadPedIntoPlayer(const PED& ped, uintptr_t pedAddress, SessionPlayer& out)
    {
        out.PedAddress = pedAddress;
        out.NavigationAddress = reinterpret_cast<uintptr_t>(ped.pCNavigation);
        out.Health = ped.CurrentHealth;
        out.MaxHealth = ped.MaxHealth;
        out.GodMode = (ped.GodFlags & 0x1) != 0;
        // 载具判定（双信号，覆盖下车再上车/换车/载具损毁全部场景）：
        // - InVehicleBits(0xE32) bit0: CT 验证的权威载具状态位，上下车即翻转
        // - pCVehicle(0xD10) 非空作副证：载具损毁弹出时位翻转可能滞后一帧
        out.InVehicle = (ped.InVehicleBits & 0x1) != 0 || ped.pCVehicle != nullptr;

        // 位置：只读 CNavigation 的 vec3（0x50 处 12 字节）
        if (ped.pCNavigation)
        {
            Vec3 position = {};
            if (DMA::Memory().Read(reinterpret_cast<uintptr_t>(ped.pCNavigation) + offsetof(CNavigation, Position), &position, sizeof(position)))
                out.Position = position;
        }

        // 护甲 PED+0x150C（CT: CPed.Armor）
        DMA::Memory().Read(pedAddress + 0x150C, &out.Armor, sizeof(out.Armor));

        // PlayerInfo: Name + Wanted（CT 验证: +0x10A8 / Name+0xFC / Wanted+0x8E8）
        // 只读需要的字段（整结构 0x10E8 远程一次读可能部分失败）
        if (ped.pPlayerInfo)
        {
            const uintptr_t infoAddress = reinterpret_cast<uintptr_t>(ped.pPlayerInfo);
            char name[20] = {};
            if (DMA::Memory().Read(infoAddress + offsetof(PlayerInfo, Name), name, sizeof(name) - 1))
            {
                name[sizeof(name) - 1] = '\0';
                // 截断 C 字符串
                for (int i = 0; i < 19; ++i)
                {
                    if (name[i] == '\0') { break; }
                    if (static_cast<unsigned char>(name[i]) < 0x20 || static_cast<unsigned char>(name[i]) == 0x7F) { name[i] = '\0'; break; }
                }
                std::memcpy(out.Name, name, sizeof(out.Name));
            }
            int32_t wanted = 0;
            if (DMA::Memory().Read(infoAddress + offsetof(PlayerInfo, WantedLevel), &wanted, sizeof(wanted)))
                out.WantedLevel = wanted;
        }
    }

    // GPBD_FM 玩家统计（全局 1855653，PlayerIndex 决定条目）
    void ReadPlayerStats(SessionPlayer& out)
    {
        constexpr uint32_t kGpbdFmBase = 1855653;
        constexpr uint32_t kEntrySize = 884;      // 每玩家条目槽数
        constexpr uint32_t kStatsSlotInEntry = 192; // PlayerStats 起始槽位
        // PlayerStats 内字段槽位（YimMenu PLAYER_STATS 核对）
        constexpr uint32_t kStatRP = 1;
        constexpr uint32_t kStatRank = 6;
        constexpr uint32_t kStatKdRatio = 22;
        constexpr uint32_t kStatKills = 24;
        constexpr uint32_t kStatDeaths = 25;
        constexpr uint32_t kStatMoney = 52;

        int32_t rank = 0, rp = 0, money = 0, kills = 0, deaths = 0;
        float kd = 0.0f;
        const uint32_t base = kGpbdFmBase + 1 + out.PlayerIndex * kEntrySize + kStatsSlotInEntry;
        DMA::GetGlobalValue(base + kStatRank, rank);
        DMA::GetGlobalValue(base + kStatRP, rp);
        DMA::GetGlobalValue(base + kStatMoney, money);
        DMA::GetGlobalValue(base + kStatKills, kills);
        DMA::GetGlobalValue(base + kStatDeaths, deaths);
        DMA::GetGlobalValue(base + kStatKdRatio, kd);
        if (rank > 0 && rank < 8000)
            out.Rank = rank;
        if (rp >= 0)
            out.RP = rp;
        if (money > 0 && money < 2100000000)
            out.Money = money;
        out.KillsOnPlayers = kills > 0 ? kills : 0;
        out.DeathsByPlayers = deaths > 0 ? deaths : 0;
        if (kd >= 0.0f && kd < 100.0f)
            out.KdRatio = kd;
    }

    // CNetworkPlayerMgr -> players[idx] -> RID（仅用于 RID；其余数据走 Ped 池）
    void ReadNetPlayerRid(uint8_t playerIndex, SessionPlayer& out)
    {
        const uintptr_t mgrAddress = GetPlayerMgrAddressImpl();
        if (!mgrAddress)
            return;
        // 定点读 players[idx] 指针（8 字节）
        uintptr_t netPlayerAddress = 0;
        if (!DMA::Memory().Read(mgrAddress + offsetof(CNetworkPlayerMgr, m_Players) + playerIndex * sizeof(uintptr_t), &netPlayerAddress, sizeof(netPlayerAddress)) || !netPlayerAddress)
            return;
        // 定点读 RID（+0x10，8 字节）
        DMA::Memory().Read(netPlayerAddress + offsetof(CNetGamePlayer, m_RockstarId), &out.RockstarId, sizeof(out.RockstarId));
    }

    uintptr_t GetPlayerMgrAddressImpl()
    {
        if (!DMA::IsReady())
            return 0;
        uintptr_t mgrAddress = 0;
        if (!DMA::Memory().Read(DMA::BaseAddress + Offsets::PlayerMgrPtr, &mgrAddress, sizeof(mgrAddress)) || !mgrAddress)
            return 0;
        return mgrAddress;
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

    const uint8_t teleportTarget = g_PendingTeleportTo.exchange(0xFF);
    if (teleportTarget != 0xFF)
        TeleportToPlayer(teleportTarget);
}

void PlayerList::RefreshPlayers()
{
    // 模式 A（优先）：YimMenuV2 加密 Ped 池（现代链路，跟版本维护）
    if (Offsets::PedPoolPtr != 0)
    {
        // PoolEncryption（定点读 0x18 字节）
        PoolEncryption enc{};
        if (!DMA::Memory().Read(DMA::BaseAddress + Offsets::PedPoolPtr, &enc, sizeof(enc)) || !enc.m_IsSet)
            return;

        const uintptr_t poolAddress = DecryptPedPool(enc);
        if (!poolAddress)
            return;

        // fwBasePool（定点读关键字段）
        uintptr_t entries = 0, flagsAddress = 0;
        uint32_t poolSize = 0, itemSize = 0;
        if (!DMA::Memory().Read(poolAddress + 0x08, &entries, sizeof(entries)) || !entries)
            return;
        if (!DMA::Memory().Read(poolAddress + 0x10, &flagsAddress, sizeof(flagsAddress)) || !flagsAddress)
            return;
        if (!DMA::Memory().Read(poolAddress + 0x18, &poolSize, sizeof(poolSize)) || poolSize == 0 || poolSize > 512)
            return;
        if (!DMA::Memory().Read(poolAddress + 0x1C, &itemSize, sizeof(itemSize)) || itemSize == 0 || itemSize > 0x4000)
            return;

        // 标志位数组一次读入（poolSize 字节）
        std::vector<uint8_t> flags(poolSize);
        if (!DMA::Memory().Read(flagsAddress, flags.data(), poolSize))
            return;

        // 逐槽扫描（有效槽: !(flags[i] & 0x80)），读 PED 关键字段
        std::vector<SessionPlayer> players;
        players.reserve(16);
        for (uint32_t slot = 0; slot < poolSize; ++slot)
        {
            if (flags[slot] & 0x80)
                continue;  // 空槽

            const uintptr_t pedAddress = entries + slot * itemSize;

            // 玩家 Ped 判定：有 PlayerInfo（+0x10A8，定点读）
            uintptr_t pedPlayerInfo = 0;
            if (!DMA::Memory().Read(pedAddress + offsetof(PED, pPlayerInfo), &pedPlayerInfo, sizeof(pedPlayerInfo)))
                continue;
            if (!pedPlayerInfo)
                continue;

            // 定点读 PED 关键字段
            PED ped{};
            ped.pPlayerInfo = reinterpret_cast<class PlayerInfo*>(pedPlayerInfo);
            DMA::Memory().Read(pedAddress + offsetof(PED, pCNavigation), &ped.pCNavigation, sizeof(ped.pCNavigation));
            DMA::Memory().Read(pedAddress + offsetof(PED, CurrentHealth), &ped.CurrentHealth, sizeof(ped.CurrentHealth));
            DMA::Memory().Read(pedAddress + offsetof(PED, MaxHealth), &ped.MaxHealth, sizeof(ped.MaxHealth));
            DMA::Memory().Read(pedAddress + offsetof(PED, GodFlags), &ped.GodFlags, sizeof(ped.GodFlags));
            DMA::Memory().Read(pedAddress + offsetof(PED, pCVehicle), &ped.pCVehicle, sizeof(ped.pCVehicle));
            DMA::Memory().Read(pedAddress + offsetof(PED, InVehicleBits), &ped.InVehicleBits, sizeof(ped.InVehicleBits));

            SessionPlayer player;
            player.PlayerIndex = 0xFF;
            ReadPedIntoPlayer(ped, pedAddress, player);

            if (player.Name[0] == '\0')
                continue;

            player.IsLocal = (pedAddress == DMA::LocalPlayerAddress);
            if (player.Position.x != 0.0f || player.Position.y != 0.0f || player.Position.z != 0.0f)
                player.Distance = DMA::LocalPlayerLocation.Distance(player.Position);

            players.push_back(player);
        }

        // mgr 槽位名册（定点读）：本地 flags 匹配 + 名字匹配 -> PlayerIndex/RID/统计
        {
            struct MgrEntry { char name[20]; int64_t rid; };
            MgrEntry mgrEntries[32];
            std::memset(mgrEntries, 0, sizeof(mgrEntries));
            const uintptr_t mgrAddress = GetPlayerMgrAddressImpl();
            if (mgrAddress)
            {
                for (uint8_t idx = 0; idx < 32; ++idx)
                {
                    uintptr_t netPlayerAddress = 0;
                    if (!DMA::Memory().Read(mgrAddress + offsetof(CNetworkPlayerMgr, m_Players) + idx * sizeof(uintptr_t), &netPlayerAddress, sizeof(netPlayerAddress)) || !netPlayerAddress)
                        continue;
                    DMA::Memory().Read(netPlayerAddress + offsetof(CNetGamePlayer, m_RockstarId), &mgrEntries[idx].rid, sizeof(mgrEntries[idx].rid));
                    // IsLocal flag（+0xD0 定点）
                    uint8_t flag = 0;
                    if (DMA::Memory().Read(netPlayerAddress + 0xD0, &flag, sizeof(flag)) && (flag & 0x1))
                    {
                        for (auto& player : players)
                        {
                            if (player.IsLocal && player.PlayerIndex == 0xFF)
                            {
                                player.PlayerIndex = idx;
                                player.RockstarId = mgrEntries[idx].rid;
                            }
                        }
                    }
                    // 名称（+0xE8 PlayerInfo -> +0xFC 定点）
                    uintptr_t playerInfoAddress = 0;
                    if (DMA::Memory().Read(netPlayerAddress + offsetof(CNetGamePlayer, m_PlayerInfo), &playerInfoAddress, sizeof(playerInfoAddress)) && playerInfoAddress)
                    {
                        DMA::Memory().Read(playerInfoAddress + offsetof(PlayerInfo, Name), mgrEntries[idx].name, sizeof(mgrEntries[idx].name) - 1);
                        mgrEntries[idx].name[sizeof(mgrEntries[idx].name) - 1] = '\0';
                    }
                }
            }

            // 远程玩家按名字匹配
            for (auto& player : players)
            {
                if (player.PlayerIndex != 0xFF)
                    continue;
                for (uint8_t idx = 0; idx < 32; ++idx)
                {
                    if (mgrEntries[idx].name[0] == '\0')
                        continue;
                    if (std::strncmp(player.Name, mgrEntries[idx].name, sizeof(player.Name)) == 0)
                    {
                        player.PlayerIndex = idx;
                        player.RockstarId = mgrEntries[idx].rid;
                        break;
                    }
                }
            }

            // 统计（索引确定后）
            for (auto& player : players)
            {
                if (player.PlayerIndex != 0xFF)
                    ReadPlayerStats(player);
            }
        }

        // 加入/退出跟踪（静默）：离开需连续 2 帧缺失才确认（防 mgr/池不同步闪烁）。
        // 只维护计数与最近事件，不再逐条打印控制台。
        {
            static std::vector<std::string> s_LastNames;
            static std::vector<std::string> s_PendingLeave;
            std::vector<std::string> currentNames;
            currentNames.reserve(players.size());
            for (const SessionPlayer& player : players)
            {
                if (player.Name[0] != '\0' && !player.IsLocal)
                    currentNames.emplace_back(player.Name);
            }

            if (!s_LastNames.empty() || !currentNames.empty())
            {
                const bool logEnabled = bLogJoinLeave.load();
                for (const auto& name : currentNames)
                {
                    if (std::find(s_LastNames.begin(), s_LastNames.end(), name) == s_LastNames.end())
                    {
                        g_TotalJoins.fetch_add(1);
                        if (logEnabled)
                            std::println("[PlayerList] + 玩家加入: {}", name);
                    }
                }
                std::vector<std::string> stillMissing;
                for (const auto& name : s_LastNames)
                {
                    if (std::find(currentNames.begin(), currentNames.end(), name) == currentNames.end())
                    {
                        if (std::find(s_PendingLeave.begin(), s_PendingLeave.end(), name) != s_PendingLeave.end())
                        {
                            g_TotalLeaves.fetch_add(1);
                            if (logEnabled)
                                std::println("[PlayerList] - 玩家离开: {}", name);
                        }
                        else
                            stillMissing.push_back(name);
                    }
                }
                s_PendingLeave = std::move(stillMissing);
            }
            s_LastNames = std::move(currentNames);
        }

        {
            static bool s_LoggedModeA = false;
            if (!s_LoggedModeA)
            {
                std::println("[PlayerList] 玩家监控就绪: {} 名玩家", players.size());
                s_LoggedModeA = true;
            }
        }

        // 排序：本地玩家置顶，其余按名字字母序（稳定输出，不随池槽位跳动）
        std::sort(players.begin(), players.end(), [](const SessionPlayer& a, const SessionPlayer& b) {
            if (a.IsLocal != b.IsLocal)
                return a.IsLocal;             // 本地最前
            if ((a.Rank > 0) != (b.Rank > 0))
                return a.Rank > 0;            // 有有效统计的靠前
            return std::strncmp(a.Name, b.Name, sizeof(a.Name)) < 0;
        });
        // 重排后序号 = 显示序（1 起）
        for (size_t i = 0; i < players.size(); ++i)
            players[i].DisplayIndex = static_cast<uint32_t>(i + 1);

        std::lock_guard<std::mutex> lock(g_PlayerMutex);
        g_Players = std::move(players);
        g_PlayerCount.store(static_cast<int>(g_Players.size()));
        g_SessionActive.store(!g_Players.empty());
        return;
    }

    // 模式 B（回退）：CNetworkPlayerMgr 槽位枚举（全定点读）
    // 名称经 CNetGamePlayer+0xE8 -> PlayerInfo+0xFC（定点读，之前整结构读得到
    // 垃圾指针是 0x370 大读部分失败所致）
    const uintptr_t mgrAddress = GetPlayerMgrAddressImpl();
    if (!mgrAddress)
    {
        std::lock_guard<std::mutex> lock(g_PlayerMutex);
        g_Players.clear();
        g_PlayerCount.store(0);
        g_SessionActive.store(false);
        return;
    }

    std::vector<SessionPlayer> players;
    players.reserve(8);
    for (uint8_t index = 0; index < 32; ++index)
    {
        uintptr_t netPlayerAddress = 0;
        if (!DMA::Memory().Read(mgrAddress + offsetof(CNetworkPlayerMgr, m_Players) + index * sizeof(uintptr_t), &netPlayerAddress, sizeof(netPlayerAddress)) || !netPlayerAddress)
            continue;

        SessionPlayer player;
        player.PlayerIndex = index;

        // RID（+0x10 定点）
        DMA::Memory().Read(netPlayerAddress + offsetof(CNetGamePlayer, m_RockstarId), &player.RockstarId, sizeof(player.RockstarId));

        // 名称（+0xE8 PlayerInfo -> +0xFC，全定点）
        uintptr_t playerInfoAddress = 0;
        if (DMA::Memory().Read(netPlayerAddress + offsetof(CNetGamePlayer, m_PlayerInfo), &playerInfoAddress, sizeof(playerInfoAddress)) && playerInfoAddress)
        {
            char name[20] = {};
            if (DMA::Memory().Read(playerInfoAddress + offsetof(PlayerInfo, Name), name, sizeof(name) - 1))
            {
                name[sizeof(name) - 1] = '\0';
                for (int i = 0; i < 19; ++i)
                {
                    if (name[i] == '\0')
                        break;
                    if (static_cast<unsigned char>(name[i]) < 0x20 || static_cast<unsigned char>(name[i]) == 0x7F)
                    {
                        name[i] = '\0';
                        break;
                    }
                }
                std::memcpy(player.Name, name, sizeof(player.Name));
            }
        }

        ReadPlayerStats(player);
        players.push_back(player);
    }

    std::lock_guard<std::mutex> lock(g_PlayerMutex);
    g_Players = std::move(players);
    g_PlayerCount.store(static_cast<int>(g_Players.size()));
    g_SessionActive.store(!g_Players.empty());
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

void PlayerList::RequestTeleportTo(uint8_t playerIndex)
{
    g_PendingTeleportTo.store(playerIndex);
}

void PlayerList::TeleportToPlayer(uint8_t playerIndex)
{
    // 我 → 目标玩家：把本地玩家的导航位置写到目标玩家位置附近
    if (DMA::NavigationAddress == 0)
        return;

    // 从最新快照取目标位置（比重新走指针链快，且快照 500ms 内有效）
    Vec3 target = {};
    {
        std::lock_guard<std::mutex> lock(g_PlayerMutex);
        for (const SessionPlayer& player : g_Players)
        {
            if (player.PlayerIndex == playerIndex)
            {
                target = player.Position;
                break;
            }
        }
    }
    if (target.x == 0.0f && target.y == 0.0f && target.z == 0.0f)
        return;

    // 与目标错开 2 米，避免卡模
    target.x += 2.0f;
    target.y += 2.0f;

    DMA::Memory().Write(
        DMA::NavigationAddress + offsetof(CNavigation, Position),
        &target, sizeof(target));

    // 载具内传送：同时写载具导航位置（与 Teleport::OverwriteLocation 同模式）
    if (DMA::VehicleNavigationAddress != 0)
    {
        DMA::Memory().Write(
            DMA::VehicleNavigationAddress + offsetof(CNavigation, Position),
            &target, sizeof(target));
    }
}

uintptr_t PlayerList::FindPedByPlayerIndex(uint8_t playerIndex)
{
    // 快照优先（ped 池模式下 PlayerIndex 即列表索引无关；用 mgr 槽位匹配 RID）
    const uintptr_t mgrAddress = GetPlayerMgrAddressImpl();
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

    const int64_t rid = netPlayer.m_RockstarId;

    // 在快照里按 RID 找 Ped
    std::lock_guard<std::mutex> lock(g_PlayerMutex);
    for (const SessionPlayer& player : g_Players)
    {
        if (player.RockstarId == rid && player.PedAddress != 0)
            return player.PedAddress;
    }
    return 0;
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
