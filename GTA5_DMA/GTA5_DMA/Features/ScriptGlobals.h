#pragma once

// ============================================================================
//  ScriptGlobals —— 脚本全局「动作格」的安全写层
//
//  寻址：与 tunable 同一个脚本全局数组（chunk = idx>>18 & 0x3F，elem = idx & 0x3FFFF，
//  分块指针取自 Offsets::GlobalPtr 指向的 64 项表）—— 直接复用 DMA::GetGlobalAddress。
//
//  安全模型（比 tunable 更保守，因为这类格子多是「动作触发」）：
//    · 解析：读到值必须落在登记表给的合法区间内，否则判「未定位」，本会话不再动它
//    · 写入：写前读一次，写后立刻读回校验；不匹配即记为「被拒写入」
//    · 脉冲：Trigger() 写动作值，OnFrame 里节流后自动还原成原始值（避免动作格长期处于触发态）
//    · 退出：RestoreAll() 把动过的格子还原成记录的原值；换进程整体重置
// ============================================================================

#include <atomic>
#include <cstdint>

class ScriptGlobals
{
public:
	static constexpr uint32_t kSlotCount = 12;          // 与 ScriptGlobalTable::kEntryCount 一致（编译期断言在 .cpp）
	static constexpr uint32_t kTriggerHoldMs = 1500;    // 脉冲动作保持时长

	// 生命周期
	static bool Resolve();                              // 可重复调用；已全部解析则直接返回
	static void Reset();
	static bool RestoreAll();
	static bool PrepareForClose();

	// 只读查询（UI / 自检）
	static int          Find(const char* name);         // 找不到返回 -1
	static bool         IsResolved(uint32_t i);
	static uintptr_t    GetAddress(uint32_t i);
	static int32_t      ReadLive(uint32_t i, bool* ok = nullptr);
	static int32_t      GetOriginal(uint32_t i);
	static int          GetResolvedCount();
	static int          GetBlockedWriteCount();
	static int          GetWriteCount();
	static const char*  GetEntryName(uint32_t i);
	static const char*  GetEntryPurpose(uint32_t i);   // 中文用途（UI 显示用；用户看不懂英文原名）
	static uint32_t     GetGlobalIndex(uint32_t i);
	static const char*  GetLocatedSummary();            // 解析日志用的一行摘要

	// 写入
	static bool Write(uint32_t i, int32_t value, const char* why);          // 持续写（由功能每帧维护）
	static bool Trigger(uint32_t i, int32_t value, const char* why);        // 脉冲写（动作格，稍后自动还原）
	static void OnFrame();                                                  // 处理脉冲还原（节流）

private:
	static bool  ResolveInternal(bool force);
};
