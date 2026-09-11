#include "pch.h"

#include "ConsoleTheme.h"

#include <cstdarg>
#include "AppFonts.h"

#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_map>

namespace
{
ImVec4 V(float r, float g, float b, float a = 1.0f) { return ImVec4(r, g, b, a); }
ImU32 RGBA(int r, int g, int b, float a = 1.0f)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a));
}

template <typename T> T Min(T a, T b) { return a < b ? a : b; }
template <typename T> T Max(T a, T b) { return a >= b ? a : b; }
template <typename T> T Clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
float LerpF(float a, float b, float t) { return a + (b - a) * t; }
float FabsF(float x) { return x < 0.0f ? -x : x; }

ConsoleThemeId g_theme = ConsoleThemeId::Night;
AccentId g_accent = AccentId::Indigo;
ConsoleColors g_colors = {};

// 强调色（与参考实现设置面板的 accent 选项一致）
const ImVec4 kAccents[kAccentCount] = {
    V(0x61 / 255.0f, 0x5D / 255.0f, 0xCE / 255.0f),   // Indigo  #615DCE
    V(0x4F / 255.0f, 0xA3 / 255.0f, 0xED / 255.0f),   // Azure   #4FA3ED
    V(0x18 / 255.0f, 0xC7 / 255.0f, 0x9E / 255.0f),   // Teal    #18C79E
    V(0xE0 / 255.0f, 0x53 / 255.0f, 0x6B / 255.0f),   // Rose    #E0536B
    V(0xF0 / 255.0f, 0xA0 / 255.0f, 0x30 / 255.0f),   // Amber   #F0A030
    V(0xC7 / 255.0f, 0x7D / 255.0f, 0xFF / 255.0f),   // Violet  #C77DFF
};

const char* kAccentNames[kAccentCount] = { "靛蓝", "天蓝", "青碧", "绯玫", "琥珀", "紫罗兰" };

std::unordered_map<std::string, float>& AnimCache()
{
    static std::unordered_map<std::string, float> cache;
    return cache;
}

void RebuildColors()
{
    const ImVec4 accent = kAccents[static_cast<int>(g_accent)];
    g_colors.accent = accent;

    if (g_theme == ConsoleThemeId::Light)
    {
        g_colors.panel        = V(1.0f, 1.0f, 1.0f, 0.55f);
        g_colors.sidebar      = V(1.0f, 1.0f, 1.0f, 0.45f);
        g_colors.box          = V(1.0f, 1.0f, 1.0f, 0.62f);
        g_colors.control      = V(1.0f, 1.0f, 1.0f, 0.80f);
        g_colors.controlHover = V(1.0f, 1.0f, 1.0f, 0.90f);
        g_colors.text         = V(0x14 / 255.0f, 0x14 / 255.0f, 0x1A / 255.0f);
        g_colors.textMuted    = V(0.0f, 0.0f, 0.0f, 0.58f);
        g_colors.headerText   = V(0.0f, 0.0f, 0.0f, 0.40f);
        g_colors.separator    = V(0.0f, 0.0f, 0.0f, 0.12f);
        g_colors.toggleOff    = V(0xC7 / 255.0f, 0xC7 / 255.0f, 0xCC / 255.0f);
        g_colors.toggleOn     = V(0x6E / 255.0f, 0x6E / 255.0f, 0x6E / 255.0f);
        g_colors.knobOff      = V(1.0f, 1.0f, 1.0f);
        g_colors.sliderBg     = V(0xD2 / 255.0f, 0xD2 / 255.0f, 0xD6 / 255.0f);
        g_colors.sliderFill   = V(0x6E / 255.0f, 0x6E / 255.0f, 0x6E / 255.0f);
        g_colors.popupBg      = V(1.0f, 1.0f, 1.0f, 0.88f);
        g_colors.ink          = V(0x14 / 255.0f, 0x14 / 255.0f, 0x1A / 255.0f);
        g_colors.success      = V(0.16f, 0.62f, 0.34f);
        g_colors.warning      = V(0.85f, 0.55f, 0.10f);
        g_colors.danger       = V(0.85f, 0.26f, 0.30f);
    }
    else
    {
        g_colors.panel        = V(0.0f, 0.0f, 0.0f, 0.52f);
        g_colors.sidebar      = V(0.0f, 0.0f, 0.0f, 0.30f);
        g_colors.box          = V(0.0f, 0.0f, 0.0f, 0.58f);
        g_colors.control      = V(0.0f, 0.0f, 0.0f, 0.70f);
        g_colors.controlHover = V(0.0f, 0.0f, 0.0f, 0.82f);
        g_colors.text         = V(1.0f, 1.0f, 1.0f, 1.0f);
        g_colors.textMuted    = V(1.0f, 1.0f, 1.0f, 0.60f);
        g_colors.headerText   = V(1.0f, 1.0f, 1.0f, 0.34f);
        g_colors.separator    = V(1.0f, 1.0f, 1.0f, 0.08f);
        g_colors.toggleOff    = V(0x1A / 255.0f, 0x1A / 255.0f, 0x1A / 255.0f);
        g_colors.toggleOn     = V(0x86 / 255.0f, 0x86 / 255.0f, 0x86 / 255.0f);
        g_colors.knobOff      = V(0x6E / 255.0f, 0x6E / 255.0f, 0x6E / 255.0f);
        g_colors.sliderBg     = V(0x2B / 255.0f, 0x2B / 255.0f, 0x2B / 255.0f);
        g_colors.sliderFill   = V(0x86 / 255.0f, 0x86 / 255.0f, 0x86 / 255.0f);
        g_colors.popupBg      = V(0.0f, 0.0f, 0.0f, 0.72f);
        g_colors.ink          = V(1.0f, 1.0f, 1.0f, 1.0f);
        g_colors.success      = V(0.35f, 0.78f, 0.48f);
        g_colors.warning      = V(0.95f, 0.72f, 0.30f);
        g_colors.danger       = V(0xFF / 255.0f, 0x70 / 255.0f, 0x70 / 255.0f);
    }
}

// 分节标题使用的极小字号
ImFont* SmallFont() { return AppFonts::Small ? AppFonts::Small : AppFonts::Regular; }

void DrawRowLabel(const char* label, const ImVec2& start, float rowHeight, ImU32 color)
{
    if (!label)
        return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = AppFonts::Regular;
    const float size = font ? font->FontSize : ImGui::GetFontSize();
    const float y = start.y + (rowHeight - size) * 0.5f - 1.0f;
    ConsoleTheme::Text(dl, font, ImVec2(start.x, y), color, label);
}

// P8 胶囊开关：42x22 轨道 + r8 滑块
void DrawToggle(const ImVec2& trackMin, float on, float hover)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = layout::toggle_w;
    const float h = layout::toggle_h;
    const ImVec2 max(trackMin.x + w, trackMin.y + h);
    const float round = h * 0.5f;

    const ImVec4 track = ConsoleTheme::MixV(g_colors.toggleOff, g_colors.toggleOn, on);
    dl->AddRectFilled(trackMin, max, ConsoleTheme::U32(track, 1.0f), round);
    if (hover > 0.01f)
        dl->AddRect(trackMin, max, ConsoleTheme::U32(g_colors.ink, 0.10f * hover), round, ImDrawFlags_RoundCornersAll, 1.0f);

    const float centerX = trackMin.x + layout::toggle_inset + on * 20.0f;
    const ImVec2 center(centerX, trackMin.y + round);
    const float r = layout::toggle_knob_r;
    const ImVec4 knob = ConsoleTheme::MixV(g_colors.knobOff, V(1.0f, 1.0f, 1.0f), on);
    dl->AddCircleFilled(ImVec2(center.x, center.y + 0.8f), r, ConsoleTheme::U32(V(0.0f, 0.0f, 0.0f, 1.0f), 0.22f), 24);
    dl->AddCircleFilled(center, r, ConsoleTheme::U32(knob, 1.0f), 24);
}

// P8 滑条：2px 底轨 + 6px 强调填充 + r7 滑块
void DrawSlider(const ImVec2& min, float width, float t, float hover, bool interactive)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float centerY = min.y;
    const float trackH = 3.0f;
    const float fillH = layout::slider_h;
    const ImVec2 a(min.x, centerY - trackH * 0.5f);
    const ImVec2 b(min.x + width, centerY + trackH * 0.5f);
    // 底轨用前景基色的低透明度版本：暗色卡片上依然可见，浅色主题下自动反相
    dl->AddRectFilled(a, b, ConsoleTheme::U32(ConsoleTheme::MixV(g_colors.sliderBg, ConsoleTheme::Ink(0.16f), 0.85f), 1.0f), trackH * 0.5f);

    const float fillW = width * t;
    if (fillW > 0.5f)
    {
        dl->AddRectFilled(ImVec2(min.x, centerY - fillH * 0.5f), ImVec2(min.x + fillW, centerY + fillH * 0.5f),
                          ConsoleTheme::U32(g_colors.accent, 1.0f), fillH * 0.5f);
    }
    const float knobX = min.x + fillW;
    const float r = 7.0f + hover * 1.0f;
    dl->AddCircleFilled(ImVec2(knobX, centerY + 0.8f), r, ConsoleTheme::U32(V(0.0f, 0.0f, 0.0f, 1.0f), 0.25f), 24);
    dl->AddCircleFilled(ImVec2(knobX, centerY), r, ConsoleTheme::U32(V(1.0f, 1.0f, 1.0f), interactive ? 1.0f : 0.92f), 24);
    dl->AddCircle(ImVec2(knobX, centerY), r, ConsoleTheme::U32(g_colors.accent, 0.9f), 24, 2.0f);
}

// 行首：返回 true 表示本行被点击；startOut/hoveredOut 带回行几何与悬停状态
struct RowState
{
    ImVec2 start = ImVec2(0.0f, 0.0f);
    float  width = 0.0f;
    bool   clicked = false;
    bool   hovered = false;
};

// 盒子内可用行宽：BoxBeginPixels 进入子窗口时写入，行助手统一读取。
// 不再依赖 BeginChild 的 WindowPadding（实测在子窗口里不可靠：内容会贴到
// 盒子上下左右边框，数值/开关顶到描边上），改由盒子显式给出内边距后的宽度。
float g_boxInnerWidth = 0.0f;

void BeginRow(const char* id, bool separator, RowState& out)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ConsoleTheme::RowWidth();
    ImGui::InvisibleButton(id, ImVec2(width, layout::row_h));

    out.start = start;
    out.width = width;
    out.clicked = ImGui::IsItemClicked();
    out.hovered = ImGui::IsItemHovered();

    if (separator)
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddLine(ImVec2(start.x, start.y + layout::row_h + 0.5f),
                    ImVec2(start.x + width, start.y + layout::row_h + 0.5f),
                    ConsoleTheme::U32(g_colors.separator, 1.0f), 1.0f);
    }

    // 关键：显式把光标落到下一行的精确位置，绝不依赖 ImGui 的自动布局。
    // 自动布局下每个 item 之后都会再叠加一次 style.ItemSpacing.y；实测它把
    // 37px 的行撑成 58px，导致布局常量 box_height() 与实际渲染高度不符：
    // 盒内最后 1~2 行被画到盒外裁掉、上下盒互相压盖。此处把行距钉死为
    // row_h(+1px 分隔线)，与 layout::box_height() 完全一致，错位不再出现。
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + layout::row_h + (separator ? layout::separator_h : 0.0f)));
}
} // namespace

float ConsoleTheme::RowWidth()
{
    return g_boxInnerWidth > 1.0f ? g_boxInnerWidth : ImGui::GetContentRegionAvail().x;
}

/* ---------- 主题 / 强调色 ---------- */

void ConsoleTheme::Apply()
{
    RebuildColors();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding    = ImVec2(0.0f, 0.0f);
    style.FramePadding     = ImVec2(10.0f, 5.0f);
    style.CellPadding      = ImVec2(10.0f, 7.0f);
    style.ItemSpacing      = ImVec2(8.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.IndentSpacing    = 16.0f;
    style.ScrollbarSize    = 10.0f;
    style.GrabMinSize      = 10.0f;

    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = layout::control_round;
    style.PopupRounding     = 12.0f;
    style.GrabRounding      = layout::control_round;
    style.TabRounding       = 6.0f;
    style.ScrollbarRounding = 6.0f;

    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 0.0f;
    style.PopupBorderSize  = 0.0f;
    style.FrameBorderSize  = 0.0f;
    style.TabBarBorderSize = 0.0f;

    style.AntiAliasedLines    = true;
    style.AntiAliasedFill     = true;
    style.WindowTitleAlign    = ImVec2(0.0f, 0.5f);
    style.ButtonTextAlign     = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                   = g_colors.text;
    c[ImGuiCol_TextDisabled]           = g_colors.textMuted;
    c[ImGuiCol_WindowBg]               = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_ChildBg]                = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_PopupBg]                = g_colors.popupBg;
    c[ImGuiCol_Border]                 = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_BorderShadow]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_FrameBg]                = g_colors.control;
    c[ImGuiCol_FrameBgHovered]         = g_colors.controlHover;
    c[ImGuiCol_FrameBgActive]          = g_colors.controlHover;
    c[ImGuiCol_TitleBg]                = g_colors.control;
    c[ImGuiCol_TitleBgActive]          = g_colors.control;
    c[ImGuiCol_TitleBgCollapsed]       = g_colors.control;
    c[ImGuiCol_MenuBarBg]              = g_colors.control;
    c[ImGuiCol_CheckMark]              = g_colors.accent;
    c[ImGuiCol_SliderGrab]             = g_colors.accent;
    c[ImGuiCol_SliderGrabActive]       = g_colors.accent;
    c[ImGuiCol_Button]                 = g_colors.control;
    c[ImGuiCol_ButtonHovered]          = g_colors.controlHover;
    c[ImGuiCol_ButtonActive]           = g_colors.controlHover;
    c[ImGuiCol_Header]                 = g_colors.control;
    c[ImGuiCol_HeaderHovered]          = g_colors.controlHover;
    c[ImGuiCol_HeaderActive]           = g_colors.controlHover;
    c[ImGuiCol_Separator]              = g_colors.separator;
    c[ImGuiCol_SeparatorHovered]       = g_colors.accent;
    c[ImGuiCol_SeparatorActive]        = g_colors.accent;
    c[ImGuiCol_ResizeGrip]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_ResizeGripHovered]      = V(g_colors.accent.x, g_colors.accent.y, g_colors.accent.z, 0.35f);
    c[ImGuiCol_ResizeGripActive]       = V(g_colors.accent.x, g_colors.accent.y, g_colors.accent.z, 0.60f);
    c[ImGuiCol_Tab]                    = g_colors.control;
    c[ImGuiCol_TabHovered]             = g_colors.controlHover;
    c[ImGuiCol_TabActive]              = MixV(g_colors.control, g_colors.accent, 0.35f);
    c[ImGuiCol_TabUnfocused]           = g_colors.control;
    c[ImGuiCol_TabUnfocusedActive]     = MixV(g_colors.control, g_colors.accent, 0.20f);
    c[ImGuiCol_TabSelected]            = MixV(g_colors.control, g_colors.accent, 0.35f);
    c[ImGuiCol_TabDimmed]              = g_colors.control;
    c[ImGuiCol_TabDimmedSelected]      = MixV(g_colors.control, g_colors.accent, 0.20f);
    // 悬浮窗化以后工作区经常比内容矮，滚动条必须是**看得见**的，
    // 否则卡片被切在底部时会被当成「错位」（原来 alpha=0 + Ink(0.14) 在玻璃上等于隐形）。
    c[ImGuiCol_ScrollbarBg]            = ImVec4(0.0f, 0.0f, 0.0f, 0.28f);
    c[ImGuiCol_ScrollbarGrab]          = Ink(0.30f);
    c[ImGuiCol_ScrollbarGrabHovered]   = Ink(0.40f);
    c[ImGuiCol_ScrollbarGrabActive]    = V(g_colors.accent.x, g_colors.accent.y, g_colors.accent.z, 0.75f);
    c[ImGuiCol_TableHeaderBg]          = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TableRowBg]             = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TableRowBgAlt]          = Wash(0.022f);
    c[ImGuiCol_TableBorderStrong]      = g_colors.separator;
    c[ImGuiCol_TableBorderLight]       = g_colors.separator;
    c[ImGuiCol_NavHighlight]           = g_colors.accent;
    c[ImGuiCol_NavWindowingHighlight]  = g_colors.accent;
    c[ImGuiCol_NavWindowingDimBg]      = V(0.0f, 0.0f, 0.0f, 0.45f);
    c[ImGuiCol_ModalWindowDimBg]       = V(0.0f, 0.0f, 0.0f, 0.60f);
    c[ImGuiCol_TextSelectedBg]         = V(g_colors.accent.x, g_colors.accent.y, g_colors.accent.z, 0.35f);
    c[ImGuiCol_PlotLines]              = g_colors.accent;
    c[ImGuiCol_PlotLinesHovered]       = g_colors.accent;
    c[ImGuiCol_PlotHistogram]          = g_colors.accent;
    c[ImGuiCol_PlotHistogramHovered]   = g_colors.accent;
    c[ImGuiCol_DragDropTarget]         = g_colors.accent;
}

void ConsoleTheme::SetTheme(ConsoleThemeId id)
{
    g_theme = id;
    RebuildColors();
}

ConsoleThemeId ConsoleTheme::GetTheme() { return g_theme; }
int ConsoleTheme::ThemeCount() { return kConsoleThemeCount; }

const char* ConsoleTheme::ThemeName(ConsoleThemeId id)
{
    return id == ConsoleThemeId::Light ? "晨光玻璃" : "夜色玻璃";
}

void ConsoleTheme::SetAccent(AccentId id)
{
    int index = static_cast<int>(id);
    if (index < 0 || index >= kAccentCount)
        index = 0;
    g_accent = static_cast<AccentId>(index);
    RebuildColors();
}

AccentId ConsoleTheme::GetAccent() { return g_accent; }
int ConsoleTheme::AccentCount() { return kAccentCount; }
const char* ConsoleTheme::AccentName(AccentId id)
{
    int index = static_cast<int>(id);
    if (index < 0 || index >= kAccentCount)
        index = 0;
    return kAccentNames[index];
}
ImVec4 ConsoleTheme::AccentColor(AccentId id)
{
    int index = static_cast<int>(id);
    if (index < 0 || index >= kAccentCount)
        index = 0;
    return kAccents[index];
}

/* ---------- 配色访问 ---------- */

const ConsoleColors& ConsoleTheme::Colors() { return g_colors; }
ImVec4 ConsoleTheme::Accent() { return g_colors.accent; }
ImVec4 ConsoleTheme::AccentAlt() { return MixV(g_colors.accent, V(1.0f, 1.0f, 1.0f), g_theme == ConsoleThemeId::Light ? 0.1f : 0.35f); }
ImVec4 ConsoleTheme::Panel() { return g_colors.panel; }
ImVec4 ConsoleTheme::Sidebar() { return g_colors.sidebar; }
ImVec4 ConsoleTheme::Box() { return g_colors.box; }
ImVec4 ConsoleTheme::Control() { return g_colors.control; }
ImVec4 ConsoleTheme::ControlHover() { return g_colors.controlHover; }
ImVec4 ConsoleTheme::TextColor() { return g_colors.text; }
ImVec4 ConsoleTheme::TextMuted() { return g_colors.textMuted; }
ImVec4 ConsoleTheme::HeaderText() { return g_colors.headerText; }
ImVec4 ConsoleTheme::SeparatorColor() { return g_colors.separator; }
ImVec4 ConsoleTheme::Success() { return g_colors.success; }
ImVec4 ConsoleTheme::Warning() { return g_colors.warning; }
ImVec4 ConsoleTheme::Danger() { return g_colors.danger; }
ImVec4 ConsoleTheme::Background() { return V(0.05f, 0.05f, 0.06f, 1.0f); }
ImVec4 ConsoleTheme::BackgroundTop() { return V(0.09f, 0.09f, 0.11f, 1.0f); }
ImVec4 ConsoleTheme::BackgroundDeep() { return V(0.0f, 0.0f, 0.0f, 0.45f); }
ImVec4 ConsoleTheme::Surface() { return g_colors.box; }
ImVec4 ConsoleTheme::SurfaceAlt() { return g_colors.control; }
ImVec4 ConsoleTheme::BorderColor() { return g_colors.separator; }
ImVec4 ConsoleTheme::Ink(float alpha) { return V(g_colors.ink.x, g_colors.ink.y, g_colors.ink.z, alpha); }
ImVec4 ConsoleTheme::Wash(float alpha) { return V(g_colors.ink.x, g_colors.ink.y, g_colors.ink.z, alpha); }

ImU32 ConsoleTheme::U32(const ImVec4& color, float alpha)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, color.w * alpha));
}

ImVec4 ConsoleTheme::MixV(const ImVec4& a, const ImVec4& b, float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

ImU32 ConsoleTheme::MixU32(ImU32 a, ImU32 b, float t)
{
    return U32(MixV(ImGui::ColorConvertU32ToFloat4(a), ImGui::ColorConvertU32ToFloat4(b), t));
}

float ConsoleTheme::Anim(const char* key, float target, float speed)
{
    auto& cache = AnimCache();
    auto it = cache.find(key);
    if (it == cache.end())
        return cache.emplace(key, target).first->second;

    float dt = ImGui::GetIO().DeltaTime;
    if (!(dt > 0.0f) || dt > 0.25f)
        dt = 1.0f / 60.0f;
    it->second = LerpF(it->second, target, Min(1.0f, dt * speed));
    if (FabsF(it->second - target) < 0.001f)
        it->second = target;
    return it->second;
}

/* ---------- 绘制原语 ---------- */

void ConsoleTheme::Text(ImDrawList* drawList, ImFont* font, ImVec2 pos, ImU32 color, const char* text)
{
    if (!text || text[0] == '\0')
        return;
    if (font)
        drawList->AddText(font, font->FontSize, pos, color, text);
    else
        drawList->AddText(pos, color, text);
}

void ConsoleTheme::TextCentered(ImDrawList* drawList, ImFont* font, const ImVec2& min, const ImVec2& max, ImU32 color, const char* text)
{
    if (!text || text[0] == '\0' || !font)
        return;
    const ImVec2 size = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.0f, text);
    drawList->AddText(font, font->FontSize,
                      ImVec2(min.x + (max.x - min.x - size.x) * 0.5f, min.y + (max.y - min.y - size.y) * 0.5f),
                      color, text);
}

void ConsoleTheme::Glass(ImVec2 min, ImVec2 max, float rounding, const ImVec4& fill, ImDrawFlags flags, bool border)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(min, max, U32(fill, 1.0f), rounding, flags);
    if (border)
        dl->AddRect(min, max, U32(Ink(0.06f), 1.0f), rounding, flags, 1.0f);
}

void ConsoleTheme::Shadow(ImVec2 min, ImVec2 max, float rounding, float spread, float strength)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    for (int i = 3; i >= 1; --i)
    {
        const float t = static_cast<float>(i) / 3.0f;
        const float grow = spread * t;
        const float alpha = 0.10f * strength * (1.0f - t * 0.7f);
        dl->AddRectFilled(ImVec2(min.x - grow * 0.4f, min.y + grow * 0.3f), ImVec2(max.x + grow * 0.4f, max.y + grow * 0.9f),
                          U32(V(0.0f, 0.0f, 0.0f, 1.0f), alpha), rounding + grow * 0.5f);
    }
}

void ConsoleTheme::HLine(ImVec2 from, float width, ImU32 color)
{
    ImGui::GetWindowDrawList()->AddLine(from, ImVec2(from.x + width, from.y), color, 1.0f);
}

void ConsoleTheme::Icon(UiIcon icon, ImVec2 center, float size, ImU32 color, float thickness)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = size * 0.5f;
    const float cx = center.x;
    const float cy = center.y;
    const float pi = 3.14159265f;

    switch (icon)
    {
    case UiIcon::Crosshair:
        dl->AddCircle(center, h * 0.62f, color, 24, thickness);
        dl->AddLine(ImVec2(cx, cy - h * 0.95f), ImVec2(cx, cy - h * 0.62f), color, thickness);
        dl->AddLine(ImVec2(cx, cy + h * 0.62f), ImVec2(cx, cy + h * 0.95f), color, thickness);
        dl->AddLine(ImVec2(cx - h * 0.95f, cy), ImVec2(cx - h * 0.62f, cy), color, thickness);
        dl->AddLine(ImVec2(cx + h * 0.62f, cy), ImVec2(cx + h * 0.95f, cy), color, thickness);
        break;
    case UiIcon::Target:
        dl->AddCircle(center, h * 0.88f, color, 28, thickness);
        dl->AddCircle(center, h * 0.44f, color, 22, thickness);
        dl->AddCircleFilled(center, h * 0.12f, color, 12);
        break;
    case UiIcon::Users:
        dl->AddCircle(ImVec2(cx - h * 0.36f, cy - h * 0.42f), h * 0.3f, color, 18, thickness);
        dl->PathArcTo(ImVec2(cx - h * 0.36f, cy + h * 0.9f), h * 0.62f, pi, 2.0f * pi, 18);
        dl->PathStroke(color, ImDrawFlags_None, thickness);
        dl->AddCircle(ImVec2(cx + h * 0.56f, cy - h * 0.3f), h * 0.24f, color, 16, thickness);
        dl->PathArcTo(ImVec2(cx + h * 0.56f, cy + h * 0.98f), h * 0.5f, pi, 2.0f * pi, 16);
        dl->PathStroke(color, ImDrawFlags_None, thickness);
        break;
    case UiIcon::Globe:
        dl->AddCircle(center, h * 0.86f, color, 28, thickness);
        dl->AddLine(ImVec2(cx - h * 0.86f, cy), ImVec2(cx + h * 0.86f, cy), color, thickness);
        dl->AddBezierCubic(ImVec2(cx, cy - h * 0.86f), ImVec2(cx - h * 1.15f, cy - h * 0.3f), ImVec2(cx - h * 1.15f, cy + h * 0.3f), ImVec2(cx, cy + h * 0.86f), color, thickness, 16);
        dl->AddBezierCubic(ImVec2(cx, cy - h * 0.86f), ImVec2(cx + h * 1.15f, cy - h * 0.3f), ImVec2(cx + h * 1.15f, cy + h * 0.3f), ImVec2(cx, cy + h * 0.86f), color, thickness, 16);
        break;
    case UiIcon::Sliders:
        dl->AddLine(ImVec2(cx - h * 0.88f, cy - h * 0.34f), ImVec2(cx + h * 0.88f, cy - h * 0.34f), color, thickness);
        dl->AddLine(ImVec2(cx - h * 0.88f, cy + h * 0.34f), ImVec2(cx + h * 0.88f, cy + h * 0.34f), color, thickness);
        dl->AddCircleFilled(ImVec2(cx - h * 0.3f, cy - h * 0.34f), h * 0.24f, color, 14);
        dl->AddCircleFilled(ImVec2(cx + h * 0.3f, cy + h * 0.34f), h * 0.24f, color, 14);
        break;
    case UiIcon::Folder:
        {
            const ImVec2 pts[6] = {
                ImVec2(cx - h * 0.88f, cy + h * 0.6f), ImVec2(cx - h * 0.88f, cy - h * 0.4f),
                ImVec2(cx - h * 0.22f, cy - h * 0.4f), ImVec2(cx + h * 0.04f, cy - h * 0.7f),
                ImVec2(cx + h * 0.88f, cy - h * 0.7f), ImVec2(cx + h * 0.88f, cy + h * 0.6f) };
            dl->AddPolyline(pts, 6, color, ImDrawFlags_Closed, thickness);
        }
        break;
    case UiIcon::Gear:
        dl->AddCircle(center, h * 0.42f, color, 24, thickness);
        dl->AddCircle(center, h * 0.16f, color, 14, thickness);
        for (int i = 0; i < 8; ++i)
        {
            const float angle = static_cast<float>(i) * pi * 0.25f;
            const float s = std::sin(angle);
            const float c2 = std::cos(angle);
            dl->AddLine(ImVec2(cx + c2 * h * 0.5f, cy + s * h * 0.5f), ImVec2(cx + c2 * h * 0.88f, cy + s * h * 0.88f), color, thickness);
        }
        break;
    case UiIcon::Search:
        dl->AddCircle(ImVec2(cx - h * 0.16f, cy - h * 0.16f), h * 0.56f, color, 22, thickness);
        dl->AddLine(ImVec2(cx + h * 0.28f, cy + h * 0.28f), ImVec2(cx + h * 0.88f, cy + h * 0.88f), color, thickness);
        break;
    case UiIcon::Save:
        dl->AddRect(ImVec2(cx - h * 0.78f, cy - h * 0.8f), ImVec2(cx + h * 0.78f, cy + h * 0.8f), color, h * 0.16f, ImDrawFlags_RoundCornersAll, thickness);
        dl->AddRect(ImVec2(cx - h * 0.34f, cy - h * 0.8f), ImVec2(cx + h * 0.34f, cy - h * 0.18f), color, h * 0.06f, ImDrawFlags_RoundCornersAll, thickness);
        dl->AddLine(ImVec2(cx - h * 0.44f, cy + h * 0.44f), ImVec2(cx + h * 0.44f, cy + h * 0.44f), color, thickness);
        break;
    case UiIcon::Car:
        dl->AddRect(ImVec2(cx - h * 0.92f, cy - h * 0.1f), ImVec2(cx + h * 0.92f, cy + h * 0.5f), color, h * 0.2f, ImDrawFlags_RoundCornersAll, thickness);
        dl->AddRect(ImVec2(cx - h * 0.44f, cy - h * 0.56f), ImVec2(cx + h * 0.42f, cy - h * 0.1f), color, h * 0.16f, ImDrawFlags_RoundCornersTop, thickness);
        dl->AddCircleFilled(ImVec2(cx - h * 0.5f, cy + h * 0.6f), h * 0.19f, color, 14);
        dl->AddCircleFilled(ImVec2(cx + h * 0.5f, cy + h * 0.6f), h * 0.19f, color, 14);
        break;
    case UiIcon::Zap:
        {
            const ImVec2 pts[6] = {
                ImVec2(cx + h * 0.28f, cy - h * 0.92f), ImVec2(cx - h * 0.55f, cy + h * 0.1f),
                ImVec2(cx - h * 0.06f, cy + h * 0.1f), ImVec2(cx - h * 0.32f, cy + h * 0.94f),
                ImVec2(cx + h * 0.58f, cy - h * 0.14f), ImVec2(cx + h * 0.06f, cy - h * 0.14f) };
            dl->AddPolyline(pts, 6, color, ImDrawFlags_Closed, thickness);
        }
        break;
    case UiIcon::Shield:
        {
            const ImVec2 pts[6] = {
                ImVec2(cx, cy - h * 0.9f), ImVec2(cx + h * 0.74f, cy - h * 0.54f), ImVec2(cx + h * 0.74f, cy + h * 0.14f),
                ImVec2(cx, cy + h * 0.94f), ImVec2(cx - h * 0.74f, cy + h * 0.14f), ImVec2(cx - h * 0.74f, cy - h * 0.54f) };
            dl->AddPolyline(pts, 6, color, ImDrawFlags_Closed, thickness);
            const ImVec2 check[3] = { ImVec2(cx - h * 0.3f, cy - h * 0.02f), ImVec2(cx - h * 0.06f, cy + h * 0.26f), ImVec2(cx + h * 0.34f, cy - h * 0.28f) };
            dl->AddPolyline(check, 3, color, ImDrawFlags_None, thickness);
        }
        break;
    case UiIcon::Eye:
        dl->PathArcTo(ImVec2(cx, cy), h * 0.9f, pi, 2.0f * pi, 22);
        dl->PathStroke(color, ImDrawFlags_None, thickness);
        dl->PathArcTo(ImVec2(cx, cy), h * 0.9f, 0.0f, pi, 22);
        dl->PathStroke(color, ImDrawFlags_None, thickness);
        dl->AddCircleFilled(center, h * 0.26f, color, 16);
        break;
    case UiIcon::Heart:
        dl->AddCircleFilled(ImVec2(cx - h * 0.36f, cy - h * 0.32f), h * 0.38f, color, 16);
        dl->AddCircleFilled(ImVec2(cx + h * 0.36f, cy - h * 0.32f), h * 0.38f, color, 16);
        dl->AddTriangleFilled(ImVec2(cx - h * 0.7f, cy - h * 0.14f), ImVec2(cx + h * 0.7f, cy - h * 0.14f), ImVec2(cx, cy + h * 0.88f), color);
        break;
    case UiIcon::Pin:
        dl->AddCircle(ImVec2(cx, cy - h * 0.3f), h * 0.42f, color, 20, thickness);
        dl->AddLine(ImVec2(cx - h * 0.3f, cy + h * 0.04f), ImVec2(cx, cy + h * 0.92f), color, thickness);
        dl->AddLine(ImVec2(cx + h * 0.3f, cy + h * 0.04f), ImVec2(cx, cy + h * 0.92f), color, thickness);
        dl->AddCircleFilled(ImVec2(cx, cy - h * 0.3f), h * 0.13f, color, 12);
        break;
    case UiIcon::Map:
        dl->AddRect(ImVec2(cx - h * 0.92f, cy - h * 0.68f), ImVec2(cx + h * 0.92f, cy + h * 0.68f), color, h * 0.2f, ImDrawFlags_RoundCornersAll, thickness);
        dl->AddLine(ImVec2(cx - h * 0.3f, cy - h * 0.68f), ImVec2(cx - h * 0.3f, cy + h * 0.68f), color, thickness);
        dl->AddLine(ImVec2(cx + h * 0.32f, cy - h * 0.68f), ImVec2(cx + h * 0.32f, cy + h * 0.68f), color, thickness);
        break;
    case UiIcon::Grid:
        for (int row = 0; row < 2; ++row)
            for (int col = 0; col < 2; ++col)
                dl->AddRect(ImVec2(cx - h * 0.86f + col * h * 0.96f, cy - h * 0.86f + row * h * 0.96f),
                            ImVec2(cx - h * 0.18f + col * h * 0.96f, cy - h * 0.18f + row * h * 0.96f),
                            color, h * 0.2f, ImDrawFlags_RoundCornersAll, thickness);
        break;
    case UiIcon::Menu:
        for (int i = -1; i <= 1; ++i)
            dl->AddLine(ImVec2(cx - h * 0.74f, cy + i * h * 0.46f), ImVec2(cx + h * 0.74f, cy + i * h * 0.46f), color, thickness);
        break;
    case UiIcon::Check:
        {
            const ImVec2 pts[3] = { ImVec2(cx - h * 0.74f, cy + h * 0.06f), ImVec2(cx - h * 0.2f, cy + h * 0.6f), ImVec2(cx + h * 0.8f, cy - h * 0.6f) };
            dl->AddPolyline(pts, 3, color, ImDrawFlags_None, thickness);
        }
        break;
    case UiIcon::Close:
        dl->AddLine(ImVec2(cx - h * 0.62f, cy - h * 0.62f), ImVec2(cx + h * 0.62f, cy + h * 0.62f), color, thickness);
        dl->AddLine(ImVec2(cx + h * 0.62f, cy - h * 0.62f), ImVec2(cx - h * 0.62f, cy + h * 0.62f), color, thickness);
        break;
    case UiIcon::Chevron:
        {
            const ImVec2 pts[3] = { ImVec2(cx - h * 0.32f, cy - h * 0.56f), ImVec2(cx + h * 0.34f, cy), ImVec2(cx - h * 0.32f, cy + h * 0.56f) };
            dl->AddPolyline(pts, 3, color, ImDrawFlags_None, thickness);
        }
        break;
    case UiIcon::ChevronDown:
        {
            const ImVec2 pts[3] = { ImVec2(cx - h * 0.56f, cy - h * 0.3f), ImVec2(cx, cy + h * 0.34f), ImVec2(cx + h * 0.56f, cy - h * 0.3f) };
            dl->AddPolyline(pts, 3, color, ImDrawFlags_None, thickness);
        }
        break;
    case UiIcon::Refresh:
        dl->PathArcTo(center, h * 0.78f, -pi * 0.25f, pi * 1.2f, 28);
        dl->PathStroke(color, ImDrawFlags_None, thickness);
        dl->AddTriangleFilled(ImVec2(cx + h * 0.62f, cy - h * 0.86f), ImVec2(cx + h * 1.0f, cy - h * 0.5f), ImVec2(cx + h * 0.62f, cy - h * 0.14f), color);
        break;
    case UiIcon::Trash:
        dl->AddLine(ImVec2(cx - h * 0.86f, cy - h * 0.56f), ImVec2(cx + h * 0.86f, cy - h * 0.56f), color, thickness);
        dl->AddLine(ImVec2(cx - h * 0.3f, cy - h * 0.56f), ImVec2(cx - h * 0.3f, cy - h * 0.86f), color, thickness);
        dl->AddLine(ImVec2(cx + h * 0.3f, cy - h * 0.56f), ImVec2(cx + h * 0.3f, cy - h * 0.86f), color, thickness);
        {
            const ImVec2 body[5] = { ImVec2(cx - h * 0.64f, cy - h * 0.56f), ImVec2(cx + h * 0.64f, cy - h * 0.56f),
                                     ImVec2(cx + h * 0.5f, cy + h * 0.86f), ImVec2(cx - h * 0.5f, cy + h * 0.86f),
                                     ImVec2(cx - h * 0.64f, cy - h * 0.56f) };
            dl->AddPolyline(body, 5, color, ImDrawFlags_None, thickness);
        }
        break;
    case UiIcon::Plus:
        dl->AddLine(ImVec2(cx - h * 0.7f, cy), ImVec2(cx + h * 0.7f, cy), color, thickness);
        dl->AddLine(ImVec2(cx, cy - h * 0.7f), ImVec2(cx, cy + h * 0.7f), color, thickness);
        break;
    case UiIcon::Activity:
        {
            const ImVec2 pts[6] = { ImVec2(cx - h * 0.95f, cy), ImVec2(cx - h * 0.34f, cy), ImVec2(cx - h * 0.1f, cy - h * 0.6f),
                                    ImVec2(cx + h * 0.16f, cy + h * 0.6f), ImVec2(cx + h * 0.42f, cy), ImVec2(cx + h * 0.95f, cy) };
            dl->AddPolyline(pts, 6, color, ImDrawFlags_None, thickness);
        }
        break;
    case UiIcon::Ellipsis:
        for (int i = -1; i <= 1; ++i)
            dl->AddCircleFilled(ImVec2(cx, cy + i * h * 0.58f), h * 0.14f, color, 10);
        break;
    case UiIcon::Dot:
        dl->AddCircleFilled(center, h * 0.34f, color, 14);
        break;
    default:
        dl->AddCircle(center, h * 0.6f, color, 20, thickness);
        break;
    }
}

/* ---------- 布局单元 ---------- */

void ConsoleTheme::SectionHeader(const char* title, const char* description, float width)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    // width=0 → 用整行可用宽度；两列布局里必须传入本列盒宽，
    // 否则左列盒子的右侧说明文字会被推到窗口最右边（跑到右列头上）。
    if (width <= 0.0f)
        width = ImGui::GetContentRegionAvail().x;

    Text(dl, SmallFont(), ImVec2(start.x, start.y + 6.0f), U32(MixV(g_colors.headerText, g_colors.text, 0.35f), 1.0f), title);
    if (description && description[0] != '\0')
    {
        ImFont* font = SmallFont();
        const float size = font ? font->FontSize : ImGui::GetFontSize();
        const ImVec2 ts = font ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, description) : ImGui::CalcTextSize(description);
        Text(dl, font, ImVec2(start.x + width - ts.x, start.y + 6.0f), U32(MixV(g_colors.headerText, g_colors.text, 0.22f), 1.0f), description);
    }
    // 关键：全宽 Dummy 会把光标 X 拉回窗口左缘，这里显式还原，
    // 否则紧随其后的盒子会画到第 0 列上（两列布局会互相覆盖）。
    const float cursorX = ImGui::GetCursorPosX();
    ImGui::Dummy(ImVec2(width, layout::section_h));
    ImGui::SetCursorPosX(cursorX);
}


/* ---------- 布局自检 ----------
   每个盒子 / 每一行的实际屏幕矩形都按帧写进 ui_check.txt：
   可核对「行是否越出所属盒子」「盒子声明行数与实际绘制行数是否一致」
   「两列底边差多少」，而不是靠肉眼猜错位。只读，不参与绘制。 */
namespace
{
struct BoxTrace
{
    bool  active = false;
    const char* id = "";
    int   declared = 0;
    int   drawn = 0;
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
};

BoxTrace    g_boxTrace;
std::string g_traceText;
bool        g_traceOn = false;
const char* g_tracePage = "";
float       g_traceWidth = 0.0f;
float       g_traceHeight = 0.0f;

std::string TraceFmt(const char* fmt, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    return std::string(buffer);
}
} // namespace

void ConsoleTheme::TraceBegin(const char* page, float width, float height)
{
    g_traceOn = true;
    g_tracePage = page ? page : "?";
    g_traceWidth = width;
    g_traceHeight = height;
    g_traceText.clear();
    g_boxTrace = BoxTrace();
}

void ConsoleTheme::TraceBoxBegin(const char* id, const ImVec2& min, const ImVec2& max, int rows)
{
    if (!g_traceOn) return;
    g_boxTrace.active = true;
    g_boxTrace.id = id;
    g_boxTrace.declared = rows;
    g_boxTrace.drawn = 0;
    g_boxTrace.x = min.x; g_boxTrace.y = min.y;
    g_boxTrace.w = max.x - min.x; g_boxTrace.h = max.y - min.y;
    g_traceText += TraceFmt("BOX %-18s x=%.0f y=%.0f w=%.0f h=%.0f rows=%d\n", id, min.x, min.y, g_boxTrace.w, g_boxTrace.h, rows);
}

void ConsoleTheme::TraceRow(const char* kind, const ImVec2& start, float width, float height)
{
    if (!g_traceOn) return;
    const float rowH = height > 0.0f ? height : layout::row_h;
    (void)width;
    if (g_boxTrace.active)
    {
        ++g_boxTrace.drawn;
        const float boxBottom = g_boxTrace.y + g_boxTrace.h;
        const float rowBottom = start.y + rowH;
        g_traceText += TraceFmt("  ROW %-9s x=%.0f y=%.0f w=%.0f bottom=%.0f boxR=%.0f%s\n", kind, start.x, start.y, width, rowBottom,
                                g_boxTrace.x + g_boxTrace.w, (start.x + width) > (g_boxTrace.x + g_boxTrace.w - 1.0f) || rowBottom > boxBottom - 1.0f ? "   [OUT-OF-BOX]" : "");
    }
    else
    {
        g_traceText += TraceFmt("  ROW %-9s y=%.0f bottom=%.0f (no box)\n", kind, start.y, start.y + rowH);
    }
}

void ConsoleTheme::TraceBoxEnd()
{
    if (!g_traceOn) return;
    if (g_boxTrace.active && g_boxTrace.declared >= 0 && g_boxTrace.declared != g_boxTrace.drawn)
    {
        g_traceText += TraceFmt("  !! %s declared=%d drawn=%d  [ROW-COUNT-MISMATCH]\n",
                                g_boxTrace.id, g_boxTrace.declared, g_boxTrace.drawn);
    }
    g_boxTrace.active = false;
}

void ConsoleTheme::TraceNote(const char* text)
{
    if (g_traceOn && text) g_traceText += std::string("NOTE ") + text + "\n";
}

void ConsoleTheme::TraceEnd(const char* path)
{
    if (!g_traceOn) return;
    std::string out = TraceFmt("PAGE %s  workspace w=%.0f h=%.0f\n", g_tracePage, g_traceWidth, g_traceHeight) + g_traceText;
    FILE* f = nullptr;
    if (fopen_s(&f, path, "wb") == 0 && f != nullptr)
    {
        std::fwrite(out.c_str(), 1, out.size(), f);
        std::fclose(f);
    }
    g_traceOn = false;
}

bool ConsoleTheme::BoxBeginPixels(const char* id, float height, const char* title, float width)
{
    const float avail = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
    if (title)
        SectionHeader(title, nullptr, avail);

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 max(origin.x + avail, origin.y + height);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, max, U32(g_colors.box, 1.0f), layout::box_round);
    dl->AddRect(origin, max, U32(Ink(0.13f), 1.0f), layout::box_round, ImDrawFlags_RoundCornersAll, 1.0f);
    // 顶部 1px 玻璃高光：卡片边界在任何壁纸亮度下都能被辨认
    dl->AddRectFilledMultiColor(ImVec2(origin.x + layout::box_round, origin.y + 1.0f),
                                ImVec2(max.x - layout::box_round, origin.y + 2.0f),
                                U32(Ink(0.10f), 1.0f), U32(Ink(0.01f), 1.0f),
                                U32(Ink(0.01f), 1.0f), U32(Ink(0.10f), 1.0f));

    TraceBoxBegin(id, origin, max, -1);

    // 子窗口 WindowPadding 恒为 0，GetContentRegionAvail() 不会扣掉右侧内边距，
    // 之前的行会一直画到盒子右边框（数值/开关贴着边框，左右不对称）。
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(layout::box_pad_x, layout::box_pad_y));
    ImGui::BeginChild(id, ImVec2(avail, height), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    // 内容原点与行宽由盒子自己钉死（不假定 BeginChild 会应用 WindowPadding）：
    // 原点 = 盒子左上 + 内边距，行宽 = 盒宽 - 左右内边距，行与控件因此左右对称、不贴描边。
    g_boxInnerWidth = avail - layout::box_pad_x * 2.0f;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + layout::box_pad_x, origin.y + layout::box_pad_y));
    return true;
}

bool ConsoleTheme::BoxBegin(const char* id, int rows, const char* title, float width)
{
    return BoxBeginPixels(id, layout::box_height(rows), title, width);
}

void ConsoleTheme::BoxEnd()
{
    if (g_traceOn && g_boxTrace.active)
    {
        // 内容底：光标推进 + 子窗口内容高二取大。GetScrollMaxY 能覆盖“不推进光标”的控件
        // （按钮之类），纯按钮盒（如 veh_handling_ops）以前会被误报成 SLACK 底部空余偏大。
        const float byCursor = ImGui::GetCursorPosY() - layout::box_pad_y;
        const float inner    = g_boxTrace.h - layout::box_pad_y * 2.0f;
        const float byItems  = ImGui::GetScrollMaxY() + inner;
        const float content  = (byItems > byCursor) ? byItems : byCursor;
        const float slack    = inner - content;
        const char* flag    = (slack < -2.0f) ? "[OVERFLOW 内容超出盒子]"
                            : (slack > 6.0f)  ? "[SLACK 底部空余偏大]" : "";
        g_traceText += TraceFmt("  BOXEND %-18s content=%.0f inner=%.0f slack=%+.0f %s\n",
                                g_boxTrace.id, content, inner, slack, flag);
        if (g_boxTrace.declared > 0 && g_boxTrace.declared != g_boxTrace.drawn)
            g_traceText += TraceFmt("  !! %s declared=%d drawn=%d\n", g_boxTrace.id, g_boxTrace.declared, g_boxTrace.drawn);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    g_boxInnerWidth = 0.0f;
    g_boxTrace.active = false;
}

void ConsoleTheme::RowSeparator()
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    HLine(ImVec2(start.x, start.y), RowWidth(), U32(g_colors.separator, 1.0f));
    ImGui::Dummy(ImVec2(0.0f, layout::separator_h));
}

bool ConsoleTheme::ToggleRow(const char* id, const char* label, const char* description, bool* value, bool separator)
{
    ImGui::PushID(id);
    RowState row;
    BeginRow("##row", separator, row);

    if (row.clicked && value)
        *value = !*value;

    const float hover = Anim((std::string("tg:") + id + ":h").c_str(), row.hovered ? 1.0f : 0.0f, 18.0f);
    const float on = Anim((std::string("tg:") + id + ":v").c_str(), (value && *value) ? 1.0f : 0.0f, 22.0f);
    const ImVec2 start = row.start;
    const float width = row.width;
    TraceRow("toggle", start, width);

    if (hover > 0.01f)
    {
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(start.x - 6.0f, start.y + 2.0f), ImVec2(start.x + width + 6.0f, start.y + layout::row_h - 2.0f),
                                                  U32(Wash(0.05f * hover), 1.0f), layout::control_round);
    }

    const bool hasDescription = description && description[0] != '\0';
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hasDescription)
    {
        ImFont* font = AppFonts::Regular;
        const float size = font ? font->FontSize : ImGui::GetFontSize();
        Text(dl, font, ImVec2(start.x, start.y + 4.0f), U32(MixV(g_colors.text, g_colors.textMuted, 0.25f), 1.0f), label);
        Text(dl, SmallFont(), ImVec2(start.x, start.y + 22.0f), U32(g_colors.textMuted, 0.88f), description);
        (void)size;
    }
    else
    {
        DrawRowLabel(label, start, layout::row_h, U32(MixV(g_colors.text, g_colors.textMuted, 0.18f), 1.0f));
    }

    const ImVec2 trackMin(start.x + width - layout::toggle_w, start.y + (layout::row_h - layout::toggle_h) * 0.5f);
    DrawToggle(trackMin, on, hover);

    ImGui::PopID();
    return row.clicked;
}

bool ConsoleTheme::SliderRow(const char* id, const char* label, float* value, float min, float max, const char* format, bool separator, float displayScale)
{
    ImGui::PushID(id);
    RowState row;
    BeginRow("##row", separator, row);
    const ImVec2 start = row.start;
    const float width = row.width;
    TraceRow("row", start, width);
    const float hover = Anim((std::string("sl:") + id).c_str(), row.hovered ? 1.0f : 0.0f, 18.0f);

    DrawRowLabel(label, start, layout::row_h, U32(MixV(g_colors.text, g_colors.textMuted, 0.18f), 1.0f));

    if (value && max > min)
    {
        float t = (*value - min) / (max - min);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;

        const float sliderW = layout::slider_w;
        const ImVec2 sliderMin(start.x + width - sliderW, start.y + layout::row_h * 0.5f);
        DrawSlider(sliderMin, sliderW, t, hover, true);

        char valueText[32];
        std::snprintf(valueText, sizeof(valueText), format ? format : "%.2f", *value * displayScale);
        ImFont* font = AppFonts::Regular;
        const float size = font ? font->FontSize : ImGui::GetFontSize();
        const ImVec2 ts = font ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, valueText) : ImGui::CalcTextSize(valueText);
        Text(ImGui::GetWindowDrawList(), font,
             ImVec2(sliderMin.x - 14.0f - ts.x, start.y + (layout::row_h - size) * 0.5f - 1.0f),
             U32(g_colors.textMuted, 1.0f), valueText);

        if (row.hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const float mouseT = Clamp((ImGui::GetIO().MousePos.x - sliderMin.x) / sliderW, 0.0f, 1.0f);
            *value = min + mouseT * (max - min);
        }
    }

    ImGui::PopID();
    return row.clicked;
}

bool ConsoleTheme::MeterRow(const char* label, float value, float maxValue, bool good, bool separator)
{
    ImGui::PushID(label);
    RowState row;
    BeginRow("##meter", separator, row);
    const ImVec2 start = row.start;
    const float width = row.width;
    TraceRow("meter", start, width);

    DrawRowLabel(label, start, layout::row_h, U32(MixV(g_colors.text, g_colors.textMuted, 0.18f), 1.0f));

    float t = maxValue > 0.0f ? value / maxValue : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    char valueText[48];
    std::snprintf(valueText, sizeof(valueText), "%.0f / %.0f", value, maxValue);

    // 只读读数：只画进度条本体，不画可拖动的滑块圆点
    const float sliderW = layout::slider_w;
    const float barH = 6.0f;
    const ImVec2 barMin(start.x + width - sliderW, start.y + layout::row_h * 0.5f - barH * 0.5f);
    const ImVec2 barMax(start.x + width, barMin.y + barH);
    ImDrawList* bar = ImGui::GetWindowDrawList();
    const ImVec4 trackTone = MixV(g_colors.sliderBg, Ink(0.16f), 0.85f);
    bar->AddRectFilled(barMin, barMax, U32(trackTone, 1.0f), barH * 0.5f);
    const float fillW = (barMax.x - barMin.x) * t;
    if (fillW > 1.0f)
    {
        const ImVec4 fillColor = good ? g_colors.accent : g_colors.warning;
        bar->AddRectFilledMultiColor(barMin, ImVec2(barMin.x + fillW, barMax.y),
                                     U32(fillColor, 1.0f), U32(MixV(fillColor, g_colors.accent, 0.15f), 1.0f),
                                     U32(MixV(fillColor, g_colors.accent, 0.15f), 1.0f), U32(fillColor, 1.0f));
    }
    const ImVec2 sliderMin = barMin;

    ImFont* font = AppFonts::Regular;
    const float size = font ? font->FontSize : ImGui::GetFontSize();
    const ImVec2 ts = font ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, valueText) : ImGui::CalcTextSize(valueText);
    Text(ImGui::GetWindowDrawList(), font,
         ImVec2(sliderMin.x - 14.0f - ts.x, start.y + (layout::row_h - size) * 0.5f - 1.0f),
         U32(good ? g_colors.text : g_colors.warning, 1.0f), valueText);
    ImGui::PopID();
    return false;
}

bool ConsoleTheme::TextRow(const char* label, const char* value, bool good, bool separator)
{
    ImGui::PushID(label);
    RowState row;
    BeginRow("##textrow", separator, row);
    const ImVec2 start = row.start;
    const float width = row.width;
    TraceRow("textrow", start, width);

    DrawRowLabel(label, start, layout::row_h, U32(MixV(g_colors.text, g_colors.textMuted, 0.18f), 1.0f));

    if (value)
    {
        ImFont* font = AppFonts::Bold ? AppFonts::Bold : AppFonts::Regular;
        const float size = font ? font->FontSize : ImGui::GetFontSize();
        const ImVec2 ts = font ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, value) : ImGui::CalcTextSize(value);
        const ImU32 color = good ? U32(g_colors.text, 1.0f) : U32(g_colors.warning, 1.0f);
        Text(ImGui::GetWindowDrawList(), font, ImVec2(start.x + width - ts.x, start.y + (layout::row_h - size) * 0.5f - 1.0f), color, value);
    }
    ImGui::PopID();
    return false;
}

/* ---------- 行式数值控件（与只读行同一套几何，控件列宽固定）---------- */

namespace
{
ImU32 RowLabelColor()
{
    return ConsoleTheme::U32(ConsoleTheme::MixV(ConsoleTheme::TextColor(), ConsoleTheme::TextMuted(), 0.18f), 1.0f);
}

void DrawRowLine(const ImVec2& start, float width)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(start.x, start.y + layout::row_h + 0.5f), ImVec2(start.x + width, start.y + layout::row_h + 0.5f),
                ConsoleTheme::U32(g_colors.separator, 1.0f), 1.0f);
}

// 输入框使用真实 ImGui 控件（可点选可键入），但外观按设计系统统一：
// 圆角、内边距、底色、描边全部走主题色，宽度固定为 layout::field_w。
void PushFieldStyle()
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, layout::control_round);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, g_colors.control);
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, g_colors.controlHover);
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, g_colors.controlHover);
    ImGui::PushStyleColor(ImGuiCol_Border, ConsoleTheme::Ink(0.10f));
    ImGui::PushStyleColor(ImGuiCol_Text, g_colors.text);
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, g_colors.accent);
}

void PopFieldStyle()
{
    ImGui::PopStyleColor(6);
    ImGui::PopStyleVar(3);
}

float FieldY(const ImVec2& start)
{
    return start.y + (layout::row_h - layout::control_h) * 0.5f - 1.0f;
}
} // namespace

bool ConsoleTheme::TextRow2(const char* labelA, const char* valueA, bool goodA,
                            const char* labelB, const char* valueB, bool goodB, bool separator)
{
    ImGui::PushID(labelA);
    RowState row;
    BeginRow("##textrow2", separator, row);
    const ImVec2 start = row.start;
    const float width = row.width;
    TraceRow("textrow2", start, width);
    const float half = width * 0.5f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* valueFont = AppFonts::Bold ? AppFonts::Bold : AppFonts::Regular;
    const float valueSize = valueFont ? valueFont->FontSize : ImGui::GetFontSize();
    const float valueY = start.y + (layout::row_h - valueSize) * 0.5f - 1.0f;

    DrawRowLabel(labelA, start, layout::row_h, RowLabelColor());
    if (valueA)
    {
        const ImVec2 ts = valueFont ? valueFont->CalcTextSizeA(valueSize, FLT_MAX, 0.0f, valueA) : ImGui::CalcTextSize(valueA);
        Text(dl, valueFont, ImVec2(start.x + half - 16.0f - ts.x, valueY),
             goodA ? U32(g_colors.text, 1.0f) : U32(g_colors.warning, 1.0f), valueA);
    }

    if (labelB)
        Text(dl, AppFonts::Regular, ImVec2(start.x + half, start.y + (layout::row_h - (AppFonts::Regular ? AppFonts::Regular->FontSize : ImGui::GetFontSize())) * 0.5f - 1.0f), RowLabelColor(), labelB);

    if (valueB)
    {
        const ImVec2 ts = valueFont ? valueFont->CalcTextSizeA(valueSize, FLT_MAX, 0.0f, valueB) : ImGui::CalcTextSize(valueB);
        Text(dl, valueFont, ImVec2(start.x + width - ts.x, valueY),
             goodB ? U32(g_colors.text, 1.0f) : U32(g_colors.warning, 1.0f), valueB);
    }

    ImGui::PopID();
    return false;
}

bool ConsoleTheme::InputRow(const char* id, const char* label, float* value, const char* format, bool separator)
{
    ImGui::PushID(id);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = RowWidth();
    const float bottom = start.y + layout::row_h + (separator ? layout::separator_h : 0.0f);
    TraceRow("input1", start, width);

    DrawRowLabel(label, start, layout::row_h, RowLabelColor());

    const float fieldW = layout::field_w;
    ImGui::SetCursorScreenPos(ImVec2(start.x + width - fieldW, FieldY(start)));
    ImGui::SetNextItemWidth(fieldW);
    PushFieldStyle();
    const bool changed = ImGui::InputFloat("##field", value, 0.0f, 0.0f, format ? format : "%.3f");
    PopFieldStyle();

    if (separator)
        DrawRowLine(start, width);
    ImGui::SetCursorScreenPos(ImVec2(start.x, bottom));
    ImGui::PopID();
    return changed;
}

bool ConsoleTheme::ComboRow(const char* id, const char* label, const char* const* items, int itemCount,
                            int* index, const char* description, bool separator)
{
    ImGui::PushID(id);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = RowWidth();
    const float bottom = start.y + layout::row_h + (separator ? layout::separator_h : 0.0f);
    TraceRow("combo", start, width);

    const bool hasDescription = description && description[0] != '\0';
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (hasDescription)
    {
        Text(dl, AppFonts::Regular, ImVec2(start.x, start.y + 4.0f), U32(MixV(g_colors.text, g_colors.textMuted, 0.25f), 1.0f), label);
        Text(dl, SmallFont(), ImVec2(start.x, start.y + 22.0f), U32(g_colors.textMuted, 0.88f), description);
    }
    else
    {
        DrawRowLabel(label, start, layout::row_h, U32(MixV(g_colors.text, g_colors.textMuted, 0.18f), 1.0f));
    }

    const float fieldW = layout::field_w;
    ImGui::SetCursorScreenPos(ImVec2(start.x + width - fieldW, FieldY(start)));
    ImGui::SetNextItemWidth(fieldW);
    PushFieldStyle();
    const int before = index ? *index : 0;
    if (index)
        ImGui::Combo("##combo", index, items, itemCount);
    PopFieldStyle();
    const bool changed = index && (*index != before);

    if (separator)
        DrawRowLine(start, width);
    ImGui::SetCursorScreenPos(ImVec2(start.x, bottom));
    ImGui::PopID();
    return changed;
}

bool ConsoleTheme::InputRow2(const char* id, const char* labelA, float* valueA,
                             const char* labelB, float* valueB, const char* format, bool separator)
{
    ImGui::PushID(id);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = RowWidth();
    const float bottom = start.y + layout::row_h + (separator ? layout::separator_h : 0.0f);
    const float half = width * 0.5f;
    const float maxFieldW = half * 0.44f;
    const float fieldW = layout::field_w < maxFieldW ? layout::field_w : maxFieldW;
    const float y = FieldY(start);
    TraceRow("input2", start, width);

    DrawRowLabel(labelA, start, layout::row_h, RowLabelColor());
    ImGui::SetCursorScreenPos(ImVec2(start.x + half - 14.0f - fieldW, y));
    ImGui::SetNextItemWidth(fieldW);
    PushFieldStyle();
    const bool changedA = ImGui::InputFloat("##fieldA", valueA, 0.0f, 0.0f, format ? format : "%.3f");
    PopFieldStyle();

    const ImU32 labelColor = RowLabelColor();
    const float labelSize = AppFonts::Regular ? AppFonts::Regular->FontSize : ImGui::GetFontSize();
    Text(ImGui::GetWindowDrawList(), AppFonts::Regular, ImVec2(start.x + half, start.y + (layout::row_h - labelSize) * 0.5f - 1.0f), labelColor, labelB);

    ImGui::SetCursorScreenPos(ImVec2(start.x + width - fieldW, y));
    ImGui::SetNextItemWidth(fieldW);
    PushFieldStyle();
    const bool changedB = ImGui::InputFloat("##fieldB", valueB, 0.0f, 0.0f, format ? format : "%.3f");
    PopFieldStyle();

    if (separator)
        DrawRowLine(start, width);
    ImGui::SetCursorScreenPos(ImVec2(start.x, bottom));
    ImGui::PopID();
    return changedA || changedB;
}

bool ConsoleTheme::IntRow2(const char* id, const char* labelA, int* valueA,
                           const char* labelB, int* valueB, bool separator)
{
    ImGui::PushID(id);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = RowWidth();
    const float bottom = start.y + layout::row_h + (separator ? layout::separator_h : 0.0f);
    const float half = width * 0.5f;
    const float maxFieldW = half * 0.44f;
    const float fieldW = layout::field_w < maxFieldW ? layout::field_w : maxFieldW;
    const float y = FieldY(start);
    TraceRow("int2", start, width);

    DrawRowLabel(labelA, start, layout::row_h, RowLabelColor());
    // step = 0 → 不画 +/- 步进按钮（旧版步进按钮把标签挤得忽左忽右）
    ImGui::SetCursorScreenPos(ImVec2(start.x + half - 14.0f - fieldW, y));
    ImGui::SetNextItemWidth(fieldW);
    PushFieldStyle();
    const bool changedA = ImGui::InputInt("##fieldA", valueA, 0, 0);
    PopFieldStyle();

    const float labelSize = AppFonts::Regular ? AppFonts::Regular->FontSize : ImGui::GetFontSize();
    Text(ImGui::GetWindowDrawList(), AppFonts::Regular, ImVec2(start.x + half, start.y + (layout::row_h - labelSize) * 0.5f - 1.0f), RowLabelColor(), labelB);

    ImGui::SetCursorScreenPos(ImVec2(start.x + width - fieldW, y));
    ImGui::SetNextItemWidth(fieldW);
    PushFieldStyle();
    const bool changedB = ImGui::InputInt("##fieldB", valueB, 0, 0);
    PopFieldStyle();

    if (separator)
        DrawRowLine(start, width);
    ImGui::SetCursorScreenPos(ImVec2(start.x, bottom));
    ImGui::PopID();
    return changedA || changedB;
}

bool ConsoleTheme::ButtonRow(const char* label, UiIcon icon, bool accent, bool danger)
{
    const float width = RowWidth();
    return accent ? AccentButton(label, icon, ImVec2(width, layout::button_h))
         : danger ? DangerButton(label, icon, ImVec2(width, layout::button_h))
                  : GhostButton(label, icon, ImVec2(width, layout::button_h));
}

bool ConsoleTheme::NavItem(const char* label, bool selected)
{
    return NavItem(label, selected, false);
}

bool ConsoleTheme::NavItem(const char* label, bool selected, bool slim)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = RowWidth();
    const float height = 34.0f;
    ImGui::InvisibleButton(label, ImVec2(width, height));
    const bool pressed = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();

    const ImVec2 max(start.x + width, start.y + height);
    const std::string key = std::string("nav:") + label;
    const float hover = Anim((key + ":h").c_str(), hovered ? 1.0f : 0.0f, 20.0f);
    const float active = Anim((key + ":s").c_str(), selected ? 1.0f : 0.0f, 20.0f);

    if (active > 0.01f)
        dl->AddRectFilledMultiColor(start, max, U32(g_colors.accent, 0.28f * active), U32(g_colors.accent, 0.0f),
                                    U32(g_colors.accent, 0.0f), U32(g_colors.accent, 0.28f * active));
    if (hover > 0.01f)
        dl->AddRectFilledMultiColor(start, max, RGBA(0x53, 0x53, 0x53, 0.068f * hover), RGBA(0xB9, 0xB9, 0xB9, 0.0f),
                                    RGBA(0xB9, 0xB9, 0xB9, 0.0f), RGBA(0x53, 0x53, 0x53, 0.068f * hover));

    const ImU32 labelColor = U32(g_colors.text, 0.55f + 0.45f * Max(active, hover));
    if (slim)
    {
        const char iconText[2] = { label[0], '\0' };
        TextCentered(dl, AppFonts::Regular, start, max, labelColor, iconText);
        if (hovered)
            ImGui::SetTooltip("%s", label);
    }
    else
    {
        ImFont* font = AppFonts::Regular;
        const float size = font ? font->FontSize : ImGui::GetFontSize();
        Text(dl, font, ImVec2(start.x + 14.0f, start.y + (height - size) * 0.5f), labelColor, label);
    }
    return pressed;
}

bool ConsoleTheme::NavItemIcon(UiIcon icon, const char* label, bool selected, bool* blocksDrag)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = RowWidth();
    const float height = layout::sidebar_tab_h;

    ImGui::InvisibleButton(label, ImVec2(width, height));
    const bool pressed = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    if (blocksDrag && (hovered || held))
        *blocksDrag = true;

    const ImVec2 max(start.x + width, start.y + height);
    const std::string key = std::string("navi:") + label;
    const float hover = Anim((key + ":h").c_str(), (hovered && !selected) ? 1.0f : 0.0f, 24.0f);
    const float active = Anim((key + ":s").c_str(), selected ? 1.0f : 0.0f, 22.0f);

    if (active > 0.01f)
    {
        dl->AddRectFilledMultiColor(start, max,
                                    U32(g_colors.accent, 0.28f * active), U32(g_colors.accent, 0.0f),
                                    U32(g_colors.accent, 0.0f), U32(g_colors.accent, 0.28f * active));
        dl->AddRectFilled(start, ImVec2(start.x + 3.0f, max.y), U32(g_colors.accent, active));
    }
    if (hover > 0.01f)
    {
        dl->AddRectFilledMultiColor(start, max,
                                    RGBA(0x53, 0x53, 0x53, 0.068f * hover), RGBA(0xB9, 0xB9, 0xB9, 0.0f),
                                    RGBA(0xB9, 0xB9, 0xB9, 0.0f), RGBA(0x53, 0x53, 0x53, 0.068f * hover));
    }

    const float accent = Max(active, hover);
    const ImVec2 iconCenter(start.x + layout::sidebar_icon_x + layout::sidebar_tab_icon * 0.5f, start.y + height * 0.5f);
    Icon(icon, iconCenter, layout::sidebar_tab_icon, U32(g_colors.text, 0.55f + 0.45f * accent), 1.7f);

    if (label && label[0] != '\0')
    {
        ImFont* font = AppFonts::Regular;
        const float size = font ? font->FontSize : ImGui::GetFontSize();
        const float textX = start.x + layout::sidebar_icon_x + layout::sidebar_tab_icon + layout::sidebar_text_gap;
        Text(dl, font, ImVec2(textX, start.y + (height - size) * 0.5f), U32(g_colors.text, 0.6f + 0.4f * accent), label);
    }
    return pressed;
}

void ConsoleTheme::StatPill(const char* label, const char* value, bool good)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float height = 42.0f;

    ImFont* labelFont = SmallFont();
    ImFont* valueFont = AppFonts::Bold ? AppFonts::Bold : AppFonts::Regular;
    const float labelSize = labelFont ? labelFont->FontSize : ImGui::GetFontSize();
    const float valueSize = valueFont ? valueFont->FontSize : ImGui::GetFontSize();
    const float labelW = labelFont ? labelFont->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, label).x : ImGui::CalcTextSize(label).x;
    const float valueW = valueFont ? valueFont->CalcTextSizeA(valueSize, FLT_MAX, 0.0f, value).x : ImGui::CalcTextSize(value).x;

    const float width = 20.0f + 7.0f + 8.0f + Max(labelW, valueW) + 20.0f;
    const ImVec2 max(start.x + width, start.y + height);

    dl->AddRectFilled(start, max, U32(g_colors.box, 1.0f), layout::box_round);
    dl->AddRect(start, max, U32(Ink(0.05f), 1.0f), layout::box_round, ImDrawFlags_RoundCornersAll, 1.0f);

    const ImVec4 dot = good ? (g_theme == ConsoleThemeId::Light ? g_colors.success : g_colors.accent) : g_colors.warning;
    const ImVec2 dotCenter(start.x + 20.0f, start.y + height * 0.5f);
    dl->AddCircleFilled(dotCenter, 3.5f, U32(dot, 1.0f), 12);
    dl->AddCircleFilled(dotCenter, 7.0f, U32(dot, 0.18f), 16);

    Text(dl, labelFont, ImVec2(start.x + 35.0f, start.y + 5.0f), U32(g_colors.headerText, 1.0f), label);
    Text(dl, valueFont, ImVec2(start.x + 35.0f, start.y + height - valueSize - 7.0f), U32(g_colors.text, 1.0f), value);

    ImGui::Dummy(ImVec2(width, height));
}

void ConsoleTheme::StatusDot(bool ok, const char* label)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float height = ImGui::GetTextLineHeight();
    dl->AddCircleFilled(ImVec2(start.x + 4.0f, start.y + height * 0.5f), 3.5f, U32(ok ? g_colors.success : g_colors.warning, 1.0f), 12);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14.0f);
    ImGui::TextUnformatted(label);
}

void ConsoleTheme::KeyCap(const char* text, float* cursorX, float cursorY, float minWidth)
{
    if (!cursorX)
        return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = SmallFont();
    const float size = font ? font->FontSize : ImGui::GetFontSize();
    const ImVec2 ts = font ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, text) : ImGui::CalcTextSize(text);
    const float natural = ts.x + 14.0f;
    const float width = minWidth > natural ? minWidth : natural;
    const float height = 19.0f;
    const ImVec2 min(*cursorX, cursorY);
    const ImVec2 max(min.x + width, min.y + height);

    dl->AddRectFilled(min, max, U32(g_colors.control, 0.9f), layout::control_round);
    dl->AddRect(min, max, U32(Ink(0.07f), 1.0f), layout::control_round, ImDrawFlags_RoundCornersAll, 1.0f);
    Text(dl, font, ImVec2(min.x + (width - ts.x) * 0.5f, min.y + (height - size) * 0.5f), U32(g_colors.textMuted, 1.0f), text);
    *cursorX = max.x + 6.0f;
}

void ConsoleTheme::Badge(const char* text, bool good, float* cursorX, float cursorY)
{
    if (!cursorX)
        return;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = SmallFont();
    const float size = font ? font->FontSize : ImGui::GetFontSize();
    const ImVec2 ts = font ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, text) : ImGui::CalcTextSize(text);
    const ImVec4 tint = good ? g_colors.success : g_colors.warning;
    const float width = ts.x + 30.0f;
    const float height = 22.0f;
    const ImVec2 min(*cursorX, cursorY);
    const ImVec2 max(min.x + width, min.y + height);

    dl->AddRectFilled(min, max, U32(tint, 0.14f), height * 0.5f);
    dl->AddRect(min, max, U32(tint, 0.40f), height * 0.5f, ImDrawFlags_RoundCornersAll, 1.0f);
    dl->AddCircleFilled(ImVec2(min.x + 12.0f, min.y + height * 0.5f), 3.0f, U32(tint, 1.0f), 10);
    Text(dl, font, ImVec2(min.x + 21.0f, min.y + (height - size) * 0.5f), U32(tint, 1.0f), text);
    *cursorX = max.x + 8.0f;
}

namespace
{
bool ButtonImpl(const char* label, UiIcon icon, const ImVec2& size, const ImVec4& fill, const ImVec4& textColor, bool solid)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID(label);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImFont* font = AppFonts::Bold ? AppFonts::Bold : AppFonts::Regular;
    const float fontSize = font ? font->FontSize : ImGui::GetFontSize();
    const ImVec2 ts = font ? font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, label) : ImGui::CalcTextSize(label);

    const float iconSize = 16.0f;
    const float contentW = (icon != UiIcon::None ? iconSize + 9.0f : 0.0f) + ts.x;
    const float width = size.x > 0.0f ? size.x : contentW + 36.0f;
    const float height = size.y > 0.0f ? size.y : layout::button_h;
    ImGui::InvisibleButton("##btn", ImVec2(width, height));
    const bool pressed = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    const ImVec2 max(start.x + width, start.y + height);

    const float hover = ConsoleTheme::Anim((std::string("btn:") + label + ":h").c_str(), hovered ? 1.0f : 0.0f, 20.0f);
    const ImVec4 base = solid ? ConsoleTheme::MixV(fill, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.16f * hover) : ConsoleTheme::MixV(fill, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.06f * hover);

    if (solid)
        ConsoleTheme::Shadow(ImVec2(start.x, start.y + 2.0f), max, layout::control_round + 2.0f, 7.0f, 0.7f);

    dl->AddRectFilled(start, max, ConsoleTheme::U32(base, 1.0f), layout::control_round + (solid ? 2.0f : 0.0f));
    if (!solid)
        dl->AddRect(start, max, ConsoleTheme::U32(ConsoleTheme::Ink(0.07f + 0.06f * hover), 1.0f), layout::control_round, ImDrawFlags_RoundCornersAll, 1.0f);

    const ImU32 labelColor = ConsoleTheme::U32(solid ? textColor : ConsoleTheme::MixV(ConsoleTheme::TextMuted(), ConsoleTheme::TextColor(), hover), 1.0f);
    const float contentX = start.x + (width - contentW) * 0.5f;
    if (icon != UiIcon::None)
        ConsoleTheme::Icon(icon, ImVec2(contentX + iconSize * 0.5f, start.y + height * 0.5f), iconSize, labelColor, 1.7f);
    ConsoleTheme::Text(dl, font, ImVec2(contentX + (icon != UiIcon::None ? iconSize + 9.0f : 0.0f), start.y + (height - fontSize) * 0.5f), labelColor, label);

    ImGui::PopID();
    return pressed;
}
} // namespace

bool ConsoleTheme::AccentButton(const char* label, UiIcon icon, const ImVec2& size)
{
    return ButtonImpl(label, icon, size, g_colors.accent, g_theme == ConsoleThemeId::Light ? V(1.0f, 1.0f, 1.0f) : V(1.0f, 1.0f, 1.0f), true);
}

bool ConsoleTheme::DangerButton(const char* label, UiIcon icon, const ImVec2& size)
{
    return ButtonImpl(label, icon, size, g_colors.danger, V(1.0f, 1.0f, 1.0f), true);
}

bool ConsoleTheme::GhostButton(const char* label, UiIcon icon, const ImVec2& size)
{
    return ButtonImpl(label, icon, size, g_colors.control, V(1.0f, 1.0f, 1.0f), false);
}

bool ConsoleTheme::IconButton(const char* id, UiIcon icon, float size)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::PushID(id);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##icon", ImVec2(size, size));
    const bool pressed = ImGui::IsItemClicked();
    const bool hovered = ImGui::IsItemHovered();
    const float hover = Anim((std::string("ib:") + id).c_str(), hovered ? 1.0f : 0.0f, 20.0f);
    const ImVec2 max(start.x + size, start.y + size);
    if (hover > 0.01f)
        dl->AddRectFilled(start, max, U32(Wash(0.08f * hover), 1.0f), layout::control_round + 2.0f);
    Icon(icon, ImVec2(start.x + size * 0.5f, start.y + size * 0.5f), size * 0.62f,
         U32(MixV(g_colors.textMuted, g_colors.text, hover), 1.0f), 1.7f);
    ImGui::PopID();
    return pressed;
}

/* ---------- 设置页控件 ---------- */

namespace
{
// 步进小按钮：不依赖图标字体，直接画 − / + 两条线，高度与输入框一致。
bool StepButton(const char* id, bool plus, float size)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 max(start.x + size, start.y + size);
    const bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const float hover = ConsoleTheme::Anim((std::string("step:") + id + (plus ? ":p" : ":m")).c_str(), hovered ? 1.0f : 0.0f, 20.0f);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec4 bg = ConsoleTheme::MixV(ConsoleTheme::Control(), ConsoleTheme::Accent(), 0.16f * hover + (held ? 0.10f : 0.0f));
    dl->AddRectFilled(start, max, ConsoleTheme::U32(bg, 1.0f), layout::control_round);
    dl->AddRect(start, max,
                ConsoleTheme::U32(hover > 0.01f ? ConsoleTheme::MixV(ConsoleTheme::Ink(0.10f), ConsoleTheme::Accent(), 0.60f * hover) : ConsoleTheme::Ink(0.10f), 1.0f),
                layout::control_round, ImDrawFlags_RoundCornersAll, 1.0f);

    const ImU32 ink = ConsoleTheme::U32(hover > 0.01f ? ConsoleTheme::MixV(ConsoleTheme::TextColor(), ConsoleTheme::Accent(), 0.80f * hover) : ConsoleTheme::TextColor(), 1.0f);
    const ImVec2 c((start.x + max.x) * 0.5f, (start.y + max.y) * 0.5f);
    const float r = size * 0.22f;
    dl->AddLine(ImVec2(c.x - r, c.y), ImVec2(c.x + r, c.y), ink, 1.6f);
    if (plus)
        dl->AddLine(ImVec2(c.x, c.y - r), ImVec2(c.x, c.y + r), ink, 1.6f);
    return pressed;
}
} // namespace

bool ConsoleTheme::StepperRow(const char* id, const char* label, float* value, float step, const char* format, bool separator)
{
    ImGui::PushID(id);
    RowState row;
    BeginRow("##stepper", separator, row);
    const ImVec2 start = row.start;
    const float width = row.width;
    TraceRow("stepper", start, width);
    DrawRowLabel(label, start, layout::row_h, RowLabelColor());

    const float btn = layout::control_h;
    const float gap = 6.0f;
    const float fieldW = layout::field_w;
    const float y = FieldY(start);
    bool changed = false;

    ImGui::SetCursorScreenPos(ImVec2(start.x + width - (btn + gap + fieldW + gap + btn), y));
    if (StepButton("##minus", false, btn) && value) { *value -= step; changed = true; }

    ImGui::SetCursorScreenPos(ImVec2(start.x + width - (fieldW + gap + btn), y));
    ImGui::SetNextItemWidth(fieldW);
    PushFieldStyle();
    changed |= ImGui::InputFloat("##field", value, 0.0f, 0.0f, format ? format : "%.3f");
    PopFieldStyle();

    ImGui::SetCursorScreenPos(ImVec2(start.x + width - btn, y));
    if (StepButton("##plus", true, btn) && value) { *value += step; changed = true; }

    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + layout::row_h + (separator ? layout::separator_h : 0.0f)));
    ImGui::PopID();
    return changed;
}

bool ConsoleTheme::StepperRow3(const char* id,
                               const char* labelA, float* valueA, float stepA, const char* formatA,
                               const char* labelB, float* valueB, float stepB, const char* formatB,
                               const char* labelC, float* valueC, float stepC, const char* formatC,
                               bool separator)
{
    ImGui::PushID(id);
    RowState row;
    BeginRow("##stepper3", separator, row);
    const ImVec2 start = row.start;
    const float width = row.width;
    TraceRow("stepper3", start, width);

    const float colW = width / 3.0f;
    const float fieldW = 88.0f;
    const float btn = layout::control_h;
    const float gap = 6.0f;
    const float y = FieldY(start);
    bool changed = false;

    const char* labels[3] = { labelA, labelB, labelC };
    float* values[3] = { valueA, valueB, valueC };
    const float steps[3] = { stepA, stepB, stepC };
    const char* formats[3] = { formatA, formatB, formatC };

    for (int i = 0; i < 3; ++i)
    {
        const float labelX = start.x + colW * static_cast<float>(i);
        const float right = start.x + colW * static_cast<float>(i + 1) - (i < 2 ? 14.0f : 0.0f);
        if (labels[i])
            DrawRowLabel(labels[i], ImVec2(labelX, start.y), layout::row_h, RowLabelColor());
        if (!values[i])
            continue;

        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(ImVec2(right - (btn + gap + fieldW + gap + btn), y));
        if (StepButton("##minus", false, btn)) { *values[i] -= steps[i]; changed = true; }

        ImGui::SetCursorScreenPos(ImVec2(right - (fieldW + gap + btn), y));
        ImGui::SetNextItemWidth(fieldW);
        PushFieldStyle();
        changed |= ImGui::InputFloat("##field", values[i], 0.0f, 0.0f, formats[i] ? formats[i] : "%.6f");
        PopFieldStyle();

        ImGui::SetCursorScreenPos(ImVec2(right - btn, y));
        if (StepButton("##plus", true, btn)) { *values[i] += steps[i]; changed = true; }
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + layout::row_h + (separator ? layout::separator_h : 0.0f)));
    ImGui::PopID();
    return changed;
}

float ConsoleTheme::ChipWidth(const char* label)
{
    ImFont* font = AppFonts::Regular;
    const float size = font ? font->FontSize : ImGui::GetFontSize();
    const char* text = label ? label : "";
    const float tw = font ? font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x : ImGui::CalcTextSize(text).x;
    return tw + 28.0f;
}

bool ConsoleTheme::ChipButton(const char* id, const char* label, float width)
{
    ImGui::PushID(id);
    const float w = width > 0.0f ? width : ChipWidth(label);
    const float h = layout::button_h;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 max(start.x + w, start.y + h);
    const bool pressed = ImGui::InvisibleButton("##chip", ImVec2(w, h));
    const bool hovered = ImGui::IsItemHovered();
    const float hover = Anim((std::string("chip:") + id + ":h").c_str(), hovered ? 1.0f : 0.0f, 20.0f);
    const float press = ImGui::IsItemActive() ? 1.0f : 0.0f;

    // 自检：把当前“正在发光”的芯片 id 记进 ui_check.txt —— 指针停在某个芯片上时，
    // 这里应当只出现 1 个芯片（若出现多个同位置不同卡片的芯片，说明动画键不够唯一）。
    if (hover > 0.01f)
    {
        char note[192];
        snprintf(note, sizeof(note), "CHIPANIM %s hover=%.2f pressed=%d", id, hover, pressed ? 1 : 0);
        TraceNote(note);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec4 fill = MixV(g_colors.control, g_colors.accent, 0.10f * hover + 0.06f * press);
    dl->AddRectFilled(start, max, U32(fill, 1.0f), layout::control_round);
    dl->AddRect(start, max,
                U32(hover > 0.01f ? MixV(Ink(0.10f), g_colors.accent, 0.55f * hover) : Ink(0.10f), 1.0f),
                layout::control_round, ImDrawFlags_RoundCornersAll, 1.0f);
    if (hover > 0.01f)
    {
        dl->AddLine(ImVec2(start.x + layout::control_round, start.y + 0.5f),
                    ImVec2(max.x - layout::control_round, start.y + 0.5f), U32(g_colors.ink, 0.10f * hover), 1.0f);
    }

    TextCentered(dl, AppFonts::Regular, start, max,
                 U32(hover > 0.01f ? MixV(g_colors.text, g_colors.accent, 0.75f * hover) : g_colors.text, 1.0f), label);
    ImGui::PopID();
    return pressed;
}

float ConsoleTheme::ChipGridHeight(const char* const* labels, int count, float avail, float gap)
{
    if (!labels || count <= 0)
        return 0.0f;
    float x = 0.0f;
    int rows = 1;
    for (int i = 0; i < count; ++i)
    {
        const float w = ChipWidth(labels[i]);
        if (x > 0.0f && x + w > avail)
        {
            x = 0.0f;
            rows += 1;
        }
        x += w + gap;
    }
    return static_cast<float>(rows) * layout::button_h + static_cast<float>(rows - 1) * gap;
}

int ConsoleTheme::ChipGrid(const char* id, const char* const* labels, int count, float avail, float gap)
{
    if (!labels || count <= 0)
        return -1;
    ImGui::PushID(id);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    float x = 0.0f;
    float y = 0.0f;
    int clicked = -1;
    int row = 0;

    for (int i = 0; i < count; ++i)
    {
        const float w = ChipWidth(labels[i]);
        if (x > 0.0f && x + w > avail)
        {
            x = 0.0f;
            y += layout::button_h + gap;
            row += 1;
        }
        ImGui::SetCursorScreenPos(ImVec2(origin.x + x, origin.y + y));
        // 自检：芯片矩形（供探针精确定位“悬停/点击”，不再靠猜坐标）
        {
            char rectNote[192];
            snprintf(rectNote, sizeof(rectNote), "CHIPRECT %s i=%d x=%.0f y=%.0f w=%.0f h=%.0f",
                     id, i, origin.x + x, origin.y + y, w, layout::button_h);
            TraceNote(rectNote);
        }
        // 芯片 ID 必须带上所属卡片 id：否则同一 (行,列) 的芯片在各卡片间共用
        // 同一个动画键，悬停/按下时会同时点亮每张卡里同位置的芯片。
        char chipId[96];
        snprintf(chipId, sizeof(chipId), "%s_c%d_%d", id, row, i);
        if (ChipButton(chipId, labels[i], w))
            clicked = i;
        x += w + gap;
    }

    const float totalH = ChipGridHeight(labels, count, avail, gap);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + totalH));
    ImGui::PopID();
    return clicked;
}

void ConsoleTheme::NoteRow(const char* text, bool warning, bool separator)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = RowWidth();
    TraceRow("note", start, width);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = SmallFont();
    const float size = font ? font->FontSize : ImGui::GetFontSize();
    const float textY = start.y + (layout::row_h - size) * 0.5f - 1.0f;

    if (warning)
    {
        dl->AddRectFilled(ImVec2(start.x, start.y + 6.0f), ImVec2(start.x + 3.0f, start.y + layout::row_h - 6.0f),
                          U32(g_colors.warning, 0.85f), 1.5f);
        Text(dl, font, ImVec2(start.x + 12.0f, textY), U32(g_colors.warning, 1.0f), text);
    }
    else
    {
        Text(dl, font, ImVec2(start.x + 12.0f, textY), U32(g_colors.textMuted, 0.95f), text);
    }

    if (separator)
        DrawRowLine(start, width);
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + layout::row_h + (separator ? layout::separator_h : 0.0f)));
}

void ConsoleTheme::RenderThemeSelector()
{
    const char* current = ThemeName(g_theme);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::BeginCombo("##console_theme", current))
    {
        for (int i = 0; i < kConsoleThemeCount; ++i)
        {
            const auto id = static_cast<ConsoleThemeId>(i);
            const bool selected = (id == g_theme);
            if (ImGui::Selectable(ThemeName(id), selected))
                SetTheme(id);
            if (selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

void ConsoleTheme::RenderThemeSwatches()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float size = 34.0f;
    for (int i = 0; i < kConsoleThemeCount; ++i)
    {
        const auto id = static_cast<ConsoleThemeId>(i);
        const bool selected = (id == g_theme);
        ImGui::PushID(100 + i);
        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##th", ImVec2(size, size));
        const bool hovered = ImGui::IsItemHovered();
        const float hover = Anim((std::string("thw") + std::to_string(i)).c_str(), hovered ? 1.0f : 0.0f, 18.0f);
        const ImVec2 max(start.x + size, start.y + size);

        const bool light = (id == ConsoleThemeId::Light);
        dl->AddRectFilled(start, max, U32(light ? V(1.0f, 1.0f, 1.0f, 0.85f) : V(0.0f, 0.0f, 0.0f, 0.72f)), 8.0f);
        dl->AddRectFilled(ImVec2(start.x + 5.0f, max.y - 11.0f), ImVec2(max.x - 5.0f, max.y - 6.0f),
                          U32(g_colors.accent, 0.9f), 2.0f);
        dl->AddRect(start, max, U32(Ink(0.15f + 0.25f * hover), 1.0f), 8.0f, ImDrawFlags_RoundCornersAll, 1.0f);
        if (selected)
            dl->AddRect(ImVec2(start.x - 3.0f, start.y - 3.0f), ImVec2(max.x + 3.0f, max.y + 3.0f), U32(g_colors.accent, 1.0f), 10.0f, ImDrawFlags_RoundCornersAll, 2.0f);

        if (ImGui::IsItemClicked())
            SetTheme(id);
        if (hovered)
            ImGui::SetTooltip("%s", ThemeName(id));
        ImGui::PopID();
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

void ConsoleTheme::RenderAccentSwatches()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float size = 28.0f;
    for (int i = 0; i < kAccentCount; ++i)
    {
        const auto id = static_cast<AccentId>(i);
        const bool selected = (id == g_accent);
        ImGui::PushID(200 + i);
        const ImVec2 start = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##ac", ImVec2(size, size));
        const bool hovered = ImGui::IsItemHovered();
        const float hover = Anim((std::string("acs") + std::to_string(i)).c_str(), hovered ? 1.0f : 0.0f, 20.0f);
        const ImVec2 center(start.x + size * 0.5f, start.y + size * 0.5f);
        const ImVec4 color = kAccents[i];

        dl->AddCircleFilled(center, size * 0.42f + hover * 1.5f, U32(color, 0.22f + 0.2f * hover), 28);
        dl->AddCircleFilled(center, size * 0.32f, U32(color, 1.0f), 28);
        if (selected)
            dl->AddCircle(center, size * 0.46f, U32(g_colors.text, 0.9f), 30, 2.0f);

        if (ImGui::IsItemClicked())
            SetAccent(id);
        if (hovered)
            ImGui::SetTooltip("%s", kAccentNames[i]);
        ImGui::PopID();
        ImGui::SameLine();
    }
    ImGui::NewLine();
}

// ─────────────────────────────────────────────────────────────────────────────
// 多列卡片排布（与 WeaponInspector / MenuManager 内部版本同构，这里提到主题层共用）
// ─────────────────────────────────────────────────────────────────────────────
static const float kCardGap = 10.0f;      // 同列卡片之间的竖向间距

void ConsoleTheme::Columns::Begin(int columnCount)
{
    origin = ImGui::GetCursorScreenPos();
    gap    = layout::content_gap;
    count  = (columnCount < 1) ? 1 : (columnCount > (int)kMaxColumns ? (int)kMaxColumns : columnCount);

    const float avail = ImGui::GetContentRegionAvail().x;
    width = (avail - gap * static_cast<float>(count - 1)) / static_cast<float>(count);
    for (int i = 0; i < kMaxColumns; ++i)
        y[i] = origin.y;
}

void ConsoleTheme::Columns::Place(int column)
{
    const int col = (column < 0 || column >= count) ? 0 : column;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + static_cast<float>(col) * (width + gap), y[col]));
}

void ConsoleTheme::Columns::Advance(int column, float cardHeight)
{
    const int col = (column < 0 || column >= count) ? 0 : column;
    y[col] += cardHeight + kCardGap;
}

int ConsoleTheme::Columns::Shortest() const
{
    int best = 0;
    for (int i = 1; i < count; ++i)
    {
        if (y[i] < y[best])
            best = i;
    }
    return best;
}

void ConsoleTheme::Columns::End()
{
    float maxY = origin.y;
    for (int i = 0; i < count; ++i)
    {
        if (y[i] > maxY)
            maxY = y[i];
    }
    char note[160];
    std::snprintf(note, sizeof(note), "COLUMN count=%d maxDelta=%.0f contentTop=%.0f",
                  count, maxY - origin.y, origin.y);
    TraceNote(note);
    ImGui::SetCursorScreenPos(ImVec2(origin.x, maxY));
    ImGui::Dummy(ImVec2(width, 0.0f));
}

float ConsoleTheme::TitledBoxHeight(int rows) { return layout::box_height(rows) + layout::section_h; }
float ConsoleTheme::TitledBoxPixels(float height) { return height + layout::section_h; }

bool ConsoleTheme::StepperRow2(const char* id,
                               const char* labelA, float* valueA, float stepA, const char* formatA,
                               const char* labelB, float* valueB, float stepB, const char* formatB,
                               bool separator)
{
    ImGui::PushID(id);
    RowState row;
    BeginRow("##stepper2", separator, row);
    const ImVec2 start = row.start;
    const float width = row.width;
    TraceRow("stepper2", start, width);

    const float colW = width * 0.5f;
    const float fieldW = 88.0f;
    const float btn = layout::control_h;
    const float gap = 6.0f;
    const float y = FieldY(start);
    bool changed = false;

    const char* labels[2] = { labelA, labelB };
    float* values[2] = { valueA, valueB };
    const float steps[2] = { stepA, stepB };
    const char* formats[2] = { formatA, formatB };

    for (int i = 0; i < 2; ++i)
    {
        const float labelX = start.x + colW * static_cast<float>(i);
        const float right = start.x + colW * static_cast<float>(i + 1) - (i < 1 ? 14.0f : 0.0f);
        if (labels[i])
            DrawRowLabel(labels[i], ImVec2(labelX, start.y), layout::row_h, RowLabelColor());
        if (!values[i])
            continue;

        ImGui::PushID(i);
        ImGui::SetCursorScreenPos(ImVec2(right - (btn + gap + fieldW + gap + btn), y));
        if (StepButton("##minus", false, btn)) { *values[i] -= steps[i]; changed = true; }

        ImGui::SetCursorScreenPos(ImVec2(right - (fieldW + gap + btn), y));
        ImGui::SetNextItemWidth(fieldW);
        PushFieldStyle();
        changed |= ImGui::InputFloat("##field", values[i], 0.0f, 0.0f, formats[i] ? formats[i] : "%.6f");
        PopFieldStyle();

        ImGui::SetCursorScreenPos(ImVec2(right - btn, y));
        if (StepButton("##plus", true, btn)) { *values[i] += steps[i]; changed = true; }
        ImGui::PopID();
    }

    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + layout::row_h + (separator ? layout::separator_h : 0.0f)));
    ImGui::PopID();
    return changed;
}
