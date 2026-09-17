#pragma once

// ============================================================================
// OffRadar —— 雷达隐身（纯 DMA：只写脚本全局 + 读网络时间指针）
//
// 参考 YimMenuV2 self/OffTheRadar.cpp：
//     *ScriptGlobal(2673276).At(58).As<int*>() = *Pointers.NetworkTime;
//   —— 把「本地玩家雷达隐身到期时间」写成当前网络时间；它是循环命令，每帧重写。
//
// 我们的实现：
//   · 网络时间从 YimMenuV2 的同一条特征码 (networkTimePtrn) 解析出的全局读；
//   · 开关打开时，在 DMA 线程里按 1 秒一次重写该全局（等价于 Yim 的循环重写）；
//   · 两道闸：写前要求「当前值 = 0 或与网络时间同量级」，写后立刻读回校验；
//   · 关闭时把该全局写 0（表示不再隐身）。
// ============================================================================

#include <atomic>
#include <cstdint>
#include <string>

class OffRadar
{
public:
    static inline std::atomic<bool> bEnabled{ false };      // 开关（UI）
    static inline std::atomic<bool> bLocated{ false };      // 网络时间指针是否已解析
    static inline std::atomic<uint32_t> networkTime{ 0 };   // 最近一次读到的网络时间
    static inline std::atomic<uint32_t> lastWritten{ 0 };   // 最近一次写入值
    static inline std::atomic<int> writeCount{ 0 };         // 已写入次数
    static inline std::atomic<int> blockedCount{ 0 };       // 被闸门拒绝次数

    // DMA 线程每帧调用
    static void OnDMAFrame();

    // 诊断（--offradar-probe）
    static int Probe();

    // UI/日志用：一句话状态
    static std::string StatusText();
};
