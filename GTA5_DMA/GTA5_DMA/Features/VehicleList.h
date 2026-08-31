#pragma once

#include <atomic>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

#include "../Core/Offsets.h"
#include "../Core/Reclass.h"

// 战局载具监控（参考 YimMenuV2 Vehicle 池方案）
// 数据链：VehiclePoolPtr(特征码) → fwVehiclePool** → fwVehiclePool → 池指针数组 + 位图
// 每槽 CVehicle：模型哈希 / 位置 / 血量 / 驾驶员 / 距离
struct SessionVehicle
{
    uint32_t DisplayIndex = 0;   // 显示序号
    uintptr_t Address = 0;       // CVehicle 地址
    uint32_t ModelHash = 0;      // 模型哈希（0x18）
    float Position[3] = {0, 0, 0}; // 世界坐标（CNavigation+0x50）
    float Health = 0;            // 血量（0x280）
    float DistanceM = 0;         // 与本地玩家距离（米）
    uintptr_t DriverPed = 0;     // 驾驶员 Ped（0x?? 由池扫描填充 — 0xD10 反向不可靠，用 0x1460 座位数判断）
    uint8_t SeatCount = 0;       // 座位数
    bool IsValid = false;
    char ModelName[20] = {0};    // 常见模型哈希 → 名字（内置映射）
};

class VehicleList
{
public:
    // DMA 线程：每帧刷新（内部按间隔节流）
    static void OnDMAFrame();

    // UI 线程：快照
    static std::vector<SessionVehicle> GetSnapshot();

    // 状态
    static bool IsActive();
    static int GetVehicleCount();

    // 刷车（DMA 线程消费）：把指定载具传送到本地玩家身边
    // modelHash=0 表示"最近的一辆"
    static void RequestSpawn(uint32_t modelHash);
    static void RequestTeleportVehicle(uintptr_t vehicleAddress);   // 指定载具传送到身边

private:
    static void RefreshVehicles();
    static void TeleportVehicleToPlayer(uintptr_t vehicleAddress);

    // 内置常见载具哈希 → 显示名（完整表太大，只放常用的）
    static const char* LookupModelName(uint32_t hash);
};
