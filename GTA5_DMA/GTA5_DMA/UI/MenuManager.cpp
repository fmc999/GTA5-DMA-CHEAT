#include "pch.h"

#include "MenuManager.h"

#include "ConsoleShell.h"
#include "ConsoleTheme.h"

#include "ArmorManager.h"
#include "GodMode.h"
#include "HealthManager.h"
#include "Invisibility.h"
#include "NoCollision.h"
#include "NoWanted.h"
#include "PlayerSpeed.h"
#include "RefreshHealth.h"
#include "Teleport.h"
#include "VehicleEditor.h"
#include "WeaponInspector.h"

#include "DMA.h"

#include <cstdio>

// 注：旧版独立窗口页面（主菜单 / 时间控制 / 任务分红等）已从活动 UI 移除，
// 其实现代码 retained 在 Attic/LegacyPages.cpp 中，恢复导航后可重新接入。

MenuManager& MenuManager::GetInstance()
{
    static MenuManager instance;
    return instance;
}

void MenuManager::SwitchToPage(MenuPage page)
{
    if (currentPage != page) {
        pageHistory.push_back(currentPage);
        currentPage = page;
    }
}

void MenuManager::GoBack()
{
    if (!pageHistory.empty()) {
        currentPage = pageHistory.back();
        pageHistory.pop_back();
    }
}

void MenuManager::RenderCurrentPage()
{
    ConsoleShell::Render(*this);
}

/* ---------- 人物控制 ---------- */

void MenuManager::RenderPlayerPageContent()
{
    if (ImGui::BeginTable("##player_metrics", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        ImGui::TextDisabled("生命值");
        ImGui::SameLine();
        ImGui::Text("%.0f", HealthManager::currentHealth);
        ImGui::TableNextColumn();
        ImGui::TextDisabled("防弹衣");
        ImGui::SameLine();
        ImGui::Text("%.0f", ArmorManager::currentArmor);
        ImGui::TableNextColumn();
        ImGui::TextDisabled("人物模型");
        ImGui::SameLine();
        ImGui::Text("0x%08X", DMA::LocalPlayerModelHash);
        ImGui::EndTable();
    }
    ImGui::Dummy(ImVec2(0.0f, 8.0f));

    ConsoleTheme::SectionHeader("生存能力", "持续状态保护");

    bool playerGodMode = GodMode::bPlayerGodMode.load();
    if (ConsoleTheme::ToggleRow("player_god", "玩家无敌", "保护人物生命与伤害状态", &playerGodMode)) {
        GodMode::bPlayerGodMode.store(playerGodMode);
        GodMode::bRequestedGodmode.store(true);
    }

    bool vehicleGodMode = GodMode::bVehicleGodMode.load();
    if (ConsoleTheme::ToggleRow("vehicle_god", "载具无敌", "进入载具时持续保护当前载具", &vehicleGodMode)) {
        GodMode::bVehicleGodMode.store(vehicleGodMode);
        GodMode::bRequestedGodmode.store(true);
    }

    ConsoleTheme::ToggleRow("no_wanted", "永不被通缉", "阻止通缉等级持续增加", &NoWanted::bEnable);
    ConsoleTheme::ToggleRow("refresh_health", "自动刷新生命值", "生命值低于阈值时自动恢复", &RefreshHealth::bEnable);
    if (RefreshHealth::bEnable) {
        ImGui::TextDisabled("    当前恢复阈值 %.0f%%", RefreshHealth::HealThresholdPercent * 100.0f);
    }

    char armorRefreshDescription[96] = {};
    std::snprintf(
        armorRefreshDescription,
        sizeof(armorRefreshDescription),
        "当前防弹衣 %.0f，低于 %.0f 自动恢复至 %.0f",
        ArmorManager::currentArmor,
        ArmorManager::ArmorRefreshThreshold,
        ArmorManager::ArmorRefreshValue);
    ConsoleTheme::ToggleRow(
        "refresh_armor",
        "自动刷新防弹衣",
        armorRefreshDescription,
        &ArmorManager::bAutoRefreshArmor);

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ConsoleTheme::SectionHeader("移动与外观", "人物表现与移动参数");

    bool invisible = Invisibility::bInvisibility.load();
    if (ConsoleTheme::ToggleRow("invisibility", "启用隐身", "切换本地人物可见状态", &invisible)) {
        Invisibility::bInvisibility.store(invisible);
    }

    ConsoleTheme::ToggleRow("no_collision", "无碰撞体积", "允许人物穿过常规碰撞体", &NoCollision::bNoCollisionUI);
    ConsoleTheme::ToggleRow("speed_control", "启用速度控制", "调整步行、奔跑与游泳速度", &PlayerSpeed::bEnableUI);
    if (PlayerSpeed::bEnableUI) {
        ImGui::Indent();
        ImGui::Checkbox("野兽模式 (速度1.5)", &PlayerSpeed::bBeastModeUI);
        if (!PlayerSpeed::bBeastModeUI) {
            ImGui::SliderFloat("人物速度", &PlayerSpeed::playerSpeedUI, 1.0f, 10.0f, "%.2f");
        }
        ImGui::Unindent();
    }

    ImGui::TextColored(ImVec4(0.31f, 0.78f, 0.56f, 1.0f), "● 无布娃娃已固定启用");

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ConsoleTheme::SectionHeader("状态锁定", "固定人物基础数值");

    char armorDescription[64] = {};
    std::snprintf(armorDescription, sizeof(armorDescription), "当前防弹衣 %.0f，目标值 200", ArmorManager::currentArmor);
    bool lockArmor = ArmorManager::bLockArmor;
    if (ConsoleTheme::ToggleRow("lock_armor", "锁定防弹衣", armorDescription, &lockArmor)) {
        ArmorManager::bLockArmor = lockArmor;
    }

    char healthDescription[72] = {};
    std::snprintf(healthDescription, sizeof(healthDescription), "当前生命值 %.0f，目标值 200（假无敌）", HealthManager::currentHealth);
    bool lockHealth = HealthManager::bLockHealth;
    if (ConsoleTheme::ToggleRow("lock_health", "锁定生命值", healthDescription, &lockHealth)) {
        HealthManager::bLockHealth = lockHealth;
    }
}

/* ---------- 武器功能 ---------- */

void MenuManager::RenderWeaponPageContent()
{
    WeaponInspector::RenderContent();
}

/* ---------- 传送功能 ---------- */

void MenuManager::RenderTeleportPageContent()
{
    ConsoleTheme::SectionHeader("传送控制", "F5 标记点 / F6 任务点");
    ConsoleTheme::ToggleRow("teleport_enable", "启用传送工具", "显示坐标编辑与预设位置", &Teleport::bEnable);

    if (Teleport::bEnable) {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        Teleport::RenderContent();
    }
}

/* ---------- 载具功能 ---------- */

void MenuManager::RenderVehiclePageContent()
{
    VehicleEditor::RenderContent();
}

/* ---------- 系统设置 ---------- */

void MenuManager::RenderSettingsPageContent()
{
    ConsoleTheme::SectionHeader("界面主题", "切换控制台配色方案");
    ConsoleTheme::RenderThemeSelector();

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ConsoleTheme::SectionHeader("快捷键", "本地与目标主机按键");
    if (ImGui::BeginTable("##hotkeys", 2, ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn(); ImGui::TextUnformatted("Insert");
        ImGui::TableNextColumn(); ImGui::TextDisabled("显示 / 隐藏控制台");
        ImGui::TableNextColumn(); ImGui::TextUnformatted("F5");
        ImGui::TableNextColumn(); ImGui::TextDisabled("传送到地图标记点");
        ImGui::TableNextColumn(); ImGui::TextUnformatted("F6");
        ImGui::TableNextColumn(); ImGui::TextDisabled("传送到任务点（Enhanced）");
        ImGui::TableNextColumn(); ImGui::TextUnformatted("End");
        ImGui::TableNextColumn(); ImGui::TextDisabled("退出程序");
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ConsoleTheme::SectionHeader("关于", "发布信息");
    ImGui::TextUnformatted("GTA5 DMA Control Console");
    ImGui::TextDisabled("bilibili 一只小微凉鸭 · 免费发布 · 请勿贩卖");
    ImGui::TextDisabled("基于 MemProcFS 与 Dear ImGui 构建，仅供技术研究与 DMA 读写学习");
    // DISABLED: 追战局 / 时间控制 / 任务分红设置入口 retained（见 Attic/LegacyPages.cpp）。
}
