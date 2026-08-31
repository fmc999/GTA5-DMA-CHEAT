#include "pch.h"

#include "ConsoleTheme.h"
#include "AppFonts.h"

namespace
{
struct Palette
{
    const char* name;
    ImVec4 accent;      // 主强调色
    ImVec4 accentSoft;  // 弱化强调（选中行底色等）
    ImVec4 bg;          // 主背景
    ImVec4 bgDeep;      // 更深背景（页眉 / 状态栏）
    ImVec4 child;       // 子区域背景
    ImVec4 frame;       // 输入框 / 按钮底色
    ImVec4 frameHover;
    ImVec4 frameActive;
    ImVec4 text;
    ImVec4 textDim;
    ImVec4 border;
    ImVec4 success;
    ImVec4 warning;
    ImVec4 danger;
    ImVec4 header;      // 列表头 / 选中项
    ImVec4 headerHover;
    ImVec4 headerActive;
};

constexpr ImVec4 C(float r, float g, float b, float a = 1.0f)
{
    return ImVec4(r, g, b, a);
}

// clang-format off
const Palette kPalettes[] = {
    { "午夜蓝",
      C(0.310f, 0.640f, 0.930f), C(0.110f, 0.220f, 0.330f),
      C(0.043f, 0.058f, 0.078f), C(0.030f, 0.040f, 0.055f), C(0.063f, 0.086f, 0.114f),
      C(0.102f, 0.137f, 0.180f), C(0.130f, 0.180f, 0.240f), C(0.150f, 0.220f, 0.300f),
      C(0.900f, 0.930f, 0.960f), C(0.540f, 0.600f, 0.680f), C(0.130f, 0.170f, 0.220f),
      C(0.310f, 0.780f, 0.560f), C(0.920f, 0.650f, 0.300f), C(0.910f, 0.360f, 0.360f),
      C(0.090f, 0.170f, 0.240f), C(0.120f, 0.250f, 0.360f), C(0.150f, 0.350f, 0.510f) },
    { "石墨灰",
      C(0.590f, 0.660f, 0.930f), C(0.170f, 0.190f, 0.280f),
      C(0.055f, 0.055f, 0.062f), C(0.038f, 0.038f, 0.044f), C(0.080f, 0.080f, 0.090f),
      C(0.130f, 0.132f, 0.145f), C(0.165f, 0.168f, 0.185f), C(0.200f, 0.205f, 0.225f),
      C(0.910f, 0.920f, 0.940f), C(0.560f, 0.575f, 0.600f), C(0.160f, 0.163f, 0.178f),
      C(0.420f, 0.800f, 0.600f), C(0.930f, 0.680f, 0.340f), C(0.920f, 0.400f, 0.400f),
      C(0.120f, 0.125f, 0.150f), C(0.160f, 0.170f, 0.210f), C(0.200f, 0.215f, 0.280f) },
    { "海洋青",
      C(0.176f, 0.831f, 0.749f), C(0.070f, 0.240f, 0.230f),
      C(0.031f, 0.065f, 0.078f), C(0.020f, 0.045f, 0.055f), C(0.050f, 0.100f, 0.115f),
      C(0.080f, 0.150f, 0.170f), C(0.105f, 0.190f, 0.215f), C(0.130f, 0.235f, 0.265f),
      C(0.880f, 0.950f, 0.940f), C(0.510f, 0.640f, 0.640f), C(0.100f, 0.170f, 0.190f),
      C(0.310f, 0.780f, 0.560f), C(0.920f, 0.650f, 0.300f), C(0.910f, 0.360f, 0.360f),
      C(0.070f, 0.190f, 0.200f), C(0.100f, 0.260f, 0.270f), C(0.130f, 0.340f, 0.350f) },
    { "绯红",
      C(0.930f, 0.360f, 0.420f), C(0.300f, 0.110f, 0.140f),
      C(0.070f, 0.043f, 0.051f), C(0.048f, 0.028f, 0.035f), C(0.105f, 0.065f, 0.078f),
      C(0.165f, 0.105f, 0.122f), C(0.210f, 0.135f, 0.155f), C(0.260f, 0.165f, 0.190f),
      C(0.940f, 0.910f, 0.920f), C(0.620f, 0.560f, 0.590f), C(0.200f, 0.130f, 0.150f),
      C(0.310f, 0.780f, 0.560f), C(0.920f, 0.650f, 0.300f), C(0.930f, 0.360f, 0.360f),
      C(0.150f, 0.090f, 0.110f), C(0.210f, 0.120f, 0.145f), C(0.280f, 0.155f, 0.190f) },
};
// clang-format on

ConsoleThemeId g_currentTheme = ConsoleThemeId::Midnight;

const Palette& CurrentPalette()
{
    return kPalettes[static_cast<int>(g_currentTheme)];
}

ImU32 WithAlpha(const ImVec4& color, float alpha)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, alpha));
}
} // namespace

/* ---------- 主题管理 ---------- */

void ConsoleTheme::Apply()
{
    ImGuiStyle& style = ImGui::GetStyle();
    const Palette& p = CurrentPalette();

    style.WindowPadding    = ImVec2(14.0f, 14.0f);
    style.FramePadding     = ImVec2(10.0f, 7.0f);
    style.ItemSpacing      = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize    = 11.0f;
    style.GrabMinSize      = 10.0f;
    style.WindowRounding   = 8.0f;
    style.ChildRounding    = 6.0f;
    style.FrameRounding    = 6.0f;
    style.PopupRounding    = 6.0f;
    style.GrabRounding     = 6.0f;
    style.TabRounding      = 5.0f;
    style.CellPadding      = ImVec2(10.0f, 8.0f);
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize  = 0.0f;
    style.PopupBorderSize  = 1.0f;
    style.FrameBorderSize  = 0.0f;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text]                 = p.text;
    c[ImGuiCol_TextDisabled]         = p.textDim;
    c[ImGuiCol_WindowBg]             = p.bg;
    c[ImGuiCol_ChildBg]              = p.child;
    c[ImGuiCol_PopupBg]              = C(p.bg.x * 1.1f, p.bg.y * 1.1f, p.bg.z * 1.1f, 0.99f);
    c[ImGuiCol_Border]               = p.border;
    c[ImGuiCol_FrameBg]              = p.frame;
    c[ImGuiCol_FrameBgHovered]       = p.frameHover;
    c[ImGuiCol_FrameBgActive]        = p.frameActive;
    c[ImGuiCol_TitleBg]              = p.bgDeep;
    c[ImGuiCol_TitleBgActive]        = p.bgDeep;
    c[ImGuiCol_TitleBgCollapsed]     = p.bgDeep;
    c[ImGuiCol_CheckMark]            = p.accent;
    c[ImGuiCol_SliderGrab]           = C(p.accent.x * 0.85f, p.accent.y * 0.85f, p.accent.z * 0.85f, 1.0f);
    c[ImGuiCol_SliderGrabActive]     = p.accent;
    c[ImGuiCol_Button]               = p.frame;
    c[ImGuiCol_ButtonHovered]        = p.frameHover;
    c[ImGuiCol_ButtonActive]         = p.frameActive;
    c[ImGuiCol_Header]               = p.header;
    c[ImGuiCol_HeaderHovered]        = p.headerHover;
    c[ImGuiCol_HeaderActive]         = p.headerActive;
    c[ImGuiCol_Separator]            = p.border;
    c[ImGuiCol_SeparatorActive]      = p.accent;
    c[ImGuiCol_SeparatorHovered]     = p.accent;
    c[ImGuiCol_ResizeGrip]           = C(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_ResizeGripHovered]    = C(p.accent.x, p.accent.y, p.accent.z, 0.35f);
    c[ImGuiCol_ResizeGripActive]     = C(p.accent.x, p.accent.y, p.accent.z, 0.60f);
    c[ImGuiCol_Tab]                  = p.frame;
    c[ImGuiCol_TabHovered]           = p.headerHover;
    c[ImGuiCol_TabActive]            = p.headerActive;
    c[ImGuiCol_TabUnfocused]         = p.frame;
    c[ImGuiCol_TabUnfocusedActive]   = p.header;
    c[ImGuiCol_ScrollbarBg]          = p.bgDeep;
    c[ImGuiCol_ScrollbarGrab]        = p.frame;
    c[ImGuiCol_ScrollbarGrabHovered] = p.frameHover;
    c[ImGuiCol_ScrollbarGrabActive]  = p.frameActive;
    c[ImGuiCol_TableHeaderBg]        = p.frame;
    c[ImGuiCol_TableRowBg]           = C(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TableRowBgAlt]        = C(p.child.x, p.child.y, p.child.z, 0.55f);
    c[ImGuiCol_TableBorderStrong]    = p.border;
    c[ImGuiCol_TableBorderLight]     = p.border;
    c[ImGuiCol_NavHighlight]         = p.accent;
    c[ImGuiCol_ModalWindowDimBg]     = C(0.0f, 0.0f, 0.0f, 0.55f);
}

void ConsoleTheme::SetTheme(ConsoleThemeId id)
{
    g_currentTheme = id;
    Apply();
}

ConsoleThemeId ConsoleTheme::GetTheme()
{
    return g_currentTheme;
}

const char* ConsoleTheme::ThemeName(ConsoleThemeId id)
{
    return kPalettes[static_cast<int>(id)].name;
}

void ConsoleTheme::RenderThemeSelector()
{
    const char* current = ThemeName(g_currentTheme);
    ImGui::SetNextItemWidth(240.0f);
    if (ImGui::BeginCombo("##console_theme", current)) {
        for (int i = 0; i < static_cast<int>(ConsoleThemeId::Crimson) + 1; ++i) {
            const auto id = static_cast<ConsoleThemeId>(i);
            const bool selected = (id == g_currentTheme);
            if (ImGui::Selectable(ThemeName(id), selected)) {
                SetTheme(id);
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
}

/* ---------- 主题色访问器 ---------- */

ImVec4 ConsoleTheme::Accent()         { return CurrentPalette().accent; }
ImVec4 ConsoleTheme::Success()        { return CurrentPalette().success; }
ImVec4 ConsoleTheme::Warning()        { return CurrentPalette().warning; }
ImVec4 ConsoleTheme::Danger()         { return CurrentPalette().danger; }
ImVec4 ConsoleTheme::Background()     { return CurrentPalette().bg; }
ImVec4 ConsoleTheme::BackgroundDeep() { return CurrentPalette().bgDeep; }

/* ---------- 共享控件 ---------- */

void ConsoleTheme::SectionHeader(const char* title, const char* description)
{
    const Palette& p = CurrentPalette();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float cursorX = ImGui::GetCursorPosX();

    drawList->AddRectFilled(start, ImVec2(start.x + 3.0f, start.y + 17.0f), WithAlpha(p.accent, 1.0f), 1.5f);
    ImGui::SetCursorPosX(cursorX + 11.0f);
    if (AppFonts::Bold) ImGui::PushFont(AppFonts::Bold);
    ImGui::TextUnformatted(title);
    if (AppFonts::Bold) ImGui::PopFont();
    if (description && description[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", description);
    }
    ImGui::SetCursorPosX(cursorX);
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    const ImVec2 line = ImGui::GetCursorScreenPos();
    drawList->AddLine(line, ImVec2(line.x + ImGui::GetContentRegionAvail().x, line.y), WithAlpha(p.border, 1.0f));
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
}

bool ConsoleTheme::ToggleRow(const char* id, const char* label, const char* description, bool* value)
{
    ImGui::PushID(id);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = description && description[0] != '\0' ? 50.0f : 38.0f;
    ImGui::InvisibleButton("##toggle_row", ImVec2(width, height));

    bool changed = false;
    if (ImGui::IsItemClicked()) {
        *value = !*value;
        changed = true;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const Palette& p = CurrentPalette();
    const ImVec2 end(start.x + width, start.y + height);
    if (ImGui::IsItemHovered()) {
        drawList->AddRectFilled(start, end, WithAlpha(p.accent, 0.08f), 6.0f);
    }
    drawList->AddLine(ImVec2(start.x, end.y), end, WithAlpha(p.border, 0.8f));

    const float labelY = description && description[0] != '\0' ? start.y + 7.0f : start.y + 10.0f;
    if (AppFonts::Bold) ImGui::PushFont(AppFonts::Bold);
    drawList->AddText(ImVec2(start.x + 10.0f, labelY), ImGui::GetColorU32(ImGuiCol_Text), label);
    if (AppFonts::Bold) ImGui::PopFont();
    if (description && description[0] != '\0') {
        drawList->AddText(ImVec2(start.x + 10.0f, start.y + 27.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), description);
    }

    constexpr float trackWidth = 36.0f;
    constexpr float trackHeight = 20.0f;
    const ImVec2 trackMin(end.x - trackWidth - 10.0f, start.y + (height - trackHeight) * 0.5f);
    const ImVec2 trackMax(trackMin.x + trackWidth, trackMin.y + trackHeight);
    drawList->AddRectFilled(trackMin, trackMax,
        *value ? WithAlpha(p.accent, 0.95f) : WithAlpha(p.frame, 1.0f), trackHeight * 0.5f);
    const float knobX = *value ? trackMax.x - 10.0f : trackMin.x + 10.0f;
    drawList->AddCircleFilled(ImVec2(knobX, trackMin.y + trackHeight * 0.5f), 7.0f, IM_COL32(240, 244, 248, 255));

    ImGui::PopID();
    return changed;
}

bool ConsoleTheme::NavItem(const char* label, bool selected)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = 36.0f;
    ImGui::InvisibleButton(label, ImVec2(width, height));
    const bool pressed = ImGui::IsItemClicked();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const Palette& p = CurrentPalette();
    const ImVec2 end(start.x + width, start.y + height);
    if (selected) {
        drawList->AddRectFilled(start, end, WithAlpha(p.accent, 0.16f), 6.0f);
    } else if (ImGui::IsItemHovered()) {
        drawList->AddRectFilled(start, end, WithAlpha(p.text, 0.06f), 6.0f);
    }
    if (selected) {
        drawList->AddRectFilled(ImVec2(start.x, start.y + 8.0f), ImVec2(start.x + 3.0f, end.y - 8.0f), WithAlpha(p.accent, 1.0f), 1.5f);
    }

    const ImU32 labelColor = selected ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
    drawList->AddText(ImVec2(start.x + 14.0f, start.y + (height - ImGui::GetTextLineHeight()) * 0.5f), labelColor, label);
    return pressed;
}

void ConsoleTheme::StatPill(const char* label, const char* value, bool good)
{
    const Palette& p = CurrentPalette();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec4& dotColor = good ? p.success : p.warning;
    const ImVec2 textSize = ImGui::CalcTextSize(value);
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    constexpr float pad = 10.0f;
    const float width = pad + 7.0f + 6.0f + labelSize.x + 6.0f + textSize.x + pad;
    constexpr float height = 24.0f;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    drawList->AddRectFilled(start, ImVec2(start.x + width, start.y + height), WithAlpha(p.bgDeep, 1.0f), height * 0.5f);
    drawList->AddCircleFilled(ImVec2(start.x + pad + 3.0f, start.y + height * 0.5f), 3.5f, WithAlpha(dotColor, 1.0f));
    drawList->AddText(ImVec2(start.x + pad + 7.0f + 6.0f, start.y + (height - labelSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), label);
    drawList->AddText(ImVec2(start.x + pad + 7.0f + 6.0f + labelSize.x + 6.0f, start.y + (height - textSize.y) * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), value);
    ImGui::Dummy(ImVec2(width, height));
}

void ConsoleTheme::StatusDot(bool ok, const char* label)
{
    const Palette& p = CurrentPalette();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    drawList->AddCircleFilled(ImVec2(start.x + 4.0f, start.y + ImGui::GetTextLineHeight() * 0.5f), 3.5f,
        WithAlpha(ok ? p.success : p.warning, 1.0f));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 14.0f);
    ImGui::TextUnformatted(label);
}
