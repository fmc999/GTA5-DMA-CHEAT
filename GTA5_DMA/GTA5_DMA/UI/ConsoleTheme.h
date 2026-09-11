#pragma once

#include "imgui.h"

// ===========================================================================
//  Portfolio #8 视觉系统
//  设计语言：暗色玻璃（壁纸 + 毛玻璃面板）、圆角 14 外壳、圆角 6 内容盒、
//  37px 细行 + 1px 分隔线、42x22 胶囊开关、细杆滑条、强调色 #615DCE。
//  颜色 / 尺寸常量与参考实现一一对应（framework/theme/colors.h、layout.h）。
// ===========================================================================

// 基础明暗模式（对应参考实现的 night / light 两套玻璃色）
enum class ConsoleThemeId
{
    Night,   // 夜色玻璃（默认）
    Light,   // 晨光玻璃
};

inline constexpr int kConsoleThemeCount = 2;

// 强调色（参考实现设置面板里的 accent color）
enum class AccentId
{
    Indigo,   // #615DCE 默认
    Azure,    // #4FA3ED
    Teal,     // #18C79E
    Rose,     // #E0536B
    Amber,    // #F0A030
    Violet,   // #C77DFF
};

inline constexpr int kAccentCount = 6;

// 矢量图标（Lucide 风格描边，24px 设计框，不依赖图标字体）
enum class UiIcon
{
    None,
    Crosshair,
    Users,
    Globe,
    Sliders,
    Folder,
    Gear,
    Search,
    Save,
    Car,
    Zap,
    Shield,
    Eye,
    Heart,
    Pin,
    Map,
    Grid,
    Menu,
    Check,
    Close,
    Chevron,
    ChevronDown,
    Refresh,
    Trash,
    Plus,
    Activity,
    Target,
    Ellipsis,
    Dot,
};

namespace layout
{
    // 参考实现（framework/theme/layout.h）的袖珍版：控制台窗口更小，等比收敛。
    inline constexpr float shell_round      = 14.0f;
    inline constexpr float shell_inset      = 10.0f;   // 面板四周留出壁纸（旧的贴边形态）

    // 悬浮窗（融合器模式）：ImGui 面板本身不再铺满窗口，而是一块浮在壁纸上的小窗——
    // 默认尺寸固定，可在窗口内拖动、可从右下角手柄缩放，几何存 ini。
    inline constexpr float panel_w          = 1180.0f;
    inline constexpr float panel_h          = 780.0f;
    // 最小尺寸 = 默认尺寸：拉到最小也不会裁掉内容（1180x780 时六个页面全部无需滚动）
    inline constexpr float panel_min_w      = 1180.0f;
    inline constexpr float panel_min_h      = 780.0f;
    inline constexpr float panel_edge       = 18.0f;   // 浮窗与窗口边缘的最小留白
    inline constexpr float panel_grip       = 20.0f;   // 右下角缩放手柄边长

    inline constexpr float sidebar_w        = 243.0f;
    inline constexpr float sidebar_tab_h    = 46.0f;
    inline constexpr float sidebar_tab_icon = 22.0f;
    inline constexpr float sidebar_icon_x   = 20.0f;
    inline constexpr float sidebar_text_gap = 12.0f;
    inline constexpr float sidebar_logo_y   = 22.0f;

    inline constexpr float topbar_h         = 74.0f;
    inline constexpr float topbar_row_y     = 16.0f;
    inline constexpr float topbar_row_h     = 42.0f;
    inline constexpr float topbar_icon      = 22.0f;

    inline constexpr float content_pad_x    = 19.0f;
    inline constexpr float content_gap      = 16.0f;

    inline constexpr float box_round        = 6.0f;
    inline constexpr float box_pad_x        = 14.0f;
    inline constexpr float box_pad_y        = 8.0f;
    inline constexpr float row_h            = 37.0f;
    inline constexpr float separator_h      = 1.0f;
    inline constexpr float section_h        = 24.0f;

    inline constexpr float toggle_w         = 42.0f;
    inline constexpr float toggle_h         = 22.0f;
    inline constexpr float toggle_knob_r    = 8.0f;
    inline constexpr float toggle_inset     = 11.0f;

    inline constexpr float control_h        = 26.0f;
    inline constexpr float control_round    = 4.0f;
    inline constexpr float button_h         = 34.0f;
    inline constexpr float slider_h         = 6.0f;
    inline constexpr float slider_w         = 180.0f;
    inline constexpr float slider_value_w   = 56.0f;
    inline constexpr float field_w          = 116.0f;  // 数值输入框统一宽度（保证各行控件列对齐）

    // 内容盒高度：上下留白 + 行高 + 行间 1px 分隔线
    inline constexpr float box_height(int rows)
    {
        return rows <= 0
            ? box_pad_y * 2.0f
            : box_pad_y * 2.0f + static_cast<float>(rows) * row_h + static_cast<float>(rows - 1) * separator_h;
    }
}

// 玻璃配色（随明暗模式重算）
struct ConsoleColors
{
    ImVec4 accent;
    ImVec4 panel;          // 主面板底
    ImVec4 sidebar;        // 侧栏底
    ImVec4 box;            // 内容盒底
    ImVec4 control;        // 输入 / 按钮底
    ImVec4 controlHover;
    ImVec4 text;
    ImVec4 textMuted;
    ImVec4 headerText;     // 分节标题（极淡）
    ImVec4 separator;
    ImVec4 toggleOff;
    ImVec4 toggleOn;
    ImVec4 knobOff;
    ImVec4 sliderBg;
    ImVec4 sliderFill;
    ImVec4 success;
    ImVec4 danger;
    ImVec4 warning;
    ImVec4 popupBg;
    ImVec4 ink;            // 前景基色（明暗翻转）
};

class ConsoleTheme
{
public:
    /* 主题 / 强调色 */
    static void Apply();                                   // 写入 ImGuiStyle
    static void SetTheme(ConsoleThemeId id);
    static ConsoleThemeId GetTheme();
    static const char* ThemeName(ConsoleThemeId id);
    static int ThemeCount();

    static void SetAccent(AccentId id);
    static AccentId GetAccent();
    static const char* AccentName(AccentId id);
    static ImVec4 AccentColor(AccentId id);
    static int AccentCount();

    static void RenderThemeSelector();                     // 设置页：明暗下拉
    static void RenderThemeSwatches();                     // 设置页：明暗预览块
    static void RenderAccentSwatches();                    // 设置页：强调色块

    /* 配色访问 */
    static const ConsoleColors& Colors();
    static ImVec4 Accent();
    static ImVec4 Panel();
    static ImVec4 Sidebar();
    static ImVec4 Box();
    static ImVec4 Control();
    static ImVec4 ControlHover();
    static ImVec4 TextColor();
    static ImVec4 TextMuted();
    static ImVec4 HeaderText();
    static ImVec4 SeparatorColor();
    static ImVec4 Success();
    static ImVec4 Warning();
    static ImVec4 Danger();
    static ImVec4 Background();        // 壁纸不可用时的回退底色
    static ImVec4 BackgroundDeep();
    static ImVec4 Surface();
    static ImVec4 SurfaceAlt();
    static ImVec4 BorderColor();
    static ImVec4 BackgroundTop();
    static ImVec4 AccentAlt();

    static ImVec4 Ink(float alpha);    // 前景基色 + alpha（明暗自适应）
    static ImVec4 Wash(float alpha);   // 悬停白/黑洗色

    /* 颜色 / 动画工具 */
    static ImU32 U32(const ImVec4& color, float alpha = 1.0f);
    static ImVec4 MixV(const ImVec4& a, const ImVec4& b, float t);
    static ImU32 MixU32(ImU32 a, ImU32 b, float t);
    static float Anim(const char* key, float target, float speed = 20.0f);

    /* 绘制原语 */
    static void Text(ImDrawList* drawList, ImFont* font, ImVec2 pos, ImU32 color, const char* text);
    static void TextCentered(ImDrawList* drawList, ImFont* font, const ImVec2& min, const ImVec2& max, ImU32 color, const char* text);
    static void Icon(UiIcon icon, ImVec2 center, float size, ImU32 color, float thickness = 1.7f);
    static void Glass(ImVec2 min, ImVec2 max, float rounding, const ImVec4& fill, ImDrawFlags flags = ImDrawFlags_RoundCornersAll, bool border = true);
    static void Shadow(ImVec2 min, ImVec2 max, float rounding, float spread, float strength);
    static void HLine(ImVec2 from, float width, ImU32 color);

    /* 布局单元 */
    static void SectionHeader(const char* title, const char* description = nullptr, float width = 0.0f);
    static bool BoxBegin(const char* id, int rows, const char* title = nullptr, float width = 0.0f);
    static bool BoxBeginPixels(const char* id, float height, const char* title = nullptr, float width = 0.0f);
    static void BoxEnd();
    /* ── 多列卡片排布 ───────────────────────────────────────────────
       内容盒都是子窗口，不参与 ImGui 的组布局，所以列必须显式定位：
       Begin(列数) 记录内容区原点与列宽，Place(col) 把光标移到该列当前行，
       Advance(col, 卡片高度) 推进该列游标，Shortest()/End() 负责落位与收尾。
       常规页面用两列；传送点这种“小卡片很多”的页面用三列，避免底部溢出。 */
    struct Columns
    {
        enum { kMaxColumns = 4 };

        ImVec2 origin = ImVec2(0.0f, 0.0f);
        float  width  = 0.0f;
        float  gap    = 16.0f;          // = layout::content_gap
        int    count  = 1;
        float  y[kMaxColumns] = { 0.0f, 0.0f, 0.0f, 0.0f };

        void Begin(int columnCount = 2);
        void Place(int column);
        void Advance(int column, float cardHeight);
        int  Shortest() const;
        void End();
    };


    // 带标题卡片的一格高度 = 标题行(section_h) + 卡片本体，供 Columns::Advance 使用
    static float TitledBoxHeight(int rows);
    static float TitledBoxPixels(float height);

    // 一行两组「标签 + 数值 + 步进按钮」：双列卡片里用，避免三列挤在半宽盒内
    static bool StepperRow2(const char* id,
                            const char* labelA, float* valueA, float stepA, const char* formatA,
                            const char* labelB, float* valueB, float stepB, const char* formatB,
                            bool separator = true);


    /* 行控件（P8 细行 + 1px 分隔线） */
    // ---- 布局自检（只读，不影响绘制）：把盒子/行的实际矩形写盘，用于核对错位 ----
    static void TraceBegin(const char* page, float width, float height);
    static void TraceBoxBegin(const char* id, const ImVec2& min, const ImVec2& max, int rows);
    static void TraceBoxEnd();
    static void TraceRow(const char* kind, const ImVec2& start, float width, float height = 0.0f);
    static void TraceNote(const char* text);
    static void TraceEnd(const char* path);

    static void RowSeparator();
    // 盒子内当前行可用宽度（由 BoxBeginPixels 钉死，行助手统一使用）
    static float RowWidth();
    static bool ToggleRow(const char* id, const char* label, const char* description, bool* value, bool separator = true);
    static bool SliderRow(const char* id, const char* label, float* value, float min, float max, const char* format = "%.2f", bool separator = true, float displayScale = 1.0f);
    static bool MeterRow(const char* label, float value, float maxValue, bool good, bool separator = true);
    static bool TextRow(const char* label, const char* value, bool good = true, bool separator = true);
    // 两列读数：同一行左右各一组「标签 + 右对齐数值」，用于紧凑的只读数据盒
    static bool TextRow2(const char* labelA, const char* valueA, bool goodA,
                         const char* labelB, const char* valueB, bool goodB, bool separator = true);
    // 可编辑数值行：标签在左、输入框统一贴齐行右端（宽度固定 → 各行控件严格对齐）
    static bool InputRow(const char* id, const char* label, float* value, const char* format = "%.3f", bool separator = true);
    static bool ComboRow(const char* id, const char* label, const char* const* items, int itemCount,
                         int* index, const char* description = nullptr, bool separator = true);   // 下拉选择行（保留原生下拉交互，外观与其它行一致）
    static bool InputRow2(const char* id, const char* labelA, float* valueA,
                          const char* labelB, float* valueB, const char* format = "%.3f", bool separator = true);
    static bool IntRow2(const char* id, const char* labelA, int* valueA,
                        const char* labelB, int* valueB, bool separator = true);
    // ---- 载具/传送页通用：步进行 · 芯片网格 · 说明行 ----
    // 步进行：左侧标签，右侧 [−][数值][+]；数值可直接键入，[−][+] 按固定步长增减。
    static bool StepperRow(const char* id, const char* label, float* value, float step,
                           const char* format = "%.3f", bool separator = true);
    // 三列步进行：一盒三列铺满盒宽，避免大屏下每行只有一两个控件而留出大片空白。
    static bool StepperRow3(const char* id,
                            const char* labelA, float* valueA, float stepA, const char* formatA,
                            const char* labelB, float* valueB, float stepB, const char* formatB,
                            const char* labelC, float* valueC, float stepC, const char* formatC,
                            bool separator = true);
    // 芯片按钮：短标签（传送点）网格单元，宽度按文本自适应。
    static bool ChipButton(const char* id, const char* label, float width = 0.0f);
    static float ChipWidth(const char* label);
    // 芯片网格：按可用宽度自动换行，返回被点击下标（-1 为无）。
    static int ChipGrid(const char* id, const char* const* labels, int count, float avail, float gap = 8.0f);
    static float ChipGridHeight(const char* const* labels, int count, float avail, float gap = 8.0f);
    // 说明行：灰字提示 / 黄字警示，占一行，可与卡片行对齐。
    static void NoteRow(const char* text, bool warning = false, bool separator = false);
    static bool ButtonRow(const char* label, UiIcon icon, bool accent = false, bool danger = false);
    static bool NavItem(const char* label, bool selected);
    static bool NavItem(const char* label, bool selected, bool slim);
    static bool NavItemIcon(UiIcon icon, const char* label, bool selected, bool* blocksDrag = nullptr);
    static void StatPill(const char* label, const char* value, bool good);
    static void StatusDot(bool ok, const char* label);
    static void KeyCap(const char* text, float* cursorX, float cursorY, float minWidth = 0.0f);
    static void Badge(const char* text, bool good, float* cursorX, float cursorY);
    static bool AccentButton(const char* label, UiIcon icon, const ImVec2& size);
    static bool DangerButton(const char* label, UiIcon icon, const ImVec2& size);
    static bool GhostButton(const char* label, UiIcon icon, const ImVec2& size);
    static bool IconButton(const char* id, UiIcon icon, float size);
};
