#pragma once

// ============================================================================
//  ScriptGlobalsTable —— 脚本全局「动作/标志」登记表（纯数据，供应用与测试共用）
//
//  与 TunableTable 的区别：
//    · tunable 是**持续值**（开着就一直写，游戏读它当参数）
//    · 这里登记的是**动作/状态格**（写 1 让游戏消费、写状态码改变行为），所以写入模型是「脉冲」
//  索引来源逐条对照 YimMenuV2（参考项目仅作实现参考）：
//    game/features/recovery/ClaimSafeEarnings.cpp   2708943 / 2708952 / 2708961 / 2708970
//                                                  / 2708979 / 2708994 / 2709001
//    game/features/network/SilencePhonecalls.cpp    23040(状态) / 23046(通话中) / 23050(来电)
//    game/features/recovery/UnlockGTAPlus.cpp       1970586 / 1970586.At(3) = +3
//
//  每条都带**合法值区间**：解析时读到的值落在区间外 → 判为「未定位」，绝不写入
//  （防止把值写进没分配/已换布局的格子里）。
// ============================================================================

#include <cstdint>

namespace ScriptGlobalTable
{
	enum class Kind : std::uint8_t
	{
		Flag,      // 0 / 1 标志
		State,     // 小范围状态码（min~max）
		Bits,      // 位集合（0 ~ 0xFFFF）
	};

	struct Entry
	{
		const char* name;
		std::uint32_t index;      // 脚本全局索引
		Kind          kind;
		std::int32_t  minValue;   // 合法值下界（含）
		std::int32_t  maxValue;   // 合法值上界（含）
		const char*   purpose;
	};

	// 保险箱：写 1 让游戏结算该产业保险箱收益（先读 GPBD 判断有没有钱是为省事，游戏也会自己判断）
	inline constexpr Entry kEntries[] = {
		{"SAFE_CLAIM_NIGHTCLUB",    2708943u, Kind::Flag,  0, 1,      "夜总会保险箱领取"},
		{"SAFE_CLAIM_ARCADE",       2708952u, Kind::Flag,  0, 1,      "游戏厅保险箱领取"},
		{"SAFE_CLAIM_AGENCY",       2708961u, Kind::Flag,  0, 1,      "事务所保险箱领取"},
		{"SAFE_CLAIM_SALVAGE_YARD", 2708970u, Kind::Flag,  0, 1,      "车场保险箱领取"},
		{"SAFE_CLAIM_BAIL_OFFICE",  2708979u, Kind::Flag,  0, 1,      "保释所保险箱领取"},
		{"SAFE_CLAIM_GARMENT",      2708994u, Kind::Flag,  0, 1,      "服装厂保险箱领取"},
		{"SAFE_CLAIM_CAR_WASH",     2709001u, Kind::Flag,  0, 1,      "洗车行保险箱领取"},

		{"PHONE_CALL_STATE",        23040u,   Kind::State, 0, 8,      "来电状态（写 6 = 直接静音）"},
		{"PHONE_CALL_IN_PROGRESS",  23046u,   Kind::Flag,  0, 1,      "通话中标志（只读判断）"},
		{"PHONE_CALL_INCOMING",     23050u,   Kind::Flag,  0, 1,      "来电标志（只读判断）"},

		{"GTA_PLUS_ENABLED",        1970586u, Kind::Flag,  0, 1,      "GTA+ 生效标志"},
		{"GTA_PLUS_BITS",           1970587u, Kind::Bits,  0, 0xFFFF, "GTA+ 权益位（写 0x0A）"},
	};

	inline constexpr std::uint32_t kEntryCount = static_cast<std::uint32_t>(sizeof(kEntries) / sizeof(kEntries[0]));
	inline constexpr std::int32_t  kGtaPlusBitsValue = 0x0A;   // (1<<3)|(1<<1)，与参考实现一致
	inline constexpr std::int32_t  kPhoneSilencedState = 6;    // 参考实现里的「静音」状态码

	// 编译期钉死：索引一旦被改错，这里先报错
	static_assert(kEntryCount == 12, "ScriptGlobalTable entry count changed");
	static_assert(kEntries[0].index == 2708943u && kEntries[6].index == 2709001u, "safe-claim global indices drifted");
	static_assert(kEntries[7].index == 23040u && kEntries[9].index == 23050u, "phone globals drifted");
	static_assert(kEntries[10].index == 1970586u && kEntries[11].index == 1970587u, "GTA+ globals drifted");
	static_assert(kGtaPlusBitsValue == 0x0A, "GTA+ bits value drifted from reference implementation");

	// 保险箱条目区间 [0, 6]，UI 与自动领取都依赖这个范围
	inline constexpr std::uint32_t kFirstSafeEntry = 0;
	inline constexpr std::uint32_t kLastSafeEntry = 6;
	inline constexpr std::uint32_t kSafeEntryCount = 7;
}
