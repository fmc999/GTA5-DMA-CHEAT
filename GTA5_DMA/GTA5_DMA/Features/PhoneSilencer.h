#pragma once

// ============================================================================
// PhoneSilencer —— 静音来电（纯脚本全局，参考 YimMenuV2 features/network/SilencePhonecalls.cpp）
//
// Yim 原实现（逐行核对过：零原生、零包装，只有 4 个脚本全局）：
//     is_incoming_call         = ScriptGlobal(23050)
//     is_phone_call_in_progress= ScriptGlobal(23046)
//     phone_call_state         = ScriptGlobal(23040)
//     calling_character        = ScriptGlobal(8818)
//   条件：会话已开始 且 状态 ∉ {0,5,6} 且 通话中 且 有来电  → 把状态写成 6（静音）
//
// 我们的实现：用 DMA::GetGlobalAddress(index) 拿地址（内部处理 chunk/element 换算），
// 读三个判断位 + 写状态位，写后立即读回校验；地址解析不出来（不在线上战局）就跳过。
// ============================================================================

#include <cstdint>

namespace PhoneSilencer
{
    // 开关
    bool Enabled();
    void SetEnabled(bool on);

    // 每帧调用（DMA 线程），内部节流
    void OnDMAFrame();

    // 界面展示用
    int         SilencedCount();      // 已静音来电数
    int32_t     LastCaller();         // 最近一次被静音的角色 ID
    const char* LastResult();         // 最近一次动作说明

    // CLI 自检用：读 4 个全局的当前值
    struct Snapshot
    {
        uintptr_t addrState = 0;
        uintptr_t addrProgress = 0;
        uintptr_t addrIncoming = 0;
        uintptr_t addrCaller = 0;
        int32_t   state = 0;
        int32_t   progress = 0;
        int32_t   incoming = 0;
        int32_t   caller = 0;
        bool      ok = false;
    };
    Snapshot Read();
}
