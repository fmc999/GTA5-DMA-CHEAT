#include "pch.h"

#include "ScriptThreads.h"

#include "DMA.h"
#include "Offsets.h"

#include <cstdio>
#include <print>

namespace
{
	std::atomic<uintptr_t> g_array{ 0 };
	std::atomic<uintptr_t> g_data{ 0 };
	std::atomic<uint32_t>  g_count{ 0 };
	std::atomic<uint32_t>  g_capacity{ 0 };
	std::atomic<uint32_t>  g_processId{ 0 };
	std::atomic<uintptr_t> g_processBase{ 0 };
	std::atomic<bool>      g_ready{ false };
	std::atomic<bool>      g_haveIdentity{ false };

	constexpr uint32_t kMaxScanCount = ScriptThreads::kMaxThreads;

	void Clear()
	{
		g_array.store(0);
		g_data.store(0);
		g_count.store(0);
		g_capacity.store(0);
		g_ready.store(false);
	}

	bool SameProcess()
	{
		return g_haveIdentity.load() && g_processId.load() == DMA::PID && g_processBase.load() == DMA::BaseAddress;
	}
}

bool ScriptThreads::Resolve()
{
	if (!DMA::IsReady())
		return false;

	if (!SameProcess())
	{
		Clear();
		g_processId.store(DMA::PID);
		g_processBase.store(DMA::BaseAddress);
		g_haveIdentity.store(true);
	}

	// Offsets::LocalScriptsPtr 是「atArray<scrThread*>」这个全局变量自身的模块相对地址
	const uintptr_t array = DMA::BaseAddress + Offsets::LocalScriptsPtr;
	if (!array || array == DMA::BaseAddress)
	{
		std::println("[ScriptThreads] LocalScriptsPtr 未解析（静态兜底为 0，特征码没命中）");
		return false;
	}

	// atArray 头：+0x00 数据指针 / +0x08 count / +0x0A capacity
	uintptr_t data = 0;
	uint16_t count = 0;
	uint16_t capacity = 0;
	if (!DMA::Memory().Read(array, &data, sizeof(data)))
	{
		std::println("[ScriptThreads] 读 atArray 头失败 @ 0x{:X}", array);
		return false;
	}
	if (!DMA::Memory().Read(array + 0x08, &count, sizeof(count)))
		return false;
	if (!DMA::Memory().Read(array + 0x0A, &capacity, sizeof(capacity)))
		return false;

	if (!data || count == 0 || count > kMaxScanCount)
	{
		std::println("[ScriptThreads] atArray 异常：data = 0x{:X}，count = {}，capacity = {}（未进战局时属正常）", data,
		             count, capacity);
		return false;
	}

	g_array.store(array);
	g_data.store(data);
	g_count.store(count);
	g_capacity.store(capacity);
	g_ready.store(true);

	std::println("[ScriptThreads] 脚本线程数组 @ 0x{:X}：data = 0x{:X}，count = {}，capacity = {}（特征码 LocalScriptsPtr）",
	             array, data, count, capacity);
	return true;
}

void ScriptThreads::Reset()
{
	Clear();
	g_haveIdentity.store(false);
	g_processId.store(0);
	g_processBase.store(0);
}

bool ScriptThreads::IsReady() { return g_ready.load(); }
uint32_t ScriptThreads::Count() { return g_ready.load() ? g_count.load() : 0; }
uintptr_t ScriptThreads::GetArrayAddress() { return g_array.load(); }

uintptr_t ScriptThreads::GetThreadAddress(uint32_t index)
{
	if (!g_ready.load() || index >= g_count.load())
		return 0;
	uintptr_t thread = 0;
	if (!DMA::Memory().Read(g_data.load() + index * 8, &thread, sizeof(thread)))
		return 0;
	return thread;
}

bool ScriptThreads::Get(uint32_t index, ThreadInfo& out)
{
	const uintptr_t thread = GetThreadAddress(index);
	if (!thread || thread < 0x10000)
		return false;

	out = ThreadInfo{};
	out.address = thread;

	uint32_t hash = 0;
	if (DMA::Memory().Read(thread + kHashOffset, &hash, sizeof(hash)))
		out.hash = hash;

	uint64_t stack = 0;
	if (DMA::Memory().Read(thread + kStackOffset, &stack, sizeof(stack)))
		out.stack = stack;

	char name[kNameLength] = {};
	if (DMA::Memory().Read(thread + kNameOffset, name, sizeof(name)))
	{
		name[kNameLength - 1] = '\0';
		std::memcpy(out.name, name, sizeof(out.name));
	}
	return true;
}

int ScriptThreads::CopyAll(ThreadInfo* out, int maxCount)
{
	if (!out || maxCount <= 0)
		return 0;

	const uint32_t total = Count();
	int copied = 0;
	for (uint32_t i = 0; i < total && copied < maxCount; ++i)
	{
		ThreadInfo info{};
		if (!Get(i, info))
			continue;
		out[copied++] = info;
	}
	return copied;
}

int ScriptThreads::FindByHash(uint32_t hash)
{
	const uint32_t total = Count();
	for (uint32_t i = 0; i < total; ++i)
	{
		ThreadInfo info{};
		if (!Get(i, info))
			continue;
		if (info.hash == hash)
			return static_cast<int>(i);
	}
	return -1;
}

int ScriptThreads::FindByName(const char* name)
{
	if (!name || !*name)
		return -1;

	const uint32_t total = Count();
	for (uint32_t i = 0; i < total; ++i)
	{
		ThreadInfo info{};
		if (!Get(i, info) || !info.name[0])
			continue;
		if (_strnicmp(info.name, name, std::strlen(name)) == 0)
			return static_cast<int>(i);
	}
	return -1;
}

uintptr_t ScriptThreads::GetLocalAddress(uint32_t threadIndex, uint32_t localIndex)
{
	ThreadInfo info{};
	if (!Get(threadIndex, info) || !info.stack)
		return 0;
	return info.stack + static_cast<uintptr_t>(localIndex) * 8;
}

uint32_t ScriptThreads::GetFreemodeHash()
{
	const int index = FindByName("freemode");
	if (index < 0)
		return 0;
	ThreadInfo info{};
	if (!Get(static_cast<uint32_t>(index), info))
		return 0;
	return info.hash;
}

void ScriptThreads::LogRunningScripts(int maxCount)
{
	if (!Resolve())
		return;

	const uint32_t total = Count();
	std::println("[ScriptThreads] 当前运行脚本 {} 个，前 {} 个：", total, maxCount);

	ThreadInfo info{};
	int printed = 0;
	for (uint32_t i = 0; i < total && printed < maxCount; ++i)
	{
		if (!Get(i, info))
			continue;
		std::println("    #{} {:<28} hash = 0x{:08X} stack = 0x{:X}", i, info.name[0] ? info.name : "(无名)", info.hash,
		             info.stack);
		++printed;
	}
}

const char* ScriptThreads::GetSummary()
{
	static char buffer[192];
	std::snprintf(buffer, sizeof(buffer), "运行脚本 %u 个（数组 @ 0x%llX，data @ 0x%llX）", Count(),
	              static_cast<unsigned long long>(GetArrayAddress()), static_cast<unsigned long long>(g_data.load()));
	return buffer;
}
