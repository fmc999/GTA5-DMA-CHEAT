#pragma once

// ============================================================================
//  ScriptThreads —— 脚本线程（rage::scrThread）只读通道
//
//  地址来源：Offsets::LocalScriptsPtr —— 特征码与本项目偏移目录里的
//      "48 8B 05 ? ? ? ? 48 89 34 F8 48 FF C7 48 39 FB 75 97"（disp 3 / insn 7）
//  指向 rage::atArray<scrThread*>（{void* data; uint16 count; uint16 capacity; uint32 pad} = 16 字节）。
//  这条特征码在 YimMenuV2 里就叫 ScriptThreads（pointers/Pointers.cpp），本项目的名字
//  LocalScriptsPtr 是历史命名，实际就是脚本线程数组。
//
//  线程布局（Enhanced，逐条对照 YimMenuV2 types/script/scrThread.hpp）：
//      m_Context @0x08（Context 0xB0 字节）｜m_Stack @0xB8
//      m_ParameterSize @0xC4｜m_ScriptHash @0x150｜m_ScriptName @0x154（64 字节）
//  脚本 locals 在线程栈上：local[i] = m_Stack + i*8 —— 本轮只提供**只读定位**，
//  写入通道留给下一轮（赌场老虎机 / 保险箱密码那一类功能要先在实机上验证栈布局）。
// ============================================================================

#include <atomic>
#include <cstdint>

class ScriptThreads
{
public:
	static constexpr uint32_t kMaxThreads = 512;
	static constexpr uint32_t kNameLength = 48;
	static constexpr uintptr_t kStackOffset = 0xB8;
	static constexpr uintptr_t kHashOffset = 0x150;
	static constexpr uintptr_t kNameOffset = 0x154;

	struct ThreadInfo
	{
		uintptr_t address = 0;
		uint32_t  hash = 0;
		uint64_t  stack = 0;
		char      name[kNameLength] = {};
	};

	static bool     Resolve();                 // 读 atArray 头（data / count / capacity）
	static void     Reset();
	static bool     IsReady();
	static uint32_t Count();
	static uintptr_t GetArrayAddress();
	static uintptr_t GetThreadAddress(uint32_t index);
	static bool     Get(uint32_t index, ThreadInfo& out);
	static int      CopyAll(ThreadInfo* out, int maxCount);         // 返回实际拷贝数
	static int      FindByHash(uint32_t hash);
	static int      FindByName(const char* name);                   // 大小写不敏感的前缀匹配
	static uintptr_t GetLocalAddress(uint32_t threadIndex, uint32_t localIndex);   // 只读定位
	static uint32_t GetFreemodeHash();
	static void     LogRunningScripts(int maxCount = 12);           // 启动时打印一次
	static const char* GetSummary();
};
