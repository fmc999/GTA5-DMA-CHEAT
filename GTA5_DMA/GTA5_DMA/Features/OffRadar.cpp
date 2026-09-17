#include "pch.h"

#include "OffRadar.h"

#include "DMA.h"
#include "Offsets.h"

#include <cstdio>
#include <ctime>

// ============================================================================
// 雷达隐身（纯 DMA）
//   YimMenuV2：*ScriptGlobal(2673276).At(58) = *Pointers.NetworkTime（每帧重写）
//   索引换算：At(58) = 2673276 + 58 = 2673334
// ============================================================================
namespace
{
    constexpr uint32_t kOffRadarGlobal = 2673276u + 58u;   // 雷达隐身到期时间（本地玩家）
    constexpr int64_t  kOneDay = 86400;                    // 合法性窗口：与网络时间相差 1 天以内

    uint32_t g_lastApplyTick = 0;      // 节流：1 秒一次
    bool     g_releasePending = false; // 关闭时补写一次 0

    bool ReadNetTime(uint32_t& out)
    {
        if (Offsets::NetworkTimePtr == 0)
            return false;
        uint32_t v = 0;
        if (!DMA::Memory().Read(Offsets::NetworkTimePtr, &v, sizeof(v)))
            return false;
        out = v;
        return v > 1000;   // 网络时间是个大整数（游戏内累计秒）
    }

    bool PlausibleCurrent(int32_t current, uint32_t netTime)
    {
        if (current == 0)
            return true;                                  // 未隐身
        const int64_t diff = static_cast<int64_t>(current) - static_cast<int64_t>(netTime);
        return diff > -kOneDay && diff < kOneDay;
    }
}

void OffRadar::OnDMAFrame()
{
    uint32_t netTime = 0;
    const bool located = ReadNetTime(netTime);
    bLocated.store(located);
    if (located)
        networkTime.store(netTime);

    const bool want = bEnabled.load();

    // 关闭后补写一次 0（解除隐身），随后不再动作
    if (!want && !g_releasePending)
        return;

    if (!located)
    {
        if (want)
            blockedCount.fetch_add(1);
        return;
    }

    // 节流：1 秒一次
    const uint32_t now = static_cast<uint32_t>(std::time(nullptr));
    if (now == g_lastApplyTick)
        return;
    g_lastApplyTick = now;

    if (!want)
    {
        // 解除：写 0，再读回确认
        int32_t cur = 0;
        if (DMA::GetGlobalValue(kOffRadarGlobal, cur) && cur != 0)
        {
            DMA::SetGlobalValue(kOffRadarGlobal, 0);
        }
        g_releasePending = false;
        lastWritten.store(0);
        return;
    }

    // 第一道闸：当前值必须合法
    int32_t current = 0;
    if (!DMA::GetGlobalValue(kOffRadarGlobal, current))
    {
        blockedCount.fetch_add(1);
        return;
    }
    if (!PlausibleCurrent(current, netTime))
    {
        blockedCount.fetch_add(1);
        return;
    }

    // 写：网络时间（等价 Yim 的 *NetworkTime，每秒刷新）
    if (!DMA::SetGlobalValue(kOffRadarGlobal, static_cast<int32_t>(netTime)))
    {
        blockedCount.fetch_add(1);
        return;
    }

    // 第二道闸：读回校验（允许 ±2 秒：游戏内网络时间自己也在走）
    int32_t after = 0;
    if (!DMA::GetGlobalValue(kOffRadarGlobal, after))
    {
        blockedCount.fetch_add(1);
        return;
    }
    const int64_t drift = static_cast<int64_t>(after) - static_cast<int64_t>(netTime);
    if (drift < -2 || drift > 2)
    {
        blockedCount.fetch_add(1);
        return;
    }

    lastWritten.store(static_cast<uint32_t>(after));
    writeCount.fetch_add(1);
    g_releasePending = true;   // 之后关闭时要补写 0
}

std::string OffRadar::StatusText()
{
    if (!bLocated.load())
        return "网络时间未定位（特征码未命中）";

    char buf[160];
    int32_t live = 0;
    const bool ok = DMA::GetGlobalValue(kOffRadarGlobal, live);
    std::snprintf(buf, sizeof(buf), "网络时间 %u｜全局 %u = %s｜写入 %d 次｜被拒 %d 次",
                  networkTime.load(), kOffRadarGlobal,
                  ok ? std::to_string(live).c_str() : "读失败",
                  writeCount.load(), blockedCount.load());
    return buf;
}

int OffRadar::Probe()
{
    std::println("");
    std::println("=== 雷达隐身实测 ===");
    std::println("  网络时间指针：0x{:X} {}", Offsets::NetworkTimePtr,
                 Offsets::NetworkTimePtr ? "（已解析）" : "（未解析 ✗）");

    uint32_t netTime = 0;
    if (ReadNetTime(netTime))
        std::println("  网络时间实读：{}", netTime);
    else
        std::println("  网络时间实读：失败 ✗");

    int32_t live = 0;
    if (DMA::GetGlobalValue(kOffRadarGlobal, live))
        std::println("  隐身全局（索引 {}）当前值：{}", kOffRadarGlobal, live);
    else
        std::println("  隐身全局（索引 {}）读取失败 ✗", kOffRadarGlobal);

    // 只读体检：不写任何东西
    std::println("");
    std::println("  （只读体检，未写入任何内存）");
    return Offsets::NetworkTimePtr && netTime > 1000 ? 0 : 1;
}
