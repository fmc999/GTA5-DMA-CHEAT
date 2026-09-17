#include "pch.h"

#include "HeistSetup.h"

#include "DMA.h"

#include <cstdio>
#include <iostream>
#include <mutex>

// ============================================================================
// 抢劫分账（纯 DMA：只写脚本全局）
//
// 索引全部来自参考项目 YimMenuV2 recovery/Heist/*.cpp 的 ScriptGlobal 写法，
// 按 YimMenu 的语义换算成全局索引：
//     At(off)     = idx + off
//     At(x, size) = idx + 1 + x * size
//
// 写入安全（沿用本仓库两道闸的思路）：
//   ① 解析/体检：当前值必须落在合法区间（分账 -1 ~ 100），否则判「未定位」拒写；
//   ② 写入后立刻读回校验，值不符则报失败。
// ============================================================================

namespace
{
    std::mutex g_HeistMutex;

    // —— 全局索引用法（全部由上面公式算出）——
    // 公寓抢劫：base1 = 1936406 + 1 → At(i,1) = base1 + 1 + i
    constexpr uint32_t kApartmentBase1 = 1936406u + 1u + 1u;        // → 1936408
    constexpr uint32_t kApartmentBase2 = 1938374u + 3008u + 1u;     // → 1941383
    // 末日抢劫：1969071 + 812 + 50 → At(i,1)
    constexpr uint32_t kDoomsdayBase = 1969071u + 812u + 50u + 1u;  // → 1969934
    // 钻石赌场：1973762 + 1497 + 736 + 92 → At(i,1)
    constexpr uint32_t kDiamondBase = 1973762u + 1497u + 736u + 92u + 1u;   // → 1976088
    // 科茨中心：标志位
    constexpr uint32_t kKortzFlag = 1935711u;

    bool IsPlausibleCut(int32_t v)
    {
        return v >= -1 && v <= 100;
    }

    int32_t ReadGlobalInt(uint32_t index)
    {
        int32_t v = 0;
        if (!DMA::GetGlobalValue(index, v))
            return INT32_MIN;
        return v;
    }

    bool WriteGlobalInt(uint32_t index, int32_t value, std::string& report)
    {
        const int32_t before = ReadGlobalInt(index);
        if (before == INT32_MIN)
        {
            report += "全局索引 " + std::to_string(index) + " 读取失败（未定位）\n";
            return false;
        }
        // 第一道闸：当前值必须是合法分账值，否则不写（防写错地方）
        if (!IsPlausibleCut(before))
        {
            report += "全局索引 " + std::to_string(index) + " 当前值 " + std::to_string(before) +
                      " 不在分账合法区间（-1~100）→ 拒绝写入\n";
            return false;
        }
        if (!DMA::SetGlobalValue(index, value))
        {
            report += "全局索引 " + std::to_string(index) + " 写入失败\n";
            return false;
        }
        // 第二道闸：读回校验
        const int32_t after = ReadGlobalInt(index);
        if (after != value)
        {
            report += "全局索引 " + std::to_string(index) + " 读回校验失败（写入 " +
                      std::to_string(value) + "，读回 " + std::to_string(after) + "）\n";
            return false;
        }
        report += "索引 " + std::to_string(index) + "：" + std::to_string(before) + " → " +
                  std::to_string(after) + " ✓\n";
        return true;
    }
}

bool HeistSetup::ReadKortzFlag(bool& set)
{
    int32_t v = 0;
    if (!DMA::GetGlobalValue(kKortzFlag, v))
        return false;
    set = (v & 1) != 0;
    return true;
}

bool HeistSetup::WriteKortzFlag(bool set, std::string& report)
{
    std::lock_guard<std::mutex> lock(g_HeistMutex);
    int32_t before = 0;
    if (!DMA::GetGlobalValue(kKortzFlag, before))
    {
        report = "科茨中心标志位读取失败（未定位）\n";
        return false;
    }
    const int32_t target = set ? (before | 1) : (before & ~1);
    if (!DMA::SetGlobalValue(kKortzFlag, target))
    {
        report = "科茨中心标志位写入失败\n";
        return false;
    }
    int32_t after = 0;
    DMA::GetGlobalValue(kKortzFlag, after);
    char buf[160];
    std::snprintf(buf, sizeof(buf), "科茨中心标志（索引 %u）：0x%X → 0x%X（读回 0x%X）%s\n",
                  kKortzFlag, before, target, after, (after == target) ? "✓" : "✗ 读回不符");
    report = buf;
    return after == target;
}

// ---------------------------------------------------------------- 分账读写
#define HEIST_CUTS_IMPL(NAME, BASE, EXTRA)                                              \
    HeistSetup::Cuts HeistSetup::Read##NAME()                                           \
    {                                                                                   \
        Cuts c{};                                                                       \
        c.player1 = ReadGlobalInt(BASE + 0);                                            \
        c.player2 = ReadGlobalInt(BASE + 1);                                            \
        c.player3 = ReadGlobalInt(BASE + 2);                                            \
        c.player4 = ReadGlobalInt(BASE + 3);                                            \
        return c;                                                                       \
    }                                                                                   \
    bool HeistSetup::Write##NAME##Cuts(const Cuts& cuts, std::string& report)           \
    {                                                                                   \
        std::lock_guard<std::mutex> lock(g_HeistMutex);                                 \
        report.clear();                                                                 \
        const int v[4] = { cuts.player1, cuts.player2, cuts.player3, cuts.player4 };    \
        bool ok = true;                                                                 \
        for (int i = 0; i < 4; ++i)                                                     \
            ok = WriteGlobalInt(BASE + i, v[i], report) && ok;                          \
        return ok;                                                                      \
    }

HEIST_CUTS_IMPL(Apartment, kApartmentBase1, kApartmentBase2)
HEIST_CUTS_IMPL(Doomsday, kDoomsdayBase, 0)
HEIST_CUTS_IMPL(Diamond, kDiamondBase, 0)

// ---------------------------------------------------------------- 写入自检
int HeistSetup::SelfTest()
{
    std::println("");
    std::println("=== 抢劫分账写入自检（写 → 读回 → 还原）===");

    struct Case { const char* name; uint32_t base; };
    const Case cases[] = {
        { "公寓抢劫", kApartmentBase1 },
        { "末日抢劫", kDoomsdayBase },
        { "钻石赌场", kDiamondBase },
    };

    int pass = 0;
    for (const Case& c : cases)
    {
        std::println("");
        std::println("  【{}】基址索引 {}", c.name, c.base);

        // 记原值
        int32_t orig[4] = {};
        bool readable = true;
        for (int i = 0; i < 4; ++i)
        {
            orig[i] = ReadGlobalInt(c.base + i);
            if (orig[i] == INT32_MIN)
                readable = false;
        }
        if (!readable)
        {
            std::println("    ✗ 读取原值失败（未定位）");
            continue;
        }
        std::println("    原值：{} / {} / {} / {}", orig[0], orig[1], orig[2], orig[3]);

        // 写判别值
        const int32_t probe[4] = { 12, 34, 56, 78 };
        std::string sink;
        for (int i = 0; i < 4; ++i)
            WriteGlobalInt(c.base + i, probe[i], sink);

        // 读回
        int32_t back[4] = {};
        bool match = true;
        for (int i = 0; i < 4; ++i)
        {
            back[i] = ReadGlobalInt(c.base + i);
            if (back[i] != probe[i])
                match = false;
        }
        std::println("    写入 {} / {} / {} / {} → 读回 {} / {} / {} / {}  {}",
                     probe[0], probe[1], probe[2], probe[3], back[0], back[1], back[2], back[3],
                     match ? "✓ 一致" : "✗ 不一致");

        // 还原
        bool restored = true;
        for (int i = 0; i < 4; ++i)
        {
            DMA::SetGlobalValue(c.base + i, orig[i]);
            if (ReadGlobalInt(c.base + i) != orig[i])
                restored = false;
        }
        std::println("    还原为 {} / {} / {} / {}  {}", orig[0], orig[1], orig[2], orig[3],
                     restored ? "✓" : "✗ 还原失败");

        if (match && restored)
            ++pass;
    }

    std::println("");
    std::println("  结果：{}/{} 家抢劫「写入 + 读回 + 还原」全部成功", pass, 3);
    return pass == 3 ? 0 : 1;
}

// ---------------------------------------------------------------- 诊断
int HeistSetup::Probe()
{
    std::println("");
    std::println("=== 抢劫相关全局索引实测 ===");

    struct Row { const char* name; uint32_t index; };
    const Row rows[] = {
        { "公寓分账 表1", kApartmentBase1 + 0 },
        { "公寓分账 表1 +1", kApartmentBase1 + 1 },
        { "公寓分账 表1 +2", kApartmentBase1 + 2 },
        { "公寓分账 表1 +3", kApartmentBase1 + 3 },
        { "公寓分账 表2", kApartmentBase2 + 0 },
        { "公寓分账 表2 +1", kApartmentBase2 + 1 },
        { "公寓分账 表2 +2", kApartmentBase2 + 2 },
        { "公寓分账 表2 +3", kApartmentBase2 + 3 },
        { "末日分账 0..3", kDoomsdayBase + 0 },
        { "末日分账 +1", kDoomsdayBase + 1 },
        { "末日分账 +2", kDoomsdayBase + 2 },
        { "末日分账 +3", kDoomsdayBase + 3 },
        { "钻石分账 0..3", kDiamondBase + 0 },
        { "钻石分账 +1", kDiamondBase + 1 },
        { "钻石分账 +2", kDiamondBase + 2 },
        { "钻石分账 +3", kDiamondBase + 3 },
        { "科茨中心标志位", kKortzFlag },
    };

    int ok = 0;
    int plausible = 0;
    for (const Row& r : rows)
    {
        int32_t v = ReadGlobalInt(r.index);
        if (v == INT32_MIN)
        {
            std::println("  {:<18} 索引 {:<9} 读取失败", r.name, r.index);
            continue;
        }
        ++ok;
        const bool pl = IsPlausibleCut(v);
        if (pl)
            ++plausible;
        std::println("  {:<18} 索引 {:<9} = {:<12}{}", r.name, r.index, v, pl ? "（像分账值 ✓）" : "（不像分账值）");
    }
    std::println("");
    std::println("  可读 {}/{}，其中 {}/{} 落在分账合法区间（-1~100）", ok, sizeof(rows) / sizeof(rows[0]), plausible, ok);
    return ok > 0 ? 0 : 1;
}
