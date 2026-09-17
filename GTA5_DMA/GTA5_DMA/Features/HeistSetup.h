#pragma once

// ============================================================================
// HeistSetup —— 抢劫分账 / 前置（纯 DMA：只写脚本全局，不调游戏函数）
//
// 参考 YimMenuV2 recovery/Heist/*（它用 ScriptGlobal + Stats + 原生调用）；
// 我们只取其中**纯脚本全局**那一层：分账人数与金额、科茨中心前置标志。
// 索引换算按 YimMenu 的 ScriptGlobal 语义：
//     At(off)      = idx + off
//     At(x, size)  = idx + 1 + x * size
// ============================================================================

#include <atomic>
#include <cstdint>
#include <string>

class HeistSetup
{
public:
    // 单个抢劫：4 人分账（0~100，单位 %），存 UI 侧
    struct Cuts
    {
        int  player1 = 0;
        int  player2 = 0;
        int  player3 = 0;
        int  player4 = 0;
    };

    static inline std::atomic<bool> bDryRun{ false };     // 只读不写（先看值对不对）

    // 四个抢劫的当前分账（从游戏读出）
    static Cuts ReadApartment();
    static Cuts ReadDoomsday();
    static Cuts ReadDiamond();

    // 写入分账（内部：地址体检 → 写 → 读回校验）
    static bool WriteApartmentCuts(const Cuts& cuts, std::string& report);
    static bool WriteDoomsdayCuts(const Cuts& cuts, std::string& report);
    static bool WriteDiamondCuts(const Cuts& cuts, std::string& report);

    // 科茨中心：前置标志（ScriptGlobal(1935711) |= 1）
    static bool ReadKortzFlag(bool& set);
    static bool WriteKortzFlag(bool set, std::string& report);

    // 诊断：把上面所有地址的当前值打印出来（--heist-probe）
    static int Probe();

    // 写入自检：写一组判别值 → 读回 → 还原原值（--heist-selftest）
    // 返回 0 = 三个抢劫的写入/读回/还原全部成功
    static int SelfTest();
};
