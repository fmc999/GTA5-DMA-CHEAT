// ============================================================================
// PhoneSilencer.cpp —— 静音来电（纯脚本全局写，不碰 .text，无原生调用）
//
// 与 YimMenuV2 的 SilencePhonecalls 逻辑一一对应；地址用 DMA::GetGlobalAddress() 换算。
// 只在「地址可解析」时动作 → 解析不到就说明不在线上战局，自动跳过（相当于 Yim 的
// IsSessionStarted 检查，但更硬：地址为 0 就一定不会乱写）。
// ============================================================================

#include "pch.h"

#include "PhoneSilencer.h"

#include "DMA.h"
#include "DynamicOffsets.h"

#include <atomic>
#include <chrono>
#include <cstdio>

namespace PhoneSilencer
{
    namespace
    {
        // Yim 用的四个全局索引
        // 第33轮：改成外部文件驱动（GTA5_DMA_offsets.txt），游戏更新后改文本即可，不用重编译
        inline uint32_t IdxCallState() { return static_cast<uint32_t>(DynamicOffsets::Get("PhoneCallState", 23040)); }
        inline uint32_t IdxInProgress() { return static_cast<uint32_t>(DynamicOffsets::Get("PhoneCallInProgress", 23046)); }
        inline uint32_t IdxIncoming() { return static_cast<uint32_t>(DynamicOffsets::Get("PhoneCallIncoming", 23050)); }
        inline uint32_t IdxCaller() { return static_cast<uint32_t>(DynamicOffsets::Get("PhoneCaller", 8818)); }

        constexpr int32_t kSilenced = 6;            // 与 Yim 一致
        constexpr int kThrottleMs = 250;            // 检查节流

        std::atomic<bool> g_enabled{ false };
        std::atomic<int>  g_silencedCount{ 0 };
        std::atomic<int32_t> g_lastCaller{ 0 };
        std::atomic<uint32_t> g_lastCheck{ 0 };
        std::atomic<uint32_t> g_lastWriteOk{ 0 };
        char g_lastResult[160] = "尚未动作";

        void SetResult(const char* text)
        {
            std::snprintf(g_lastResult, sizeof(g_lastResult), "%s", text);
        }

        uint32_t NowMs()
        {
            using namespace std::chrono;
            return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
        }

        bool ReadI32(uintptr_t addr, int32_t* out)
        {
            return DMA::Memory().Read(addr, out, sizeof(*out));
        }
    }  // namespace

    bool Enabled() { return g_enabled.load(); }
    void SetEnabled(bool on)
    {
        g_enabled.store(on);
        if (on)
            SetResult("已开启，等待来电");
        else
            SetResult("已关闭");
    }

    Snapshot Read()
    {
        Snapshot s{};
        s.addrState = DMA::GetGlobalAddress(IdxCallState());
        s.addrProgress = DMA::GetGlobalAddress(IdxInProgress());
        s.addrIncoming = DMA::GetGlobalAddress(IdxIncoming());
        s.addrCaller = DMA::GetGlobalAddress(IdxCaller());
        if (!s.addrState || !s.addrProgress || !s.addrIncoming || !s.addrCaller)
            return s;

        ReadI32(s.addrState, &s.state);
        ReadI32(s.addrProgress, &s.progress);
        ReadI32(s.addrIncoming, &s.incoming);
        ReadI32(s.addrCaller, &s.caller);
        s.ok = true;
        return s;
    }

    void OnDMAFrame()
    {
        if (!g_enabled.load() || !DMA::IsReady())
            return;

        const uint32_t now = NowMs();
        const uint32_t last = g_lastCheck.load();
        if (last != 0 && (now - last) < static_cast<uint32_t>(kThrottleMs))
            return;
        g_lastCheck.store(now);

        const Snapshot s = Read();
        if (!s.ok)
        {
            SetResult("全局地址解析不到（多半不在线上战局），跳过");
            return;
        }

        // Yim 的判据：状态 ∉ {0,5,6} 且 通话中 且 有来电
        const bool interesting = (s.state != 0 && s.state != kSilenced && s.state != 5) &&
                                 s.progress != 0 && s.incoming != 0;
        if (!interesting)
            return;

        const int32_t desired = kSilenced;
        if (!DMA::Memory().Write(s.addrState, &desired, sizeof(desired)))
        {
            SetResult("写入失败（内存写接口报错）");
            return;
        }

        int32_t readback = 0;
        if (!ReadI32(s.addrState, &readback) || readback != desired)
        {
            SetResult("写入未通过读回校验");
            return;
        }

        g_silencedCount.fetch_add(1);
        g_lastCaller.store(s.caller);
        g_lastWriteOk.store(now);

        char msg[160];
        std::snprintf(msg, sizeof(msg), "已静音来电（角色 %d，状态 %d → 6）", s.caller, s.state);
        SetResult(msg);

        std::println("[PhoneSilencer] 已静音来电：角色 {}，状态 {} → 6（读回校验通过）", s.caller, s.state);
    }

    int SilencedCount() { return g_silencedCount.load(); }
    int32_t LastCaller() { return g_lastCaller.load(); }
    const char* LastResult() { return g_lastResult; }
}
