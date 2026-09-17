#include "pch.h"

#include "ScriptGlobals.h"

#include "DMA.h"
#include "ScriptGlobalsTable.h"

#include <print>

namespace
{
	struct SlotState
	{
		std::atomic<uintptr_t> address{ 0 };
		std::atomic<bool>      resolved{ false };
		std::atomic<bool>      haveOriginal{ false };
		std::atomic<bool>      triggered{ false };

		int32_t  original = 0;
		int32_t  lastWritten = 0;
		uint32_t triggeredAt = 0;
	};

	SlotState g_slots[ScriptGlobals::kSlotCount];
	std::atomic<int> g_resolvedCount{ 0 };
	std::atomic<int> g_blockedWrites{ 0 };
	std::atomic<int> g_refusalStreak{ 0 };         // 连续拒绝计数 → 触发自动重新解析
	std::atomic<int> g_rebaselineCount{ 0 };       // 自动重新基线次数（界面常显）
	std::atomic<uint32_t> g_lastReresolveTick{ 0 };
	std::atomic<uint32_t> g_lastRefusalLogTick{ 0 };
	std::atomic<int> g_writeCount{ 0 };
	std::atomic<uint32_t> g_processId{ 0 };
	std::atomic<uintptr_t> g_processBase{ 0 };
	std::atomic<bool> g_haveIdentity{ false };
	std::atomic<uint32_t> g_lastResolveAttempt{ 0 };
	std::atomic<bool> g_loggedUnavailable{ false };

	constexpr uint32_t kResolveIntervalMs = 5000;

	bool SameProcess()
	{
		return g_haveIdentity.load() && g_processId.load() == DMA::PID && g_processBase.load() == DMA::BaseAddress;
	}

	void ClearAll()
	{
		for (uint32_t i = 0; i < ScriptGlobals::kSlotCount; ++i)
		{
			g_slots[i].address.store(0);
			g_slots[i].resolved.store(false);
			g_slots[i].haveOriginal.store(false);
			g_slots[i].triggered.store(false);
			g_slots[i].original = 0;
			g_slots[i].lastWritten = 0;
			g_slots[i].triggeredAt = 0;
		}
		g_resolvedCount.store(0);
		g_loggedUnavailable.store(false);
	}

	// 值是否落在登记表给的合法区间内
	bool ValueInRange(const ScriptGlobalTable::Entry& entry, int32_t value)
	{
		return value >= entry.minValue && value <= entry.maxValue;
	}
}

int ScriptGlobals::Find(const char* name)
{
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		if (std::strcmp(ScriptGlobalTable::kEntries[i].name, name) == 0)
			return static_cast<int>(i);
	}
	return -1;
}

bool ScriptGlobals::ReResolve()
{
	g_refusalStreak.store(0);
	return ResolveInternal(true);
}

bool ScriptGlobals::Resolve()
{
	return ResolveInternal(false);
}

bool ScriptGlobals::ResolveInternal(bool force)
{
	if (!DMA::IsReady())
		return false;

	if (!SameProcess())
	{
		ClearAll();
		g_processId.store(DMA::PID);
		g_processBase.store(DMA::BaseAddress);
		g_haveIdentity.store(true);
		g_lastResolveAttempt.store(0);
	}

	if (!force && g_resolvedCount.load() == static_cast<int>(kSlotCount))
		return true;

	const uint32_t now = GetTickCount();
	const uint32_t last = g_lastResolveAttempt.load();
	if (!force && last != 0 && (now - last) < kResolveIntervalMs)
		return false;
	g_lastResolveAttempt.store(now);

	int resolved = 0;
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		const ScriptGlobalTable::Entry& entry = ScriptGlobalTable::kEntries[i];

		const uintptr_t address = DMA::GetGlobalAddress(entry.index);
		if (!address)
		{
			if (!g_loggedUnavailable.exchange(true))
			{
				std::println("[ScriptGlobals] 全局块不可用：索引 0x{:X}（chunk {} 的分块指针 = 0，"
				             "通常表示还没进在线战局）", entry.index, (entry.index >> 18) & 0x3F);
			}
			continue;
		}

		int32_t value = 0;
		if (!DMA::Memory().Read(address, &value, sizeof(value)))
			continue;

		if (!ValueInRange(entry, value))
		{
			std::println("[ScriptGlobals] {} 体检未通过：索引 0x{:X} 读到 {}（合法区间 [{}, {}]），本会话跳过该条",
			             entry.name, entry.index, value, entry.minValue, entry.maxValue);
			continue;
		}

		if (!g_slots[i].haveOriginal.load() || g_slots[i].address.load() != address)
		{
			g_slots[i].original = value;
			g_slots[i].haveOriginal.store(true);
		}
		g_slots[i].address.store(address);
		g_slots[i].resolved.store(true);
		++resolved;

		std::println("[ScriptGlobals] {} @ 0x{:X}（全局索引 0x{:X}）= {}  [{}]",
		             entry.name, address, entry.index, value, entry.purpose);
	}

	g_resolvedCount.store(resolved);
	std::println("[ScriptGlobals] 解析完成：{}/{} 条通过体检", resolved, kSlotCount);
	return resolved > 0;
}

void ScriptGlobals::Reset()
{
	ClearAll();
	g_haveIdentity.store(false);
	g_processId.store(0);
	g_processBase.store(0);
	g_blockedWrites.store(0);
	g_writeCount.store(0);
}

bool ScriptGlobals::Write(uint32_t i, int32_t value, const char* why)
{
	if (i >= kSlotCount || !g_slots[i].resolved.load())
		return false;

	const uintptr_t address = g_slots[i].address.load();
	if (!address)
		return false;

	int32_t current = 0;
	if (!DMA::Memory().Read(address, &current, sizeof(current)))
		return false;
	if (current == value)
		return true;

	// 写入前体检：当前值必须是「我们记录的原值」或「我们上次写的值」，
	// 否则说明这个格子被别的工具/自己以外的逻辑动过 —— 拒绝写入。
	const int32_t original = g_slots[i].original;
	if (current != original && current != g_slots[i].lastWritten)
	{
		g_blockedWrites.fetch_add(1);
		const int streak = g_refusalStreak.fetch_add(1) + 1;
		const uint32_t nowTick = GetTickCount();
		// 拒绝日志限流：全局 30 秒最多一条（否则每帧刷屏）
		const uint32_t lastLog = g_lastRefusalLogTick.load();
		if (lastLog == 0 || nowTick - lastLog > 30000u)
		{
			g_lastRefusalLogTick.store(nowTick);
			std::println("[ScriptGlobals] 拒绝写入 {}：当前值 {} 既非原值 {} 也非上次写入值 {}（连续第 {} 次）",
			             ScriptGlobalTable::kEntries[i].name, current, original, g_slots[i].lastWritten, streak);
		}
		// 技能库《外部写入闸门》§3：连续拒绝够多 → 判定值漂移/换战局 → 自动重新解析并重新基线
		constexpr int kStreakTrigger = 24;
		constexpr uint32_t kMinIntervalMs = 10000;
		if (streak >= kStreakTrigger)
		{
			const uint32_t lastRr = g_lastReresolveTick.load();
			if (lastRr == 0 || nowTick - lastRr >= kMinIntervalMs)
			{
				g_lastReresolveTick.store(nowTick);
				g_refusalStreak.store(0);
				g_rebaselineCount.fetch_add(1);
				std::println("[ScriptGlobals] 连续拒绝 {} 次 → 判定值漂移/换战局，自动重新解析并重新基线（第 {} 次）",
				             kStreakTrigger, g_rebaselineCount.load());
				g_slots[i].original = current;      // 用现场值当新基线
				g_slots[i].lastWritten = current;
				ResolveInternal(true);
			}
		}
		return false;
	}
	g_refusalStreak.store(0);   // 写入成功 → 连续拒绝清零

	if (!DMA::Memory().Write(address, &value, sizeof(value)))
		return false;

	int32_t verify = 0;
	if (!DMA::Memory().Read(address, &verify, sizeof(verify)) || verify != value)
	{
		g_blockedWrites.fetch_add(1);
		std::println("[ScriptGlobals] 写入校验失败 {}：期望 {}，读回 {}", ScriptGlobalTable::kEntries[i].name, value,
		             verify);
		return false;
	}

	g_slots[i].lastWritten = value;
	g_writeCount.fetch_add(1);
	std::println("[ScriptGlobals] {} = {}{}{}", ScriptGlobalTable::kEntries[i].name, value,
	             why ? "  (" : "", why ? why : "");
	return true;
}

bool ScriptGlobals::Trigger(uint32_t i, int32_t value, const char* why)
{
	if (!Write(i, value, why))
		return false;
	g_slots[i].triggeredAt = GetTickCount();
	g_slots[i].triggered.store(true);
	return true;
}

void ScriptGlobals::OnFrame()
{
	const uint32_t now = GetTickCount();
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		if (!g_slots[i].triggered.load())
			continue;
		if ((now - g_slots[i].triggeredAt) < kTriggerHoldMs)
			continue;

		g_slots[i].triggered.store(false);
		if (!g_slots[i].haveOriginal.load())
			continue;

		// 脉冲还原：写回记录的原值（Write 内部会做写入前体检与读回校验）
		const int32_t original = g_slots[i].original;
		Write(i, original, "脉冲还原");
	}
}

bool ScriptGlobals::RestoreAll()
{
	bool ok = true;
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		if (!g_slots[i].resolved.load() || !g_slots[i].haveOriginal.load())
			continue;
		const int32_t original = g_slots[i].original;
		if (Write(i, original, "还原") == false)
			ok = false;
	}
	return ok;
}

bool ScriptGlobals::PrepareForClose()
{
	const bool ok = RestoreAll();
	Reset();
	return ok;
}

bool ScriptGlobals::IsResolved(uint32_t i) { return i < kSlotCount && g_slots[i].resolved.load(); }
uintptr_t ScriptGlobals::GetAddress(uint32_t i) { return i < kSlotCount ? g_slots[i].address.load() : 0; }
int32_t ScriptGlobals::GetOriginal(uint32_t i) { return i < kSlotCount ? g_slots[i].original : 0; }
int ScriptGlobals::GetResolvedCount() { return g_resolvedCount.load(); }
int ScriptGlobals::GetBlockedWriteCount() { return g_blockedWrites.load(); }
int ScriptGlobals::GetRebaselineCount() { return g_rebaselineCount.load(); }
int ScriptGlobals::GetWriteCount() { return g_writeCount.load(); }
const char* ScriptGlobals::GetEntryPurpose(uint32_t i)
{
	if (i >= kSlotCount)
		return "?";
	return ScriptGlobalTable::kEntries[i].purpose;
}

const char* ScriptGlobals::GetEntryName(uint32_t i)
{
	return i < kSlotCount ? ScriptGlobalTable::kEntries[i].name : "?";
}
uint32_t ScriptGlobals::GetGlobalIndex(uint32_t i)
{
	return i < kSlotCount ? ScriptGlobalTable::kEntries[i].index : 0;
}

int32_t ScriptGlobals::ReadLive(uint32_t i, bool* ok)
{
	if (i >= kSlotCount || !g_slots[i].resolved.load())
	{
		if (ok)
			*ok = false;
		return 0;
	}
	int32_t value = 0;
	const bool read = DMA::Memory().Read(g_slots[i].address.load(), &value, sizeof(value));
	if (ok)
		*ok = read;
	return value;
}

const char* ScriptGlobals::GetLocatedSummary()
{
	static char buffer[192];
	std::snprintf(buffer, sizeof(buffer), "%d/%d 条已定位（拒写 %d 次）", GetResolvedCount(),
	              static_cast<int>(kSlotCount), GetBlockedWriteCount());
	return buffer;
}

static_assert(ScriptGlobals::kSlotCount == ScriptGlobalTable::kEntryCount,
              "ScriptGlobals::kSlotCount must match ScriptGlobalTable::kEntryCount");
