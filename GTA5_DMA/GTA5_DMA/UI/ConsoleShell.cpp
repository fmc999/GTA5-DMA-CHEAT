#include "pch.h"

#include "ConsoleShell.h"

#include "AppFonts.h"
#include "AppRuntime.h"
#include "Backdrop.h"
#include "ConsoleTheme.h"
#include "DMA.h"
#include "Features.h"
#include "InputManager.h"
#include "MenuManager.h"
#include "Offsets.h"
#include "UiToast.h"
#include "WindowState.h"

#include <cmath>
#include <cstdio>

namespace
{
float MaxF(float a, float b) { return a >= b ? a : b; }
float MinF(float a, float b) { return a <= b ? a : b; }

// 悬浮窗输入自检（探针用）：每帧记录手柄 / 拖动条 / 鼠标看到的原始状态
static bool  gDbgGripHot = false, gDbgGripAct = false, gDbgBarHot = false, gDbgBarAct = false;
static bool  gDbgDown = false;
static float gDbgDx = 0.0f, gDbgDy = 0.0f;
float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float LerpF(float a, float b, float t) { return a + (b - a) * t; }

// 旧布局宽度常量保留（窄窗 / 折叠回退用），主布局使用 layout::sidebar_w。
constexpr float kSidebarWidth    = 190.0f;
constexpr float kSidebarSlim     = 56.0f;
constexpr float kHeaderHeight    = 54.0f;
constexpr float kStatusBarHeight = 30.0f;

struct PageDef
{
    MenuPage page;
    UiIcon icon;
    const char* label;
    const char* description;
};

// 侧栏导航（Portfolio #8：图标 + 标签 + 选中渐变 + 左侧强调条）
const PageDef kPages[] = {
    { MenuPage::PLAYER,   UiIcon::Users,     "人物控制", "生命、移动与外观状态" },
    { MenuPage::VEHICLE,  UiIcon::Car,       "载具编辑", "载具状态与操控参数" },
    { MenuPage::WEAPON,   UiIcon::Crosshair, "武器功能", "武器数据与命中参数" },
    { MenuPage::SESSION,  UiIcon::Globe,     "战局玩家", "在线玩家列表与玩家操作" },
    { MenuPage::TELEPORT, UiIcon::Pin,       "位置传送", "坐标、标记点与任务点" },
    { MenuPage::SETTINGS, UiIcon::Gear,      "系统设置", "主题、快捷键与发布信息" },
};

const PageDef& PageInfo(MenuPage page)
{
    for (const PageDef& def : kPages)
        if (def.page == page)
            return def;
    return kPages[0];
}

const char* GameLabel()
{
    switch (currentGameType)
    {
    case GameType::GTA5_Enhanced: return "GTA5 Enhanced";
    case GameType::GTA5:          return "GTA5";
    default:                      return "等待游戏";
    }
}

// 紧凑状态胶囊：圆点 + 标签 + 值，单行 34px
float CompactPill(float x, float y, const char* label, const char* value, bool good)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* smallFont = AppFonts::Small ? AppFonts::Small : AppFonts::Regular;
    ImFont* boldFont = AppFonts::Bold ? AppFonts::Bold : AppFonts::Regular;
    const float smallSize = smallFont ? smallFont->FontSize : ImGui::GetFontSize();
    const float boldSize = boldFont ? boldFont->FontSize : ImGui::GetFontSize();
    const float labelW = smallFont ? smallFont->CalcTextSizeA(smallSize, FLT_MAX, 0.0f, label).x : ImGui::CalcTextSize(label).x;
    const float valueW = boldFont ? boldFont->CalcTextSizeA(boldSize, FLT_MAX, 0.0f, value).x : ImGui::CalcTextSize(value).x;

    constexpr float pad = 14.0f;
    constexpr float height = 34.0f;
    const float width = pad * 2.0f + 13.0f + labelW + 7.0f + valueW;
    const ImVec2 boxMin(x, y);
    const ImVec2 boxMax(x + width, y + height);

    drawList->AddRectFilled(boxMin, boxMax, ConsoleTheme::U32(ConsoleTheme::Box(), 1.0f), layout::box_round);
    drawList->AddRect(boxMin, boxMax, ConsoleTheme::U32(ConsoleTheme::Ink(0.05f), 1.0f), layout::box_round, ImDrawFlags_RoundCornersAll, 1.0f);

    const ImVec4 tint = good ? ConsoleTheme::Accent() : ConsoleTheme::Warning();
    const ImVec2 dotCenter(boxMin.x + pad + 3.0f, boxMin.y + height * 0.5f);
    drawList->AddCircleFilled(dotCenter, 7.5f, ConsoleTheme::U32(tint, 0.18f), 16);
    drawList->AddCircleFilled(dotCenter, 3.5f, ConsoleTheme::U32(tint, 1.0f), 14);

    ConsoleTheme::Text(drawList, smallFont, ImVec2(boxMin.x + pad + 13.0f, boxMin.y + (height - smallSize) * 0.5f - 1.0f),
                       ConsoleTheme::U32(ConsoleTheme::Ink(0.46f), 1.0f), label);
    ConsoleTheme::Text(drawList, boldFont, ImVec2(boxMin.x + pad + 13.0f + labelW + 7.0f, boxMin.y + (height - boldSize) * 0.5f - 1.0f),
                       ConsoleTheme::U32(ConsoleTheme::TextColor(), 1.0f), value);
    return boxMax.x + 10.0f;
}

// Portfolio #8 字标：多层柔和外发光 + 实心字
void DrawWordmark(ImDrawList* drawList, ImFont* font, float size, ImVec2 pos, const char* text)
{
    if (!font || !text)
        return;
    const int spokes = 10;
    for (int ring = 1; ring <= 2; ++ring)
    {
        const float radius = 1.7f * static_cast<float>(ring);
        const ImU32 glow = ConsoleTheme::U32(ConsoleTheme::Ink(0.05f / static_cast<float>(ring)), 1.0f);
        for (int k = 0; k < spokes; ++k)
        {
            const float angle = 6.28318f * static_cast<float>(k) / static_cast<float>(spokes) + static_cast<float>(ring) * 0.31f;
            drawList->AddText(font, size, ImVec2(pos.x + std::cos(angle) * radius, pos.y + std::sin(angle) * radius), glow, text);
        }
    }
    drawList->AddText(font, size, pos, ConsoleTheme::U32(ConsoleTheme::TextColor(), 1.0f), text);
}

// 说明：旧版侧栏「快速控制」开关组已按功能归位到人物页各功能盒内
//（避免同一开关在侧栏与页面出现两份），此实现 retained 以便回退到侧栏布局。
[[maybe_unused]] void RenderQuickControls(bool slim)
{
    if (slim)
        return;

    bool playerGod = GodMode::bPlayerGodMode.load();
    if (ConsoleTheme::ToggleRow("quick_player_god", "玩家无敌", nullptr, &playerGod, false)) {
        GodMode::bPlayerGodMode.store(playerGod);
        GodMode::bRequestedGodmode.store(true);
        WindowState::QuickGodMode = playerGod;
        UiToast::Show(playerGod ? "已开启 玩家无敌" : "已关闭 玩家无敌",
                      playerGod ? ToastKind::Success : ToastKind::Info);
    }
    bool noWanted = NoWanted::bEnable;
    if (ConsoleTheme::ToggleRow("quick_no_wanted", "永不通缉", nullptr, &noWanted, false)) {
        NoWanted::bEnable = noWanted;
        WindowState::QuickNoWanted = noWanted;
        UiToast::Show(noWanted ? "已开启 永不通缉" : "已关闭 永不通缉",
                      noWanted ? ToastKind::Success : ToastKind::Info);
    }
    bool invisible = Invisibility::bInvisibility.load();
    if (ConsoleTheme::ToggleRow("quick_invisible", "隐身", nullptr, &invisible, false)) {
        Invisibility::bInvisibility.store(invisible);
        WindowState::QuickInvisible = invisible;
        UiToast::Show(invisible ? "已开启 隐身" : "已关闭 隐身",
                      invisible ? ToastKind::Success : ToastKind::Info);
    }
    bool noCollision = NoCollision::bNoCollisionUI;
    if (ConsoleTheme::ToggleRow("quick_collision", "无碰撞", nullptr, &noCollision, false)) {
        NoCollision::bNoCollisionUI = noCollision;
        WindowState::QuickNoCollision = noCollision;
        UiToast::Show(noCollision ? "已开启 无碰撞" : "已关闭 无碰撞",
                      noCollision ? ToastKind::Success : ToastKind::Info);
    }
}

void RenderNavigation(MenuManager& menu, bool slim)
{
    MenuPage current = menu.GetCurrentPage();
    if (current == MenuPage::MAIN)
        current = MenuPage::PLAYER;

    // Portfolio #8 的 tab 之间没有额外间距：行距完全由 tab 高度决定。
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
    for (const PageDef& def : kPages)
    {
        const char* label = slim ? "" : def.label;
        if (ConsoleTheme::NavItemIcon(def.icon, label, current == def.page))
            menu.SetCurrentPage(def.page);
    }
    ImGui::PopStyleVar();
}

void RenderSidebar(MenuManager& menu, const ImVec2& min, const ImVec2& max, bool slim)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const float width = max.x - min.x;
    const float height = max.y - min.y;

    drawList->AddRectFilled(min, max, ConsoleTheme::U32(ConsoleTheme::Sidebar(), 1.0f), layout::shell_round, ImDrawFlags_RoundCornersLeft);

    ImGui::SetCursorScreenPos(min);
    ImGui::BeginChild("##sidebar_body", ImVec2(width, height), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);

    ImFont* smallFont = AppFonts::Small ? AppFonts::Small : AppFonts::Regular;

    // 字标 + 发布信息
    if (!slim)
    {
        ImFont* logo = AppFonts::Logo ? AppFonts::Logo : AppFonts::Bold;
        if (logo)
        {
            const char* wordmark = "GTA5 DMA";
            const ImVec2 ts = logo->CalcTextSizeA(logo->FontSize, FLT_MAX, 0.0f, wordmark);
            const float x = std::floor((width - ts.x) * 0.5f);
            DrawWordmark(drawList, logo, logo->FontSize, ImVec2(x, layout::sidebar_logo_y), wordmark);
        }
        ConsoleTheme::Text(drawList, smallFont, ImVec2(24.0f, layout::sidebar_logo_y + 36.0f),
                           ConsoleTheme::U32(ConsoleTheme::Ink(0.46f), 1.0f), "bilibili 一只小微凉鸭");
    }

    // 折叠按钮
    ImGui::SetCursorPos(ImVec2(width - 32.0f, 12.0f));
    ImGui::PushID("##collapse");
    if (ConsoleTheme::IconButton("##collapse_btn", slim ? UiIcon::Chevron : UiIcon::Menu, 26.0f))
        WindowState::SidebarCollapsed = !WindowState::SidebarCollapsed;
    ImGui::PopID();

    // 导航
    ImGui::SetCursorPos(ImVec2(0.0f, 76.0f));
    RenderNavigation(menu, slim);

    // 导航之后是一条极淡分隔线：侧栏只承担「导航 + 身份」，功能开关全部归位到各自页面。
    if (!slim)
    {
        const float afterTabs = 76.0f + static_cast<float>(sizeof(kPages) / sizeof(kPages[0])) * layout::sidebar_tab_h + 14.0f;
        drawList->AddLine(ImVec2(min.x + 18.0f, min.y + afterTabs), ImVec2(max.x - 18.0f, min.y + afterTabs),
                          ConsoleTheme::U32(ConsoleTheme::Ink(0.07f), 1.0f), 1.0f);
    }

    // 底部身份块（对应参考实现的用户信息区）
    if (!slim)
    {
        const float baseY = height - 58.0f;
        const ImVec2 avatarCenter(min.x + 34.0f, baseY + 20.0f);
        drawList->AddCircleFilled(ImVec2(avatarCenter.x, avatarCenter.y + 1.0f), 24.0f, ConsoleTheme::U32(ConsoleTheme::Accent(), 0.14f), 32);
        drawList->AddCircleFilled(avatarCenter, 20.0f, ConsoleTheme::U32(ConsoleTheme::Accent(), 0.90f), 32);
        ConsoleTheme::TextCentered(drawList, AppFonts::Bold, ImVec2(avatarCenter.x - 20.0f, avatarCenter.y - 20.0f),
                                   ImVec2(avatarCenter.x + 20.0f, avatarCenter.y + 20.0f),
                                   ConsoleTheme::U32(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 1.0f), "G");

        char status[72];
        std::snprintf(status, sizeof(status), "%s · PID %lu", DMA::IsReady() ? "已连接" : "等待连接",
                      static_cast<unsigned long>(DMA::PID));
        ConsoleTheme::Text(drawList, AppFonts::Bold, ImVec2(min.x + 62.0f, baseY + 4.0f),
                           ConsoleTheme::U32(ConsoleTheme::TextColor(), 0.88f), "GTA5 DMA 控制台");
        ConsoleTheme::Text(drawList, smallFont, ImVec2(min.x + 62.0f, baseY + 28.0f),
                           ConsoleTheme::U32(ConsoleTheme::Ink(0.46f), 1.0f), status);

        const float lineY = min.y + baseY - 14.0f;
        drawList->AddLine(ImVec2(min.x + 18.0f, lineY), ImVec2(max.x - 18.0f, lineY),
                          ConsoleTheme::U32(ConsoleTheme::Ink(0.07f), 1.0f), 1.0f);
    }

    // 悬浮窗抓手：导航项与底部身份块之间那大片空白做成「拖动条」（ImGui 隐形按钮），
    // 按住它可以把整块面板拖到窗口任意位置，偏移写进 ini。
    if (!slim)
    {
        const float barTop = 76.0f + static_cast<float>(sizeof(kPages) / sizeof(kPages[0])) * layout::sidebar_tab_h + 40.0f;
        const float barH = MaxF(height - barTop - 74.0f, 40.0f);
        ImGui::SetCursorPos(ImVec2(12.0f, barTop));
        ImGui::InvisibleButton("##panel_drag", ImVec2(MaxF(width - 24.0f, 40.0f), barH));
        const bool barHot = ImGui::IsItemHovered();
        const bool barActive = ImGui::IsItemActive();
        gDbgBarHot = barHot;
        gDbgBarAct = barActive;
        if (barActive)
        {
            const ImVec2 d = ImGui::GetIO().MouseDelta;
            if (d.x != 0.0f || d.y != 0.0f)
            {
                WindowState::PanelOffsetX += d.x;
                WindowState::PanelOffsetY += d.y;
            }
        }
        if (barHot || barActive)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            const char* hint = "按住拖动此面板";
            const ImVec2 ts = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0.0f, hint);
            ConsoleTheme::Text(drawList, smallFont,
                               ImVec2(min.x + (width - ts.x) * 0.5f, min.y + barTop + 8.0f),
                               ConsoleTheme::U32(ConsoleTheme::Ink(0.40f), 1.0f), hint);
        }
    }

    ImGui::EndChild();
}

// 顶部栏：状态胶囊 + 保存 / 设置（Portfolio #8 的 topbar 结构）
void RenderStatusHeader(MenuManager& menu)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    const float rowY = origin.y + (layout::topbar_h - 34.0f) * 0.5f;
    char fps[32];
    std::snprintf(fps, sizeof(fps), "%.0f", ImGui::GetIO().Framerate);

    float pillX = origin.x + 18.0f;
    pillX = CompactPill(pillX, rowY, "DMA", DMA::IsReady() ? "已连接" : "等待", DMA::IsReady());
    pillX = CompactPill(pillX, rowY, "进程", GameLabel(), DMA::IsReady());
    pillX = CompactPill(pillX, rowY, "主机热键", g_inputManager.IsReady() ? "已连接" : "不可用", g_inputManager.IsReady());
    pillX = CompactPill(pillX, rowY, "FPS", fps, true);

    const float iconBtn = 34.0f;
    const float saveW = 124.0f;
    ImGui::SetCursorScreenPos(ImVec2(origin.x + width - iconBtn - 18.0f - 10.0f - saveW, origin.y + (layout::topbar_h - 38.0f) * 0.5f));
    if (ConsoleTheme::GhostButton("保存状态", UiIcon::Save, ImVec2(saveW, 38.0f)))
    {
        WindowState::Save();
        UiToast::Show("窗口状态与开关已保存", ToastKind::Success);
    }

    ImGui::SetCursorScreenPos(ImVec2(origin.x + width - iconBtn - 18.0f, origin.y + (layout::topbar_h - iconBtn) * 0.5f));
    if (ConsoleTheme::IconButton("##topbar_gear", UiIcon::Gear, iconBtn))
        menu.SetCurrentPage(MenuPage::SETTINGS);

    (void)drawList;
}

// 页标题（Portfolio #8 的分节标题风格：淡色小字 + 细线）
void RenderPageHeader()
{
    const MenuManager& menu = MenuManager::GetInstance();
    const PageDef& info = PageInfo(menu.GetCurrentPage());

    ImGui::PushFont(AppFonts::Title);
    ImGui::TextUnformatted(info.label);
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", info.description);

    const ImVec2 line = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ConsoleTheme::HLine(ImVec2(line.x, line.y + 8.0f), ImGui::GetContentRegionAvail().x,
                        ConsoleTheme::U32(ConsoleTheme::Ink(0.07f), 1.0f));
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
}

void RenderFooter(const ImVec2& min, const ImVec2& max)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImFont* smallFont = AppFonts::Small ? AppFonts::Small : AppFonts::Regular;
    const float size = smallFont ? smallFont->FontSize : ImGui::GetFontSize();
    const float y = min.y + (max.y - min.y - size) * 0.5f;

    char left[160];
    std::snprintf(left, sizeof(left), "PID %lu   ·   基址 0x%llX   ·   人物模型 0x%08X   ·   构建 %s",
                  static_cast<unsigned long>(DMA::PID),
                  static_cast<unsigned long long>(DMA::BaseAddress),
                  DMA::LocalPlayerModelHash,
                  DMA::BuildTag);
    ConsoleTheme::Text(drawList, smallFont, ImVec2(min.x, y), ConsoleTheme::U32(ConsoleTheme::Ink(0.46f), 1.0f), left);

    // 键位提示：每组占等宽槽位（键帽统一 30px、标签统一宽度），四组严格对齐。
    // 旧实现让键帽和标签各自变宽，右端四组参差不齐。
    float cursorX = max.x;
    const char* keys[] = { "END", "F6", "F5", "INS" };
    const char* labels[] = { "退出", "任务点", "标记点", "显隐" };
    const float capSlotW = 30.0f;
    const float labelSlotW = 54.0f;
    const float slotW = capSlotW + 9.0f + labelSlotW + 12.0f;
    const float capY = min.y + (max.y - min.y - 19.0f) * 0.5f;
    for (int i = 3; i >= 0; --i)
    {
        const float slotX = cursorX - slotW;
        float capX = slotX;
        ConsoleTheme::KeyCap(keys[i], &capX, capY, capSlotW);
        ConsoleTheme::Text(drawList, smallFont, ImVec2(capX + 3.0f, y), ConsoleTheme::U32(ConsoleTheme::Ink(0.46f), 1.0f), labels[i]);
        cursorX = slotX;
    }
}

void RenderPage(MenuManager& menu)
{
    MenuPage page = menu.GetCurrentPage();
    if (page == MenuPage::MAIN) {
        menu.SetCurrentPage(MenuPage::PLAYER);
        page = MenuPage::PLAYER;
    }

    switch (page) {
    case MenuPage::PLAYER:   menu.RenderPlayerPageContent(); break;
    case MenuPage::VEHICLE:  menu.RenderVehiclePageContent(); break;
    case MenuPage::WEAPON:   menu.RenderWeaponPageContent(); break;
    case MenuPage::TELEPORT: menu.RenderTeleportPageContent(); break;
    case MenuPage::SESSION:  menu.RenderSessionPageContent(); break;
    // DISABLED: 时间 / 任务分红路由 retained for later restoration。
    case MenuPage::SETTINGS: menu.RenderSettingsPageContent(); break;
    default: break;
    }
}
} // namespace

void ConsoleShell::Render(MenuManager& menu)
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::Begin("GTA5 DMA 控制台 bilibili 一只小微凉鸭 免费发布 请勿贩卖", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleVar(2);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 origin = viewport->Pos;
    const ImVec2 viewSize = viewport->Size;
    const ImVec2 screenMax(origin.x + viewSize.x, origin.y + viewSize.y);

    // ---- 1. 全屏壁纸（cover 裁剪）----
    ImVec2 bgUv0(0.0f, 0.0f);
    ImVec2 bgUv1(1.0f, 1.0f);
    const ImVec2 bgSrc = Backdrop::SharpSize();
    if (bgSrc.x > 0.0f && bgSrc.y > 0.0f)
    {
        const float scale = MaxF(viewSize.x / bgSrc.x, viewSize.y / bgSrc.y);
        const ImVec2 covered(bgSrc.x * scale, bgSrc.y * scale);
        const ImVec2 crop((covered.x - viewSize.x) * 0.5f / MaxF(covered.x, 1.0f),
                          (covered.y - viewSize.y) * 0.5f / MaxF(covered.y, 1.0f));
        bgUv0 = crop;
        bgUv1 = ImVec2(1.0f - crop.x, 1.0f - crop.y);
    }
    const auto screen_uv = [&](const ImVec2& p) {
        // 面板像是盖在壁纸上的一扇窗：用窗口内坐标取 UV，壁纸才对得上位置
        return ImVec2(LerpF(bgUv0.x, bgUv1.x, ClampF((p.x - origin.x) / MaxF(viewSize.x, 1.0f), 0.0f, 1.0f)),
                      LerpF(bgUv0.y, bgUv1.y, ClampF((p.y - origin.y) / MaxF(viewSize.y, 1.0f), 0.0f, 1.0f)));
    };

    // 融合器模式：IMGUI 面板以外一律纯黑，那块画面留给另一台电脑 / 外部视频源；
    // 背景图片只出现在面板矩形内部（见下面第 2 段）。
    drawList->AddRectFilled(origin, screenMax, IM_COL32(0, 0, 0, 255));
    ConsoleTheme::TraceNote("NOTE BACKDROP outside=black inside=wallpaper");

    // ---- 2. 悬浮窗面板（融合器模式）----
    // 面板不再铺满整个窗口：默认固定 1180x780 浮在壁纸上，可拖动 / 可缩放，几何存 ini。
    // 窗口比面板还小时自动收进窗口内，且永远留出 panel_edge 宽的壁纸边框。
    const float panelEdge = layout::panel_edge;
    float panelW = WindowState::PanelW > 0.0f ? WindowState::PanelW : layout::panel_w;
    float panelH = WindowState::PanelH > 0.0f ? WindowState::PanelH : layout::panel_h;
    const float panelFitW = MaxF(viewSize.x - panelEdge * 2.0f, 320.0f);
    const float panelFitH = MaxF(viewSize.y - panelEdge * 2.0f, 240.0f);
    panelW = ClampF(panelW, MinF(layout::panel_min_w, panelFitW), panelFitW);
    panelH = ClampF(panelH, MinF(layout::panel_min_h, panelFitH), panelFitH);

    // 默认在窗口里居中，再叠加拖动偏移，最后夹回窗口内（拖不出去）
    const float panelBaseX = origin.x + (viewSize.x - panelW) * 0.5f;
    const float panelBaseY = origin.y + (viewSize.y - panelH) * 0.5f;
    const float panelX = ClampF(panelBaseX + WindowState::PanelOffsetX, origin.x + panelEdge,
                                MaxF(screenMax.x - panelEdge - panelW, origin.x + panelEdge));
    const float panelY = ClampF(panelBaseY + WindowState::PanelOffsetY, origin.y + panelEdge,
                                MaxF(screenMax.y - panelEdge - panelH, origin.y + panelEdge));
    WindowState::PanelOffsetX = panelX - panelBaseX;   // 夹紧后的真实偏移写回，越拖越偏就没了
    WindowState::PanelOffsetY = panelY - panelBaseY;

    const ImVec2 panelMin(panelX, panelY);
    const ImVec2 panelMax(panelX + panelW, panelY + panelH);
    const float panelRound = layout::shell_round;

    ConsoleTheme::Shadow(panelMin, panelMax, panelRound, 26.0f, 1.0f);
    if (Backdrop::Ready())
    {
        // 背景图片只铺在面板矩形内部：清晰壁纸打底 + 毛玻璃叠加，保留 Portfolio #8 的磨砂观感
        drawList->AddImageRounded(Backdrop::Sharp(), panelMin, panelMax,
                                  screen_uv(panelMin), screen_uv(panelMax), IM_COL32_WHITE, panelRound);
        drawList->AddImageRounded(Backdrop::Blurred(), panelMin, panelMax,
                                  screen_uv(panelMin), screen_uv(panelMax), IM_COL32(255, 255, 255, 168), panelRound);
    }
    drawList->AddRectFilled(panelMin, panelMax, ConsoleTheme::U32(ConsoleTheme::Panel(), 1.0f), panelRound);
    drawList->AddRect(panelMin, panelMax, ConsoleTheme::U32(ConsoleTheme::Ink(0.10f), 1.0f), panelRound, ImDrawFlags_RoundCornersAll, 1.0f);
    drawList->AddRectFilledMultiColor(ImVec2(panelMin.x + panelRound, panelMin.y + 1.0f), ImVec2(panelMax.x - panelRound, panelMin.y + 2.0f),
                                      ConsoleTheme::U32(ConsoleTheme::Ink(0.10f), 1.0f), ConsoleTheme::U32(ConsoleTheme::Ink(0.01f), 1.0f),
                                      ConsoleTheme::U32(ConsoleTheme::Ink(0.01f), 1.0f), ConsoleTheme::U32(ConsoleTheme::Ink(0.10f), 1.0f));

    // ---- 2b. 悬浮窗交互：右下角手柄缩放 ----
    // 输入交给 ImGui 的隐形按钮（手写 IsMouseHoveringRect 在真程序里吃不到按下事件）。
    {
        const float grip = layout::panel_grip;
        const ImVec2 gripMax(panelMax.x - 6.0f, panelMax.y - 6.0f);
        ImGui::SetCursorScreenPos(ImVec2(gripMax.x - grip, gripMax.y - grip));
        ImGui::InvisibleButton("##panel_grip", ImVec2(grip, grip));
        const bool gripHot = ImGui::IsItemHovered();
        const bool gripActive = ImGui::IsItemActive();
        gDbgGripHot = gripHot;
        gDbgGripAct = gripActive;
        gDbgDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        gDbgDx = ImGui::GetIO().MouseDelta.x;
        gDbgDy = ImGui::GetIO().MouseDelta.y;
        for (int i = 0; i < 3; ++i)
        {
            const float off = 7.0f + static_cast<float>(i) * 4.0f;
            drawList->AddLine(ImVec2(gripMax.x - off, gripMax.y - 1.0f), ImVec2(gripMax.x - 1.0f, gripMax.y - off),
                              ConsoleTheme::U32(ConsoleTheme::Ink((gripHot || gripActive) ? 0.58f : 0.26f), 1.0f), 1.5f);
        }
        if (gripActive)
        {
            const ImVec2 d = ImGui::GetIO().MouseDelta;
            WindowState::PanelW = ClampF(panelW + d.x, MinF(layout::panel_min_w, panelFitW), panelFitW);
            WindowState::PanelH = ClampF(panelH + d.y, MinF(layout::panel_min_h, panelFitH), panelFitH);
        }
        if (gripHot || gripActive)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
    }

    const bool slim = WindowState::SidebarCollapsed;
    const float sidebarW = slim ? kSidebarSlim : layout::sidebar_w;
    const ImVec2 sidebarMax(panelMin.x + sidebarW, panelMax.y);

    RenderSidebar(menu, panelMin, sidebarMax, slim);

    // ---- 3. 顶部栏 ----
    const float contentLeft = sidebarMax.x + layout::content_pad_x;
    const float contentWidth = panelMax.x - contentLeft - layout::content_pad_x;

    ImGui::SetCursorScreenPos(ImVec2(sidebarMax.x, panelMin.y));
    ImGui::BeginChild("##topbar", ImVec2(panelMax.x - sidebarMax.x, layout::topbar_h), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);
    RenderStatusHeader(menu);
    ImGui::EndChild();

    drawList->AddLine(ImVec2(sidebarMax.x + layout::content_pad_x, panelMin.y + layout::topbar_h),
                      ImVec2(panelMax.x - layout::content_pad_x, panelMin.y + layout::topbar_h),
                      ConsoleTheme::U32(ConsoleTheme::Ink(0.07f), 1.0f), 1.0f);

    // ---- 4. 工作区（切页淡入 + 上滑）----
    static MenuPage lastPage = MenuPage::MAIN;
    const MenuPage current = menu.GetCurrentPage();
    if (current != lastPage)
    {
        lastPage = current;
        ConsoleTheme::Anim("page:fade", 0.0f, 1.0f);
    }
    const float pageFade = ConsoleTheme::Anim("page:fade", 1.0f, 14.0f);
    const float slide = (1.0f - pageFade) * 10.0f;

    const float footerH = 34.0f;
    const float contentTop = panelMin.y + layout::topbar_h + 12.0f + slide;
    const float contentHeight = MaxF(panelMax.y - contentTop - footerH - 8.0f, 120.0f);

    ImGui::SetCursorScreenPos(ImVec2(contentLeft, contentTop));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(layout::content_gap, 10.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, pageFade);
    ImGui::BeginChild("##workspace", ImVec2(contentWidth, contentHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoBackground);
    ConsoleTheme::TraceBegin(PageInfo(menu.GetCurrentPage()).label, contentWidth, contentHeight);
    {
        // 自检：浮窗矩形（探针据此算侧栏 / 导航的绝对坐标，不靠硬编码）
        char panelNote[192];
        std::snprintf(panelNote, sizeof(panelNote), "NOTE PANEL x0 %.0f y0 %.0f x1 %.0f y1 %.0f w %.0f h %.0f",
                      panelMin.x, panelMin.y, panelMax.x, panelMax.y, panelW, panelH);
        ConsoleTheme::TraceNote(panelNote);
    }
    {
        char fnote[240];
        std::snprintf(fnote, sizeof(fnote),
                      "NOTE FLOATINPUT down=%d delta=%.1f,%.1f gripHot=%d gripAct=%d barHot=%d barAct=%d "
                      "off=%.0f,%.0f size=%.0f,%.0f items=%d",
                      gDbgDown ? 1 : 0, gDbgDx, gDbgDy, gDbgGripHot ? 1 : 0, gDbgGripAct ? 1 : 0,
                      gDbgBarHot ? 1 : 0, gDbgBarAct ? 1 : 0, WindowState::PanelOffsetX, WindowState::PanelOffsetY,
                      WindowState::PanelW, WindowState::PanelH, ImGui::IsAnyItemHovered() ? 1 : 0);
        ConsoleTheme::TraceNote(fnote);
    }
    RenderPageHeader();
    RenderPage(menu);
    ImGui::Dummy(ImVec2(0.0f, 6.0f));   // 底部留白：最后一行不贴住信息条

    // ---- 溢出提示（悬浮窗变小后内容会被工作区裁掉，这里明确告诉用户「还能往下滚」）----
    // 只在「内容确实高于工作区」且「还没滚到底」时出现；滚到底自动消失，不占布局。
    {
        const float scrollMax = ImGui::GetScrollMaxY();
        const float scrollCur = ImGui::GetScrollY();
        char snote[128];
        std::snprintf(snote, sizeof(snote), "NOTE SCROLL cur=%.0f max=%.0f", scrollCur, scrollMax);
        ConsoleTheme::TraceNote(snote);

        if (scrollMax > 1.0f && scrollCur < scrollMax - 2.0f)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 wpos  = ImGui::GetWindowPos();
            const ImVec2 wsize = ImGui::GetWindowSize();
            const float  barH  = 26.0f;
            const ImVec2 bmin(wpos.x + 10.0f, wpos.y + wsize.y - barH - 8.0f);
            const ImVec2 bmax(wpos.x + wsize.x - 22.0f, wpos.y + wsize.y - 8.0f);

            dl->AddRectFilled(bmin, bmax, ImGui::GetColorU32(ImVec4(0.0f, 0.0f, 0.0f, 0.55f)), 6.0f);
            const char* hint = "下方还有内容 · 滚轮下滑查看";
            const ImVec2 ts  = ImGui::CalcTextSize(hint);
            dl->AddText(ImVec2(bmin.x + (bmax.x - bmin.x - ts.x) * 0.5f, bmin.y + (barH - ts.y) * 0.5f),
                        ImGui::GetColorU32(ImGuiCol_TextDisabled), hint);
        }
    }
    ConsoleTheme::TraceEnd("ui_check.txt");
    ImGui::EndChild();
    ImGui::PopStyleVar(3);

    // ---- 5. 底部信息条 ----
    RenderFooter(ImVec2(contentLeft, panelMax.y - footerH + 6.0f), ImVec2(panelMax.x - layout::content_pad_x, panelMax.y - 8.0f));

    // ---- 6. Toast（最上层）----
    UiToast::Render();

    ImGui::End();
}
