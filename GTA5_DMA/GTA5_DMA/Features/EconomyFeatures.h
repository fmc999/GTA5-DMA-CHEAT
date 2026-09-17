#pragma once

// ============================================================================
//  EconomyFeatures —— 经济与自动化批次（第17轮）
//
//  全部走「脚本全局 + 少量引擎标志」这一条已经实机验证过的纯 DMA 通道，不调原生、不注入：
//    · 一键领取产业保险箱收益（7 个产业，逐个写各自的动作格；参考 ClaimSafeEarnings.cpp）
//    · 静音来电（读 状态/通话中/来电 三个格，条件成立就把状态写成 6；参考 SilencePhonecalls.cpp）
//    · GTA+ 解锁（写 GTA_PLUS_ENABLED/GTA_PLUS_BITS + 引擎侧 HasGTAPlus 标志；参考 UnlockGTAPlus.cpp）
//    · 自动领取（周期重触发，默认 30 秒）
//
//  所有写入都必须先过 ScriptGlobals 的「合法区间体检」与「写入前体检」，失败只会记数不会硬写。
// ============================================================================

#include <atomic>
#include <cstdint>

class EconomyFeatures
{
public:
	static inline std::atomic<bool> bAutoClaimSafeEarnings{ false };
	static inline std::atomic<bool> bAutoSilenceCalls{ false };
	static inline std::atomic<bool> bUnlockGTAPlus{ false };
	static inline std::atomic<int>  claimIntervalSeconds{ 30 };

	static constexpr int kSafeCount = 7;

	// 生命周期（与其它功能一致：DMA 初始化后 Resolve，退出前 PrepareForClose）
	static bool Resolve();
	static void Reset();
	static void OnDMAFrame();
	static bool RestoreAll();
	static bool PrepareForClose();

	// 一次性动作（UI 按钮 / 自动领取内部调用）
	static bool ClaimAllSafes();
	static bool ClaimSafe(uint32_t businessSlot);         // 0..6
	static bool SilenceCurrentCall();
	static bool SetGTAPlus(bool enable);

	// 只读查询（UI 自检）
	static const char* GetBusinessName(uint32_t slot);
	static uint32_t    GetBusinessGlobalIndex(uint32_t slot);
	static bool        IsBusinessReady(uint32_t slot);
	static int         GetClaimedCount();
	static int         GetSilenceCount();
	static int         GetBlockedWriteCount();
	static const char* GetLastAction();
	static const char* GetStatusLine();
	static int         SelfTest();                             // --economy-selftest：写 → 读回 → 还原 实证
	static int         GetGtaPlusFlagAddress();           // 引擎侧标志是否解析到（0 = 未解析）
};
