"""Scan the UI/feature sources for every non-ASCII character used in string literals
and emit UI/GlyphRanges.h — an exact ImWchar set so CJK text never loses a glyph,
while keeping the font atlas small enough for the 4096x4096 limit.

Usage: python tools/gen_glyph_ranges.py
"""
import pathlib
import re

root = pathlib.Path(__file__).resolve().parent.parent
out = root / "UI" / "GlyphRanges.h"

sources = sorted(list(root.glob("**/*.cpp")) + list(root.glob("**/*.h")))
chars = set()
skipped = []
scanned = 0
for path in sources:
    if path.name in ("GlyphRanges.h", "EmbeddedAssets.h"):
        continue
    try:
        text = path.read_text(encoding="utf-8")
    except (UnicodeDecodeError, OSError):
        continue
    scanned += 1
    for ch in text:
        cp = ord(ch)
        if cp > 0x7E:            # 只收集非 ASCII（ASCII 由基础区间覆盖）
            if cp > 0xFFFF:
                # ImWchar 是 16 位，非 BMP 码点会被截断（emoji 在 msyh 里也没有字形，
                # 放进区间只会污染图集），因此跳过并警告。
                skipped.append((str(path.relative_to(root)), cp))
                continue
            chars.add(cp)

# 基础区间：ASCII + 常用标点 + 全角标点 + 常被 UI 用到的符号块
ranges = [(0x0020, 0x007E), (0x00A0, 0x00FF), (0x2000, 0x206F), (0x2190, 0x21FF),
          (0x2200, 0x22FF), (0x2460, 0x24FF), (0x2500, 0x257F), (0x25A0, 0x25FF),
          (0x2600, 0x26FF), (0x3000, 0x303F), (0xFF00, 0xFFEF)]
covered = set()
for lo, hi in ranges:
    covered.update(range(lo, hi + 1))

extra = sorted(cp for cp in chars if cp not in covered)

# 把连续码点合并成区间，减小数组长度
merged = []
for cp in extra:
    if merged and cp == merged[-1][1] + 1:
        merged[-1][1] = cp
    else:
        merged.append([cp, cp])
ranges = sorted(ranges + [(lo, hi) for lo, hi in merged])

rows = []
row = []
for lo, hi in ranges:
    for v in (lo, hi):
        row.append(f"0x{v:04X},")
        if len(row) == 8:
            rows.append("    " + " ".join(row))
            row = []
if row:
    rows.append("    " + " ".join(row))

body = "\n".join(rows)
header = f"""#pragma once

// 由 tools/gen_glyph_ranges.py 生成，请勿手改。
// 说明：正文/强调字体使用 ImGui 的「中文全字形」，字号较小的两套字体若也用全字形，
// 5 套字体会撑爆 4096x4096 字体图集（后面的字号整段丢字形）。这里用源码里实际
// 出现过的全部字符 + 常用符号块，生成精确区间，既保证字形完整又保持图集紧凑。
#include "imgui.h"

namespace glyph_ranges
{{
// 覆盖 {len(ranges)} 个区间（含源码中出现的 {len(extra)} 个非 ASCII 码点）
inline constexpr ImWchar ui[] = {{
{body}
    0x0000,
}};
}} // namespace glyph_ranges
"""
out.write_text(header, encoding="utf-8")
print(f"scanned {scanned} files | non-ascii codepoints: {len(chars)} | extra ranges: {len(merged)}")
if skipped:
    print(f"WARNING: skipped {len(skipped)} non-BMP codepoint(s):",
          ", ".join(f"U+{cp:04X} in {f}" for f, cp in skipped[:8]))
print("wrote", out, f"{out.stat().st_size} bytes")
