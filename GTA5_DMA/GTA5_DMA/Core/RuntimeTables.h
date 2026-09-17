#pragma once

#include <cstdint>
#include <string>
#include <string_view>

// ============================================================================
//  RuntimeTables —— 让 exe 不依赖重编译就能跟上游戏更新
//
//  设计原则：**名字稳定，索引/偏移会漂**。
//    · tunable 的名字（joaat 源串）在游戏更新里基本不变 → 哈希稳定
//    · tunable 的全局索引、特征码地址会随重编译漂移
//  所以本模块在**运行时**把「名字 → 索引」重新算一遍，编译期表只当最后兜底种子：
//
//    ① 外部覆盖文件（exe 同目录 GTA5_DMA_tunables.txt）—— 人工/工具直接改，不用重编译
//    ② YimMenu 风格的 tunables.bin（%APPDATA%\YimMenuV2\tunables.bin，可被覆盖文件改写路径）
//       —— 「joaat(名字) → 全局索引」缓存，游戏更新后由缓存维护方刷新
//    ③ 值序列自发现（四连锚的推广）—— 连缓存都没有时，靠一组已知值在 tunable 块里的
//       连续出现位置反推整组索引。例：抢劫 6 个主目标价值 (400000,560000,616000,910000,
//       1100000,1900000) 连续出现只可能是一处
//    ④ 编译期候选索引（TunableTable.h）—— 全部前三级都失败时的兜底
//
//  特征码同理：OffsetResolver 每条偏移自带 4 个备选；本模块再允许用
//  GTA5_DMA_patterns.txt 追加候选（格式：偏移名 = 特征码字节，可多行），
//  游戏重编译后如果内置候选全失配，贴一行新特征码即可，**仍然不用重编译**。
//
//  安全边界不变：无论索引从哪一级来，落到格子后都必须通过体检（默认值/区间），
//  写入仍然要过「当前值 == 原值或上次写入值 + 写完读回校验」两道闸。
// ============================================================================

class RuntimeTables
{
public:
	// 读外部文件 + tunables.bin；缺文件时写出模板（首次运行就有一份可编辑的）
	static bool Initialize();
	static void Shutdown();

	// 名字 → 全局索引。返回 0 表示三级都查不到（调用方用编译种子兜底）。
	// sourceOut 回填来源字符串（"外部覆盖文件" / "tunables.bin" / "编译种子"）
	static uint32_t ResolveTunableIndex(const char* name, uint32_t joaatHash, uint32_t compiledSeed, const char** sourceOut);

	// 值序列自发现：在 tunable 块（chunk 1）里找 values[] 连续出现的位置，
	// 返回该组**首元素对应的全局索引**；找不到返回 -1。
	// occurrencesOut 回填「整组在块里出现的次数」——唯一出现才最有说服力（调用方据此决定是否采信）。
	static int64_t FindValueRunGlobalIndex(const int32_t* values, uint32_t count, uint32_t* occurrencesOut = nullptr);

	// 外部追加的特征码候选（文件格式支持自带 rel32 位移/指令长度，缺省则沿用主特征码的）
	struct PatternOverride
	{
		std::string pattern;
		std::size_t displacementOffset = 0;
		std::size_t instructionSize = 0;
		bool        hasLayout = false;   // 文件中是否显式写了 disp=/insn=
	};

	// 特征码外部追加候选：把 offsetName 对应文件里追加的候选填进 out，返回条数
	static uint32_t GetPatternOverrides(const char* offsetName, PatternOverride* out, uint32_t maxCount);

	static const char* GetBinPath();
	static const char* GetBinStatus();
	static const char* GetTunablesFilePath();
	static const char* GetPatternsFilePath();
	static uint32_t GetBinEntryCount();
	static uint32_t GetOverrideEntryCount();
	static uint32_t GetPatternOverrideCount();

	// 诊断用摘要（多行）
	static std::string GetReport();

	static uint32_t Joaat(const char* text);
};
