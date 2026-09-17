#include "pch.h"

#include "ProgressFeatures.h"

#include "DMA.h"
#include "Tunables.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace
{
	constexpr uint32_t kApplyIntervalMs = 500;

	std::mutex g_reportMutex;
	ProgressFeatures::Report g_report;
	uint32_t g_lastApply = 0;
	uint32_t g_lastLog = 0;

	int g_rpSlot = -1;
	int g_cooldownSlot = -1;
	int g_chargeSlot = -1;

	// 金额/额度类：槽位 i → tunable 条目下标（Resolve 时按表里的 Value 型顺序填）
	int  g_valueEntry[ProgressFeatures::kValueSlotCount] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
	bool g_valueResolved = false;

	int32_t ClampToRange(uint32_t entry, int32_t value)
	{
		const int32_t lo = Tunables::GetMinValue(entry);
		const int32_t hi = Tunables::GetMaxValue(entry);
		if (lo != 0 && value < lo)
			return lo;
		if (hi != 0 && value > hi)
			return hi;
		return value;
	}

	bool g_resolved = false;

	void SetReport(const char* text)
	{
		std::lock_guard<std::mutex> lock(g_reportMutex);
		std::snprintf(g_report.last, sizeof(g_report.last), "%s", text ? text : "");
		g_report.appliedSlots = 0;
		g_report.blockedWrites = Tunables::GetBlockedWriteCount();
	}

	void LogOnce(const char* text)
	{
		const uint32_t now = GetTickCount();
		if (now - g_lastLog < 15000)
			return;
		g_lastLog = now;
		std::println("[Progress] {}", text);
	}
}

bool ProgressFeatures::Resolve()
{
	if (!DMA::IsReady())
		return false;

	if (!Tunables::Resolve())
		return false;

	g_rpSlot = Tunables::Find("XP_MULTIPLIER");
	g_cooldownSlot = Tunables::Find("CHARACTER_APPEARANCE_COOLDOWN");
	g_chargeSlot = Tunables::Find("CHARACTER_APPEARANCE_CHARGE");

	const bool ok = g_rpSlot >= 0 && Tunables::IsResolved(static_cast<uint32_t>(g_rpSlot)) &&
	                g_cooldownSlot >= 0 && Tunables::IsResolved(static_cast<uint32_t>(g_cooldownSlot)) &&
	                g_chargeSlot >= 0 && Tunables::IsResolved(static_cast<uint32_t>(g_chargeSlot));

	// ---- 金额/额度类槽位（第18轮）----
	{
		int slot = 0;
		for (uint32_t i = 0; i < Tunables::kSlotCount && slot < static_cast<int>(kValueSlotCount); ++i)
		{
			if (!Tunables::IsValueKind(i))
				continue;
			g_valueEntry[slot++] = static_cast<int>(i);
		}
		for (int i = slot; i < static_cast<int>(kValueSlotCount); ++i)
			g_valueEntry[i] = -1;

		int ready = 0;
		for (uint32_t i = 0; i < kValueSlotCount; ++i)
		{
			if (g_valueEntry[i] >= 0 && Tunables::IsResolved(static_cast<uint32_t>(g_valueEntry[i])))
				++ready;
		}
		if (ready > 0 && !g_valueResolved)
		{
			g_valueResolved = true;
			std::println("[Progress] 金额类 tunable 就绪：{}/{} 条（佩里克主目标价值 / 跳伞挑战奖励，区间体检）",
			             ready, static_cast<int>(kValueSlotCount));
		}
	}

	if (ok && !g_resolved)
	{
		g_resolved = true;
		std::println("[Progress] 进度类 tunable 就绪：XP_MULTIPLIER / CHARACTER_APPEARANCE_COOLDOWN / CHARACTER_APPEARANCE_CHARGE");
	}
	return ok;
}

void ProgressFeatures::OnDMAFrame()
{
	if (!DMA::IsReady())
		return;

	if (!Resolve())
		return;

	const uint32_t now = GetTickCount();
	if (now - g_lastApply < kApplyIntervalMs)
		return;
	g_lastApply = now;

	int applied = 0;

	// ---- RP 倍率 ----
	{
		const uint32_t slot = static_cast<uint32_t>(g_rpSlot);
		const float wanted = bRpMultiplier.load() ? rpMultiplier.load() : 1.0f;
		bool ok = false;
		const int32_t live = Tunables::ReadLive(slot, &ok);
		float liveFloat = 0.0f;
		std::memcpy(&liveFloat, &live, sizeof(liveFloat));
		if (!ok || liveFloat != wanted)
		{
			if (Tunables::WriteFloat(slot, wanted, bRpMultiplier.load() ? "RP 倍率" : "还原"))
				++applied;
		}
		else
		{
			++applied;
		}
	}

	// ---- 外貌冷却 / 收费 ----
	{
		const uint32_t slot = static_cast<uint32_t>(g_cooldownSlot);
		const int32_t desired = bNoAppearanceCooldown.load() ? 0 : Tunables::GetOriginal(slot);
		if (Tunables::ReadLive(slot) != desired)
		{
			if (Tunables::Write(slot, desired, bNoAppearanceCooldown.load() ? "外貌免冷却" : "还原"))
				++applied;
		}
		else
		{
			++applied;
		}
	}
	{
		const uint32_t slot = static_cast<uint32_t>(g_chargeSlot);
		const int32_t desired = bFreeAppearance.load() ? 0 : Tunables::GetOriginal(slot);
		if (Tunables::ReadLive(slot) != desired)
		{
			if (Tunables::Write(slot, desired, bFreeAppearance.load() ? "外貌免费" : "还原"))
				++applied;
		}
		else
		{
			++applied;
		}
	}

	// ---- 金额/额度类（抢劫收益 / 挑战奖励）：倍率 × 原值，clamp 到合法区间 ----
	int valueApplied = 0;
	{
		const bool master = bValueMultiplier.load();
		const float mult = valueMultiplier.load();
		for (uint32_t i = 0; i < kValueSlotCount; ++i)
		{
			const int entry = g_valueEntry[i];
			if (entry < 0)
				continue;
			const uint32_t slot = static_cast<uint32_t>(entry);
			if (!Tunables::IsResolved(slot))
				continue;

			const int32_t original = Tunables::GetOriginal(slot);
			const bool enabled = master && valueSlotEnabled[i].load();
			int32_t desired = enabled ? static_cast<int32_t>(std::lround(static_cast<double>(original) * mult)) : original;
			desired = ClampToRange(slot, desired);

			if (Tunables::ReadLive(slot) != desired)
			{
				if (Tunables::Write(slot, desired, enabled ? "金额倍率" : "还原"))
				{
					++applied;
					if (enabled)
						++valueApplied;
				}
			}
			else
			{
				++applied;
				if (enabled)
					++valueApplied;
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock(g_reportMutex);
		g_report.appliedSlots = applied;
		g_report.valueApplied = valueApplied;
		g_report.blockedWrites = Tunables::GetBlockedWriteCount();
	}

	if (bRpMultiplier.load())
		LogOnce("RP 倍率已生效（XP_MULTIPLIER 已按设定值写入，游戏刷新 tunables 后本模块会重新写回）");
	if (bNoAppearanceCooldown.load() || bFreeAppearance.load())
		LogOnce("外貌相关 tunable 已生效（冷却写 0 / 收费写 0）");
}

bool ProgressFeatures::RestoreAll()
{
	return Tunables::RestoreAll();
}

bool ProgressFeatures::PrepareForClose()
{
	return Tunables::PrepareForClose();
}

void ProgressFeatures::Reset()
{
	Tunables::Reset();
	g_rpSlot = g_cooldownSlot = g_chargeSlot = -1;
	for (uint32_t i = 0; i < kValueSlotCount; ++i)
	{
		g_valueEntry[i] = -1;
		valueSlotEnabled[i].store(false);
	}
	g_valueResolved = false;
	g_resolved = false;
}

int ProgressFeatures::GetValueSlotByEntry(uint32_t entry)
{
	for (uint32_t i = 0; i < kValueSlotCount; ++i)
	{
		if (g_valueEntry[i] >= 0 && static_cast<uint32_t>(g_valueEntry[i]) == entry)
			return static_cast<int>(i);
	}
	return -1;
}

const char* ProgressFeatures::GetValueSlotLabel(uint32_t slot)
{
	if (slot >= kValueSlotCount || g_valueEntry[slot] < 0)
		return "?";
	return Tunables::GetEntryLabel(static_cast<uint32_t>(g_valueEntry[slot]));
}

const char* ProgressFeatures::GetValueSlotName(uint32_t slot)
{
	if (slot >= kValueSlotCount || g_valueEntry[slot] < 0)
		return "?";
	return Tunables::GetEntryName(static_cast<uint32_t>(g_valueEntry[slot]));
}

int32_t ProgressFeatures::GetValueSlotOriginal(uint32_t slot)
{
	if (slot >= kValueSlotCount || g_valueEntry[slot] < 0)
		return 0;
	return Tunables::GetOriginal(static_cast<uint32_t>(g_valueEntry[slot]));
}

int32_t ProgressFeatures::GetValueSlotLive(uint32_t slot, bool* ok)
{
	if (slot >= kValueSlotCount || g_valueEntry[slot] < 0)
	{
		if (ok)
			*ok = false;
		return 0;
	}
	return Tunables::ReadLive(static_cast<uint32_t>(g_valueEntry[slot]), ok);
}

ProgressFeatures::Report ProgressFeatures::GetReport()
{
	std::lock_guard<std::mutex> lock(g_reportMutex);
	return g_report;
}

int ProgressFeatures::GetResolvedCount()
{
	int count = 0;
	if (g_rpSlot >= 0 && Tunables::IsResolved(static_cast<uint32_t>(g_rpSlot)))
		++count;
	if (g_cooldownSlot >= 0 && Tunables::IsResolved(static_cast<uint32_t>(g_cooldownSlot)))
		++count;
	if (g_chargeSlot >= 0 && Tunables::IsResolved(static_cast<uint32_t>(g_chargeSlot)))
		++count;
	return count;
}

