#include "pch.h"

#include "EconomyFeatures.h"

#include "DMA.h"
#include "Offsets.h"
#include "ScriptGlobals.h"
#include "ScriptGlobalsTable.h"

#include <cstdio>
#include <print>

namespace
{
	std::atomic<int>  g_claimed{ 0 };
	std::atomic<int>  g_silenced{ 0 };
	std::atomic<uint32_t> g_lastClaimTick{ 0 };
	std::atomic<uint32_t> g_lastSilenceTick{ 0 };
	std::atomic<uint32_t> g_lastGtaPlusTick{ 0 };

	char g_lastAction[192] = "（还没执行过动作）";

	// 引擎侧 HasGTAPlus（Offsets::GTAPlusPtr → int*），关闭时要还原
	std::atomic<bool>      g_gtaPlusResolved{ false };
	std::atomic<int32_t>   g_gtaPlusOriginal{ 0 };
	std::atomic<bool>      g_gtaPlusApplied{ false };

	constexpr uint32_t kSilenceThrottleMs = 1000;
	constexpr uint32_t kGtaPlusThrottleMs = 5000;

	void SetLastAction(const char* fmt, ...)
	{
		va_list args;
		va_start(args, fmt);
		std::vsnprintf(g_lastAction, sizeof(g_lastAction), fmt, args);
		va_end(args);
	}

	bool PhoneCallWantsSilencing()
	{
		const int stateSlot = ScriptGlobals::Find("PHONE_CALL_STATE");
		if (stateSlot < 0 || !ScriptGlobals::IsResolved(static_cast<uint32_t>(stateSlot)))
			return false;

		const int32_t state = ScriptGlobals::ReadLive(static_cast<uint32_t>(stateSlot));
		// 参考实现：状态既不是「空闲(0)」也不是「已静音(5/6)」时才动手
		if (state == 0 || state == 5 || state == 6)
			return false;

		const int inProgressSlot = ScriptGlobals::Find("PHONE_CALL_IN_PROGRESS");
		const int incomingSlot = ScriptGlobals::Find("PHONE_CALL_INCOMING");
		if (inProgressSlot < 0 || incomingSlot < 0)
			return false;

		const bool inProgress = ScriptGlobals::ReadLive(static_cast<uint32_t>(inProgressSlot)) != 0;
		const bool incoming = ScriptGlobals::ReadLive(static_cast<uint32_t>(incomingSlot)) != 0;
		return inProgress && incoming;
	}

	bool ResolveGtaPlusFlag()
	{
		if (g_gtaPlusResolved.load())
			return true;
		if (!DMA::IsReady() || Offsets::GTAPlusPtr == 0)
			return false;

		const uintptr_t address = DMA::BaseAddress + Offsets::GTAPlusPtr;
		int32_t value = 0;
		if (!DMA::Memory().Read(address, &value, sizeof(value)))
			return false;

		g_gtaPlusOriginal.store(value);
		g_gtaPlusResolved.store(true);
		std::println("[Economy] GTA+ 引擎标志已定位 @ 0x{:X}（当前值 {}）", address, value);
		return true;
	}
}

bool EconomyFeatures::Resolve()
{
	bool ok = ScriptGlobals::Resolve();
	if (!ok)
	{
		std::println("[Economy] 脚本全局未就绪（未进在线战局时属正常），本轮动作暂不可用");
		return false;
	}

	ResolveGtaPlusFlag();

	const int safeCount = static_cast<int>(ScriptGlobalTable::kSafeEntryCount);
	int ready = 0;
	for (int i = 0; i < safeCount; ++i)
	{
		if (ScriptGlobals::IsResolved(static_cast<uint32_t>(i)))
			++ready;
	}

	std::println("[Economy] 经济功能就绪：保险箱动作格 {}/{}，电话格 {}，GTA+ 全局 {}，GTA+ 引擎标志 {}",
	             ready, safeCount,
	             ScriptGlobals::IsResolved(static_cast<uint32_t>(ScriptGlobals::Find("PHONE_CALL_STATE"))) ? "是" : "否",
	             ScriptGlobals::IsResolved(static_cast<uint32_t>(ScriptGlobals::Find("GTA_PLUS_ENABLED"))) ? "是" : "否",
	             g_gtaPlusResolved.load() ? "是" : "否");
	return true;
}

void EconomyFeatures::Reset()
{
	ScriptGlobals::Reset();
	g_gtaPlusResolved.store(false);
	g_gtaPlusApplied.store(false);
	g_lastClaimTick.store(0);
	g_lastSilenceTick.store(0);
	g_lastGtaPlusTick.store(0);
}

const char* EconomyFeatures::GetBusinessName(uint32_t slot)
{
	if (slot >= static_cast<uint32_t>(ScriptGlobalTable::kSafeEntryCount))
		return "?";
	return ScriptGlobalTable::kEntries[ScriptGlobalTable::kFirstSafeEntry + slot].purpose;
}

uint32_t EconomyFeatures::GetBusinessGlobalIndex(uint32_t slot)
{
	if (slot >= static_cast<uint32_t>(ScriptGlobalTable::kSafeEntryCount))
		return 0;
	return ScriptGlobalTable::kEntries[ScriptGlobalTable::kFirstSafeEntry + slot].index;
}

bool EconomyFeatures::IsBusinessReady(uint32_t slot)
{
	if (slot >= static_cast<uint32_t>(ScriptGlobalTable::kSafeEntryCount))
		return false;
	return ScriptGlobals::IsResolved(ScriptGlobalTable::kFirstSafeEntry + slot);
}

bool EconomyFeatures::ClaimSafe(uint32_t businessSlot)
{
	if (businessSlot >= static_cast<uint32_t>(ScriptGlobalTable::kSafeEntryCount))
		return false;

	const uint32_t slot = ScriptGlobalTable::kFirstSafeEntry + businessSlot;
	if (!ScriptGlobals::IsResolved(slot))
		return false;

	// 脉冲写：动作格写 1，ScriptGlobals::OnFrame 稍后自动还原
	if (!ScriptGlobals::Trigger(slot, 1, "领取保险箱收益"))
		return false;

	g_claimed.fetch_add(1);
	SetLastAction("已触发「%s」保险箱领取", GetBusinessName(businessSlot));
	return true;
}

bool EconomyFeatures::ClaimAllSafes()
{
	int ok = 0;
	int failed = 0;
	for (uint32_t slot = 0; slot < static_cast<uint32_t>(ScriptGlobalTable::kSafeEntryCount); ++slot)
	{
		if (ClaimSafe(slot))
			++ok;
		else
			++failed;
	}

	SetLastAction("一键领取：成功触发 %d 个产业，%d 个不可用（未定位或写入被拒）", ok, failed);
	std::println("[Economy] {}", g_lastAction);
	return ok > 0;
}

bool EconomyFeatures::SilenceCurrentCall()
{
	if (!PhoneCallWantsSilencing())
	{
		SetLastAction("当前没有需要静音的来电（状态格为 0/5/6 或未定位）");
		return false;
	}

	const int slot = ScriptGlobals::Find("PHONE_CALL_STATE");
	if (slot < 0)
		return false;

	if (!ScriptGlobals::Trigger(static_cast<uint32_t>(slot), ScriptGlobalTable::kPhoneSilencedState, "静音来电"))
		return false;

	g_silenced.fetch_add(1);
	SetLastAction("已把来电状态写成 %d（静音）", ScriptGlobalTable::kPhoneSilencedState);
	std::println("[Economy] {}", g_lastAction);
	return true;
}

bool EconomyFeatures::SetGTAPlus(bool enable)
{
	bool any = false;

	const int enabledSlot = ScriptGlobals::Find("GTA_PLUS_ENABLED");
	const int bitsSlot = ScriptGlobals::Find("GTA_PLUS_BITS");
	if (enabledSlot >= 0 && ScriptGlobals::IsResolved(static_cast<uint32_t>(enabledSlot)))
	{
		any |= ScriptGlobals::Write(static_cast<uint32_t>(enabledSlot), enable ? 1 : ScriptGlobals::GetOriginal(static_cast<uint32_t>(enabledSlot)), "GTA+");
	}
	if (bitsSlot >= 0 && ScriptGlobals::IsResolved(static_cast<uint32_t>(bitsSlot)))
	{
		any |= ScriptGlobals::Write(static_cast<uint32_t>(bitsSlot),
		                            enable ? ScriptGlobalTable::kGtaPlusBitsValue : ScriptGlobals::GetOriginal(static_cast<uint32_t>(bitsSlot)),
		                            "GTA+ 权益位");
	}

	if (ResolveGtaPlusFlag())
	{
		const uintptr_t address = DMA::BaseAddress + Offsets::GTAPlusPtr;
		const int32_t value = enable ? 1 : g_gtaPlusOriginal.load();
		if (DMA::Memory().Write(address, &value, sizeof(value)))
		{
			g_gtaPlusApplied.store(enable);
			any = true;
		}
	}

	SetLastAction(enable ? "GTA+ 已解锁（退出或关闭开关时还原）" : "GTA+ 已还原");
	return any;
}

void EconomyFeatures::OnDMAFrame()
{
	// 触发态还原（保险箱等动作格）
	ScriptGlobals::OnFrame();

	const uint32_t now = GetTickCount();

	// 自动领取：周期重触发（游戏自己判断有没有钱可领）
	if (bAutoClaimSafeEarnings.load())
	{
		const uint32_t interval = static_cast<uint32_t>(claimIntervalSeconds.load()) * 1000u;
		const uint32_t last = g_lastClaimTick.load();
		if (last == 0 || (now - last) >= interval)
		{
			g_lastClaimTick.store(now);
			ClaimAllSafes();
		}
	}

	// 自动静音来电
	if (bAutoSilenceCalls.load())
	{
		const uint32_t last = g_lastSilenceTick.load();
		if (last == 0 || (now - last) >= kSilenceThrottleMs)
		{
			g_lastSilenceTick.store(now);
			if (PhoneCallWantsSilencing())
				SilenceCurrentCall();
		}
	}

	// GTA+ 解锁（持续维持，游戏刷新时会被覆盖所以定期复查）
	if (bUnlockGTAPlus.load())
	{
		const uint32_t last = g_lastGtaPlusTick.load();
		if (last == 0 || (now - last) >= kGtaPlusThrottleMs)
		{
			g_lastGtaPlusTick.store(now);
			SetGTAPlus(true);
		}
	}
}

bool EconomyFeatures::RestoreAll()
{
	if (g_gtaPlusApplied.load() && g_gtaPlusResolved.load())
	{
		const uintptr_t address = DMA::BaseAddress + Offsets::GTAPlusPtr;
		const int32_t value = g_gtaPlusOriginal.load();
		if (DMA::Memory().Write(address, &value, sizeof(value)))
		{
			g_gtaPlusApplied.store(false);
			std::println("[Economy] GTA+ 引擎标志已还原为 {}", value);
		}
	}
	return ScriptGlobals::RestoreAll();
}

bool EconomyFeatures::PrepareForClose()
{
	const bool ok = RestoreAll();
	Reset();
	return ok;
}

int EconomyFeatures::SelfTest()
{
	if (!Resolve())
	{
		std::println("[economy-selftest] 脚本全局未就绪（未进在线战局时属正常）");
		return 1;
	}

	int rc = 0;

	// 1) 保险箱动作格：脉冲写 1 → 读回 → 还原 0 → 读回
	const uint32_t safeSlot = ScriptGlobalTable::kFirstSafeEntry;
	if (!ScriptGlobals::IsResolved(safeSlot))
	{
		std::println("[economy-selftest] {} 未定位 → FAIL", ScriptGlobals::GetEntryName(safeSlot));
		rc = 1;
	}
	else
	{
		const int32_t original = ScriptGlobals::GetOriginal(safeSlot);
		std::println("[economy-selftest] {} 写前 = {}", ScriptGlobals::GetEntryName(safeSlot), original);

		const bool ok = ScriptGlobals::Trigger(safeSlot, 1, "自检");
		const int32_t back = ScriptGlobals::ReadLive(safeSlot);
		std::println("[economy-selftest] 脉冲写 1：写入{} → 读回 {} → {}", ok ? "成功" : "失败", back, back == 1 ? "PASS" : "FAIL");
		if (!ok || back != 1)
			rc = 1;

		const bool restored = ScriptGlobals::Write(safeSlot, original, "自检还原");
		const int32_t rest = ScriptGlobals::ReadLive(safeSlot);
		std::println("[economy-selftest] 还原：写入{} → 读回 {} → {}", restored ? "成功" : "失败", rest,
		             rest == original ? "PASS" : "FAIL");
		if (!restored || rest != original)
			rc = 1;
	}

	// 2) 电话状态格：写 6（静音） → 读回 → 还原
	const int phoneSlot = ScriptGlobals::Find("PHONE_CALL_STATE");
	if (phoneSlot < 0 || !ScriptGlobals::IsResolved(static_cast<uint32_t>(phoneSlot)))
	{
		std::println("[economy-selftest] PHONE_CALL_STATE 未定位 → FAIL");
		rc = 1;
	}
	else
	{
		const uint32_t slot = static_cast<uint32_t>(phoneSlot);
		const int32_t original = ScriptGlobals::GetOriginal(slot);
		std::println("[economy-selftest] PHONE_CALL_STATE 写前 = {}", original);

		const bool ok = ScriptGlobals::Write(slot, ScriptGlobalTable::kPhoneSilencedState, "自检");
		const int32_t back = ScriptGlobals::ReadLive(slot);
		std::println("[economy-selftest] 写 {}：写入{} → 读回 {} → {}", ScriptGlobalTable::kPhoneSilencedState,
		             ok ? "成功" : "失败", back, back == ScriptGlobalTable::kPhoneSilencedState ? "PASS" : "FAIL");
		if (!ok || back != ScriptGlobalTable::kPhoneSilencedState)
			rc = 1;

		const bool restored = ScriptGlobals::Write(slot, original, "自检还原");
		const int32_t rest = ScriptGlobals::ReadLive(slot);
		std::println("[economy-selftest] 还原：写入{} → 读回 {} → {}", restored ? "成功" : "失败", rest,
		             rest == original ? "PASS" : "FAIL");
		if (!restored || rest != original)
			rc = 1;
	}

	// 3) GTA+：全局 + 引擎标志，写完还原
	if (!SetGTAPlus(true))
	{
		std::println("[economy-selftest] GTA+ 解锁写入失败 → FAIL");
		rc = 1;
	}
	else
	{
		const int enabledSlot = ScriptGlobals::Find("GTA_PLUS_ENABLED");
		const int32_t enabled = enabledSlot >= 0 ? ScriptGlobals::ReadLive(static_cast<uint32_t>(enabledSlot)) : -1;
		std::println("[economy-selftest] GTA+ 全局写入 → GTA_PLUS_ENABLED 读回 {} → {}", enabled, enabled == 1 ? "PASS" : "FAIL");
		if (enabled != 1)
			rc = 1;

		// 还原的期望值 = 写入前记录的原值（这个会话里 GTA+ 可能本来就是 1，不能写死 0）
		const int32_t expectBack = enabledSlot >= 0 ? ScriptGlobals::GetOriginal(static_cast<uint32_t>(enabledSlot)) : -1;
		SetGTAPlus(false);
		const int32_t back = enabledSlot >= 0 ? ScriptGlobals::ReadLive(static_cast<uint32_t>(enabledSlot)) : -1;
		std::println("[economy-selftest] GTA+ 还原 → GTA_PLUS_ENABLED 读回 {}（期望原值 {}）→ {}", back, expectBack,
		             back == expectBack ? "PASS" : "FAIL");
		if (back != expectBack)
			rc = 1;
	}

	std::println("[economy-selftest] 结论：{}", rc == 0 ? "脚本全局写入路径实证通过（结束时已还原）" : "存在问题（见上）");
	return rc;
}

int EconomyFeatures::GetClaimedCount() { return g_claimed.load(); }
int EconomyFeatures::GetSilenceCount() { return g_silenced.load(); }
int EconomyFeatures::GetBlockedWriteCount() { return ScriptGlobals::GetBlockedWriteCount(); }
const char* EconomyFeatures::GetLastAction() { return g_lastAction; }

const char* EconomyFeatures::GetStatusLine()
{
	static char buffer[224];
	std::snprintf(buffer, sizeof(buffer), "领取 %d 次｜静音 %d 次｜拒写 %d 次｜全局定位 %s", GetClaimedCount(),
	              GetSilenceCount(), GetBlockedWriteCount(), ScriptGlobals::GetLocatedSummary());
	return buffer;
}

int EconomyFeatures::GetGtaPlusFlagAddress()
{
	if (!g_gtaPlusResolved.load())
		return 0;
	return static_cast<int>(DMA::BaseAddress + Offsets::GTAPlusPtr);
}
