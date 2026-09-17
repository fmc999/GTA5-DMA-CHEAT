#pragma once

// ============================================================================
//  VehicleRepair — 载具修复（当前载具 / 一键修复战局载具）
//
//  参考 YimMenuV2：
//    src/game/features/vehicle/Fix.cpp            （修复当前载具，原生 SET_VEHICLE_FIXED）
//    src/game/features/vehicle/FixAllVehicles.cpp （一次性修复全部载具）
//
//  YimMenu 走原生；本项目是外部 DMA，不能调原生，改为直接写 CVehicle 的四段健康值
//  并逐字段读回校验（写入值与 VehicleEditor 的修复动作一致：1000）。
//
//  写入字段（Core/Reclass.h 用 static_assert 钉死偏移，挪位会在编译期报错）：
//    CVehicle::Health           0x0280   载具血量
//    CVehicle::BodyHealth       0x0830   车身
//    CVehicle::PetrolTankHealth 0x0834   油箱
//    CVehicle::EngineHealth     0x0910   引擎
//
//  安全设计：
//    · 只在 DMA 就绪且地址非 0 时写；地址来自 VehicleList 快照 / DMA::VehicleAddress。
//    · 每个字段写完立即读回校验，页面显示的「字段校验 n/m」是实测结果而不是假设。
//    · 不碰 0x972 这类语义未验证的“外观修复”魔数，避免在未知字段上盲写。
//    · 自动修复带 1.5 秒节流且只在血量低于 80% 时触发，不会每帧刷 DMA 写。
// ============================================================================

#include <atomic>
#include <cstdint>

// 最近一次修复的落地报告（UI 线程读，用于页面自检）
struct VehicleRepairReport
{
    bool  Sent = false;          // 是否收到过请求
    bool  All = false;           // true = 一键修复战局载具；false = 修复当前载具
    bool  Auto = false;          // true = 由自动修复触发
    int   Requested = 0;         // 本次覆盖的载具数
    int   Fixed = 0;             // 四段全部写入并读回校验通过的载具数
    int   VerifiedFields = 0;    // 读回校验通过的字段数
    int   TotalFields = 0;       // 尝试写入的字段数
    float HealthBefore = 0.0f;   // 第一辆目标载具修复前的血量
    float HealthAfter = 0.0f;    // 修复后读回的血量
    uintptr_t Vehicle = 0;       // 第一辆目标载具地址
};

class VehicleRepair
{
public:
    // 自动修复当前载具（UI 线程写，DMA 线程消费）
    static inline std::atomic<bool> bAutoRepair{ false };

    // UI 线程 → DMA 线程的请求
    static void RequestRepairCurrent();
    static void RequestRepairAll();

    static void OnDMAFrame();

    // UI 只读状态
    static VehicleRepairReport GetLastReport();
    static void ResetReport();

    static constexpr uint32_t kFieldCount = 4;   // 载具血量 / 车身 / 油箱 / 引擎
};
