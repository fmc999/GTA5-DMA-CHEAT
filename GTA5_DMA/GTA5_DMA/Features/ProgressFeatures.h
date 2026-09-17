#pragma once

#include <atomic>
#include <cstdint>

// ============================================================================
//  ProgressFeatures —— 进度/解锁类功能（全部走 Tunables，纯 DMA 写脚本全局单元）
//
//  参考 YimMenuV2：
//    src/game/features/recovery/RpMultiplier.cpp            Tunable XP_MULTIPLIER（float）
//    src/game/features/recovery/NoChangeAppearanceCooldown.cpp  Tunable CHARACTER_APPEARANCE_COOLDOWN
//    src/game/features/recovery/FreeChangeAppearance.cpp    Tunable CHARACTER_APPEARANCE_CHARGE
//
//  YimMenu 是 LoopedCommand（每帧写）；本项目同样每次 DMA 循环复查重写，
//  因为游戏在进战局 / 刷新 tunables 时会把值覆盖回默认。
//
//  实测默认值（2026-09-17 本机）：
//    XP_MULTIPLIER = 1.0f        → 写 >1 放大 RP 收益
//    CHARACTER_APPEARANCE_COOLDOWN = 2880000（48 分钟）→ 写 0 免冷却
//    CHARACTER_APPEARANCE_CHARGE   = 100000（$100k）  → 写 0 免费
// ============================================================================

class ProgressFeatures
{
public:
    static inline std::atomic<bool>  bRpMultiplier{ false };
    static inline std::atomic<float> rpMultiplier{ 2.0f };
    static inline std::atomic<bool>  bNoAppearanceCooldown{ false };
    static inline std::atomic<bool>  bFreeAppearance{ false };

    // ---- 第18轮：金额/额度类（抢劫收益、挑战奖励）----
    //  这些 tunable 没有固定默认值（游戏按配置浮动），用「倍率 × 解析时记录的原值」方式，
    //  写之前 clamp 到登记表给的合法区间；关闭时写回原值。
    static constexpr uint32_t kValueSlotCount = 9;
    static inline std::atomic<bool>  bValueMultiplier{ false };     // 总开关
    static inline std::atomic<float> valueMultiplier{ 2.0f };       // 倍率
    static inline std::atomic<bool>  valueSlotEnabled[kValueSlotCount] = {};

    static bool Resolve();
    static void OnDMAFrame();
    static bool RestoreAll();
    static bool PrepareForClose();
    static void Reset();

    // 金额类槽位查询（UI）
    static int         GetValueSlotByEntry(uint32_t entry);            // 条目下标 → 槽位，-1 = 不是金额类
    static const char* GetValueSlotName(uint32_t slot);
    static const char* GetValueSlotLabel(uint32_t slot);   // 中文短名（UI 显示用）
    static int32_t     GetValueSlotOriginal(uint32_t slot);
    static int32_t     GetValueSlotLive(uint32_t slot, bool* ok = nullptr);

    // UI 自检报告
    struct Report
    {
        int  appliedSlots = 0;   // 已成功写入的槽位数
        int  blockedWrites = 0;  // 被拒绝/被覆盖的写入次数
        int  valueApplied = 0;   // 本轮已生效的金额类槽位数
        char last[160] = {};     // 最近一次动作描述
    };
    static Report GetReport();
    static int GetResolvedCount();
};

