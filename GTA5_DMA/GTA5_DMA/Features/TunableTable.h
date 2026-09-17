#pragma once

// ============================================================================
//  TunableTable.h —— 由 tools/gen_tunables.py 从 YimMenuV2 的 tunables.bin 生成，请勿手改。
//
//  脚本 tunable 住在脚本全局数组里：起点 TUNABLE_BASE_ADDRESS = 0x40001，每项一个 8 字节单元。
//  下表是「joaat(名字) → 全局索引」的固化映射（已在本机实机读值对账，见 _uiwork/round16_verification.txt）。
//
//  来源: C:\Users\Administrator\AppData\Roaming\YimMenuV2\tunables.bin
//        cacheVersion=0 fileVersion=0x6A4F97F6 dataSize=294372 条目数=36796
//
//  expectedDefault 是「没被动过手脚时的原始值」，解析时用它体检：
//  读出来既不是默认值、也不是本工具写过的值 → 判定索引对错位置，拒绝写入。
// ============================================================================

#include <cstdint>

namespace TunableTable
{
	inline constexpr uint32_t kTunableBaseAddress = 0x40001;   // 脚本 tunable 块起点（全局索引）

	enum class Kind
	{
		Int,          // 精确比对 expectedDefault
		Float,        // 按位比对 expectedDefault 的 IEEE 位型
		Value,        // 金额/额度类：只做区间体检 [minValue, maxValue]
	};

	struct Entry
	{
		const char* name;        // tunable 名（joaat 源串，工具/日志用）
		const char* label;       // 中文短名（UI 显示用；用户看不懂英文原名）
		uint32_t    hash;        // joaat(name)
		uint32_t    globalIndex; // 脚本全局索引（= 0x40001 + 块内序号）
		Kind        kind;
		int32_t     expectedDefault;   // Int/Float 用；Value 型不看它
		int32_t     minValue;          // Value 型：合法值下界（含）
		int32_t     maxValue;          // Value 型：合法值上界（含）
		int32_t     altValue;          // 额外接受的值（0 = 不启用）；如踢出计时的 INT_MAX 禁用态
		int8_t      runId;             // 值序列锚分组（-1 = 无）；索引漂移时靠它自发现
		int8_t      runOffset;         // 在本组里的偏移
		const char* purpose;
	};

	// 值序列锚：一组已知值在 tunable 块里连续出现 → 反推整组索引（不依赖编译期索引）
	inline constexpr int32_t kRunIdleKick[] = { 120000, 300000, 600000, 900000 };   // 空闲踢出四连（120000/300000/600000/900000）
	inline constexpr int32_t kRunConstrainedKick[] = { 30000, 60000, 90000, 120000 };   // 受限踢出四连（30000/60000/90000/120000）
	inline constexpr int32_t kRunAppearance[] = { 100000, 2880000 };   // 改外貌收费/冷却（100000/2880000）
	inline constexpr int32_t kRunHeistPrimaryValue[] = { 400000, 560000, 616000, 910000, 1100000, 1900000 };   // 佩里克主目标价值六连（400000/560000/616000/910000/1100000/1900000）
	inline constexpr int32_t kRunSkydiveReward[] = { 2000, 2000, 1000 };   // 跳伞挑战奖励（2000/2000/1000）

	struct ValueRun
	{
		const int32_t* values;
		uint32_t       count;
	};

	inline constexpr ValueRun kRuns[] = { { kRunIdleKick, 4 }, { kRunConstrainedKick, 4 }, { kRunAppearance, 2 }, { kRunHeistPrimaryValue, 6 }, { kRunSkydiveReward, 3 } };
	inline constexpr int32_t kRunCount = static_cast<int32_t>(sizeof(kRuns) / sizeof(kRuns[0]));

	inline constexpr Entry kEntries[] = {
		{ "IDLEKICK_WARNING1", "空闲踢出警告 1", 0xB3A4D684u, 0x40055u, Kind::Int, 120000, 0, 0, 2147483647, 0, 0, "空闲踢出：第 1 次警告" },
		{ "IDLEKICK_WARNING2", "空闲踢出警告 2", 0xC16F7219u, 0x40056u, Kind::Int, 300000, 0, 0, 2147483647, 0, 1, "空闲踢出：第 2 次警告" },
		{ "IDLEKICK_WARNING3", "空闲踢出警告 3", 0xE009AF49u, 0x40057u, Kind::Int, 600000, 0, 0, 2147483647, 0, 2, "空闲踢出：第 3 次警告" },
		{ "IDLEKICK_KICK", "空闲踢出计时", 0x949C8AADu, 0x40058u, Kind::Int, 900000, 0, 0, 2147483647, 0, 3, "空闲踢出：踢出计时" },
		{ "ConstrainedKick_Warning1", "受限踢出警告 1", 0x1A768E30u, 0x42131u, Kind::Int, 30000, 0, 0, 2147483647, 1, 0, "受限踢出：第 1 次警告" },
		{ "ConstrainedKick_Warning2", "受限踢出警告 2", 0x28C02AC3u, 0x42132u, Kind::Int, 60000, 0, 0, 2147483647, 1, 1, "受限踢出：第 2 次警告" },
		{ "ConstrainedKick_Warning3", "受限踢出警告 3", 0xBE1A5575u, 0x42133u, Kind::Int, 90000, 0, 0, 2147483647, 1, 2, "受限踢出：第 3 次警告" },
		{ "ConstrainedKick_Kick", "受限踢出计时", 0x1245EB8Au, 0x42134u, Kind::Int, 120000, 0, 0, 2147483647, 1, 3, "受限踢出：踢出计时" },
		{ "XP_MULTIPLIER", "RP 经验倍率", 0xB6E46AB9u, 0x40002u, Kind::Float, 1065353216 /* 1.0f */, 0, 0, 0, -1, -1, "RP 倍率（写 >1 放大收益）" },
		{ "CHARACTER_APPEARANCE_COOLDOWN", "改外貌冷却", 0x5C0C54C5u, 0x44A45u, Kind::Int, 2880000, 0, 0, 0, 2, 1, "改外貌冷却（写 0 免冷却）" },
		{ "CHARACTER_APPEARANCE_CHARGE", "改外貌收费", 0x5B6092F1u, 0x44A44u, Kind::Int, 100000, 0, 0, 0, 2, 0, "改外貌收费（写 0 免费）" },
		{ "IH_PRIMARY_TARGET_VALUE_TEQUILA", "佩里克：龙舌兰", 0x07EAAECBu, 0x47392u, Kind::Value, 0, 1000, 100000000, 0, 3, 0, "佩里克主目标价值：龙舌兰（提高 = 抢劫收益更高）" },
		{ "IH_PRIMARY_TARGET_VALUE_PEARL_NECKLACE", "佩里克：珍珠项链", 0xCE301261u, 0x47393u, Kind::Value, 0, 1000, 100000000, 0, 3, 1, "佩里克主目标价值：珍珠项链" },
		{ "IH_PRIMARY_TARGET_VALUE_BEARER_BONDS", "佩里克：不记名债券", 0x9916CE10u, 0x47394u, Kind::Value, 0, 1000, 100000000, 0, 3, 2, "佩里克主目标价值：不记名债券" },
		{ "IH_PRIMARY_TARGET_VALUE_PINK_DIAMOND", "佩里克：粉钻", 0x6FBEC6FFu, 0x47395u, Kind::Value, 0, 1000, 100000000, 0, 3, 3, "佩里克主目标价值：粉钻" },
		{ "IH_PRIMARY_TARGET_VALUE_MADRAZO_FILES", "佩里克：Madrazo 文件", 0x7FB9C812u, 0x47396u, Kind::Value, 0, 1000, 100000000, 0, 3, 4, "佩里克主目标价值：Madrazo 文件" },
		{ "IH_PRIMARY_TARGET_VALUE_SAPPHIRE_PANTHER_STATUE", "佩里克：蓝宝石黑豹雕像", 0x0FE77A52u, 0x47397u, Kind::Value, 0, 1000, 100000000, 0, 3, 5, "佩里克主目标价值：蓝宝石黑豹雕像" },
		{ "SKYDIVING_CHALLENGE_CASH_REWARD_ALL_CHECKPOINTS_COLLECTED", "跳伞奖励：全检查点", 0x37DF02B0u, 0x47F1Fu, Kind::Value, 0, 100, 10000000, 0, 4, 0, "跳伞挑战奖励：全检查点" },
		{ "SKYDIVING_CHALLENGE_CASH_REWARD_PAR_TIME", "跳伞奖励：达标时间", 0x02C60A60u, 0x47F20u, Kind::Value, 0, 100, 10000000, 0, 4, 1, "跳伞挑战奖励：达标时间" },
		{ "SKYDIVING_CHALLENGE_CASH_REWARD_ACCURATE_LANDING", "跳伞奖励：精准落地", 0x1A99881Cu, 0x47F21u, Kind::Value, 0, 100, 10000000, 0, 4, 2, "跳伞挑战奖励：精准落地" },
	};

	inline constexpr uint32_t kEntryCount = static_cast<uint32_t>(sizeof(kEntries) / sizeof(kEntries[0]));

	// 运行期闸门与离线单测共用的判定：当前值是否是「游戏自己放回来的合法状态」
	//   · Value 型：落在登记区间内
	//   · Int/Float 型：等于表内已知默认值（换战局/刷新 tunables 时游戏会重置回默认）
	//   · 或等于该条登记的可接受替代值（altValue，如踢出计时的 INT_MAX 禁用态）
	inline bool IsLegitValue(const Entry& entry, int32_t bits)
	{
		if (entry.kind == Kind::Value)
			return bits >= entry.minValue && bits <= entry.maxValue;
		if (bits == entry.expectedDefault)
			return true;
		if (entry.altValue != 0 && bits == entry.altValue)
			return true;
		return false;
	}

	// 编译期钉死：索引一旦漂移（游戏更新/表生成错），这里先报错，而不是运行期写错格子
	static_assert(kEntryCount == 20, "tunable table size changed");
	static_assert(kEntries[0].globalIndex == 0x40055u, "IDLEKICK_WARNING1 index drifted");
	static_assert(kEntries[8].globalIndex == 0x40002u, "XP_MULTIPLIER index drifted");
	static_assert(kEntries[10].globalIndex == 0x44A44u, "CHARACTER_APPEARANCE_CHARGE index drifted");
	static_assert(kEntries[11].globalIndex == 0x47392u, "IH_PRIMARY_TARGET_VALUE_TEQUILA index drifted");
	static_assert(kEntries[16].globalIndex == 0x47397u, "IH_PRIMARY_TARGET_VALUE_SAPPHIRE_PANTHER_STATUE index drifted");
}
