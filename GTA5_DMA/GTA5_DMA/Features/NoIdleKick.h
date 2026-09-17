#pragma once

#include <atomic>
#include <cstdint>

// 在线挂机踢出保护（本轮起改为走 Tunables 注册表，索引由动态定位得出）。
//
// 参考 YimMenuV2 src/game/features/self/NoIdleKick.cpp：把空闲/受限踢出的计时 tunable 顶到上限。
// 历史缺陷：本模块曾硬编码全局索引 0x80056..0x80059 / 0x82132..0x82135，
// 而 tunable 的真身是 0x40055..0x40058 / 0x42131..0x42134 —— 正好多加了 TUNABLE_BASE_ADDRESS(0x40001)
// 一遍，所以它从来没读到过块（实机日志一直是 "tunable block unavailable"）。
// 现在索引由 Tunables 的值锚动态定位 + 默认值体检给出，硬编码表已删除。
class NoIdleKick
{
public:
    static inline std::atomic<bool> bEnable{ false };

    static bool Resolve();
    static void OnDMAFrame();
    static bool RestoreAll();
    static bool PrepareForClose();
    static void Reset();

    static int GetResolvedCount();
    static int GetAppliedCount();
    static uintptr_t GetSlot(uint32_t slot);           // 槽位对应的脚本全局单元地址
    static uint32_t GetTunableIndex(uint32_t slot);    // 槽位对应的全局索引（来自 Tunables）
    static const char* GetSlotName(uint32_t slot);

    // 8 个槽位：IDLEKICK_WARNING1..KICK + ConstrainedKick_Warning1..Kick
    static constexpr uint32_t kSlotCount = 8;
};
