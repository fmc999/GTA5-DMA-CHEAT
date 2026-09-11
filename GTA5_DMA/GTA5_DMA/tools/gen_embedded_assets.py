"""Generate UI/EmbeddedAssets.h: wallpaper + frosted copy + logo font embedded as C arrays."""
import io
import pathlib
from PIL import Image, ImageFilter, ImageEnhance

# 用法：python tools/gen_embedded_assets.py（路径相对于本脚本所在工程）
root = pathlib.Path(__file__).resolve().parent.parent
src = root / "assets" / "backdrop.jpg"
out = root / "UI" / "EmbeddedAssets.h"

im = Image.open(src).convert("RGB")
print("source:", im.size)

# 1) 清晰壁纸：只用于窗口背景的 cover 裁剪，1920 宽足够（再大只是浪费体积）
sharp = im.resize((1920, 1080), Image.LANCZOS)
buf = io.BytesIO(); sharp.save(buf, format="JPEG", quality=84, optimize=True, progressive=True)
sharp_bytes = buf.getvalue()

# 2) 毛玻璃副本：本身就是模糊图，低分辨率不会损失观感，体积大幅下降
blur = im.filter(ImageFilter.GaussianBlur(18))
blur = ImageEnhance.Brightness(blur).enhance(0.94).resize((960, 540), Image.LANCZOS)
buf = io.BytesIO(); blur.save(buf, format="JPEG", quality=74, optimize=True)
blur_bytes = buf.getvalue()

font_bytes = (root / "assets" / "ZenDots-Regular.ttf").read_bytes()
print("sharp jpg: %.1f KB | blur jpg: %.1f KB | font: %.1f KB"
      % (len(sharp_bytes) / 1024, len(blur_bytes) / 1024, len(font_bytes) / 1024))


def emit(name, data):
    lines = [f"alignas(4) inline constexpr unsigned char {name}[] = {{"]
    row = []
    for i, b in enumerate(data):
        row.append(f"0x{b:02X},")
        if len(row) == 16:
            lines.append("    " + " ".join(row))
            row = []
    if row:
        lines.append("    " + " ".join(row))
    lines.append("};")
    lines.append(f"inline constexpr std::size_t {name}_size = sizeof({name});")
    return "\n".join(lines)


body = "\n\n".join([
    emit("backdrop_jpg", sharp_bytes),
    emit("backdrop_blur_jpg", blur_bytes),
    emit("logo_ttf", font_bytes),
])

header = f"""#pragma once

// 由 tools/gen_embedded_assets.py 生成，请勿手改。
// 壁纸与字标直接编译进二进制：运行时不依赖任何外部文件路径，
// 单文件 exe 交付即可正常显示毛玻璃背景（Portfolio #8 视觉）。
#include <cstddef>

namespace embedded_assets
{{
{body}
}} // namespace embedded_assets
"""
out.write_text(header, encoding="utf-8")
print("wrote", out, "%.2f MB" % (out.stat().st_size / 1048576))
