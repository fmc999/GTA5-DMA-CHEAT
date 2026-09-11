#include "pch.h"

#include "MenuManager.h"

#include "ConsoleShell.h"
#include "ConsoleTheme.h"
#include "AppFonts.h"

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

namespace
{
// 盒子之间的纵向间距（Portfolio #8 的 content 间距）
constexpr float kBoxGap = 10.0f;

// 两列排布：内容盒是子窗口，不参与 ImGui 的组布局，因此必须显式定位。
// Begin() 记录内容区原点与列宽，Place(col) 把光标移到该列当前行，
// Advance(col, h) 推进该列游标，End() 把光标落到两列底部之后。
struct TwoColumn
{
    ImVec2 origin = ImVec2(0.0f, 0.0f);
    float  width = 0.0f;
    float  gap = layout::content_gap;
    float  y[2] = { 0.0f, 0.0f };

    void Begin()
    {
        origin = ImGui::GetCursorScreenPos();
        width = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
        y[0] = y[1] = origin.y;
    }

    void Place(int column)
    {
        const int col = (column < 0 || column > 1) ? 0 : column;
        ImGui::SetCursorScreenPos(ImVec2(origin.x + static_cast<float>(col) * (width + gap), y[col]));
    }

    void Advance(int column, float boxHeight)
    {
        const int col = (column < 0 || column > 1) ? 0 : column;
        y[col] += boxHeight + kBoxGap;
    }

    void End()
    {
        char note[128];
        std::snprintf(note, sizeof(note), "COLUMN y0=%.0f y1=%.0f delta=%.0f contentTop=%.0f",
                      y[0] - origin.y, y[1] - origin.y, (y[0] > y[1] ? y[0] : y[1]) - (y[0] < y[1] ? y[0] : y[1]),
                      origin.y);
        ConsoleTheme::TraceNote(note);
        ImGui::SetCursorScreenPos(ImVec2(origin.x, y[0] > y[1] ? y[0] : y[1]));
        ImGui::Dummy(ImVec2(width, 0.0f));
    }
};

float BoxHeight(int rows)
{
    return layout::box_height(rows);
}

// 带标题的盒子实际占位 = 标题行(section_h) + 盒子本体高度。
// 之前只推进了盒子本体的高度，导致下一个盒子的标题画进了上一个盒子底部（错位 26px）。
float TitledBoxHeight(int rows)
{
    return layout::box_height(rows) + layout::section_h;
}

float TitledBoxPixels(float height)
{
    return height + layout::section_h;
}
} // namespace

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
    // 左列放「功能」（开关类），右列放「读数与信息」：分区固定，找开关不用左右横跳。
    TwoColumn layout2;
    layout2.Begin();

    /* ================== 左列：功能开关 ================== */

    // 1) 防护：持续保护开关
    {
        layout2.Place(0);
        ConsoleTheme::BoxBegin("player_protection", 4, "防护", layout2.width);

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

        ConsoleTheme::ToggleRow("no_wanted", "永不被通缉", "阻止通缉等级持续增加", &NoWanted::bEnable, false);

        // 布娃娃由 DMA 线程固定写入（Ragdoll::OnDMAFrame 内硬编码为禁用），
        // 这里只做状态展示，不做成开关，避免给出"可以关掉"的假象。
        ConsoleTheme::TextRow("无布娃娃", "已固定启用", true, false);
        ConsoleTheme::BoxEnd();
        layout2.Advance(0, TitledBoxHeight(4));
    }

    // 2) 恢复与锁定：开关与其参数（阈值）同盒，数值锁定紧随其后
    {
        const bool thresholdVisible = RefreshHealth::bEnable;
        const int recoveryRows = 4 + (thresholdVisible ? 1 : 0);

        char armorRefreshDescription[96] = {};
        std::snprintf(
            armorRefreshDescription,
            sizeof(armorRefreshDescription),
            "当前 %.0f，低于 %.0f 自动恢复至 %.0f",
            ArmorManager::currentArmor,
            ArmorManager::ArmorRefreshThreshold,
            ArmorManager::ArmorRefreshValue);

        layout2.Place(0);
        ConsoleTheme::BoxBegin("player_recovery", recoveryRows, "恢复与锁定", layout2.width);

        ConsoleTheme::ToggleRow("refresh_health", "自动刷新生命值", "生命值低于阈值时自动恢复", &RefreshHealth::bEnable, thresholdVisible);
        if (thresholdVisible)
        {
            ConsoleTheme::SliderRow("refresh_threshold", "恢复阈值", &RefreshHealth::HealThresholdPercent,
                                    0.10f, 1.0f, "%.0f%%", true, 100.0f);
        }
        ConsoleTheme::ToggleRow("refresh_armor", "自动刷新防弹衣", armorRefreshDescription, &ArmorManager::bAutoRefreshArmor);

        char armorDescription[64] = {};
        std::snprintf(armorDescription, sizeof(armorDescription), "当前防弹衣 %.0f，目标值 200", ArmorManager::currentArmor);
        bool lockArmor = ArmorManager::bLockArmor;
        if (ConsoleTheme::ToggleRow("lock_armor", "锁定防弹衣", armorDescription, &lockArmor)) {
            ArmorManager::bLockArmor = lockArmor;
        }

        char healthDescription[72] = {};
        std::snprintf(healthDescription, sizeof(healthDescription), "当前生命值 %.0f，目标值 200（假无敌）", HealthManager::currentHealth);
        bool lockHealth = HealthManager::bLockHealth;
        if (ConsoleTheme::ToggleRow("lock_health", "锁定生命值", healthDescription, &lockHealth, false)) {
            HealthManager::bLockHealth = lockHealth;
        }
        ConsoleTheme::BoxEnd();
        layout2.Advance(0, TitledBoxHeight(recoveryRows));
    }

    // 3) 移动与外观：速度开关与其参数（野兽模式 / 速度值）同盒
    {
        const int moveRows = 3 + (PlayerSpeed::bEnableUI ? (PlayerSpeed::bBeastModeUI ? 1 : 2) : 0);
        layout2.Place(0);
        ConsoleTheme::BoxBegin("player_move", moveRows, "移动与外观", layout2.width);

        bool invisible = Invisibility::bInvisibility.load();
        if (ConsoleTheme::ToggleRow("invisibility", "启用隐身", "切换本地人物可见状态", &invisible)) {
            Invisibility::bInvisibility.store(invisible);
        }

        ConsoleTheme::ToggleRow("no_collision", "无碰撞体积", "允许人物穿过常规碰撞体", &NoCollision::bNoCollisionUI, PlayerSpeed::bEnableUI);
        ConsoleTheme::ToggleRow("speed_control", "启用速度控制", "调整步行、奔跑与游泳速度", &PlayerSpeed::bEnableUI, PlayerSpeed::bEnableUI);

        if (PlayerSpeed::bEnableUI)
        {
            ConsoleTheme::ToggleRow("beast_mode", "野兽模式", "速度锁定为 1.5 倍", &PlayerSpeed::bBeastModeUI, !PlayerSpeed::bBeastModeUI);
            if (!PlayerSpeed::bBeastModeUI)
            {
                ConsoleTheme::SliderRow("player_speed", "人物速度", &PlayerSpeed::playerSpeedUI, 1.0f, 10.0f, "%.2f", false);
            }
        }
        ConsoleTheme::BoxEnd();
        layout2.Advance(0, TitledBoxHeight(moveRows));
    }

    /* ================== 右列：读数与信息 ================== */
    // 只做展示，不引入任何新的读写逻辑：数值全部来自 DMA / 各功能模块已有的公开状态。

    // 4) 实时状态：一眼可见的三项核心读数
    {
        char model[24] = {};
        std::snprintf(model, sizeof(model), "0x%08X", DMA::LocalPlayerModelHash);

        layout2.Place(1);
        ConsoleTheme::BoxBegin("player_status", 3, "实时状态", layout2.width);
        ConsoleTheme::MeterRow("生命值", HealthManager::currentHealth, 200.0f, HealthManager::currentHealth > 30.0f);
        ConsoleTheme::MeterRow("防弹衣", ArmorManager::currentArmor, 200.0f, ArmorManager::currentArmor > 0.0f);
        ConsoleTheme::TextRow("人物模型", model, true);
        ConsoleTheme::BoxEnd();
        layout2.Advance(1, TitledBoxHeight(3));
    }

    // 5) 人物坐标：来自 DMA::LocalPlayerLocation（传送/载具页同源）
    {
        char sx[32] = {};
        char sy[32] = {};
        char sz[32] = {};
        std::snprintf(sx, sizeof(sx), "%.1f", DMA::LocalPlayerLocation.x);
        std::snprintf(sy, sizeof(sy), "%.1f", DMA::LocalPlayerLocation.y);
        std::snprintf(sz, sizeof(sz), "%.1f", DMA::LocalPlayerLocation.z);

        layout2.Place(1);
        ConsoleTheme::BoxBegin("player_coords", 3, "人物坐标", layout2.width);
        ConsoleTheme::TextRow("X 轴", sx, true);
        ConsoleTheme::TextRow("Y 轴", sy, true);
        ConsoleTheme::TextRow("Z 轴", sz, true);
        ConsoleTheme::BoxEnd();
        layout2.Advance(1, TitledBoxHeight(3));
    }

    // 6) 连接与地址：只读诊断信息（底部状态栏的三个读数 + 人物 / 载具基址）
    {
        char pid[24] = {};
        char base[32] = {};
        char player[32] = {};
        char vehicle[32] = {};
        std::snprintf(pid, sizeof(pid), "%lu", static_cast<unsigned long>(DMA::PID));
        std::snprintf(base, sizeof(base), "0x%llX", static_cast<unsigned long long>(DMA::BaseAddress));
        std::snprintf(player, sizeof(player), "0x%llX", static_cast<unsigned long long>(DMA::LocalPlayerAddress));
        std::snprintf(vehicle, sizeof(vehicle), "0x%llX", static_cast<unsigned long long>(DMA::VehicleAddress));
        const bool ready = DMA::IsReady();

        layout2.Place(1);
        ConsoleTheme::BoxBegin("player_link", 5, "连接与地址", layout2.width);
        ConsoleTheme::TextRow("连接状态", ready ? "已连接" : "等待连接", ready);
        ConsoleTheme::TextRow("进程 PID", pid, ready);
        ConsoleTheme::TextRow("模块基址", base, ready);
        ConsoleTheme::TextRow("人物基址", player, ready);
        ConsoleTheme::TextRow("载具基址", vehicle, ready, false);
        ConsoleTheme::BoxEnd();
        layout2.Advance(1, TitledBoxHeight(5));
    }

    layout2.End();
}


/* ---------- 武器功能 ---------- */

void MenuManager::RenderWeaponPageContent()
{
    // 武器页的分组标题由页面自己按盒子给出（SectionHeader 不重复叠两层）
    WeaponInspector::RenderContent();
}

/* ---------- 传送功能 ---------- */

void MenuManager::RenderTeleportPageContent()
{
    ConsoleTheme::SectionHeader("传送控制", "F5 标记点 / F6 任务点");
    // 传送页不再需要总开关：进入页面即可用（用户反馈“不需要那个启用传送的开关”）
    Teleport::bEnable = true;
    Teleport::RenderContent();
}

/* ---------- 载具功能 ---------- */

void MenuManager::RenderVehiclePageContent()
{
    // 方向说明（页首常显）：点「传送到它」= 把你送到那辆车旁边（我 → 载具），
    // 方向不会反过来把车拉过来；构建标记用于确认正跑的 exe 是不是最新修复版。
    // 放页首的原因：不展开「战局载具」也能一眼看到方向与版本。
    ImGui::TextDisabled("方向: 你 → 载具（错开 2 米防卡模） · 构建 %s", DMA::BuildTag);
    {
        // 自检：这一行渲染时把方向/构建标记写进 ui_check.txt，
        // 实机上点完按钮可直接把 ui_check.txt 发回来核对是哪条分支。
        char dirTrace[128] = {};
        std::snprintf(dirTrace, sizeof(dirTrace), "NOTE TP2VEHUI build=%s dir=me->veh", DMA::BuildTag);
        ConsoleTheme::TraceNote(dirTrace);

        const TpToVehicleReport rep = VehicleList::GetLastTeleport();
        if (rep.Sent)
        {
            ImGui::TextDisabled("最近一次: 你 → 载具 #%u  落点 (%.1f, %.1f, %.1f)  读回校验 %s",
                                rep.Index, rep.Landed[0], rep.Landed[1], rep.Landed[2],
                                rep.Ok ? "通过" : "未通过");

            char resTrace[192] = {};
            std::snprintf(resTrace, sizeof(resTrace),
                          "NOTE TP2VEH #%u veh=0x%llX 载具=(%.1f,%.1f,%.1f) 落点=(%.1f,%.1f,%.1f) ok=%d",
                          rep.Index, (unsigned long long)rep.Vehicle,
                          rep.VehiclePos[0], rep.VehiclePos[1], rep.VehiclePos[2],
                          rep.Landed[0], rep.Landed[1], rep.Landed[2], rep.Ok ? 1 : 0);
            ConsoleTheme::TraceNote(resTrace);
        }
    }
    ImGui::Dummy(ImVec2(0.0f, 10.0f));

    VehicleEditor::RenderContent();

    ImGui::Dummy(ImVec2(0.0f, 12.0f));

    // 战局载具：默认折叠（主区是载具编辑），展开后显示池扫描表
    static bool showSessionVehicles = false;
    ConsoleTheme::SectionHeader("战局载具", showSessionVehicles ? "点击标题收起" : "实时扫描载具池，点击展开");
    if (ConsoleTheme::NavItem(showSessionVehicles ? "收起载具扫描" : "展开载具扫描", false))
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
    if (ConsoleTheme::BoxBeginPixels("##session_vehicles_box", tableHeight + 20.0f))
    {
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
                if (ImGui::Button("传送到它"))
                {
                    // 我 → 载具：把玩家送到这一行载具的位置（错开 2 米防卡模）。
                    // 目标坐标 = 本行显示的那份快照值，所见即所得。
                    VehicleList::RequestTeleportToVehicle(v.Address, v.DisplayIndex);
                    char tpMsg[160] = {};
                    std::snprintf(tpMsg, sizeof(tpMsg),
                                  "你 → 载具 #%u  (%.1f, %.1f) 错开 2 米 [%s]",
                                  v.DisplayIndex, v.Position[0], v.Position[1], DMA::BuildTag);
                    UiToast::Show(tpMsg, ToastKind::Info);
                }

                ImGui::PopID();
                ++row;
            }
            ImGui::EndTable();
        }
        ConsoleTheme::BoxEnd();
    }
}

/* ---------- 战局玩家 ---------- */

void MenuManager::RenderSessionPageContent()
{
    if (!PlayerList::IsSessionActive())
    {
        TwoColumn layout2;
        layout2.Begin();

        {
            layout2.Place(0);
            ConsoleTheme::BoxBegin("session_empty", 3, "战局状态", layout2.width);
            ConsoleTheme::TextRow("在线战局", "未检测到", false);
            ConsoleTheme::TextRow("玩家池", "未激活", false);
            ConsoleTheme::TextRow("刷新方式", "进入线上模式后自动刷新", true, false);
            ConsoleTheme::BoxEnd();
            layout2.Advance(0, TitledBoxHeight(3));

            layout2.Place(0);
            ImGui::PushFont(AppFonts::Small);
            ImGui::TextDisabled("提示：需先进入 GTA5 线上模式，DMA 才会解析玩家池指针。");
            ImGui::PopFont();
        }

        {
            char pid[24] = {};
            char baseAddr[24] = {};
            char localPlayer[24] = {};
            std::snprintf(pid, sizeof(pid), "%lu", static_cast<unsigned long>(DMA::PID));
            std::snprintf(baseAddr, sizeof(baseAddr), "0x%llX", static_cast<unsigned long long>(DMA::BaseAddress));
            std::snprintf(localPlayer, sizeof(localPlayer), "0x%llX", static_cast<unsigned long long>(DMA::LocalPlayerAddress));

            layout2.Place(1);
            ConsoleTheme::BoxBegin("session_link", 4, "连接状态", layout2.width);
            ConsoleTheme::TextRow("读取通道", DMA::vmh ? "MemProcFS 已就绪" : "未初始化", DMA::vmh != 0);
            ConsoleTheme::TextRow("游戏进程", DMA::PID ? pid : "未连接", DMA::PID != 0);
            ConsoleTheme::TextRow("游戏基址", DMA::BaseAddress ? baseAddr : "未解析", DMA::BaseAddress != 0);
            ConsoleTheme::TextRow("本地玩家", DMA::LocalPlayerAddress ? localPlayer : "未解析", DMA::LocalPlayerAddress != 0, false);
            ConsoleTheme::BoxEnd();
            layout2.Advance(1, TitledBoxHeight(4));
        }

        layout2.End();
        return;
    }

    std::vector<SessionPlayer> players = PlayerList::GetSnapshot();

    // 搜索过滤（名称子串，不区分大小写）：先过滤再画「筛选」盒，计数才准确
    static char search[32] = "";
    const size_t totalPlayers = players.size();
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

    ConsoleTheme::BoxBegin("session_filter", 1, "筛选", 0.0f);
    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##player_search", "搜索玩家…", search, sizeof(search));
    ImGui::SameLine();
    ImGui::TextDisabled("显示 %zu / 共 %zu 人", players.size(), totalPlayers);
    ConsoleTheme::BoxEnd();
    ImGui::Dummy(ImVec2(0.0f, 4.0f));

    static int selectedIndex = -1;
    const ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;

    // 表格高度：先给下方详情区留出空间，再按需收缩
    const bool hasSelection = selectedIndex >= 0 && selectedIndex < static_cast<int>(players.size());
    float reserve = hasSelection ? 190.0f : 40.0f;
    float tableHeight = ImGui::GetContentRegionAvail().y - reserve;
    if (tableHeight < 150.0f) tableHeight = 150.0f;

    ConsoleTheme::SectionHeader("玩家列表", "点击行选中玩家");
    if (ConsoleTheme::BoxBeginPixels("##session_players_box", tableHeight + 20.0f))
    {
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
        ConsoleTheme::BoxEnd();
    }

    if (!hasSelection)
    {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextDisabled("选中上方列表中的玩家以执行操作");
        return;
    }

    const SessionPlayer& selected = players[selectedIndex];
    if (selected.IsLocal)
    {
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextDisabled("选中玩家是本地玩家，操作不可用");
        return;
    }

    ImGui::Dummy(ImVec2(0.0f, 6.0f));

    // 详情与操作：左列 = 数据，右列 = 可执行动作
    TwoColumn detail;
    detail.Begin();

    {
        char position[64];
        std::snprintf(position, sizeof(position), "%.0f, %.0f, %.0f", selected.Position.x, selected.Position.y, selected.Position.z);
        char money[48];
        std::snprintf(money, sizeof(money), "%d", selected.Money);
        char rank[48];
        std::snprintf(rank, sizeof(rank), "%d / %d", selected.Rank, selected.RP);
        char rid[48];
        std::snprintf(rid, sizeof(rid), "%lld", static_cast<long long>(selected.RockstarId));

        detail.Place(0);
        ConsoleTheme::BoxBegin("player_detail", 4, "玩家详情", detail.width);
        ConsoleTheme::TextRow("等级 / RP", rank, true);
        ConsoleTheme::TextRow("金钱", money, true);
        ConsoleTheme::TextRow("RID", rid, true);
        ConsoleTheme::TextRow("坐标", position, true, false);
        ConsoleTheme::BoxEnd();
        detail.Advance(0, TitledBoxHeight(4));

        char kd[64];
        std::snprintf(kd, sizeof(kd), "%.2f (%d/%d)", selected.KdRatio, selected.KillsOnPlayers, selected.DeathsByPlayers);
        detail.Place(0);
        ConsoleTheme::BoxBegin("player_combat", 3, "战斗数据", detail.width);
        ConsoleTheme::TextRow("K/D", kd, true);
        ConsoleTheme::TextRow("通缉等级", std::to_string(selected.WantedLevel).c_str(), true);
        ConsoleTheme::TextRow("载具状态", selected.InVehicle ? "载具中" : "步行", true, false);
        ConsoleTheme::BoxEnd();
        detail.Advance(0, TitledBoxHeight(3));
    }

    {
        detail.Place(1);
        ConsoleTheme::BoxBegin("player_actions", 2, "对目标执行", detail.width);
        if (ConsoleTheme::ButtonRow("传送到此玩家", UiIcon::Pin, true))
        {
            PlayerList::RequestTeleportTo(selected.PlayerIndex);
            UiToast::Show(std::string("传送 → ") + selected.Name, ToastKind::Success);
        }
        if (ConsoleTheme::ButtonRow("击杀（血量清零）", UiIcon::Zap, false, true))
        {
            PlayerList::RequestKill(selected.PlayerIndex);
            UiToast::Show(std::string("已请求击杀 ") + selected.Name, ToastKind::Danger);
        }
        ConsoleTheme::BoxEnd();
        detail.Advance(1, TitledBoxHeight(2));

        detail.Place(1);
        ImGui::TextDisabled("传送会错开 2 米防卡模；击杀对无敌目标无效。");
    }

    detail.End();
}

/* ---------- 系统设置 ---------- */

void MenuManager::RenderSettingsPageContent()
{
    TwoColumn layout2;
    layout2.Begin();

    // ---- 左列：外观 / 界面 ----
    {
        layout2.Place(0);
        ConsoleTheme::BoxBeginPixels("settings_appearance", 132.0f, "外观", layout2.width);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
        ImGui::TextDisabled("明暗模式");
        ConsoleTheme::RenderThemeSwatches();
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::TextDisabled("强调色 · %s", ConsoleTheme::AccentName(ConsoleTheme::GetAccent()));
        ConsoleTheme::RenderAccentSwatches();
        ImGui::PopStyleVar();
        ConsoleTheme::BoxEnd();
        layout2.Advance(0, TitledBoxPixels(132.0f));
    }

    {
        layout2.Place(1);
        ConsoleTheme::BoxBegin("settings_layout", 1, "界面", layout2.width);
        bool collapsed = WindowState::SidebarCollapsed;
        if (ConsoleTheme::ToggleRow("sidebar_collapsed", "收起侧边栏", "仅保留图标，扩大工作区", &collapsed, false))
        {
            WindowState::SidebarCollapsed = collapsed;
        }
        ConsoleTheme::BoxEnd();
        layout2.Advance(1, TitledBoxHeight(1));
    }

    // ---- 右列：通知 / 快捷键 / 关于 ----
    {
        layout2.Place(1);
        ConsoleTheme::BoxBegin("settings_session", 1, "战局通知", layout2.width);
        bool logJoinLeave = PlayerList::bLogJoinLeave.load();
        if (ConsoleTheme::ToggleRow("log_join_leave", "加入/离开日志", "在控制台输出玩家进出战局的消息", &logJoinLeave, false))
        {
            PlayerList::bLogJoinLeave.store(logJoinLeave);
        }
        ConsoleTheme::BoxEnd();
        layout2.Advance(1, TitledBoxHeight(1));
    }

    {
        layout2.Place(0);
        ConsoleTheme::BoxBegin("settings_hotkeys", 4, "快捷键", layout2.width);
        ConsoleTheme::TextRow("Insert", "显示 / 隐藏控制台");
        ConsoleTheme::TextRow("F5", "传送到地图标记点");
        ConsoleTheme::TextRow("F6", "传送到任务点（Enhanced）");
        ConsoleTheme::TextRow("End", "退出程序", true, false);
        ConsoleTheme::BoxEnd();
        layout2.Advance(0, TitledBoxHeight(4));
    }

    {
        layout2.Place(1);
        ConsoleTheme::BoxBegin("settings_about", 4, "关于", layout2.width);
        ConsoleTheme::TextRow("程序", "GTA5 DMA 控制台");
        ConsoleTheme::TextRow("发布", "bilibili 一只小微凉鸭", true);
        ConsoleTheme::TextRow("声明", "免费发布 · 请勿贩卖", true);
        ConsoleTheme::TextRow("技术栈", "MemProcFS + Dear ImGui", true, false);
        ConsoleTheme::BoxEnd();
        layout2.Advance(1, TitledBoxHeight(4));

        layout2.Place(1);
        ImGui::TextDisabled("仅供技术研究与 DMA 读写学习");
    }

    layout2.End();
}
