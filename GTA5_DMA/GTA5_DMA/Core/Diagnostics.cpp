#include "pch.h"

#include "Diagnostics.h"
#include "RuntimeTables.h"

#include "DMA.h"
#include "Offsets.h"

#include "EconomyFeatures.h"
#include "ScriptGlobals.h"
#include "ScriptThreads.h"
#include "Tunables.h"
#include "TunableTable.h"

#include <ctime>
#include <fstream>
#include <print>
#include <string>

namespace
{
	char g_defaultPath[MAX_PATH] = {};
	char g_lastPath[MAX_PATH] = {};
	bool g_written = false;

	std::string NowString()
	{
		const std::time_t t = std::time(nullptr);
		std::tm local{};
		localtime_s(&local, &t);
		char buffer[64] = {};
		std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
		return buffer;
	}

	std::string ExeDirectory()
	{
		char path[MAX_PATH] = {};
		const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
		std::string full(path, len ? len : 0);
		const std::size_t slash = full.find_last_of("\\/");
		return slash == std::string::npos ? std::string(".") : full.substr(0, slash);
	}

	void WriteOffsetBlock(std::ofstream& out)
	{
		out << "=== 已解析偏移（pattern = 特征码扫描命中；static = 目录静态兜底）===\n";
		out << "  BaseAddress             = 0x" << std::hex << DMA::BaseAddress << std::dec << "\n";
		out << "  WorldPtr                = 0x" << std::hex << Offsets::WorldPtr << std::dec << "\n";
		out << "  GlobalPtr               = 0x" << std::hex << Offsets::GlobalPtr << std::dec << "\n";
		out << "  BlipPtr                 = 0x" << std::hex << Offsets::BlipPtr << std::dec << "\n";
		out << "  PlayerMgrPtr            = 0x" << std::hex << Offsets::PlayerMgrPtr << std::dec << "\n";
		out << "  AimCPedPtr              = 0x" << std::hex << Offsets::AimCPedPtr << std::dec << "\n";
		out << "  LocalScriptsPtr(线程表) = 0x" << std::hex << Offsets::LocalScriptsPtr << std::dec << "\n";
		out << "  GTAPlusPtr              = 0x" << std::hex << Offsets::GTAPlusPtr << std::dec << "\n";
		out << "  PedPoolPtr              = 0x" << std::hex << Offsets::PedPoolPtr << std::dec << "\n";
		out << "  VehiclePoolPtr          = 0x" << std::hex << Offsets::VehiclePoolPtr << std::dec << "\n";
		out << "\n";
	}

	void WriteTunablesBlock(std::ofstream& out)
	{
		out << "=== 脚本 tunable（共 " << Tunables::kSlotCount << " 条）===\n";
		out << "  定位方式：四连锚+相对 / 绝对（候选位置值匹配）/ 未定位\n";
		int resolved = 0;
		for (uint32_t i = 0; i < Tunables::kSlotCount; ++i)
		{
			const bool ok = Tunables::IsResolved(i);
			bool liveOk = false;
			const int32_t live = Tunables::ReadLive(i, &liveOk);
			if (ok)
				++resolved;

			char value[64] = {};
			if (Tunables::IsValueKind(i))
				std::snprintf(value, sizeof(value), "%s%d", liveOk ? "" : "读失败 ", live);
			else if (std::strstr(Tunables::GetEntryName(i), "XP_MULTIPLIER"))
			{
				float f = 0.0f;
				std::memcpy(&f, &live, sizeof(f));
				std::snprintf(value, sizeof(value), "%.4f", f);
			}
			else
				std::snprintf(value, sizeof(value), "%s%d", liveOk ? "" : "读失败 ", live);

			out << "  [" << (ok ? "已定位" : "未定位") << "] " << Tunables::GetEntryName(i) << "  索引 0x" << std::hex
			    << Tunables::GetGlobalIndex(i) << std::dec << "  " << value;
			if (Tunables::IsValueKind(i))
				out << "  合法区间 [" << Tunables::GetMinValue(i) << ", " << Tunables::GetMaxValue(i) << "]";
			else
				out << "  期望默认 " << Tunables::GetExpectedDefault(i);
			out << "  " << Tunables::GetLocatedBy(i) << "\n";
		}
		out << "  小计：" << resolved << "/" << Tunables::kSlotCount << " 已定位，拒绝写入 "
		    << Tunables::GetBlockedWriteCount() << " 次"
		    << "；换战局/刷新后自动重新基线 " << Tunables::GetRebaselineCount() << " 次"
		    << "（值被游戏重置回默认时自动继续，不再每帧拒绝）\n\n";
	}

	void WriteGlobalsBlock(std::ofstream& out)
	{
		out << "=== 脚本全局动作格（共 " << ScriptGlobals::kSlotCount << " 条）===\n";
		for (uint32_t i = 0; i < ScriptGlobals::kSlotCount; ++i)
		{
			bool liveOk = false;
			const int32_t live = ScriptGlobals::ReadLive(i, &liveOk);
			out << "  [" << (ScriptGlobals::IsResolved(i) ? "已定位" : "未定位") << "] " << ScriptGlobals::GetEntryName(i)
			    << "  索引 0x" << std::hex << ScriptGlobals::GetGlobalIndex(i) << std::dec << "  实读 "
			    << (liveOk ? std::to_string(live) : std::string("读失败")) << "\n";
		}
		out << "  小计：" << ScriptGlobals::GetResolvedCount() << "/" << ScriptGlobals::kSlotCount
		    << " 已定位，写入 " << ScriptGlobals::GetWriteCount() << " 次，拒绝写入 "
		    << ScriptGlobals::GetBlockedWriteCount() << " 次\n\n";
	}

	// 探测 tunable 块（chunk 1）在当前会话里到底分配了多大：
	// 元素按 512 步长试读，报告最后一个可读元素 —— 超出分配范围的条目会"读失败"，
	// 这是「读失败」与「区间体检不通过」两类未定位的区分依据。
	void WriteRuntimeTablesBlock(std::ofstream& out)
	{
		out << "=== 运行时表（免重编译路径）===\n";
		out << RuntimeTables::GetReport();
		out << "\n";
	}

void WriteBlockExtentBlock(std::ofstream& out)
	{
		out << "=== tunable 块可读范围探测（chunk 1）===\n";
		const uintptr_t chunkBase = DMA::GetGlobalAddress(TunableTable::kTunableBaseAddress);
		if (!chunkBase)
		{
			out << "  分块未分配（未进在线战局）\n\n";
			return;
		}
		out << "  chunk 1 基址 = 0x" << std::hex << chunkBase << std::dec << "\n";

		uint32_t lastReadable = 0;
		uint32_t lastProbed = 0;
		uint32_t probe = 0;
		constexpr uint32_t kElementStride = 512;
		constexpr uint32_t kMaxElement = 36600;      // 36,796 条 tunable 的上界
		for (uint32_t element = 0; element < kMaxElement; element += kElementStride)
		{
			int32_t value = 0;
			lastProbed = element;
			if (DMA::Memory().Read(chunkBase + static_cast<uintptr_t>(element) * 8, &value, sizeof(value)))
				lastReadable = element;
		}
		out << "  按 " << kElementStride << " 元素步长试读到 " << lastProbed << "："
		    << "最后可读元素 ≈ " << lastReadable << "（对应字节偏移 ≈ 0x" << std::hex << (lastReadable * 8) << std::dec << "）\n";
		out << "  含义：元素位置大于该值的 tunable 在本会话读不到 → 会显示「读失败 / 未定位」（不是写错地方）\n\n";
	}

	void WriteThreadsBlock(std::ofstream& out)
	{
		out << "=== 脚本线程 ===\n";
		out << "  " << ScriptThreads::GetSummary() << "\n";
		const uint32_t total = ScriptThreads::Count();
		const uint32_t limit = total < 200 ? total : 200;
		for (uint32_t i = 0; i < limit; ++i)
		{
			ScriptThreads::ThreadInfo info{};
			if (!ScriptThreads::Get(i, info))
				continue;
			out << "    #" << i << " " << (info.name[0] ? info.name : "(无名)") << "  hash=0x" << std::hex << info.hash
			    << std::dec << "  stack=0x" << std::hex << info.stack << std::dec << "\n";
		}
		if (total > limit)
			out << "    …… 其余 " << (total - limit) << " 个省略\n";
		out << "\n";
	}
}

const char* Diagnostics::GetDefaultPath()
{
	if (!g_defaultPath[0])
	{
		const std::string dir = ExeDirectory();
		std::snprintf(g_defaultPath, sizeof(g_defaultPath), "%s\\GTA5_DMA_diag.txt", dir.c_str());
	}
	return g_defaultPath;
}

const char* Diagnostics::GetLastPath() { return g_lastPath; }
bool Diagnostics::HasWritten() { return g_written; }

bool Diagnostics::WriteReport(const char* path)
{
	const char* target = (path && *path) ? path : GetDefaultPath();

	std::ofstream out(target, std::ios::out | std::ios::trunc);
	if (!out.is_open())
	{
		std::println("[Diagnostics] 诊断文件写入失败：{}", target);
		return false;
	}

	const bool ready = DMA::IsReady();
	out << "GTA5-DMA 诊断报告\n";
	out << "生成时间   : " << NowString() << "\n";
	out << "构建标记   : " << DMA::BuildTag << "\n";
	out << "进程       : GTA5_Enhanced.exe  PID = " << DMA::PID << "  附加 = " << (ready ? "是" : "否") << "\n";
	out << "模块基址   : 0x" << std::hex << DMA::BaseAddress << std::dec << "\n";
	out << "\n";

	WriteOffsetBlock(out);
	WriteTunablesBlock(out);
	WriteBlockExtentBlock(out);
	WriteRuntimeTablesBlock(out);
	WriteGlobalsBlock(out);
	WriteThreadsBlock(out);

	out << "=== 经济功能 ===\n";
	out << "  " << EconomyFeatures::GetStatusLine() << "\n";
	out << "  最近动作：" << EconomyFeatures::GetLastAction() << "\n";
	out << "  开关：自动领取=" << (EconomyFeatures::bAutoClaimSafeEarnings.load() ? "开" : "关")
	    << "  自动静音=" << (EconomyFeatures::bAutoSilenceCalls.load() ? "开" : "关")
	    << "  GTA+=" << (EconomyFeatures::bUnlockGTAPlus.load() ? "开" : "关") << "\n";
	out << "  保险箱动作格就绪：";
	for (uint32_t i = 0; i < static_cast<uint32_t>(EconomyFeatures::kSafeCount); ++i)
		out << (i ? ", " : "") << EconomyFeatures::GetBusinessName(i) << (EconomyFeatures::IsBusinessReady(i) ? "(可)" : "(未定位)");
	out << "\n";
	out << "\n=== 报告结束 ===\n";
	out.close();

	std::snprintf(g_lastPath, sizeof(g_lastPath), "%s", target);
	g_written = true;
	std::println("[Diagnostics] 诊断已写入 {}", target);
	return true;
}
