#include "pch.h"
#include "ConsoleShell.h"

#include "AppRuntime.h"
#include "ConsoleTheme.h"
#include "DMA.h"
#include "Features.h"
#include "InputManager.h"
#include "MenuManager.h"
#include "Offsets.h"

namespace
{
constexpr float kSidebarWidth = 190.0f;
constexpr float kTopBarHeight = 44.0f;
constexpr float kStatusBarHeight = 30.0f;
constexpr ImVec4 kAccent = ImVec4(0.31f, 0.64f, 0.93f, 1.0f);
constexpr ImVec4 kSuccess = ImVec4(0.31f, 0.78f, 0.56f, 1.0f);
constexpr ImVec4 kWarning = ImVec4(0.93f, 0.65f, 0.29f, 1.0f);

const char* PageTitle(MenuPage page)
{
    switch (page) {
    case MenuPage::PLAYER: return "人物控制";
    case MenuPage::VEHICLE: return "载具编辑";
    case MenuPage::WEAPON: return "武器功能";
    case MenuPage::TELEPORT: return "传送功能";
    case MenuPage::TIME: return "时间控制";
    case MenuPage::HEIST_DIVIDEND: return "任务与分红";
    case MenuPage::SETTINGS: return "设置";
    default: return "人物控制";
    }
}

const char* PageDescription(MenuPage page)
{
    switch (page) {
    case MenuPage::PLAYER: return "生命、移动与外观状态";
    case MenuPage::VEHICLE: return "载具状态与操控参数";
    case MenuPage::WEAPON: return "武器数据与命中参数";
    case MenuPage::TELEPORT: return "坐标、标记点与任务点";
    case MenuPage::SETTINGS: return "界面与运行选项";
    default: return "实时参数与功能控制";
    }
}

const char* GameLabel()
{
    switch (currentGameType) {
    case GameType::GTA5_Enhanced: return "GTA5 Enhanced";
    case GameType::GTA5: return "GTA5";
    default: return "等待游戏";
    }
}

bool NavItem(const char* label, MenuPage page, MenuPage current)
{
    const bool selected = page == current;
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.09f, 0.18f, 0.26f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.11f, 0.22f, 0.31f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.13f, 0.29f, 0.42f, 1.0f));
    const bool pressed = ImGui::Selectable(label, selected, 0, ImVec2(0.0f, 38.0f));
    ImGui::PopStyleColor(3);
    if (selected) {
        ImGui::GetWindowDrawList()->AddRectFilled(start, ImVec2(start.x + 3.0f, start.y + 38.0f),
            IM_COL32(79, 162, 236, 255), 2.0f);
    }
    return pressed;
}

void RenderQuickToggle(const char* id, const char* label, bool* value)
{
    ConsoleTheme::ToggleRow(id, label, nullptr, value);
}

void RenderQuickControls()
{
    ImGui::TextDisabled("快速控制");
    ImGui::Dummy(ImVec2(0.0f, 3.0f));

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
    ImGui::Dummy(ImVec2(0.0f, 3.0f));
    if (NavItem("  人物控制", MenuPage::PLAYER, current)) menu.SetCurrentPage(MenuPage::PLAYER);
    if (NavItem("  载具编辑", MenuPage::VEHICLE, current)) menu.SetCurrentPage(MenuPage::VEHICLE);
    if (NavItem("  武器功能", MenuPage::WEAPON, current)) menu.SetCurrentPage(MenuPage::WEAPON);
    if (NavItem("  位置传送", MenuPage::TELEPORT, current)) menu.SetCurrentPage(MenuPage::TELEPORT);
    // DISABLED: time/task implementations are retained for later restoration.
    // if (NavItem("时间", MenuPage::TIME, current)) menu.SetCurrentPage(MenuPage::TIME);
    // if (NavItem("任务", MenuPage::HEIST_DIVIDEND, current)) menu.SetCurrentPage(MenuPage::HEIST_DIVIDEND);
    if (NavItem("  系统设置", MenuPage::SETTINGS, current)) menu.SetCurrentPage(MenuPage::SETTINGS);
}

void RenderStatusHeader()
{
    ImGui::BeginChild("##top", ImVec2(0.0f, kTopBarHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(15.0f, 12.0f));
    ImGui::TextColored(DMA::IsReady() ? kSuccess : kWarning, DMA::IsReady() ? "● DMA 已连接" : "● 等待 DMA");
    ImGui::SameLine();
    ImGui::TextDisabled("实时链路");

    const char* gameLabel = GameLabel();
    const float gameLabelWidth = ImGui::CalcTextSize(gameLabel).x;
    ImGui::SameLine(ImGui::GetWindowWidth() - gameLabelWidth - 16.0f);
    ImGui::TextColored(kAccent, "%s", gameLabel);
    ImGui::EndChild();
}

void RenderPage(MenuManager& menu)
{
    MenuPage page = menu.GetCurrentPage();
    if (page == MenuPage::MAIN) {
        menu.SetCurrentPage(MenuPage::PLAYER);
        page = MenuPage::PLAYER;
    }

    switch (page) {
    case MenuPage::PLAYER: menu.RenderPlayerPageContent(); break;
    case MenuPage::VEHICLE: menu.RenderVehiclePageContent(); break;
    case MenuPage::WEAPON: menu.RenderWeaponPageContent(); break;
    case MenuPage::TELEPORT: menu.RenderTeleportPageContent(); break;
    // 时间与任务路由暂时停用；保留对应页面实现，恢复导航时可重新启用。
    // case MenuPage::TIME: menu.RenderTimePageContent(); break;
    // case MenuPage::HEIST_DIVIDEND: menu.RenderHeistDividendPageContent(); break;
    case MenuPage::SETTINGS: menu.RenderSettingsPageContent(); break;
    default: break;
    }
}
}

void ConsoleShell::Render(MenuManager& menu)
{
    ImGui::SetNextWindowSize(ImVec2(980.0f, 680.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(820.0f, 560.0f), ImVec2(FLT_MAX, FLT_MAX));
    if (!ImGui::Begin("GTA5 DMA 控制台  bilibili 一只小微凉鸭 免费发布 请勿贩卖", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImDrawList* background = ImGui::GetBackgroundDrawList();
    const ImVec2 panelMin = ImGui::GetWindowPos();
    const ImVec2 panelMax(panelMin.x + ImGui::GetWindowWidth(), panelMin.y + ImGui::GetWindowHeight());
    background->AddRectFilled(ImVec2(panelMin.x + 10.0f, panelMin.y + 12.0f), ImVec2(panelMax.x + 10.0f, panelMax.y + 12.0f), IM_COL32(0, 0, 0, 125), 6.0f);
    const ImVec2 available = ImGui::GetContentRegionAvail();

    RenderStatusHeader();

    const float centerHeight = available.y - kTopBarHeight - kStatusBarHeight - ImGui::GetStyle().ItemSpacing.y * 2.0f;
    ImGui::BeginChild("##sidebar", ImVec2(kSidebarWidth, centerHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(12.0f, 14.0f));
    ImGui::BeginGroup();
    RenderQuickControls();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    RenderNavigation(menu);
    ImGui::EndGroup();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("##workspace", ImVec2(0.0f, centerHeight), true);
    ImGui::SetCursorPos(ImVec2(18.0f, 16.0f));
    ImGui::TextColored(kAccent, "%s", PageTitle(menu.GetCurrentPage()));
    ImGui::TextDisabled("%s", PageDescription(menu.GetCurrentPage()));
    ImGui::Dummy(ImVec2(0.0f, 2.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    RenderPage(menu);
    ImGui::EndChild();

    ImGui::BeginChild("##status", ImVec2(0.0f, kStatusBarHeight), true, ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPos(ImVec2(12.0f, 7.0f));
    ImGui::TextDisabled("PID: %lu", static_cast<unsigned long>(DMA::PID));
    ImGui::SameLine();
    ImGui::TextDisabled("Base: 0x%llX", static_cast<unsigned long long>(DMA::BaseAddress));
    ImGui::SameLine();
    ImGui::TextColored(g_inputManager.IsReady() ? kSuccess : kWarning,
        g_inputManager.IsReady() ? "主机热键: 已连接" : "主机热键: 不可用");
    ImGui::SameLine(ImGui::GetWindowWidth() - 160.0f);
    ImGui::TextDisabled("INS 显隐  |  END 退出");
    ImGui::EndChild();

    ImGui::End();
}
