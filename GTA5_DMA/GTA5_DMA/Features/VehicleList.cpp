#include "pch.h"

#include "VehicleList.h"

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
    std::atomic<uintptr_t> g_PendingTeleportFull{ 0 };

    constexpr auto kRefreshInterval = std::chrono::milliseconds(700);
    std::chrono::steady_clock::time_point g_LastRefresh{};

    // 常见载具哈希 → 显示名（社区常用款；未命中显示哈希）
    struct ModelNameEntry { uint32_t hash; const char* name; };
    const ModelNameEntry kModelNames[] = {
        { 0xB779A091, "Zentorno" },  { 0x2902077D, "T20" },
        { 0x513F0D6E, "Adder" },     { 0x1BB29EA1, "Osiris" },
        { 0x84D52DA6, "Tyrus" },     { 0x4C03A120, "Osiris2" },
        { 0x18030F92, "Kuruma" },    { 0xC8DA3461, "Z-Type" },
        { 0x0520A6E9, "Tampa" },     { 0xA4305DEA, "Elegy RH8" },
        { 0xB92E7E0F, "Deluxo" },    { 0x4E3F2FA4, "Scramjet" },
        { 0x9F5B4EEE, "Oppressor" }, { 0x51B36726, "Krieger" },
        { 0xE2C290F5, "Vagner" },    { 0x77D3C6C3, "Deveste" },
        { 0x9D0450CA, "Devil" },     { 0x6C0BEF58, "Bati 801" },
        { 0xE65A8E1F, "Akuma" },     { 0x3BE2A26B, "Sanchez" },
        { 0xD199EC0D, "Toreador" },  { 0x5CA49B2C, "Stromberg" },
        { 0x056FE778, "Sultan RS" }, { 0x60C6DED1, "Sultan" },
        { 0xB5FE537C, "Faggio" },    { 0xF92F2ED4, "BMX" },
    };
}

const char* VehicleList::LookupModelName(uint32_t hash)
{
    for (const auto& e : kModelNames)
        if (e.hash == hash)
            return e.name;
    return nullptr;
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

void VehicleList::RequestTeleportVehicle(uintptr_t vehicleAddress)
{
    if (vehicleAddress == 0)
        return;
    g_PendingTeleportFull.store(vehicleAddress);
}

void VehicleList::OnDMAFrame()
{
    const auto now = std::chrono::steady_clock::now();

    // 处理传送请求（立即，不节流）
    const uintptr_t pendingAddr = g_PendingTeleportFull.exchange(0);
    if (pendingAddr)
        TeleportVehicleToPlayer(pendingAddr);

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
