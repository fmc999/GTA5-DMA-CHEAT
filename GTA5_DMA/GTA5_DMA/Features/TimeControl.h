#pragma once

// ============================================================================
// TimeControl —— 游戏内时钟（纯 DMA：直接写时钟结构体字节）
//
// 参考本仓库 Attic/TimeControl.cpp（旧实现）+ 参考项目 world/TimeControl.cpp。
// 旧实现用静态地址 BaseAddress + 0x47CDF70，无特征码兜底，失效后被停用。
// 本版：
//   · 先**实测**该地址是否仍是时钟（时/分/秒/日 字段是否合理、秒是否每秒 +1）；
//   · 两道闸：写前要求字段在合法区间（时 0~23、分/秒 0~59），写后立刻读回校验；
//   · 支持「冻结时间」与「设定时间」两种用法；
//   · 只写本地时钟字段 —— 不改钱、不改统计、不改坐标，属于纯本地视觉类功能。
//
// 字段布局（沿用旧实现，先实测确认）：
//   +(-0x0C)  day    (1 字节)
//   +0x00     hour   (1 字节)
//   +0x04     minute (1 字节)
//   +0x08     second (1 字节)
// ============================================================================

#include <atomic>
#include <cstdint>
#include <string>

class TimeControl
{
public:
    // —— UI 状态 ——
    static inline std::atomic<bool> bEnable{ false };      // 总开关（开=持续写实时钟）
    static inline std::atomic<bool> bFreeze{ false };      // 冻结（分/秒按住不动）
    static inline std::atomic<int> hour{ 12 };
    static inline std::atomic<int> minute{ 0 };
    static inline std::atomic<int> second{ 0 };
    static inline std::atomic<int> day{ 0 };

    // —— 运行时状态（UI 显示 / 诊断）——
    static inline std::atomic<bool> bLocated{ false };     // 时钟地址是否实测通过
    static inline std::atomic<int> liveHour{ -1 };
    static inline std::atomic<int> liveMinute{ -1 };
    static inline std::atomic<int> liveSecond{ -1 };
    static inline std::atomic<int> writeCount{ 0 };
    static inline std::atomic<int> blockedCount{ 0 };
    static inline std::atomic<int> lastWrittenHour{ -1 };

    static void OnDMAFrame();

    // 一句话状态（UI）
    static std::string StatusText();

    // 只读体检（--clock-probe）：读时钟字段并采样两次看秒是否在走
    static int Probe();

    // 重新定位游戏时钟（--clock-scan）：扫模块可写数据段找「时/分/秒」三连字段并验证秒在走
    static int ClockScan();

    // 写入自检（--clock-selftest）：记录原值 → 写 → 读回 → 还原
    static int SelfTest();
};
