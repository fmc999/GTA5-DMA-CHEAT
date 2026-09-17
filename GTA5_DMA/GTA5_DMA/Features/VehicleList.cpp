#include "pch.h"

#include "VehicleList.h"
#include "VehicleNames.h"

#include "DMA.h"
#include "Offsets.h"
#include "Reclass.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>

namespace
{
    std::mutex g_VehicleMutex;
    std::vector<SessionVehicle> g_Vehicles;
    std::atomic<int> g_VehicleCount{ 0 };
    std::atomic<bool> g_VehicleActive{ false };

    std::atomic<uint32_t> g_PendingSpawnModel{ 0 };
    std::atomic<uintptr_t> g_PendingTeleportToVehicle{ 0 };   // 我 → 载具（载具页「传送到它」唯一入口）

    // 最近一次「我 → 载具」的落地报告（UI 线程读 → 页面上显示方向/落点/读回校验）
    std::mutex g_TpReportMutex;
    TpToVehicleReport g_TpReport;

    constexpr auto kRefreshInterval = std::chrono::milliseconds(700);
    std::chrono::steady_clock::time_point g_LastRefresh{};

    // 常见载具哈希 → 显示名（社区常用款；未命中显示哈希）
}

const char* VehicleList::LookupModelName(uint32_t hash)
{
    // 第27轮：改用 VehicleNames.h 的 joaat 权威表（原手写表 13/13 条哈希与名字对不上）
    const VehicleNameEntry* e = LookupVehicleName(hash);
    return e ? e->model : nullptr;
}

void VehicleList::RefreshVehicles()
{
    // 数据链：VehiclePoolPtr → fwVehiclePool**（三重指针）
    // 解析值是模块相对偏移（ResolveRuntimeOffsets 输出），需要加运行时基址。
    const uintptr_t poolPtrSlot = DMA::BaseAddress + Offsets::VehiclePoolPtr;
    uintptr_t poolIndirect = 0;
    if (!DMA::Memory().Read(poolPtrSlot, &poolIndirect, sizeof(poolIndirect)))
        return;

    // poolIndirect = fwVehiclePool**（指向一个存 fwVehiclePool* 的位置）
    uintptr_t poolAddr = 0;
    if (!DMA::Memory().Read(poolIndirect, &poolAddr, sizeof(poolAddr)) || poolAddr == 0)
        return;

    FwVehiclePool pool{};
    if (!DMA::Memory().Read(poolAddr, &pool, sizeof(pool)))
        return;

    if (pool.m_PoolAddress == nullptr || pool.m_BitArray == nullptr || pool.m_Size == 0 || pool.m_Size > 2048)
        return;

    // 位图 + 指针表（分块读）
    const uint32_t flagBytes = ((pool.m_Size + 31) / 32) * 4;
    std::vector<uint8_t> flags(flagBytes);
    if (!DMA::Memory().Read(reinterpret_cast<uintptr_t>(pool.m_BitArray), flags.data(), flagBytes))
        return;

    std::vector<SessionVehicle> vehicles;
    vehicles.reserve(pool.m_ItemCount);

    for (uint32_t i = 0; i < pool.m_Size; ++i)
    {
        const bool valid = (flags[i >> 3] >> (i & 7)) & 1;
        if (!valid)
            continue;

        uintptr_t vehAddress = 0;
        if (!DMA::Memory().Read(reinterpret_cast<uintptr_t>(pool.m_PoolAddress) + i * sizeof(uintptr_t),
                                &vehAddress, sizeof(vehAddress)) || vehAddress == 0)
            continue;

        SessionVehicle v{};
        v.Address = vehAddress;
        v.IsValid = true;

        // 模型哈希 + 血量 + 导航（定点小读，DMA 上大块读易失败）
        DMA::Memory().Read(vehAddress + offsetof(CVehicle, EntityModelHash), &v.ModelHash, sizeof(v.ModelHash));
        DMA::Memory().Read(vehAddress + offsetof(CVehicle, Health), &v.Health, sizeof(v.Health));
        uintptr_t nav = 0;
        if (DMA::Memory().Read(vehAddress + offsetof(CVehicle, pCNavigation), &nav, sizeof(nav)) && nav)
        {
            float pos[3];
            if (DMA::Memory().Read(nav + 0x50, pos, sizeof(pos)))
            {
                v.Position[0] = pos[0]; v.Position[1] = pos[1]; v.Position[2] = pos[2];
            }
        }

        // 距离过滤：只保留本地玩家 50 米内的载具（传送距离限制，超出的不显示）
        if (DMA::LocalPlayerLocation.x != 0.0f || DMA::LocalPlayerLocation.y != 0.0f)
        {
            const float dx = v.Position[0] - DMA::LocalPlayerLocation.x;
            const float dy = v.Position[1] - DMA::LocalPlayerLocation.y;
            const float dz = v.Position[2] - DMA::LocalPlayerLocation.z;
            v.DistanceM = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (v.DistanceM > 50.0f)
                continue;
        }
        else
        {
            continue;   // 本地位置未知时不列载具（无法判距离）
        }

        const char* name = LookupModelName(v.ModelHash);
        if (name)
        {
            const size_t n = std::strlen(name);
            if (n >= sizeof(v.ModelName))
                std::memcpy(v.ModelName, name, sizeof(v.ModelName) - 1);
            else
                std::memcpy(v.ModelName, name, n);
        }

        vehicles.push_back(v);
    }

    // 排序：距离近的在前
    std::sort(vehicles.begin(), vehicles.end(), [](const SessionVehicle& a, const SessionVehicle& b) {
        return a.DistanceM < b.DistanceM;
    });
    for (size_t i = 0; i < vehicles.size(); ++i)
        vehicles[i].DisplayIndex = static_cast<uint32_t>(i + 1);

    std::lock_guard<std::mutex> lock(g_VehicleMutex);
    g_Vehicles = std::move(vehicles);
    g_VehicleCount.store(static_cast<int>(g_Vehicles.size()));
    g_VehicleActive.store(!g_Vehicles.empty());
}

void VehicleList::RequestSpawn(uint32_t modelHash)
{
    // 刷车 MVP：在池里找该模型的载具并传送到玩家身边。
    // （外部 DMA 无法调用 CREATE_VEHICLE 原生——完整刷车需要脚本线程劫持，
    //   需要实机迭代调试；先用"传送已有载具"满足立即可用）
    if (modelHash == 0)
        return;
    g_PendingSpawnModel.store(modelHash);
}

void VehicleList::OnDMAFrame()
{
    const auto now = std::chrono::steady_clock::now();

    // 我 → 载具（立即处理，不节流）——「传送到它」的唯一入口。
    // 方向契约：本分支只写本地玩家的导航位置；目标载具的导航位置绝不被写入。
    const uintptr_t pendingToVehicle = g_PendingTeleportToVehicle.exchange(0);
    if (pendingToVehicle)
        TeleportPlayerToVehicle(pendingToVehicle);

    const uint32_t pendingModel = g_PendingSpawnModel.exchange(0);
    if (pendingModel)
    {
        // 找池里该模型的最近载具 → 传送到身边
        auto snapshot = GetSnapshot();
        const SessionVehicle* best = nullptr;
        for (const auto& v : snapshot)
        {
            if (v.ModelHash == pendingModel && (!best || v.DistanceM < best->DistanceM))
                best = &v;
        }
        if (best)
            TeleportVehicleToPlayer(best->Address);
    }

    if (now - g_LastRefresh < kRefreshInterval)
        return;
    g_LastRefresh = now;

    if (Offsets::VehiclePoolPtr == 0)
        return;

    RefreshVehicles();
}

// ⚠ 方向：载具 → 我（把载具拉到我身边）。只服务于「刷车」（RequestSpawn）。
//    载具页的「传送到它」不经过这里——那条路是 TeleportPlayerToVehicle（我 → 它）。
void VehicleList::TeleportVehicleToPlayer(uintptr_t vehicleAddress)
{
    // 载具位置 = CNavigation+0x50（与玩家传送同链）。写到本地玩家前方 5 米，
    // 朝向与玩家一致。物理引擎下一帧会接管。
    uintptr_t nav = 0;
    if (!DMA::Memory().Read(vehicleAddress + offsetof(CVehicle, pCNavigation), &nav, sizeof(nav)) || !nav)
        return;

    // 目标点：本地玩家位置 + 朝向前方 5 米
    const auto& pos = DMA::LocalPlayerLocation;
    if (pos.x == 0.0f && pos.y == 0.0f)
        return;

    // 玩家朝向：PlayerInfo+0x20 (f32)。简化：直接用玩家位置 + 固定偏移
    const float dx = 5.0f;
    const float target[3] = { pos.x + dx, pos.y, pos.z };

    if (!DMA::Memory().Write(nav + 0x50, target, sizeof(target)))
    {
        std::println("[VehicleList] 载具传送失败: 导航位置写入失败");
        return;
    }
    std::println("[VehicleList] 已传送载具 0x{:X} 到身边", vehicleAddress);
}

void VehicleList::RequestTeleportToVehicle(uintptr_t vehicleAddress, uint32_t displayIndex)
{
    if (vehicleAddress == 0)
        return;

    {
        std::lock_guard<std::mutex> lock(g_TpReportMutex);
        g_TpReport.Sent = true;
        g_TpReport.Ok = false;
        g_TpReport.Index = displayIndex;
        g_TpReport.Vehicle = vehicleAddress;
        g_TpReport.Me[0] = DMA::LocalPlayerLocation.x;
        g_TpReport.Me[1] = DMA::LocalPlayerLocation.y;
        g_TpReport.Me[2] = DMA::LocalPlayerLocation.z;
        g_TpReport.VehiclePos[0] = g_TpReport.VehiclePos[1] = g_TpReport.VehiclePos[2] = 0.0f;
        g_TpReport.Landed[0] = g_TpReport.Landed[1] = g_TpReport.Landed[2] = 0.0f;
    }

    g_PendingTeleportToVehicle.store(vehicleAddress);
}

TpToVehicleReport VehicleList::GetLastTeleport()
{
    std::lock_guard<std::mutex> lock(g_TpReportMutex);
    return g_TpReport;
}

bool VehicleList::IsLocalPlayerInVehicle()
{
    if (DMA::LocalPlayerAddress == 0)
        return false;
    uint8_t bits = 0;
    if (!DMA::Memory().Read(DMA::LocalPlayerAddress + offsetof(PED, InVehicleBits), &bits, sizeof(bits)))
        return false;
    return (bits & 0x1) != 0;
}

void VehicleList::TeleportPlayerToVehicle(uintptr_t vehicleAddress)
{
    // ── 方向契约（不要再改回「载具 → 我」）────────────────────────────
    // 我 → 它：只把「本地玩家的导航位置」写到目标载具所在处。
    // 目标载具自己的 CNavigation 在本函数内【绝不写入】；唯一例外是玩家确实坐在
    // 载具里时同步写"自己的"载具导航（否则人会掉出车外），且必须 InVehicleBits 为真——
    // 旧版没做这个确认，PED.pCVehicle 的残留旧指针会把一辆已经下车的载具一起拖到落点，
    // 看上去就像"车跟着人跑 / 车被拉到我这"。
    if (vehicleAddress == 0 || DMA::NavigationAddress == 0)
    {
        std::println("[TP2VEH] 跳过: 地址未就绪 veh=0x{:X} nav=0x{:X}", vehicleAddress, DMA::NavigationAddress);
        return;
    }

    const Vec3 me = DMA::LocalPlayerLocation;
    if (me.x == 0.0f && me.y == 0.0f)
    {
        std::println("[TP2VEH] 跳过: 本地玩家坐标未就绪（等待首帧定位）");
        return;
    }

    // 点到自己正坐着的这辆车 → 无事可做（否则等于把自己传送到原地）
    if (DMA::VehicleAddress != 0 && vehicleAddress == DMA::VehicleAddress)
    {
        std::println("[TP2VEH] 跳过: 目标就是你正坐的载具 0x{:X}", vehicleAddress);
        return;
    }

    // 目标坐标：优先用「列表里显示的那一份快照坐标」——用户点哪一行、看到多少米，
    // 就落到哪里（所见即所得）；快照里找不到才实时走指针链补读一次。
    Vec3 vehiclePos = {};
    bool haveTarget = false;
    {
        const std::vector<SessionVehicle> snapshot = GetSnapshot();
        for (const SessionVehicle& v : snapshot)
        {
            if (v.Address == vehicleAddress)
            {
                vehiclePos = { v.Position[0], v.Position[1], v.Position[2] };
                haveTarget = (vehiclePos.x != 0.0f || vehiclePos.y != 0.0f || vehiclePos.z != 0.0f);
                break;
            }
        }
    }
    if (!haveTarget)
    {
        uintptr_t nav = 0;
        if (DMA::Memory().Read(vehicleAddress + offsetof(CVehicle, pCNavigation), &nav, sizeof(nav)) && nav)
        {
            Vec3 live = {};
            if (DMA::Memory().Read(nav + 0x50, &live, sizeof(live)))
            {
                vehiclePos = live;
                haveTarget = (live.x != 0.0f || live.y != 0.0f || live.z != 0.0f);
            }
        }
    }
    if (!haveTarget)
    {
        std::println("[TP2VEH] 失败: 载具坐标读不到 0x{:X}", vehicleAddress);
        return;
    }

    const float gapX = vehiclePos.x - me.x;
    const float gapY = vehiclePos.y - me.y;
    const float gap = std::sqrt(gapX * gapX + gapY * gapY);
    if (gap < 2.5f)
    {
        std::println("[TP2VEH] 跳过: 目标已在你旁边（{:.1f} 米）", gap);
        return;
    }

    // 错开 2 米防卡模（与玩家传送同一约定）
    Vec3 target = vehiclePos;
    target.x += 2.0f;
    target.y += 2.0f;

    if (!DMA::Memory().Write(DMA::NavigationAddress + offsetof(CNavigation, Position), &target, sizeof(target)))
    {
        std::println("[TP2VEH] 失败: 玩家导航位置写入失败 nav=0x{:X}", DMA::NavigationAddress);
        return;
    }

    // 载具内同步：只有"确实坐在载具里"才写自己的载具导航（防止残留指针把旧车拖走）
    const bool inVehicle = IsLocalPlayerInVehicle();
    if (inVehicle && DMA::VehicleNavigationAddress != 0 && DMA::VehicleNavigationAddress != DMA::NavigationAddress)
    {
        DMA::Memory().Write(
            DMA::VehicleNavigationAddress + offsetof(CNavigation, Position), &target, sizeof(target));
    }

    // 读回校验：确认玩家导航真的落到了目标点
    Vec3 readback = {};
    const bool ok = DMA::Memory().Read(DMA::NavigationAddress + offsetof(CNavigation, Position), &readback, sizeof(readback)) &&
                    std::fabs(readback.x - target.x) < 1.0f &&
                    std::fabs(readback.y - target.y) < 1.0f;

    {
        std::lock_guard<std::mutex> lock(g_TpReportMutex);
        g_TpReport.Ok = ok;
        g_TpReport.Me[0] = me.x;
        g_TpReport.Me[1] = me.y;
        g_TpReport.Me[2] = me.z;
        g_TpReport.VehiclePos[0] = vehiclePos.x;
        g_TpReport.VehiclePos[1] = vehiclePos.y;
        g_TpReport.VehiclePos[2] = vehiclePos.z;
        g_TpReport.Landed[0] = target.x;
        g_TpReport.Landed[1] = target.y;
        g_TpReport.Landed[2] = target.z;
    }

    std::println("[TP2VEH] dir=我->它 veh=0x{:X} 我=({:.1f},{:.1f},{:.1f}) 载具=({:.1f},{:.1f},{:.1f}) "
                 "落点=({:.1f},{:.1f},{:.1f}) 载具内={} 读回=({:.1f},{:.1f}) ok={}",
                 vehicleAddress, me.x, me.y, me.z, vehiclePos.x, vehiclePos.y, vehiclePos.z,
                 target.x, target.y, target.z, inVehicle ? "是" : "否",
                 readback.x, readback.y, ok ? 1 : 0);
}

std::vector<SessionVehicle> VehicleList::GetSnapshot()
{
    std::lock_guard<std::mutex> lock(g_VehicleMutex);
    return g_Vehicles;
}

bool VehicleList::IsActive()
{
    return g_VehicleActive.load();
}

int VehicleList::GetVehicleCount()
{
    return g_VehicleCount.load();
}
