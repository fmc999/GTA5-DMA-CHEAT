#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成 Features/TunableTable.h —— 把 YimMenuV2 的 tunables.bin 映射固化成编译期表。

背景：
  脚本 tunable 全部住在脚本全局数组里，起点 TUNABLE_BASE_ADDRESS = 0x40001，每项一个 8 字节单元。
  YimMenuV2 通过 hook 原生建立「joaat 哈希 → 全局索引」映射并缓存到 tunables.bin：
      CacheHeader{ u32 cacheVersion; u32 fileVersion; u64 dataSize }
      data: u32 count; count × { u32 hash; u32 globalIndex }

本工具只导出本项目**用得到**的那几条（KICK 计时、外貌冷却/收费、XP 倍率），
并把「期望默认值」一并写进表里 —— 解析时用它做体检（读出来的值必须是默认值或已被本工具写过的值，
否则说明索引对应错了地方，绝不盲写）。

用法：
    python GTA5_DMA/GTA5_DMA/tools/gen_tunables.py [--bin <path>] [--out <path>] [--check]
"""
import argparse
import io
import os
import struct
import sys

DEFAULT_BIN = os.path.join(os.environ.get("APPDATA", ""), "YimMenuV2", "tunables.bin")
DEFAULT_OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Features", "TunableTable.h")

# (名字, 类型, 期望默认值, 用途说明)
CURATED = [
    ("IDLEKICK_WARNING1", "int", 120000, "空闲踢出：第 1 次警告", "空闲踢出警告 1"),
    ("IDLEKICK_WARNING2", "int", 300000, "空闲踢出：第 2 次警告", "空闲踢出警告 2"),
    ("IDLEKICK_WARNING3", "int", 600000, "空闲踢出：第 3 次警告", "空闲踢出警告 3"),
    ("IDLEKICK_KICK", "int", 900000, "空闲踢出：踢出计时", "空闲踢出计时"),
    ("ConstrainedKick_Warning1", "int", 30000, "受限踢出：第 1 次警告", "受限踢出警告 1"),
    ("ConstrainedKick_Warning2", "int", 60000, "受限踢出：第 2 次警告", "受限踢出警告 2"),
    ("ConstrainedKick_Warning3", "int", 90000, "受限踢出：第 3 次警告", "受限踢出警告 3"),
    ("ConstrainedKick_Kick", "int", 120000, "受限踢出：踢出计时", "受限踢出计时"),
    ("XP_MULTIPLIER", "float", 1.0, "RP 倍率（写 >1 放大收益）", "RP 经验倍率"),
    ("CHARACTER_APPEARANCE_COOLDOWN", "int", 2880000, "改外貌冷却（写 0 免冷却）", "改外貌冷却"),
    ("CHARACTER_APPEARANCE_CHARGE", "int", 100000, "改外貌收费（写 0 免费）", "改外貌收费"),

    # ---- 第18轮：抢劫 / 经济价值类（金额，用区间体检；名字逐条取自参考项目源码后经 tunables.bin 反查）----
    ("IH_PRIMARY_TARGET_VALUE_TEQUILA", "value", (1000, 100000000), "佩里克主目标价值：龙舌兰（提高 = 抢劫收益更高）", "佩里克：龙舌兰"),
    ("IH_PRIMARY_TARGET_VALUE_PEARL_NECKLACE", "value", (1000, 100000000), "佩里克主目标价值：珍珠项链", "佩里克：珍珠项链"),
    ("IH_PRIMARY_TARGET_VALUE_BEARER_BONDS", "value", (1000, 100000000), "佩里克主目标价值：不记名债券", "佩里克：不记名债券"),
    ("IH_PRIMARY_TARGET_VALUE_PINK_DIAMOND", "value", (1000, 100000000), "佩里克主目标价值：粉钻", "佩里克：粉钻"),
    ("IH_PRIMARY_TARGET_VALUE_MADRAZO_FILES", "value", (1000, 100000000), "佩里克主目标价值：Madrazo 文件", "佩里克：Madrazo 文件"),
    ("IH_PRIMARY_TARGET_VALUE_SAPPHIRE_PANTHER_STATUE", "value", (1000, 100000000), "佩里克主目标价值：蓝宝石黑豹雕像", "佩里克：蓝宝石黑豹雕像"),
    ("SKYDIVING_CHALLENGE_CASH_REWARD_ALL_CHECKPOINTS_COLLECTED", "value", (100, 10000000), "跳伞挑战奖励：全检查点", "跳伞奖励：全检查点"),
    ("SKYDIVING_CHALLENGE_CASH_REWARD_PAR_TIME", "value", (100, 10000000), "跳伞挑战奖励：达标时间", "跳伞奖励：达标时间"),
    ("SKYDIVING_CHALLENGE_CASH_REWARD_ACCURATE_LANDING", "value", (100, 10000000), "跳伞挑战奖励：精准落地", "跳伞奖励：精准落地"),
]

# 值序列锚（第19轮）：这些条目在 tunable 块里**连续排列**，且整组值很独特。
# 游戏更新导致索引漂移时，程序可以靠「这组值连续出现的位置」把索引重新推出来 —— 不用重编译。
GROUPS = [
    ("kRunIdleKick", "空闲踢出四连（120000/300000/600000/900000）",
     [("IDLEKICK_WARNING1", 120000), ("IDLEKICK_WARNING2", 300000),
      ("IDLEKICK_WARNING3", 600000), ("IDLEKICK_KICK", 900000)]),
    ("kRunConstrainedKick", "受限踢出四连（30000/60000/90000/120000）",
     [("ConstrainedKick_Warning1", 30000), ("ConstrainedKick_Warning2", 60000),
      ("ConstrainedKick_Warning3", 90000), ("ConstrainedKick_Kick", 120000)]),
    ("kRunAppearance", "改外貌收费/冷却（100000/2880000）",
     [("CHARACTER_APPEARANCE_CHARGE", 100000), ("CHARACTER_APPEARANCE_COOLDOWN", 2880000)]),
    ("kRunHeistPrimaryValue", "佩里克主目标价值六连（400000/560000/616000/910000/1100000/1900000）",
     [("IH_PRIMARY_TARGET_VALUE_TEQUILA", 400000), ("IH_PRIMARY_TARGET_VALUE_PEARL_NECKLACE", 560000),
      ("IH_PRIMARY_TARGET_VALUE_BEARER_BONDS", 616000), ("IH_PRIMARY_TARGET_VALUE_PINK_DIAMOND", 910000),
      ("IH_PRIMARY_TARGET_VALUE_MADRAZO_FILES", 1100000),
      ("IH_PRIMARY_TARGET_VALUE_SAPPHIRE_PANTHER_STATUE", 1900000)]),
    ("kRunSkydiveReward", "跳伞挑战奖励（2000/2000/1000）",
     [("SKYDIVING_CHALLENGE_CASH_REWARD_ALL_CHECKPOINTS_COLLECTED", 2000),
      ("SKYDIVING_CHALLENGE_CASH_REWARD_PAR_TIME", 2000),
      ("SKYDIVING_CHALLENGE_CASH_REWARD_ACCURATE_LANDING", 1000)]),
]

# 可接受的替代值（第20轮）：这些格子被本工具/别处写成 INT_MAX 表示「已禁用」是**合法状态**，
# 体检时一并接受，否则挂着用一段时间后这些条目会一直显示「未定位」，功能看起来坏了。
ALT_ACCEPT = {
    "IDLEKICK_WARNING1": 2147483647, "IDLEKICK_WARNING2": 2147483647,
    "IDLEKICK_WARNING3": 2147483647, "IDLEKICK_KICK": 2147483647,
    "ConstrainedKick_Warning1": 2147483647, "ConstrainedKick_Warning2": 2147483647,
    "ConstrainedKick_Warning3": 2147483647, "ConstrainedKick_Kick": 2147483647,
}

RUN_LOOKUP = {}
for _run_index, (_run_name, _run_desc, _run_entries) in enumerate(GROUPS):
    for _offset, (_entry_name, _value) in enumerate(_run_entries):
        RUN_LOOKUP[_entry_name] = (_run_index, _offset)


def joaat(text):
    h = 0
    for ch in text.lower().encode("ascii", "ignore"):
        h = (h + ch) & 0xFFFFFFFF
        h = (h + (h << 10)) & 0xFFFFFFFF
        h ^= h >> 6
    h = (h + (h << 3)) & 0xFFFFFFFF
    h ^= h >> 11
    h = (h + (h << 15)) & 0xFFFFFFFF
    return h


def load_bin(path):
    raw = open(path, "rb").read()
    cache_version, file_version, data_size = struct.unpack_from("<IIQ", raw, 0)
    count = struct.unpack_from("<I", raw, 16)[0]
    table = {}
    off = 20
    for _ in range(count):
        hsh, idx = struct.unpack_from("<II", raw, off)
        table[hsh] = idx
        off += 8
    return cache_version, file_version, data_size, count, table


def render(bin_path, meta, rows):
    cv, fv, ds, count = meta
    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// ============================================================================")
    lines.append("//  TunableTable.h —— 由 tools/gen_tunables.py 从 YimMenuV2 的 tunables.bin 生成，请勿手改。")
    lines.append("//")
    lines.append("//  脚本 tunable 住在脚本全局数组里：起点 TUNABLE_BASE_ADDRESS = 0x%X，每项一个 8 字节单元。" % 0x40001)
    lines.append("//  下表是「joaat(名字) → 全局索引」的固化映射（已在本机实机读值对账，见 _uiwork/round16_verification.txt）。")
    lines.append("//")
    lines.append("//  来源: %s" % bin_path)
    lines.append("//        cacheVersion=%d fileVersion=0x%08X dataSize=%d 条目数=%d" % (cv, fv, ds, count))
    lines.append("//")
    lines.append("//  expectedDefault 是「没被动过手脚时的原始值」，解析时用它体检：")
    lines.append("//  读出来既不是默认值、也不是本工具写过的值 → 判定索引对错位置，拒绝写入。")
    lines.append("// ============================================================================")
    lines.append("")
    lines.append("#include <cstdint>")
    lines.append("")
    lines.append("namespace TunableTable")
    lines.append("{")
    lines.append("\tinline constexpr uint32_t kTunableBaseAddress = 0x40001;   // 脚本 tunable 块起点（全局索引）")
    lines.append("")
    lines.append("\tenum class Kind")
    lines.append("\t{")
    lines.append("\t\tInt,          // 精确比对 expectedDefault")
    lines.append("\t\tFloat,        // 按位比对 expectedDefault 的 IEEE 位型")
    lines.append("\t\tValue,        // 金额/额度类：只做区间体检 [minValue, maxValue]")
    lines.append("\t};")
    lines.append("")
    lines.append("\tstruct Entry")
    lines.append("\t{")
    lines.append("\t\tconst char* name;        // tunable 名（joaat 源串，工具/日志用）")
    lines.append("\t\tconst char* label;       // 中文短名（UI 显示用；用户看不懂英文原名）")
    lines.append("\t\tuint32_t    hash;        // joaat(name)")
    lines.append("\t\tuint32_t    globalIndex; // 脚本全局索引（= 0x40001 + 块内序号）")
    lines.append("\t\tKind        kind;")
    lines.append("\t\tint32_t     expectedDefault;   // Int/Float 用；Value 型不看它")
    lines.append("\t\tint32_t     minValue;          // Value 型：合法值下界（含）")
    lines.append("\t\tint32_t     maxValue;          // Value 型：合法值上界（含）")
    lines.append("\t\tint32_t     altValue;          // 额外接受的值（0 = 不启用）；如踢出计时的 INT_MAX 禁用态")
    lines.append("\t\tint8_t      runId;             // 值序列锚分组（-1 = 无）；索引漂移时靠它自发现")
    lines.append("\t\tint8_t      runOffset;         // 在本组里的偏移")
    lines.append("\t\tconst char* purpose;")
    lines.append("\t};")
    lines.append("")
    lines.append("\t// 值序列锚：一组已知值在 tunable 块里连续出现 → 反推整组索引（不依赖编译期索引）")
    for run_name, run_desc, run_entries in GROUPS:
        values = ", ".join(str(v) for _n, v in run_entries)
        lines.append("\tinline constexpr int32_t %s[] = { %s };   // %s" % (run_name, values, run_desc))
    lines.append("")
    lines.append("\tstruct ValueRun")
    lines.append("\t{")
    lines.append("\t\tconst int32_t* values;")
    lines.append("\t\tuint32_t       count;")
    lines.append("\t};")
    lines.append("")
    run_refs = ", ".join("{ %s, %d }" % (rn, len(re)) for rn, _d, re in GROUPS)
    lines.append("\tinline constexpr ValueRun kRuns[] = { %s };" % run_refs)
    lines.append("\tinline constexpr int32_t kRunCount = static_cast<int32_t>(sizeof(kRuns) / sizeof(kRuns[0]));")
    lines.append("")
    lines.append("\tinline constexpr Entry kEntries[] = {")
    for name, kind, default, purpose, label in CURATED:
        if name not in rows:
            continue
        hsh, idx = rows[name]
        run_id, run_offset = RUN_LOOKUP.get(name, (-1, -1))
        if kind == "value":
            lo, hi = default
            lines.append('\t\t{ "%s", "%s", 0x%08Xu, 0x%Xu, Kind::Value, 0, %d, %d, 0, %d, %d, "%s" },'
                         % (name, label, hsh, idx, int(lo), int(hi), run_id, run_offset, purpose))
            continue
        enum_kind = "Kind::Float" if kind == "float" else "Kind::Int"
        if kind == "float":
            bits = struct.unpack("<i", struct.pack("<f", float(default)))[0]
            default_literal = "%d /* %.1ff */" % (bits, float(default))
        else:
            default_literal = str(int(default))
        lines.append('\t\t{ "%s", "%s", 0x%08Xu, 0x%Xu, %s, %s, 0, 0, %d, %d, %d, "%s" },'
                     % (name, label, hsh, idx, enum_kind, default_literal, ALT_ACCEPT.get(name, 0), run_id, run_offset, purpose))
    lines.append("\t};")
    lines.append("")
    lines.append("\tinline constexpr uint32_t kEntryCount = static_cast<uint32_t>(sizeof(kEntries) / sizeof(kEntries[0]));")
    lines.append("")
    lines.append("\t// 编译期钉死：索引一旦漂移（游戏更新/表生成错），这里先报错，而不是运行期写错格子")
    lines.append("\tstatic_assert(kEntryCount == %d, \"tunable table size changed\");" % len([1 for n in CURATED if n[0] in rows]))
    anchors = [
        ("IDLEKICK_WARNING1", 0x40055),
        ("XP_MULTIPLIER", 0x40002),
        ("CHARACTER_APPEARANCE_CHARGE", 0x44A44),
        ("IH_PRIMARY_TARGET_VALUE_TEQUILA", 0x47392),
        ("IH_PRIMARY_TARGET_VALUE_SAPPHIRE_PANTHER_STATUE", 0x47397),
    ]
    for anchor_name, anchor_index in anchors:
        if anchor_name in rows:
            lines.append('\tstatic_assert(kEntries[%d].globalIndex == 0x%Xu, "%s index drifted");'
                         % (next(i for i, (n, k, d, p, lb) in enumerate([e for e in CURATED if e[0] in rows])
                                 if n == anchor_name), anchor_index, anchor_name))
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=DEFAULT_BIN)
    ap.add_argument("--out", default=os.path.normpath(DEFAULT_OUT))
    ap.add_argument("--check", action="store_true", help="只校验 bin 里是否都有这些条目，不写文件")
    args = ap.parse_args()

    if not os.path.exists(args.bin):
        print("tunables.bin not found: %s" % args.bin)
        print("（YimMenuV2 生成一次即可：运行 YimMenuV2 到在线战局，让它缓存 tunables）")
        return 2

    meta = load_bin(args.bin)
    cv, fv, ds, count, table = meta
    print("tunables.bin: cacheVersion=%d fileVersion=0x%08X 条目=%d" % (cv, fv, count))

    rows = {}
    missing = []
    for name, kind, default, purpose, label in CURATED:
        hsh = joaat(name)
        idx = table.get(hsh)
        if idx is None:
            missing.append(name)
            print("  MISSING %-32s joaat=0x%08X" % (name, hsh))
            continue
        rows[name] = (hsh, idx)
        print("  OK      %-32s joaat=0x%08X → 全局索引 0x%X (chunk %d, element %d)" %
              (name, hsh, idx, (idx >> 18) & 0x3F, idx & 0x3FFFF))

    if missing:
        print("\n有 %d 条 tunable 不在 bin 里：%s" % (len(missing), ", ".join(missing)))
        if args.check:
            return 1

    text = render(args.bin, (cv, fv, ds, count), rows)
    if args.check:
        print("\n--check：不写文件（共 %d 条）" % len(rows))
        return 0 if not missing else 1

    io.open(args.out, "w", encoding="utf-8", newline="\n").write(text)
    print("\nwrote %s (%d bytes, %d entries)" % (args.out, len(text.encode("utf-8")), len(rows)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
