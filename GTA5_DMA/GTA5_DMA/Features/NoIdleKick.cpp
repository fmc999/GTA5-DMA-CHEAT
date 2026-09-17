#include "pch.h"

#include "NoIdleKick.h"

#include "DMA.h"
#include "Tunables.h"

#include <array>
#include <climits>
#include <cstdio>

namespace
{
	// 槽位名（顺序即 UI 与自检顺序；索引一律由 Tunables 动态定位）
	constexpr std::array<const char*, NoIdleKick::kSlotCount> kSlotNames = {
		"IDLEKICK_WARNING1",
		"IDLEKICK_WARNING2",
		"IDLEKICK_WARNING3",
		"IDLEKICK_KICK",
		"ConstrainedKick_Warning1",
		"ConstrainedKick_Warning2",
		"ConstrainedKick_Warning3",
		"ConstrainedKick_Kick",
	};

	std::array<int, NoIdleKick::kSlotCount> g_entryIndex{};
	std::array<uint32_t, NoIdleKick::kSlotCount> g_lastAttempt{};
	uint32_t g_lastApplyLog = 0;
	bool     g_readyLogged = false;
}

bool NoIdleKick::Resolve()
{
	if (!DMA::IsReady())
		return false;

	// 已经定位过就直接返回：本函数每帧都会被调用，日志只允许在状态变化时出现一次
	if (g_readyLogged)
		return true;

	if (!Tunables::Resolve())
		return false;

	for (uint32_t i = 0; i < kSlotCount; ++i)
		g_entryIndex[i] = Tunables::Find(kSlotNames[i]);

	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		if (g_entryIndex[i] < 0 || !Tunables::IsResolved(static_cast<uint32_t>(g_entryIndex[i])))
		{
			std::println("[NoIdleKick] {} 未定位（tunable 索引未通过体检）", kSlotNames[i]);
			return false;
		}
	}

	std::println("[NoIdleKick] 8/8 踢出计时 tunable 已定位（走 Tunables 动态定位，不再是硬编码索引）");
	g_readyLogged = true;
	return true;
}

void NoIdleKick::OnDMAFrame()
{
	if (!DMA::IsReady())
		return;

	if (Resolve())
	{
		const bool want = bEnable.load();
		const uint32_t now = GetTickCount();

		for (uint32_t i = 0; i < kSlotCount; ++i)
		{
			const uint32_t slot = static_cast<uint32_t>(g_entryIndex[i]);
			if (!Tunables::IsResolved(slot))
				continue;

			// 开启 → 顶到 INT_MAX；关闭 → 还原默认计时值（Tunables 内部按原始值还原）
			const int32_t original = Tunables::GetOriginal(slot);
			const int32_t desired = want ? INT_MAX : original;

			int32_t current = Tunables::ReadLive(slot);
			if (current == desired)
				continue;

			if (g_lastAttempt[i] != 0 && (now - g_lastAttempt[i]) < 250)
				continue;
			g_lastAttempt[i] = now;

			Tunables::Write(slot, desired, want ? "防挂机踢出" : nullptr);
		}

		if (want && (now - g_lastApplyLog) > 15000)
		{
			g_lastApplyLog = now;
			std::println("[NoIdleKick] 已生效：8 条踢出计时 = INT_MAX（当前禁用状态 {}）",
			             bEnable.load() ? "开启" : "关闭");
		}
	}
}

bool NoIdleKick::RestoreAll()
{
	return Tunables::RestoreAll();
}

bool NoIdleKick::PrepareForClose()
{
	return Tunables::PrepareForClose();
}

void NoIdleKick::Reset()
{
	Tunables::Reset();
	g_readyLogged = false;
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		g_entryIndex[i] = -1;
		g_lastAttempt[i] = 0;
	}
}

int NoIdleKick::GetResolvedCount()
{
	int count = 0;
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		if (g_entryIndex[i] >= 0 && Tunables::IsResolved(static_cast<uint32_t>(g_entryIndex[i])))
			++count;
	}
	return count;
}

int NoIdleKick::GetAppliedCount()
{
	int count = 0;
	for (uint32_t i = 0; i < kSlotCount; ++i)
	{
		if (g_entryIndex[i] >= 0 && Tunables::HasWritten(static_cast<uint32_t>(g_entryIndex[i])))
			++count;
	}
	return count;
}

uintptr_t NoIdleKick::GetSlot(uint32_t slot)
{
	if (slot >= kSlotCount || g_entryIndex[slot] < 0)
		return 0;
	return Tunables::GetAddress(static_cast<uint32_t>(g_entryIndex[slot]));
}

uint32_t NoIdleKick::GetTunableIndex(uint32_t slot)
{
	if (slot >= kSlotCount || g_entryIndex[slot] < 0)
		return 0;
	return Tunables::GetGlobalIndex(static_cast<uint32_t>(g_entryIndex[slot]));
}

const char* NoIdleKick::GetSlotName(uint32_t slot)
{
	return slot < kSlotCount ? kSlotNames[slot] : "?";
}

