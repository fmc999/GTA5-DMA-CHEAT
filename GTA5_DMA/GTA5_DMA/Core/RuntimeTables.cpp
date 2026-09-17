#include "pch.h"

#include "RuntimeTables.h"

#include "DMA.h"
#include "Offsets.h"
#include "TunableTable.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
	std::mutex g_mutex;

	std::string g_exeDir;
	std::string g_binPath;
	std::string g_binStatus = "未加载";
	std::string g_tunablesFilePath;
	std::string g_patternsFilePath;

	std::unordered_map<uint32_t, uint32_t> g_binByHash;                // joaat → 全局索引
	std::unordered_map<std::string, uint32_t> g_overrides;             // 名字 → 全局索引
	std::unordered_map<std::string, std::vector<RuntimeTables::PatternOverride>> g_patternOverrides;  // 偏移名 → 追加特征码

	uint32_t g_patternOverrideCount = 0;

	std::string Trim(const std::string& s)
	{
		const auto b = s.find_first_not_of(" \t\r\n");
		if (b == std::string::npos)
			return {};
		const auto e = s.find_last_not_of(" \t\r\n");
		return s.substr(b, e - b + 1);
	}

	std::string ExeDirectory()
	{
		char path[MAX_PATH]{};
		const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
		std::string full(path, len ? len : 0);
		const auto pos = full.find_last_of("\\/");
		return pos == std::string::npos ? std::string(".") : full.substr(0, pos);
	}

	std::string DefaultBinPath()
	{
		char appdata[MAX_PATH]{};
		if (GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH) == 0)
			return {};
		return std::string(appdata) + "\\YimMenuV2\\tunables.bin";
	}

	bool ParseHexOrDec(const std::string& text, uint32_t& out)
	{
		try
		{
			std::size_t used = 0;
			const auto value = std::stoull(text, &used, 0);
			if (used != text.size())
				return false;
			out = static_cast<uint32_t>(value);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	// GTA5_DMA_tunables.txt：
	//   # 注释
	//   bin = C:\path\to\tunables.bin        （可选，改缓存位置）
	//   名字 = 0x47392                        （可选，人工覆盖）
	void LoadTunablesFile()
	{
		std::ifstream in(g_tunablesFilePath);
		if (!in)
			return;

		std::string line;
		while (std::getline(in, line))
		{
			line = Trim(line);
			if (line.empty() || line[0] == '#' || line[0] == ';')
				continue;
			const auto eq = line.find('=');
			if (eq == std::string::npos)
				continue;
			const std::string key = Trim(line.substr(0, eq));
			const std::string value = Trim(line.substr(eq + 1));
			if (key.empty() || value.empty())
				continue;

			if (key == "bin")
			{
				g_binPath = value;
				continue;
			}

			uint32_t index = 0;
			if (ParseHexOrDec(value, index))
				g_overrides[key] = index;
		}
	}

	// GTA5_DMA_patterns.txt：偏移名 = 48 8B 0D ? ? ? ? 48 89 34 F8
	void LoadPatternsFile()
	{
		std::ifstream in(g_patternsFilePath);
		if (!in)
			return;

		std::string line;
		while (std::getline(in, line))
		{
			line = Trim(line);
			if (line.empty() || line[0] == '#' || line[0] == ';')
				continue;
			const auto eq = line.find('=');
			if (eq == std::string::npos)
				continue;
			const std::string key = Trim(line.substr(0, eq));
			const std::string value = Trim(line.substr(eq + 1));
			if (key.empty() || value.empty())
				continue;

			// 允许「特征码 | disp=3 insn=7」形式：主/备特征是不同指令，位移必须能单独给，
			// 否则会按主模式的值算出完全错误的地址（项目里发生过：GlobalPtr 解出 0x4737178）。
			RuntimeTables::PatternOverride candidate{};
			auto parseLayout = [&candidate](const std::string& layoutText) {
				std::istringstream stream(layoutText);
				std::string token;
				while (stream >> token)
				{
					const auto assign = token.find('=');
					if (assign == std::string::npos)
						continue;
					const std::string name = token.substr(0, assign);
					uint32_t number = 0;
					if (!ParseHexOrDec(token.substr(assign + 1), number))
						continue;
					if (name == "disp")
					{
						candidate.displacementOffset = number;
						candidate.hasLayout = true;
					}
					else if (name == "insn")
					{
						candidate.instructionSize = number;
						candidate.hasLayout = true;
					}
				}
			};

			std::string patternText = value;
			if (const auto bar = value.find('|'); bar != std::string::npos)
			{
				patternText = Trim(value.substr(0, bar));
				parseLayout(value.substr(bar + 1));
			}
			if (patternText.empty())
				continue;
			candidate.pattern = patternText;

			g_patternOverrides[key].push_back(std::move(candidate));
			++g_patternOverrideCount;
		}
	}

	bool LoadBin()
	{
		if (g_binPath.empty())
		{
			g_binStatus = "路径为空";
			return false;
		}

		std::ifstream in(g_binPath, std::ios::binary);
		if (!in)
		{
			g_binStatus = "打不开（文件不存在？）";
			return false;
		}

		uint32_t cacheVersion = 0;
		uint32_t fileVersion = 0;
		uint64_t dataSize = 0;
		uint32_t count = 0;
		in.read(reinterpret_cast<char*>(&cacheVersion), 4);
		in.read(reinterpret_cast<char*>(&fileVersion), 4);
		in.read(reinterpret_cast<char*>(&dataSize), 8);
		in.read(reinterpret_cast<char*>(&count), 4);
		if (!in || count == 0 || count > 500000)
		{
			g_binStatus = "头部不合法（格式变了？）";
			return false;
		}

		g_binByHash.clear();
		g_binByHash.reserve(count * 2);
		for (uint32_t i = 0; i < count; ++i)
		{
			uint32_t hash = 0;
			uint32_t index = 0;
			in.read(reinterpret_cast<char*>(&hash), 4);
			in.read(reinterpret_cast<char*>(&index), 4);
			if (!in)
			{
				g_binStatus = "尾部截断";
				return false;
			}
			g_binByHash[hash] = index;
		}

		g_binStatus = "已加载 " + std::to_string(g_binByHash.size()) + " 条";
		return true;
	}

	// 首次运行写出模板，让"改文本 = 免重编译"这条路径随时可用
	void WriteSeedFiles()
	{
		{
			std::ifstream probe(g_tunablesFilePath);
			if (!probe)
			{
				std::ofstream out(g_tunablesFilePath);
				out << "# GTA5-DMA 运行时 tunable 表（改这个文件不需要重新编译 exe）\n";
				out << "# 格式：名字 = 索引        （索引可写 0x47392 或十进制）\n";
				out << "# 这里写的值优先级最高：一旦写了，就覆盖 tunables.bin 与内置兜底表\n";
				out << "# bin = C:\\Users\\<你>\\AppData\\Roaming\\YimMenuV2\\tunables.bin\n";
				out << "#        ↑ 想指向别的 tunables.bin 就取消这行注释并改路径\n";
				out << "#\n";
				out << "# 游戏更新后：先用「导出诊断」看哪些条目变成 [未定位]，\n";
				out << "# 把新索引填在下面（或用 tools/gen_tunables.py 重新生成整表）\n";
				out << "#\n";
				for (uint32_t i = 0; i < TunableTable::kEntryCount; ++i)
				{
					const auto& entry = TunableTable::kEntries[i];
					char buffer[128]{};
					std::snprintf(buffer, sizeof(buffer), "# %s = 0x%X\n", entry.name, entry.globalIndex);
					out << buffer;
				}
			}
		}
		{
			std::ifstream probe(g_patternsFilePath);
			if (!probe)
			{
				std::ofstream out(g_patternsFilePath);
				out << "# GTA5-DMA 运行时特征码追加表（改这个文件不需要重新编译 exe）\n";
				out << "# 格式：偏移名 = 48 8B 0D ? ? ? ? 48 89 34 F8\n";
				out << "# 同一个偏移名可以写多行，程序会按 内置候选 → 本文件候选 的顺序依次尝试，\n";
				out << "# 谁先通过校验（命中数 + 落点体检）就用谁。\n";
				out << "#\n";
				out << "# 游戏大版本更新、内置特征码全失配时，在这里补一条新特征码即可，不用重编译。\n";
				out << "# 常用偏移名见「导出诊断」报告里的「已解析偏移」段。\n";
			}
		}
	}
}

bool RuntimeTables::Initialize()
{
	std::lock_guard<std::mutex> lock(g_mutex);

	g_exeDir = ExeDirectory();
	g_tunablesFilePath = g_exeDir + "\\GTA5_DMA_tunables.txt";
	g_patternsFilePath = g_exeDir + "\\GTA5_DMA_patterns.txt";
	g_binPath = DefaultBinPath();

	LoadTunablesFile();
	LoadPatternsFile();
	WriteSeedFiles();

	// 第一次加载之后，覆盖文件里可能指定了别的 bin 路径
	LoadBin();

	std::string line = "[RuntimeTables] 运行时表：";
	line += g_binStatus;
	line += "；外部覆盖 " + std::to_string(g_overrides.size()) + " 条";
	line += "；特征码追加 " + std::to_string(g_patternOverrideCount) + " 条";
	std::println("{}", line);
	return true;
}

void RuntimeTables::Shutdown()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	g_binByHash.clear();
	g_overrides.clear();
	g_patternOverrides.clear();
	g_patternOverrideCount = 0;
	g_binStatus = "未加载";
}

uint32_t RuntimeTables::Joaat(const char* text)
{
	uint32_t hash = 0;
	for (const char* p = text; p && *p; ++p)
	{
		const unsigned char ch = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(*p)));
		hash += ch;
		hash += hash << 10;
		hash ^= hash >> 6;
	}
	hash += hash << 3;
	hash ^= hash >> 11;
	hash += hash << 15;
	return hash;
}

uint32_t RuntimeTables::ResolveTunableIndex(const char* name, uint32_t joaatHash, uint32_t compiledSeed,
                                            const char** sourceOut)
{
	std::lock_guard<std::mutex> lock(g_mutex);

	// ① 外部覆盖文件（人工/工具改的，优先级最高）
	if (const auto it = g_overrides.find(name); it != g_overrides.end())
	{
		if (sourceOut)
			*sourceOut = "外部覆盖文件";
		return it->second;
	}

	// ② tunables.bin（名字哈希稳定，索引漂移自动跟上）
	const uint32_t hash = joaatHash ? joaatHash : Joaat(name);
	if (const auto it = g_binByHash.find(hash); it != g_binByHash.end())
	{
		if (sourceOut)
			*sourceOut = "tunables.bin";
		return it->second;
	}

	// ④ 编译期兜底（值锚自发现由调用方在体检失败时用 FindValueRunGlobalIndex 触发）
	if (sourceOut)
		*sourceOut = "编译种子";
	return compiledSeed;
}

int64_t RuntimeTables::FindValueRunGlobalIndex(const int32_t* values, uint32_t count, uint32_t* occurrencesOut)
{
	if (occurrencesOut)
		*occurrencesOut = 0;
	if (!values || count == 0 || count > 16)
		return -1;

	// tunable 块（chunk 1）：从 kTunableBaseAddress 起逐格读
	const uint32_t chunkIndex = (TunableTable::kTunableBaseAddress >> 18) & 0x3F;
	const uintptr_t slot = DMA::BaseAddress + Offsets::GlobalPtr + chunkIndex * sizeof(uintptr_t);
	uintptr_t chunkBase = 0;
	if (!DMA::Memory().Read(slot, &chunkBase, sizeof(chunkBase)) || !chunkBase)
		return -1;

	const int32_t first = values[0];
	int64_t firstMatch = -1;
	for (uint32_t element = 1; element + count <= 36796; ++element)
	{
		int32_t candidate = 0;
		if (!DMA::Memory().Read(chunkBase + static_cast<uintptr_t>(element) * 8, &candidate, sizeof(candidate)))
			continue;
		if (candidate != first)
			continue;

		bool allMatch = true;
		for (uint32_t k = 1; k < count; ++k)
		{
			int32_t next = 0;
			if (!DMA::Memory().Read(chunkBase + static_cast<uintptr_t>(element + k) * 8, &next, sizeof(next)) ||
			    next != values[k])
			{
				allMatch = false;
				break;
			}
		}
		if (allMatch)
		{
			if (occurrencesOut)
				*occurrencesOut += 1;
			if (firstMatch < 0)
				firstMatch = static_cast<int64_t>((static_cast<uint32_t>(chunkIndex) << 18) | element);
		}
	}
	return firstMatch;
}

uint32_t RuntimeTables::GetPatternOverrides(const char* offsetName, PatternOverride* out, uint32_t maxCount)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	uint32_t written = 0;
	if (const auto it = g_patternOverrides.find(offsetName); it != g_patternOverrides.end())
	{
		for (const auto& item : it->second)
		{
			if (written >= maxCount)
				break;
			out[written++] = item;
		}
	}
	return written;
}

const char* RuntimeTables::GetBinPath()
{
	return g_binPath.c_str();
}

const char* RuntimeTables::GetBinStatus()
{
	return g_binStatus.c_str();
}

const char* RuntimeTables::GetTunablesFilePath()
{
	return g_tunablesFilePath.c_str();
}

const char* RuntimeTables::GetPatternsFilePath()
{
	return g_patternsFilePath.c_str();
}

uint32_t RuntimeTables::GetBinEntryCount()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return static_cast<uint32_t>(g_binByHash.size());
}

uint32_t RuntimeTables::GetOverrideEntryCount()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return static_cast<uint32_t>(g_overrides.size());
}

uint32_t RuntimeTables::GetPatternOverrideCount()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	return g_patternOverrideCount;
}

std::string RuntimeTables::GetReport()
{
	std::lock_guard<std::mutex> lock(g_mutex);
	std::ostringstream out;
	out << "  设计：名字/值稳定 → 运行时重新解析索引，编译期表只当兜底种子\n";
	out << "  tunables.bin   : " << (g_binPath.empty() ? "(未设置)" : g_binPath) << "  → " << g_binStatus << "\n";
	out << "  外部覆盖文件   : " << g_tunablesFilePath << "（已生效 " << g_overrides.size() << " 条）\n";
	out << "  特征码追加文件 : " << g_patternsFilePath << "（已生效 " << g_patternOverrideCount << " 条）\n";
	out << "  解析优先级     : 外部覆盖文件 → tunables.bin → 值序列自发现 → 编译种子\n";
	return out.str();
}
