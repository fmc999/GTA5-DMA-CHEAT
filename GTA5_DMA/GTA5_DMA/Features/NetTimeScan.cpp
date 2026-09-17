#include "pch.h"

#include "NetTimeScan.h"

#include "DMA.h"
#include "Offsets.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
#include <vector>

// ============================================================================
// 网络时间基准扫描（值锚定，版本无关）
//
// 用途：雷达隐身（OffRadar）要往「隐身到期时间」写游戏自己的网络时间。
//       游戏网络时间存在某个脚本全局里，每秒 +1。
//
// 做法：按「块」批量读（一个全局分块 = 0x40000 个槽 = 1 MB，一次 DMA 读完），
//       间隔 N 秒读两遍，找「第二次 - 第一次 == N」且量级像时间戳的槽位。
//       命中后写入 net_time.txt（下次启动直接用，不必再扫）。
//
// 注：YimMenuV2 的 networkTimePtrn（89 05 ?? ?? ?? ?? 80 3D ...）在当前线上版本
//     已失配（实测 .text 里找不到），所以这里改用值锚定 —— 游戏更新也不会失效。
// ============================================================================
namespace
{
    constexpr uint32_t kScanStart = 0x40000;      // 从 tunable 基址开始
    constexpr uint32_t kScanCount = 0xFC0000;     // 扫满全局空间（64 块 × 0x40000 槽）
    constexpr uint32_t kChunkElems = 0x40000;     // 一个全局分块的槽数
    constexpr int kIntervalSec = 3;

    std::string CachePath()
    {
        char buf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::string p(buf);
        const std::size_t slash = p.find_last_of("\\/");
        if (slash != std::string::npos)
            p.resize(slash + 1);
        return p + "net_time.txt";
    }

    bool WriteCache(uint32_t index, uint32_t value)
    {
        std::ofstream out(CachePath(), std::ios::trunc);
        if (!out)
            return false;
        out << "# 网络时间全局索引（由 --time-scan 自动写入；雷达隐身用）\n";
        out << "index=" << index << "\n";
        out << "value=" << value << "\n";
        return true;
    }
}

int NetTimeScan::Scan()
{
    std::println("");
    std::println("=== 网络时间基准扫描（值锚定，间隔 {} 秒）===", kIntervalSec);
    std::println("  范围：全局索引 {} ~ {}（{} 个槽，按块批量读）", kScanStart,
                 kScanStart + kScanCount - 1, kScanCount);

    const uint32_t chunks = (kScanCount + kChunkElems - 1) / kChunkElems;
    std::vector<std::vector<int32_t>> before(chunks);
    std::vector<std::vector<int32_t>> after(chunks);
    std::vector<bool> chunkOk(chunks, false);

    const auto readAll = [&](std::vector<std::vector<int32_t>>& dst, const char* tag) {
        int ok = 0;
        for (uint32_t c = 0; c < chunks; ++c)
        {
            const uint32_t baseIndex = kScanStart + c * kChunkElems;
            const uintptr_t addr = DMA::GetGlobalAddress(baseIndex);
            dst[c].assign(kChunkElems, 0);
            bool good = false;
            if (addr != 0 && DMA::Memory().Read(addr, dst[c].data(), kChunkElems * sizeof(int32_t)))
            {
                good = true;
                chunkOk[c] = true;
                ++ok;
            }
            std::println("    [{}] 块 {} 索引 {}..{} {}", tag, c, baseIndex,
                         baseIndex + kChunkElems - 1, good ? "✓" : "✗ 读失败");
            std::fflush(stdout);
        }
        return ok;
    };

    const int ok1 = readAll(before, "第一遍");
    if (ok1 == 0)
    {
        std::println("  ✗ 一个块都读不到（游戏没运行，或 GlobalPtr 未定位）");
        return 1;
    }
    std::this_thread::sleep_for(std::chrono::seconds(kIntervalSec));
    const int ok2 = readAll(after, "第二遍");
    if (ok2 == 0)
    {
        std::println("  ✗ 第二遍读取失败");
        return 1;
    }

    struct Hit { uint32_t index; int32_t b; int32_t a; };
    std::vector<Hit> hits;
    for (uint32_t c = 0; c < chunks; ++c)
    {
        if (!chunkOk[c] || before[c].size() != after[c].size())
            continue;
        for (uint32_t i = 0; i < before[c].size(); ++i)
        {
            const int64_t d = static_cast<int64_t>(after[c][i]) - static_cast<int64_t>(before[c][i]);
            // 容差：DMA 往返有抖动，允许 2~5 秒之间（标称 3 秒）
            if (d >= 2 && d <= 5 && after[c][i] > 100000)
                hits.push_back({ kScanStart + c * kChunkElems + i, before[c][i], after[c][i] });
        }
    }

    std::println("");
    std::println("  命中 {} 个「{} 秒左右 +{}」的槽位：", hits.size(), kIntervalSec, kIntervalSec);
    const std::size_t show = hits.size() < 25 ? hits.size() : 25;
    for (std::size_t i = 0; i < show; ++i)
        std::println("    索引 {:<9} {} → {}", hits[i].index, hits[i].b, hits[i].a);

    if (hits.empty())
    {
        std::println("  ✗ 没找到网络时间基准");
        return 1;
    }

    const uint32_t best = hits[0].index;
    std::println("");
    std::println("  → 选定网络时间全局索引 {}（当前值 {}）", best, hits[0].a);
    if (WriteCache(best, static_cast<uint32_t>(hits[0].a)))
        std::println("  → 已写入缓存：{}", CachePath());
    else
        std::println("  → 缓存写入失败（不影响本次使用）");
    return 0;
}
