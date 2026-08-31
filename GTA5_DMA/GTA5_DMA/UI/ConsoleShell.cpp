#include "pch.h"

#include "ConsoleShell.h"

#include "AppFonts.h"
#include "AppRuntime.h"
#include "ConsoleTheme.h"
#include "DMA.h"
#include "Features.h"
#include "InputManager.h"
#include "MenuManager.h"
#include "Offsets.h"

#include <cstdio>

namespace
{
constexpr float kSidebarWidth    = 190.0f;
constexpr float kHeaderHeight    = 54.0f;
constexpr float kStatusBarHeight = 30.0f;

const char* PageTitle(MenuPage page)
{
    switch (page) {
    case MenuPage::PLAYER:   return "人物控制";
    case MenuPage::VEHICLE:  return "载具编辑";
    case MenuPage::WEAPON:   return "武器功能";
    case MenuPage::TELEPORT: return "位置传送";
    case MenuPage::SETTINGS: return "系统设置";
    default:                 return "人物控制";
    }
}

const char* PageDescription(MenuPage page)
{
    switch (page) {
    case MenuPage::PLAYER:   return "生命、移动与外观状态";
    case MenuPage::VEHICLE:  return "载具状态与操控参数";
    case MenuPage::WEAPON:   return "武器数据与命中参数";
    case MenuPage::TELEPORT: return "坐标、标记点与任务点";
    case MenuPage::SETTINGS: return "主题、快捷键与发布信息";
    default:                 return "实时参数与功能控制";
    }
}

const char* GameLabel()
{
    switch (currentGameType) {
    case GameType::GTA5_Enhanced: return "GTA5 Enhanced";
    case GameType::GTA5:          return "GTA5";
    default:                      return "等待游戏";
    }
}

void FpsText(char* buffer, size_t size)
{
    std::snprintf(buffer, size, "%.0f", ImGui::GetIO().Framerate);
}

// 页眉右侧的状态胶囊：圆点 + 标签 + 值，从右往左排布，返回下一个胶囊的右边界。
float HeaderPill(float rightX, float y, const char* label, const char* value, bool good)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 labelSize = ImGui::CalcTextSize(label);
    const ImVec2 valueSize = ImGui::CalcTextSize(value);
    constexpr float pad = 9.0f;
    const float width = pad + 7.0f + 6.0f + labelSize.x + 6.0f + valueSize.x + pad;
    const ImVec2 min(rightX - width, y);
    const ImVec2 max(rightX, y + 24.0f);
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_FrameBg), 12.0f);
    const ImVec4 dotColor = good ? ConsoleTheme::Success() : ConsoleTheme::Warning();
    drawList->AddCircleFilled(ImVec2(min.x + pad + 3.0f, y + 12.0f), 3.5f, ImGui::ColorConvertFloat4ToU32(dotColor));
    drawList->AddText(ImVec2(min.x + pad + 13.0f, y + 12.0f - labelSize.y * 0.5f), ImGui::GetColorU32(ImGuiCol_TextDisabled), label);
    drawList->AddText(ImVec2(min.x + pad + 13.0f + labelSize.x + 6.0f, y + 12.0f - valueSize.y * 0.5f), ImGui::GetColorU32(ImGuiCol_Text), value);
    return min.x - 10.0f;
}

void RenderQuickToggle(const char* id, const char* label, bool* value)
{
    ConsoleTheme::ToggleRow(id, label, nullptr, value);
}

void RenderQuickControls()
{
    ImGui::TextDisabled("快速控制");
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    bool playerGod = GodMode::bPlayerGodMode.load();
    if (ConsoleTheme::ToggleRow("quick_player_god", "玩家无敌", nullptr, &playerGod)) {
        GodMode::bPlayerGodMode.store(playerGod);
        GodMode::bRequestedGodmode.store(true);
    }
    RenderQuickToggle("quick_no_wanted", "永不通缉", &NoWanted::bEnable);
    bool invisible = Invisibility::bInvisibility.load();
    if (ConsoleTheme::ToggleRow("quick_invisible", "隐身", nullptr, &invisible)) {
        Invisibility::bInvisibility.store(invisible);
    }
    RenderQuickToggle("quick_collision", "无碰撞", &NoCollision::bNoCollisionUI);
}

void RenderNavigation(MenuManager& menu)
{
    MenuPage current = menu.GetCurrentPage();
    if (current == MenuPage::MAIN) current = MenuPage::PLAYER;

    ImGui::TextDisabled("功能模块");
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    if (ConsoleTheme::NavItem("人物控制", current == MenuPage::PLAYER))   menu.SetCurrentPage(MenuPage::PLAYER);
    if (ConsoleTheme::NavItem("载具编辑", current == MenuPage::VEHICLE))  menu.SetCurrentPage(MenuPage::VEHICLE);
    if (ConsoleTheme::NavItem("武器功能", current == MenuPage::WEAPON))   menu.SetCurrentPage(MenuPage::WEAPON);
    if (ConsoleTheme::NavItem("位置传送", current == MenuPage::TELEPORT)) menu.SetCurrentPage(MenuPage::TELEPORT);
    // DISABLED: 时间控制 / 任务分红导航入口 retained（实现见 Attic/LegacyPages.cpp）。
    if (ConsoleTheme::NavItem("系统设置", current == MenuPage::SETTINGS)) menu.SetCurrentPage(MenuPage::SETTINGS);
}

void RenderStatusHeader()
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;

    // 页眉深色底带
    drawList->AddRectFilled(start, ImVec2(start.x + width, start.y + kHeaderHeight),
        ImGui::ColorConvertFloat4ToU32(ConsoleTheme::BackgroundDeep()), 8.0f);

    // Logo 方块
    drawList->AddRectFilled(ImVec2(start.x + 16.0f, start.y + 13.0f), ImVec2(start.x + 41.0f, start.y + 38.0f),
        ImGui::ColorConvertFloat4ToU32(ConsoleTheme::Accent()), 6.0f);
    if (AppFonts::Bold) {
        ImGui::PushFont(AppFonts::Bold);
        drawList->AddText(AppFonts::Bold, 17.0f, ImVec2(start.x + 23.0f, start.y + 17.5f), IM_COL32(12, 16, 22, 255), "G");
        ImGui::PopFont();
    }

    // 标题 + 副标题
    if (AppFonts::Bold) ImGui::PushFont(AppFonts::Bold);
    drawList->AddText(ImVec2(start.x + 52.0f, start.y + 12.0f), ImGui::GetColorU32(ImGuiCol_Text), "GTA5 DMA 控制台");
    if (AppFonts::Bold) ImGui::PopFont();
    drawList->AddText(ImVec2(start.x + 52.0f, start.y + 31.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled),
        "bilibili 一只小微凉鸭 · 免费发布 请勿贩卖");

    // 右侧状态胶囊：FPS / 主机热键 / 游戏进程 / DMA
    char fps[32];
    FpsText(fps, sizeof(fps));
    float right = start.x + width - 16.0f;
    const float y = start.y + 15.0f;
    right = HeaderPill(right, y, "FPS", fps, true);
    right = HeaderPill(right, y, "主机热键", g_inputManager.IsReady() ? "已连接" : "不可用", g_inputManager.IsReady());
    right = HeaderPill(right, y, "进程", GameLabel(), DMA::IsReady());
    right = HeaderPill(right, y, "DMA", DMA::IsReady() ? "已连接" : "等待", DMA::IsReady());

    // 页眉底部分隔线
    drawList->AddLine(ImVec2(start.x, start.y + kHeaderHeight), ImVec2(start.x + width, start.y + kHeaderHeight),
        ImGui::GetColorU32(ImGuiCol_Border));

    ImGui::Dummy(ImVec2(width, kHeaderHeight));
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
    // DISABLED: 时间 / 任务分红路由 retained for later restoration。
    // case MenuPage::TIME: menu.RenderTimePageContent(); break;
    // case MenuPage::HEIST_DIVIDEND: menu.RenderHeistDividendPageContent(); break;
    case MenuPage::SETTINGS: menu.RenderSettingsPageContent(); break;
    default: break;
    }
}
} // namespace

void ConsoleShell::Render(MenuManager& menu)
{
    // 宿主窗口铺满整个视口（Win32 客户区），内部自行绘制页眉 / 侧边栏 / 状态栏。
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ConsoleTheme::Background());
    ImGui::Begin("GTA5 DMA 控制台 bilibili 一只小微凉鸭 免费发布 请勿贩卖", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);

    RenderStatusHeader();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float bodyHeight = avail.y - kStatusBarHeight - ImGui::GetStyle().ItemSpacing.y;

    // ---- 侧边栏 ----
    ImGui::BeginChild("##sidebar", ImVec2(kSidebarWidth, bodyHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(14.0f, 14.0f));
    ImGui::BeginGroup();
    RenderQuickControls();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    RenderNavigation(menu);
    ImGui::EndGroup();

    // 侧边栏底部版本信息
    const float afterY = ImGui::GetCursorPosY();
    const float bottomY = ImGui::GetWindowHeight() - 26.0f;
    if (bottomY > afterY) {
        ImGui::SetCursorPos(ImVec2(14.0f, bottomY));
        ImGui::TextDisabled("v2.0 · 重构版");
    }
    ImGui::EndChild();

    // 侧边栏与工作区分隔线
    const ImVec2 sepTop = ImGui::GetCursorScreenPos();
    ImGui::SameLine();
    ImGui::GetWindowDrawList()->AddLine(sepTop, ImVec2(sepTop.x, sepTop.y + bodyHeight), ImGui::GetColorU32(ImGuiCol_Border));

    // ---- 工作区 ----
    ImGui::BeginChild("##workspace", ImVec2(0.0f, bodyHeight), ImGuiChildFlags_None);
    ImGui::SetCursorPos(ImVec2(18.0f, 16.0f));
    if (AppFonts::Bold) ImGui::PushFont(AppFonts::Bold);
    ImGui::TextColored(ConsoleTheme::Accent(), "%s", PageTitle(menu.GetCurrentPage()));
    if (AppFonts::Bold) ImGui::PopFont();
    ImGui::TextDisabled("%s", PageDescription(menu.GetCurrentPage()));
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    {
        const ImVec2 line = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine(line, ImVec2(line.x + ImGui::GetContentRegionAvail().x, line.y), ImGui::GetColorU32(ImGuiCol_Border));
    }
    ImGui::Dummy(ImVec2(0.0f, 8.0f));
    RenderPage(menu);
    ImGui::EndChild();

    // ---- 状态栏 ----
    ImGui::BeginChild("##status", ImVec2(0.0f, kStatusBarHeight), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(16.0f, 7.0f));
    ImGui::TextDisabled("PID %lu", static_cast<unsigned long>(DMA::PID));
    ImGui::SameLine();
    ImGui::TextDisabled("Base 0x%llX", static_cast<unsigned long long>(DMA::BaseAddress));
    ImGui::SameLine();
    ImGui::TextDisabled("人物模型 0x%08X", DMA::LocalPlayerModelHash);
    ImGui::SameLine(ImGui::GetWindowWidth() - 250.0f);
    ImGui::TextDisabled("INS 显隐 · F5 标点 · F6 任务点 · END 退出");
    ImGui::EndChild();

    ImGui::End();
}
