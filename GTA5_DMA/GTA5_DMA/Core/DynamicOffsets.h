#pragma once

// ============================================================================
// DynamicOffsets —— 结构体字段偏移 / 脚本索引的「外部文件驱动」层
//
// 为什么需要它：地址类的东西可以靠特征码动态扫（OffsetResolver 已经这么做），
// 但**结构体字段偏移**（ped+0xD10、载具+0x20、血量 0x280 …）和**脚本全局索引**
// 没法可靠地用特征码找 —— 不过它们同样会随游戏更新漂移。
//
// 所以这一层把它们变成「外部文本文件里的键值」：
//      GTA5_DMA_offsets.txt        （exe 同目录，首次运行自动生成并带注释）
//          PedVehiclePtr        = 0xD10
//          VehicleModelInfo     = 0x20
//          ModelInfoHash        = 0x18
//          VehicleHealth        = 0x280
//          VehicleEngineHealth  = 0x910
//          PhoneCallState       = 23040
//          ...
// 游戏更新后：改这个文本 → 重启工具 → 生效。**不需要重编译**。
// 文件不存在就用内置默认值（也就是当前实测通过的那套值）。
// ============================================================================

#include <cstdint>

namespace DynamicOffsets
{
    // 读 exe 同目录的 GTA5_DMA_offsets.txt（不存在则按内置默认值自动生成一份）
    bool Load();

    // 取一个偏移/索引（十六进制或十进制都能写，0x 前缀可选）；键不存在则返回 def
    uint64_t Get(const char* key, uint64_t def);

    // 便捷包装
    uintptr_t GetPtr(const char* key, uintptr_t def);
    int32_t   GetI32(const char* key, int32_t def);

    // 外部文件路径（诊断/体检里显示用）
    const char* FilePath();
    int         LoadedKeyCount();
    int         OverriddenKeyCount();   // 实际被外部文件改掉的条数
}
