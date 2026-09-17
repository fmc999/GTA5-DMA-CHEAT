#include "pch.h"

#include "TimeControl.h"

#include "DMA.h"
#include "Offsets.h"

#include <cstdio>
#include <ctime>

// ============================================================================
// 游戏内时钟（纯 DMA）
//   结构体基址 = BaseAddress + Offsets::TimeBasePtr（静态 0x47CDF70，需实测确认）
//   字段（沿用 Attic 旧实现）：day @ -0x0C、hour @ +0x00、minute @ +0x04、second @ +0x08
// ============================================================================
namespace
{
    uintptr_t ClockBase()
    {
        if (Offsets::TimeBasePtr == 0)
            return 0;
        return DMA::BaseAddress + Offsets::TimeBasePtr;
    }

    bool ReadByte(uintptr_t addr, uint8_t& out)
    {
        return DMA::Memory().Read(addr, &out, sizeof(out));
    }

    bool WriteByte(uintptr_t addr, uint8_t value)
    {
        return DMA::Memory().Write(addr, &value, sizeof(value));
    }

    bool InRange(int v, int lo, int hi)
    {
        return v >= lo && v <= hi;
    }

    // 读一组时钟字段；任一字段不合法则返回 false
    bool ReadClock(int& d, int& h, int& m, int& s)
    {
        const uintptr_t base = ClockBase();
        if (base == 0)
            return false;
        uint8_t bd = 0, bh = 0, bm = 0, bs = 0;
        if (!ReadByte(base - 0x0C, bd) || !ReadByte(base, bh) || !ReadByte(base + 0x04, bm) ||
            !ReadByte(base + 0x08, bs))
            return false;
        d = bd;
        h = bh;
        m = bm;
        s = bs;
        return true;
    }

    bool PlausibleClock(int d, int h, int m, int s)
    {
        return InRange(d, 0, 31) && InRange(h, 0, 23) && InRange(m, 0, 59) && InRange(s, 0, 59);
    }
}

void TimeControl::OnDMAFrame()
{
    if (!bEnable.load())
        return;

    int d = 0, h = 0, m = 0, s = 0;
    if (!ReadClock(d, h, m, s) || !PlausibleClock(d, h, m, s))
    {
        bLocated.store(false);
        blockedCount.fetch_add(1);
        return;
    }
    bLocated.store(true);
    liveHour.store(h);
    liveMinute.store(m);
    liveSecond.store(s);

    const uintptr_t base = ClockBase();
    const int wantH = hour.load();
    const int wantM = minute.load();
    const int wantS = second.load();

    // 第一道闸：UI 给的目标值必须合法
    if (!InRange(wantH, 0, 23) || !InRange(wantM, 0, 59) || !InRange(wantS, 0, 59))
    {
        blockedCount.fetch_add(1);
        return;
    }

    // 冻结：分/秒按当前实测值回写（等价「时间停住」）；否则写 UI 设定值
    const int outM = bFreeze.load() ? m : wantM;
    const int outS = bFreeze.load() ? s : wantS;

    WriteByte(base + 0x00, static_cast<uint8_t>(wantH));
    WriteByte(base + 0x04, static_cast<uint8_t>(outM));
    WriteByte(base + 0x08, static_cast<uint8_t>(outS));
    if (day.load() > 0)
        WriteByte(base - 0x0C, static_cast<uint8_t>(day.load()));

    // 第二道闸：读回校验（秒字段游戏自己也会走，允许 ±2）
    int d2 = 0, h2 = 0, m2 = 0, s2 = 0;
    if (!ReadClock(d2, h2, m2, s2))
    {
        blockedCount.fetch_add(1);
        return;
    }
    const int ds = s2 - outS;
    const bool hourOk = (h2 == wantH);
    const bool minOk = (m2 == outM);
    const bool secOk = (ds >= -2 && ds <= 2);
    if (!hourOk || !minOk || !secOk)
    {
        blockedCount.fetch_add(1);
        return;
    }
    lastWrittenHour.store(h2);
    writeCount.fetch_add(1);
}

std::string TimeControl::StatusText()
{
    if (ClockBase() == 0)
        return "时钟基址为 0（未配置）";
    int d = 0, h = 0, m = 0, s = 0;
    if (!ReadClock(d, h, m, s))
        return "时钟读取失败（地址不可读）";
    char buf[192];
    std::snprintf(buf, sizeof(buf), "游戏内 %02d:%02d:%02d（日 %d）｜写入 %d 次｜被拒 %d 次",
                  h, m, s, d, writeCount.load(), blockedCount.load());
    return buf;
}

int TimeControl::Probe()
{
    std::println("");
    std::println("=== 游戏时钟实测（只读）===");
    std::println("  BaseAddress = 0x{:X}", DMA::BaseAddress);
    std::println("  TimeBasePtr = 0x{:X}（静态值）", Offsets::TimeBasePtr);
    std::println("  时钟结构基址 = 0x{:X}", ClockBase());

    // 原始字节：把 -0x10 ~ +0x0F 一共 32 字节打出来，肉眼核对布局
    const uintptr_t base = ClockBase();
    if (base == 0)
    {
        std::println("  ✗ 基址为 0，无法读取");
        return 1;
    }

    uint8_t raw[32] = {};
    if (!DMA::Memory().Read(base - 0x10, raw, sizeof(raw)))
    {
        std::println("  ✗ 读取失败（地址不可读 —— 静态地址已失效）");
        return 1;
    }
    std::print("  原始 32 字节（相对 -0x10）：");
    for (int i = 0; i < 32; ++i)
        std::print(" {:02X}", raw[i]);
    std::println("");

    int d = 0, h = 0, m = 0, s = 0;
    if (!ReadClock(d, h, m, s))
    {
        std::println("  ✗ 字段读取失败");
        return 1;
    }
    std::println("  按旧布局解读：日 {} 时 {} 分 {} 秒 {}", d, h, m, s);
    const bool plausible = PlausibleClock(d, h, m, s);
    std::println("  区间体检（日 0~31 / 时 0~23 / 分 0~59 / 秒 0~59）：{}",
                 plausible ? "通过 ✓" : "不通过 ✗");

    // 采样两次看「秒」是否在走（时钟的关键特征）
    std::println("  采样中（间隔 3 秒）...");
    std::fflush(stdout);
    Sleep(3000);
    int d2 = 0, h2 = 0, m2 = 0, s2 = 0;
    ReadClock(d2, h2, m2, s2);
    int ds = s2 - s;
    if (ds < 0)
        ds += 60;   // 跨分
    std::println("  第二次：日 {} 时 {} 分 {} 秒 {}（秒变化 {}）", d2, h2, m2, s2, ds);
    const bool ticking = (ds >= 2 && ds <= 4);
    std::println("  秒在走（3 秒约 +3）：{}", ticking ? "是 ✓" : "否 ✗");

    std::println("");
    if (plausible && ticking)
    {
        std::println("  → 结论：0x47CDF70 仍是游戏时钟，可以直接用");
        return 0;
    }
    std::println("  → 结论：这个静态地址**不是**当前版本的时钟，需要重新定位");
    return 1;
}

int TimeControl::ClockScan()
{
    std::println("");
    std::println("=== 游戏时钟重新定位（扫模块可写数据段）===");

    if (DMA::BaseAddress == 0)
    {
        std::println("  ✗ 基址为 0");
        return 1;
    }

    const auto reader = [](std::uintptr_t address, void* buffer, std::size_t size) {
        DWORD bytesRead = 0;
        return VMMDLL_MemReadEx(DMA::vmh, DMA::PID, static_cast<ULONG64>(address),
                                static_cast<PBYTE>(buffer), static_cast<DWORD>(size), &bytesRead,
                                VMMDLL_FLAG_NOCACHE | VMMDLL_FLAG_ZEROPAD_ON_FAIL) != FALSE;
    };

    // ---- 读 PE 头，拿可写数据段 ----
    uint8_t dos[0x40] = {};
    if (!reader(DMA::BaseAddress, dos, sizeof(dos)) || dos[0] != 'M' || dos[1] != 'Z')
    {
        std::println("  ✗ DOS 头读取失败");
        return 1;
    }
    uint32_t e_lfanew = *reinterpret_cast<uint32_t*>(dos + 0x3C);
    uint8_t nt[4096] = {};   // 需要覆盖：文件头 24 + 可选头 240 + 段表 14*40 —— 之前开 0x108 刚好越界
    if (!reader(DMA::BaseAddress + e_lfanew, nt, sizeof(nt)))
    {
        std::println("  ✗ NT 头读取失败");
        return 1;
    }
    const uint16_t numSections = *reinterpret_cast<uint16_t*>(nt + 4 + 2);
    const uint16_t optSize = *reinterpret_cast<uint16_t*>(nt + 4 + 16);
    std::println("  PE: 段数 {}，可选头大小 {}", numSections, optSize);

    struct Range { std::string name; uintptr_t addr; uint32_t size; };
    std::vector<Range> writable;
    const uint8_t* sec = nt + 4 + 20 + optSize;
    for (uint16_t i = 0; i < numSections && i < 32; ++i, sec += 40)
    {
        char nm[9] = {};
        std::memcpy(nm, sec, 8);
        const uint32_t virtSize = *reinterpret_cast<const uint32_t*>(sec + 8);
        const uint32_t virtAddr = *reinterpret_cast<const uint32_t*>(sec + 12);
        const uint32_t chars = *reinterpret_cast<const uint32_t*>(sec + 36);
        const bool isWrite = (chars & 0x80000000u) != 0;
        const bool isExec = (chars & 0x20000000u) != 0;
        if (isWrite && !isExec && virtSize > 0)
        {
            writable.push_back({ nm, DMA::BaseAddress + virtAddr, virtSize });
            std::println("    可写段 {:<8} 偏移 0x{:X} 大小 {} 字节", nm, virtAddr, virtSize);
        }
    }
    if (writable.empty())
    {
        std::println("  ✗ 没有可写数据段");
        return 1;
    }

    // ---- 两遍全量读 → 本地差分，找「在走的时钟」 ----
    // 布局沿用 Attic 旧实现：day @ 基址-0x0C、hour @ 基址+0x00、minute @ +0x04、second @ +0x08
    // 判据：设候选指向「秒」字段 X ——
    //   after[X] 相对 before[X] 增加 4±1（间隔 4 秒），且 after[X]∈[0,60)、
    //   after[X-4]（分）∈[0,60)、after[X-8]（时）∈[0,24)
    // 同时按 整型 与 浮点 两种解读都试（旧实现写的是字节，实测该地址已失效；浮点是常见布局）。
    struct Buf { uintptr_t addr; uint32_t len; std::vector<uint8_t> before, after; };
    std::vector<Buf> bufs;
    constexpr uint32_t kChunk = 8u << 20;

    for (const Range& r : writable)
    {
        for (uint32_t off = 0; off < r.size; off += kChunk)
        {
            const uint32_t len = (r.size - off) < kChunk ? (r.size - off) : kChunk;
            Buf b;
            b.addr = r.addr + off;
            b.len = len;
            b.before.resize(len);
            b.after.assign(len, 0);
            if (!reader(b.addr, b.before.data(), len))
                continue;
            bufs.push_back(std::move(b));
        }
    }
    if (bufs.empty())
    {
        std::println("  ✗ 一段都没读到");
        return 1;
    }
    std::size_t totalBytes = 0;
    for (const Buf& b : bufs)
        totalBytes += b.len;
    std::println("  共读 {} 段 / {} 字节；采样中（间隔 4 秒）...", bufs.size(), totalBytes);
    std::fflush(stdout);
    Sleep(4000);

    for (Buf& b : bufs)
        reader(b.addr, b.after.data(), b.len);

    int winners = 0;
    const auto readI32 = [](const std::vector<uint8_t>& v, std::size_t off, int32_t& out) {
        if (off + 4 > v.size())
            return false;
        std::memcpy(&out, v.data() + off, 4);
        return true;
    };
    const auto readF32 = [](const std::vector<uint8_t>& v, std::size_t off, float& out) {
        if (off + 4 > v.size())
            return false;
        std::memcpy(&out, v.data() + off, 4);
        return true;
    };

    for (const Buf& b : bufs)
    {
        for (std::size_t i = 8; i + 4 <= b.before.size(); i += 4)
        {
            int32_t b0 = 0, b1 = 0, mi = 0, hi = 0;
            float bf0 = 0.0f, bf1 = 0.0f, mf = 0.0f, hf = 0.0f;
            if (!readI32(b.before, i, b0) || !readI32(b.after, i, b1))
                continue;
            readF32(b.before, i, bf0);
            readF32(b.after, i, bf1);
            readI32(b.after, i - 4, mi);
            readI32(b.after, i - 8, hi);
            readF32(b.after, i - 4, mf);
            readF32(b.after, i - 8, hf);

            // 整型解读
            const int64_t di = static_cast<int64_t>(b1) - static_cast<int64_t>(b0);
            int32_t miBefore = 0, hiBefore = 0;
            readI32(b.before, i - 4, miBefore);
            readI32(b.before, i - 8, hiBefore);
            // 决定性判据：秒在走（+4±1），但分/时在 4 秒内**不变**（允许跨分 / 跨时 +1）
            const bool neighStatic = (mi == miBefore || mi == miBefore + 1) &&
                                     (hi == hiBefore || hi == hiBefore + 1);
            const bool intTick = (di >= 3 && di <= 5) && b1 >= 0 && b1 < 60 && mi >= 0 && mi < 60 &&
                                 hi >= 0 && hi < 24 && neighStatic;
            // 浮点解读
            const double df = static_cast<double>(bf1) - static_cast<double>(bf0);
            float mfBefore = 0.0f, hfBefore = 0.0f;
            readF32(b.before, i - 4, mfBefore);
            readF32(b.before, i - 8, hfBefore);
            const bool fNeighStatic = (mf == mfBefore || mf >= mfBefore) && (hf == hfBefore || hf >= hfBefore);
            const bool floatTick = (df >= 3.0 && df <= 5.0) && bf1 >= 0.0f && bf1 < 60.0f &&
                                   mf >= 0.0f && mf < 60.0f && hf >= 0.0f && hf < 24.0f && fNeighStatic;
            if (!intTick && !floatTick)
                continue;

            ++winners;
            const uintptr_t baseAddr = b.addr + i - 8;
            std::println("  ★ 命中（{}）：时 {} 分 {} 秒 {} → 秒 {}",
                         intTick ? "整型" : "浮点", hi, mi, b1,
                         intTick ? b1 : static_cast<int>(bf1));
            std::println("     结构基址 0x{:X}，模块偏移 0x{:X}", baseAddr, baseAddr - DMA::BaseAddress);
            if (winners >= 15)
                break;
        }
        if (winners >= 15)
            break;
    }

    std::println("");
    std::println("  在走的时钟候选：{} 个", winners);
    return winners > 0 ? 0 : 1;
}

int TimeControl::SelfTest()
{
    std::println("");
    std::println("=== 时钟写入自检（记原值 → 写 → 读回 → 还原）===");

    int d = 0, h = 0, m = 0, s = 0;
    if (!ReadClock(d, h, m, s) || !PlausibleClock(d, h, m, s))
    {
        std::println("  ✗ 原值读取/体检不通过，放弃写入");
        return 1;
    }
    std::println("  原值：日 {} 时 {} 分 {} 秒 {}", d, h, m, s);

    const uintptr_t base = ClockBase();
    const int probeHour = (h + 6) % 24;      // 判别值：原小时 +6（循环）
    std::println("  写入判别值：时 = {}（原 {} + 6 取模 24）", probeHour, h);

    WriteByte(base + 0x00, static_cast<uint8_t>(probeHour));
    Sleep(200);

    int d2 = 0, h2 = 0, m2 = 0, s2 = 0;
    ReadClock(d2, h2, m2, s2);
    std::println("  读回：日 {} 时 {} 分 {} 秒 {}", d2, h2, m2, s2);
    const bool ok = (h2 == probeHour);
    std::println("  写入是否生效：{}", ok ? "是 ✓" : "否 ✗");

    // 还原
    WriteByte(base + 0x00, static_cast<uint8_t>(h));
    Sleep(200);
    int d3 = 0, h3 = 0, m3 = 0, s3 = 0;
    ReadClock(d3, h3, m3, s3);
    const bool restored = (h3 == h);
    std::println("  还原为 时 {} → 读回 时 {}：{}", h, h3, restored ? "✓" : "✗");

    std::println("");
    std::println("  结果：{}", (ok && restored) ? "写入 + 读回 + 还原 全部成功 ✓" : "失败 ✗");
    return (ok && restored) ? 0 : 1;
}
