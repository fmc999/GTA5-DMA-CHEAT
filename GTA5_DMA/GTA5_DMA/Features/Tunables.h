#pragma once

// ============================================================================
//  Tunables —— 脚本 tunable 的 DMA 读写层（参考 YimMenuV2 game/backend/Tunables.*）
//
//  脚本 tunable 住在脚本全局数组里：起点 TUNABLE_BASE_ADDRESS = 0x40001，
//  每个 tunable 占一个 8 字节单元。YimMenu 用 hook 原生 + 运行注册脚本建立
//  「joaat 哈希 → 全局索引」映射并缓存到 tunables.bin；本项目是外部 DMA，改成：
//
//    ① 值锚定位（主）：分页读 tunable 块，按**值的比例特征**扫出锚点
//         · ConstrainedKick 组 = (a, 2a, 3a, 4a)   ← 比例 1:2:3:4，极难撞车
//         · IDLEKICK 组      = (a, 2.5a, 5a, 7.5a)  ← 比例 2.5/5/7.5
//       实机 2026-09-17：element 8497..8500 = 30000/60000/90000/120000，
//                        element 85..88    = 120000/300000/600000/900000 ✓
//    ② 相对定位：同一构建内各 tunable 之间的**相对偏移**稳定，其余条目由「锚 + 相对差值」定出
//    ③ 邻域兜底：候选单元的当前值不等于期望默认值时，在期望位置 ±kNeighborhood 个单元内
//       按「默认值精确匹配」重新搜索
//
//  安全设计（每条都要过体检才会被写入）：
//    · 解析时：单元当前值必须 == 期望默认值（或本会话已写过的值），否则拒绝该条；
//    · 写入时：先读当前值，若既不等于原始值也不等于上次写入值 → 拒绝写入并计数；
//    · 写完立即读回校验；退出前把动过的条目按记录还原；
//    · 换目标进程时整体重置（不会把上一个进程的原值当成本进程的）。
// ============================================================================

#include <atomic>
#include <cstdint>
#include <string>

#include "TunableTable.h"

class Tunables
{
public:
    static constexpr uint32_t kSlotCount = TunableTable::kEntryCount;
    static constexpr int32_t  kNeighborhood = 256;      // 邻域搜索半径（单元数）

    // 解析（可重复调用；失败时返回 false 并保留上一次成功结果）
    static bool Resolve();
    static void Reset();

    // 生命周期：退出/断连前把动过的条目还原
    static bool RestoreAll();
    static bool PrepareForClose();
    // ---- 只读查询（UI / 自检）----
    static int          Find(const char* name);          // 找不到返回 -1
    static bool         IsResolved(uint32_t i);
    static uintptr_t    GetAddress(uint32_t i);
    static uint32_t     GetGlobalIndex(uint32_t i);
    static int32_t      GetOriginal(uint32_t i);
    static int32_t      GetLastWritten(uint32_t i);
    static bool         HasWritten(uint32_t i);
    static int          GetResolvedCount();
    static int          GetBlockedWriteCount();
    static int          GetAnchorElement();              // 值锚定位到的 element（-1 = 未找到）
    static const char*  GetLocatedBy(uint32_t i);        // "锚"/"相对"/"邻域"/"未定位"
    static const char*  GetEntryName(uint32_t i);
    static const char*  GetEntryLabel(uint32_t i);   // 中文短名（UI 显示用）
    static int32_t      GetExpectedDefault(uint32_t i);
    static bool         IsValueKind(uint32_t i);          // Value 型：金额/额度，区间体检
    static int32_t      GetMinValue(uint32_t i);
    static int32_t      GetMaxValue(uint32_t i);
    static int32_t      ReadLive(uint32_t i, bool* ok = nullptr);   // 现场读（返回 int32 位型）

    // ---- 写入 ----
    static bool Write(uint32_t i, int32_t value, const char* why);
    static bool WriteFloat(uint32_t i, float value, const char* why);

    // 写入自检（--tunable-selftest）：一条 float + 一条 int 做「写 → 读回 → 还原 → 读回」
    // 返回 0 = 全通过；用的是本体的 DMA 写入路径，不依赖界面
    static int SelfTest();

private:
    static bool  BlockAddressRange(uintptr_t& outBegin, uint32_t& outElements);
};

