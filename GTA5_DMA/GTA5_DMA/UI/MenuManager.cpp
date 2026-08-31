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
#include "PlayerList.h"
#include "VehicleList.h"
#include "PlayerSpeed.h"
#include "RefreshHealth.h"
#include "Teleport.h"
#include "VehicleEditor.h"
#include "WeaponInspector.h"

#include "DMA.h"
#include "UiToast.h"
#include "WindowState.h"

#include <cstdio>
#include <string>

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
    {
        char hp[16], ar[16], mdl[16];
        std::snprintf(hp, sizeof(hp), "%.0f", HealthManager::currentHealth);
        std::snprintf(ar, sizeof(ar), "%.0f", ArmorManager::currentArmor);
        std::snprintf(mdl, sizeof(mdl), "0x%08X", DMA::LocalPlayerModelHash);
        ConsoleTheme::StatPill("生命", hp, HealthManager::currentHealth > 30.0f);
        ImGui::SameLine();
        ConsoleTheme::StatPill("防弹衣", ar, ArmorManager::currentArmor > 0.0f);
        ImGui::SameLine();
        ConsoleTheme::StatPill("人物模型", mdl, true);
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

    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    // 战局载具：默认折叠（主区是载具编辑），展开后显示池扫描表
    static bool showSessionVehicles = false;
    ConsoleTheme::SectionHeader("战局载具", showSessionVehicles ? "点击标题收起" : "实时扫描载具池，点击展开");
    if (ConsoleTheme::NavItem(showSessionVehicles ? "收起  ∧" : "展开  ∨", false))
    {
        showSessionVehicles = !showSessionVehicles;
    }
    if (!showSessionVehicles)
        return;

    const std::vector<SessionVehicle> vehicles = VehicleList::GetSnapshot();
    if (vehicles.empty())
    {
        ImGui::TextDisabled("载具池未激活（需进入游戏并成功解析 VehiclePoolPtr）");
        return;
    }

    ImGui::TextDisabled("50 米内载具: %d 辆（超出不显示）", (int)vehicles.size());
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    const ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
    const float tableHeight = 240.0f;
    if (ImGui::BeginTable("##session_vehicles", 5, flags, ImVec2(0.0f, tableHeight)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("序号", ImGuiTableColumnFlags_WidthFixed, 44.0f);
        ImGui::TableSetupColumn("载具", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("血量", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("距离", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("操作", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableHeadersRow();

        int row = 0;
        for (const SessionVehicle& v : vehicles)
        {
            ImGui::TableNextRow();
            ImGui::PushID(row);

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(std::to_string(v.DisplayIndex).c_str());
            ImGui::TableNextColumn();
            if (v.ModelName[0] != '\0')
                ImGui::TextUnformatted(v.ModelName);
            else
                ImGui::Text("0x%08X", v.ModelHash);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f", v.Health);
            ImGui::TableNextColumn();
            ImGui::Text("%.0f m", v.DistanceM);
            ImGui::TableNextColumn();
            if (ImGui::Button("传送到身边"))
            {
                VehicleList::RequestTeleportVehicle(v.Address);
                UiToast::Show("载具传送请求已发送（仅静止载具有效）", ToastKind::Info);
            }

            ImGui::PopID();
            ++row;
        }
        ImGui::EndTable();
    }
}

/* ---------- 战局玩家 ---------- */

void MenuManager::RenderSessionPageContent()
{
    if (!PlayerList::IsSessionActive())
    {
        ImGui::Dummy(ImVec2(0.0f, 30.0f));
        ImGui::TextColored(ConsoleTheme::Warning(), "未检测到在线战局");
        ImGui::TextDisabled("进入线上模式后，此处将显示战局内的所有玩家");
        return;
    }

    std::vector<SessionPlayer> players = PlayerList::GetSnapshot();

    ConsoleTheme::SectionHeader("战局玩家", "点击行选中玩家");

    // 搜索过滤（名称子串，不区分大小写）
    static char search[32] = "";
    ImGui::SetNextItemWidth(220.0f);
    ImGui::InputTextWithHint("##player_search", "搜索玩家…", search, sizeof(search));
    ImGui::SameLine();
    ImGui::TextDisabled("%zu 人", players.size());
    if (search[0] != '\0')
    {
        const std::string needle(search);
        std::string lower;
        lower.reserve(needle.size());
        for (char c : needle)
            lower.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
        std::vector<SessionPlayer> filtered;
        filtered.reserve(players.size());
        for (const auto& p : players)
        {
            std::string nameLower(p.Name);
            for (char& c : nameLower)
                c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            if (nameLower.find(lower) != std::string::npos)
                filtered.push_back(p);
        }
        players = std::move(filtered);
    }
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // 玩家表格
    static int selectedIndex = -1;
    const ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

    // 表格高度：详情区约 150px；窗口太小时保底 160px，避免布局挤爆
    float tableHeight = ImGui::GetContentRegionAvail().y - 150.0f;
    if (tableHeight < 160.0f) tableHeight = 160.0f;
    if (ImGui::BeginTable("##session_players", 9, flags, ImVec2(0.0f, tableHeight)))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("序号", ImGuiTableColumnFlags_WidthFixed, 44.0f);
        ImGui::TableSetupColumn("名称", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("等级", ImGuiTableColumnFlags_WidthFixed, 52.0f);
        ImGui::TableSetupColumn("金钱", ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableSetupColumn("K/D", ImGuiTableColumnFlags_WidthFixed, 58.0f);
        ImGui::TableSetupColumn("血量", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("护甲", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("距离", ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 96.0f);
        ImGui::TableHeadersRow();

        int row = 0;
        for (const SessionPlayer& player : players)
        {
            ImGui::TableNextRow();
            ImGui::PushID(row);

            ImGui::TableNextColumn();
            const bool selected = (row == selectedIndex);
            if (ImGui::Selectable(std::to_string(player.DisplayIndex > 0 ? player.DisplayIndex : row + 1).c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns))
            {
                selectedIndex = row;
            }

            ImGui::TableNextColumn();
            ImGui::TextUnformatted(player.Name);
            if (player.IsLocal)
            {
                ImGui::SameLine();
                ImGui::TextColored(ConsoleTheme::Accent(), "(本地)");
            }

            ImGui::TableNextColumn();
            if (player.Rank > 0)
                ImGui::Text("%d", player.Rank);
            else
                ImGui::TextDisabled("-");

            ImGui::TableNextColumn();
            if (player.Money > 0)
                ImGui::Text("%.1fM", player.Money / 1000000.0);
            else
                ImGui::TextDisabled("-");

            ImGui::TableNextColumn();
            if (player.KillsOnPlayers + player.DeathsByPlayers > 0)
                ImGui::Text("%.2f", player.KdRatio);
            else
                ImGui::TextDisabled("-");

            ImGui::TableNextColumn();
            ImGui::Text("%.0f", player.Health);

            ImGui::TableNextColumn();
            ImGui::Text("%.0f", player.Armor);

            ImGui::TableNextColumn();
            ImGui::Text("%.0fm", player.Distance);

            ImGui::TableNextColumn();
            if (player.GodMode)
                ImGui::TextColored(ConsoleTheme::Danger(), "无敌");
            else if (player.InVehicle)
                ImGui::TextColored(ConsoleTheme::Accent(), "载具中");
            else
                ImGui::TextDisabled("步行");

            ImGui::PopID();
            ++row;
        }
        ImGui::EndTable();
    }

    // 选中玩家详情 + 操作
    ImGui::Dummy(ImVec2(0.0f, 6.0f));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    if (selectedIndex < 0 || selectedIndex >= static_cast<int>(players.size()))
    {
        ImGui::TextDisabled("选中上方列表中的玩家以执行操作");
        return;
    }

    const SessionPlayer& selected = players[selectedIndex];
    if (selected.IsLocal)
    {
        ImGui::TextDisabled("选中玩家是本地玩家，操作不可用");
        return;
    }

    // 详情统计表
    if (ImGui::BeginTable("##player_detail", 4, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableNextColumn(); ImGui::TextDisabled("等级");
        ImGui::TableNextColumn(); ImGui::Text("%d", selected.Rank);
        ImGui::TableNextColumn(); ImGui::TextDisabled("金钱");
        ImGui::TableNextColumn(); ImGui::Text("%d", selected.Money);

        ImGui::TableNextColumn(); ImGui::TextDisabled("RP");
        ImGui::TableNextColumn(); ImGui::Text("%d", selected.RP);
        ImGui::TableNextColumn(); ImGui::TextDisabled("K/D");
        ImGui::TableNextColumn(); ImGui::Text("%.2f (%d/%d)", selected.KdRatio, selected.KillsOnPlayers, selected.DeathsByPlayers);

        ImGui::TableNextColumn(); ImGui::TextDisabled("RID");
        ImGui::TableNextColumn(); ImGui::Text("%lld", static_cast<long long>(selected.RockstarId));
        ImGui::TableNextColumn(); ImGui::TextDisabled("通缉");
        ImGui::TableNextColumn(); ImGui::Text("%d", selected.WantedLevel);

        ImGui::TableNextColumn(); ImGui::TextDisabled("位置");
        ImGui::TableNextColumn(); ImGui::Text("%.0f, %.0f, %.0f", selected.Position.x, selected.Position.y, selected.Position.z);
        ImGui::TableNextColumn(); ImGui::TextDisabled("Ped");
        ImGui::TableNextColumn(); ImGui::Text("0x%llX", static_cast<unsigned long long>(selected.PedAddress));
        ImGui::EndTable();
    }

    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    // 传送按钮（主题色）
    if (ImGui::Button("传送到此玩家", ImVec2(150.0f, 0.0f)))
    {
        PlayerList::RequestTeleportTo(selected.PlayerIndex);
        UiToast::Show(std::string("传送 → ") + selected.Name, ToastKind::Success);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("把本地玩家传送到目标位置（错开 2 米防卡模）");

    ImGui::Dummy(ImVec2(0.0f, 2.0f));

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.80f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.30f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.70f, 0.10f, 0.10f, 1.0f));
    if (ImGui::Button("击杀 (血量清零)", ImVec2(150.0f, 0.0f)))
    {
        PlayerList::RequestKill(selected.PlayerIndex);
        UiToast::Show(std::string("已请求击杀 ") + selected.Name, ToastKind::Danger);
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine();
    ImGui::TextDisabled("通过内存写入目标玩家血量，对无敌目标无效");
}

/* ---------- 系统设置 ---------- */

void MenuManager::RenderSettingsPageContent()
{
    ConsoleTheme::SectionHeader("界面主题", "切换控制台配色方案（自动保存）");
    ConsoleTheme::RenderThemeSelector();
    WindowState::ThemeIndex = static_cast<int>(ConsoleTheme::GetTheme());

    ImGui::Dummy(ImVec2(0.0f, 10.0f));
    ConsoleTheme::SectionHeader("战局玩家", "玩家加入/离开日志");
    bool logJoinLeave = PlayerList::bLogJoinLeave.load();
    if (ConsoleTheme::ToggleRow("log_join_leave", "加入/离开日志", "在控制台输出玩家进出战局的消息", &logJoinLeave))
    {
        PlayerList::bLogJoinLeave.store(logJoinLeave);
    }

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
