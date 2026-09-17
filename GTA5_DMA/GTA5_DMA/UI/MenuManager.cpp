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
#include "VehicleRepair.h"
#include "PlayerSpeed.h"
#include "RefreshHealth.h"
#include "Teleport.h"
#include "VehicleEditor.h"
#include "WeaponInspector.h"
#include "AimAid.h"
#include "NoIdleKick.h"
#include "Tunables.h"
#include "ProgressFeatures.h"
#include "EconomyFeatures.h"
#include "Diagnostics.h"
#include "ScriptGlobals.h"
#include "ScriptThreads.h"

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
        ConsoleTheme::BoxBegin("player_protection", 5, "防护", layout2.width);

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

        bool noIdleKick = NoIdleKick::bEnable.load();
        if (ConsoleTheme::ToggleRow("no_idle_kick", "防止挂机踢出",
                                    "在线模式中将空闲和受限踢出计时延长至上限", &noIdleKick))
        {
            NoIdleKick::bEnable.store(noIdleKick);
        }

        // 布娃娃由 DMA 线程固定写入（Ragdoll::OnDMAFrame 内硬编码为禁用），
        // 这里只做状态展示，不做成开关，避免给出"可以关掉"的假象。
        ConsoleTheme::TextRow("无布娃娃", "已固定启用", true, false);
        ConsoleTheme::BoxEnd();
        layout2.Advance(0, TitledBoxHeight(5));
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

    // ---- 载具修复（参考 YimMenuV2 Fix / FixAllVehicles，纯 DMA 写健康值）----
    ImGui::Dummy(ImVec2(0.0f, 12.0f));
    ConsoleTheme::SectionHeader("载具修复", "DMA 直写四段健康值并逐字段读回校验");

    {
        const VehicleRepairReport repair = VehicleRepair::GetLastReport();
        const std::vector<SessionVehicle> repairTargets = VehicleList::GetSnapshot();

        ConsoleTheme::BoxBegin("veh_repair", 4, "修复", 0.0f);

        bool autoRepair = VehicleRepair::bAutoRepair.load();
        if (ConsoleTheme::ToggleRow("veh_auto_repair", "自动修复当前载具",
                                    "血量低于 80% 时自动写满四段健康值", &autoRepair))
        {
            VehicleRepair::bAutoRepair.store(autoRepair);
        }

        if (ConsoleTheme::ButtonRow("修复当前载具", UiIcon::Heart, true))
        {
            VehicleRepair::RequestRepairCurrent();
            UiToast::Show("已请求修复当前载具", ToastKind::Info);
        }

        char repairAllLabel[64] = {};
        std::snprintf(repairAllLabel, sizeof(repairAllLabel), "一键修复战局载具（%d 辆）",
                      static_cast<int>(repairTargets.size()));
        if (ConsoleTheme::ButtonRow(repairAllLabel, UiIcon::Refresh))
        {
            VehicleRepair::RequestRepairAll();
            char repairMsg[96] = {};
            std::snprintf(repairMsg, sizeof(repairMsg), "已请求修复 %d 辆战局载具",
                          static_cast<int>(repairTargets.size()));
            UiToast::Show(repairMsg, ToastKind::Info);
        }

        char repairText[192] = {};
        if (!repair.Sent)
        {
            std::snprintf(repairText, sizeof(repairText), "尚未执行");
        }
        else
        {
            std::snprintf(repairText, sizeof(repairText),
                          "%s%s · 修复 %d/%d · 字段校验 %d/%d · 血量 %.0f → %.0f",
                          repair.All ? "全部载具" : "当前载具", repair.Auto ? "（自动）" : "",
                          repair.Fixed, repair.Requested, repair.VerifiedFields, repair.TotalFields,
                          repair.HealthBefore, repair.HealthAfter);
        }
        ConsoleTheme::TextRow("最近一次", repairText, !repair.Sent || repair.Fixed > 0, false);
        ConsoleTheme::BoxEnd();
    }

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
        ConsoleTheme::BoxBegin("player_actions", 3, "对目标执行", detail.width);
        if (ConsoleTheme::ButtonRow("传送到此玩家", UiIcon::Pin, true))
        {
            PlayerList::RequestTeleportTo(selected.PlayerIndex);
            UiToast::Show(std::string("传送 → ") + selected.Name, ToastKind::Success);
        }
        if (ConsoleTheme::ButtonRow("拉到我这里", UiIcon::Target))
        {
            // 目标 → 我（参考 YimMenuV2 Bring）：只写目标玩家的位置，落点在我旁边错开 2 米
            PlayerList::RequestBring(selected.PlayerIndex);
            UiToast::Show(std::string("拉到我这里 ← ") + selected.Name, ToastKind::Info);
        }
        if (ConsoleTheme::ButtonRow("击杀（血量清零）", UiIcon::Zap, false, true))
        {
            PlayerList::RequestKill(selected.PlayerIndex);
            UiToast::Show(std::string("已请求击杀 ") + selected.Name, ToastKind::Danger);
        }
        ConsoleTheme::BoxEnd();
        detail.Advance(1, TitledBoxHeight(3));

        detail.Place(1);
        ConsoleTheme::BoxBegin("player_bring", 2, "最近一次拉人", detail.width);
        const BringReport bring = PlayerList::GetLastBring();
        char bringTarget[64] = {};
        if (bring.Sent)
            std::snprintf(bringTarget, sizeof(bringTarget), "%s",
                          bring.Name[0] != '\0' ? bring.Name : "（未取到名字）");
        else
            std::snprintf(bringTarget, sizeof(bringTarget), "尚未执行");
        ConsoleTheme::TextRow("目标", bringTarget, bring.Sent);
        char bringResult[96] = {};
        std::snprintf(bringResult, sizeof(bringResult), "落点 %.1f, %.1f, %.1f · 读回校验 %s",
                      bring.Landed[0], bring.Landed[1], bring.Landed[2], bring.Ok ? "通过" : "未通过");
        ConsoleTheme::TextRow("结果", bring.Sent ? bringResult : "-", bring.Ok, false);
        ConsoleTheme::BoxEnd();
        detail.Advance(1, TitledBoxHeight(2));

        detail.Place(1);
        ImGui::TextDisabled("传送 / 拉人都错开 2 米防卡模；击杀对无敌目标无效。");
    }

    detail.End();
}

/* ---------- 自瞄 ---------- */

void MenuManager::RenderAimPageContent()
{
    // 本页只保留「自瞄」本身：用 DMA 直接改写游戏里辅助瞄准的四处判定代码
    // （参考 YimMenuV2 Aimbot 的字节补丁，改成纯 DMA 写入，无注入 / 无远程线程）。
    // 补丁点由 AimAid::Resolve() 在启动时按特征码定位；任一失败则该条保持禁用，
    // 绝不在未解析的地址上盲写。
    TwoColumn col;
    col.Begin();

    /* ================== 左列：自瞄开关 ================== */

    // 1) 自瞄
    {
        col.Place(0);
        ConsoleTheme::BoxBegin("aim_main", 3, "自瞄", col.width);

        bool assisted = AimAid::bAssistedAim.load();
        if (ConsoleTheme::ToggleRow("aim_assisted", "辅助瞄准增强",
                                    "解锁目标排除 + 锁定辅助瞄准类型", &assisted, true))
        {
            AimAid::bAssistedAim.store(assisted);
        }

        bool head = AimAid::bAimForHead.load();
        if (ConsoleTheme::ToggleRow("aim_head", "锁定头部",
                                    "瞄准点直接取头部（爆头）", &head, true))
        {
            AimAid::bAimForHead.store(head);
        }

        bool driver = AimAid::bDriverLockOn.load();
        if (ConsoleTheme::ToggleRow("aim_driver", "驾驶员锁定",
                                    "允许对载具驾驶员锁定", &driver, false))
        {
            AimAid::bDriverLockOn.store(driver);
        }
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxHeight(3));
    }

    /* ================== 右列：补丁点状态 ================== */

    // 2) 补丁点解析（自检：四处特征码是否都定位到了、是否已生效）
    {
        col.Place(1);
        ConsoleTheme::BoxBegin("aim_patch", 6, "补丁点状态", col.width);

        char resolvedText[32] = {};
        std::snprintf(resolvedText, sizeof(resolvedText), "%d / 4", AimAid::GetResolvedCount());
        ConsoleTheme::TextRow("已定位", resolvedText, AimAid::GetResolvedCount() == 4, true);

        // 逐条列出生效状态与补丁地址
        const AimAid::PatchId ids[4] = {
            AimAid::PatchId::ShouldNotTarget,
            AimAid::PatchId::AssistedAimType,
            AimAid::PatchId::LockOnPos,
            AimAid::PatchId::DriverLockOn,
        };
        const char* names[4] = { "目标排除 A", "瞄准类型 B", "锁定头部 C", "驾驶员 D" };
        for (int i = 0; i < 4; ++i)
        {
            const bool res = AimAid::IsResolved(ids[i]);
            const bool app = AimAid::IsApplied(ids[i]);
            char buf[56] = {};
            if (!res)
                std::snprintf(buf, sizeof(buf), "未定位");
            else
                std::snprintf(buf, sizeof(buf), "%s 0x%llX", app ? "已生效" : "待命",
                              static_cast<unsigned long long>(AimAid::GetSite(ids[i])));
            ConsoleTheme::TextRow(names[i], buf, res, i < 3);
        }

        // 写入争用：被其它工具立刻覆盖的次数（>0 说明该补丁点在和别人抢）
        const int contend = AimAid::GetContentionCount(AimAid::PatchId::ShouldNotTarget)
                          + AimAid::GetContentionCount(AimAid::PatchId::AssistedAimType)
                          + AimAid::GetContentionCount(AimAid::PatchId::LockOnPos)
                          + AimAid::GetContentionCount(AimAid::PatchId::DriverLockOn);
        char cText[48] = {};
        if (contend == 0)
            std::snprintf(cText, sizeof(cText), "无");
        else
            std::snprintf(cText, sizeof(cText), "被覆盖 %d 次", contend);
        ConsoleTheme::TextRow("写入争用", cText, contend == 0, false);
        ConsoleTheme::BoxEnd();
        col.Advance(1, TitledBoxHeight(6));
    }

    // 3) 写入通道
    {
        col.Place(1);
        ConsoleTheme::BoxBegin("aim_link", 3, "写入通道", col.width);
        ConsoleTheme::TextRow("游戏进程", DMA::IsReady() ? "已连接" : "未连接", DMA::IsReady(), true);
        ConsoleTheme::TextRow("写入方式", "DMA 直写 .text（绕过页保护）", true, true);
        ConsoleTheme::TextRow("还原方式", "关闭开关即写回原始字节", true, false);
        ConsoleTheme::BoxEnd();
        col.Advance(1, TitledBoxHeight(3));
    }

    col.End();
}

/* ---------- 进度与解锁（tunable）---------- */

void MenuManager::RenderProgressPageContent()
{
    // 本页所有开关都写脚本 tunable（纯 DMA 写脚本全局单元），
    // 索引由 Tunables 的值锚动态定位 + 默认值体检给出；页面右侧常显实读值，便于一眼核对。
    TwoColumn col;
    col.Begin();

    /* ================== 左列：功能开关 ================== */

    {
        col.Place(0);
        ConsoleTheme::BoxBegin("prog_recovery", 4, "进度与解锁", col.width);

        bool rp = ProgressFeatures::bRpMultiplier.load();
        if (ConsoleTheme::ToggleRow("rp_multiplier", "RP 倍率",
                                    "写 tunable XP_MULTIPLIER（游戏刷新 tunables 后自动重写）", &rp, true))
        {
            ProgressFeatures::bRpMultiplier.store(rp);
        }

        float rpv = ProgressFeatures::rpMultiplier.load();
        if (ConsoleTheme::SliderRow("rp_multiplier_value", "倍率", &rpv, 0.5f, 10.0f, "%.2f x", true))
        {
            ProgressFeatures::rpMultiplier.store(rpv);
        }

        bool freeAppearance = ProgressFeatures::bFreeAppearance.load();
        if (ConsoleTheme::ToggleRow("free_appearance", "改外貌免费",
                                    "写 tunable CHARACTER_APPEARANCE_CHARGE = 0", &freeAppearance, true))
        {
            ProgressFeatures::bFreeAppearance.store(freeAppearance);
        }

        bool noCooldown = ProgressFeatures::bNoAppearanceCooldown.load();
        if (ConsoleTheme::ToggleRow("no_appearance_cooldown", "改外貌免冷却",
                                    "写 tunable CHARACTER_APPEARANCE_COOLDOWN = 0", &noCooldown, false))
        {
            ProgressFeatures::bNoAppearanceCooldown.store(noCooldown);
        }
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxHeight(4));
    }

    {
        // 第18轮：金额/额度类（抢劫收益、挑战奖励）。这些 tunable 没有固定默认值，
        // 用「倍率 × 解析时记录的原值」写入，clamp 到登记表区间；关闭即写回原值。
        // 高度必须按**实际内容行数**算：总开关 + 滑杆 + 每条 1 行。
        // 之前这里写死 12、而内容是 9 条 × 2 行 = 20 行 → 溢出框外压住下一个框（用户看到的"错位乱套"）。
        const int valueRows = 2 + static_cast<int>(ProgressFeatures::kValueSlotCount);
        col.Place(0);
        ConsoleTheme::BoxBegin("prog_value", valueRows, "抢劫与经济价值（金额类）", col.width);

        bool master = ProgressFeatures::bValueMultiplier.load();
        if (ConsoleTheme::ToggleRow("value_master", "启用金额倍率",
                                    "对下面勾选的条目写入「原值 × 倍率」，关闭后写回原值", &master, true))
            ProgressFeatures::bValueMultiplier.store(master);

        float mult = ProgressFeatures::valueMultiplier.load();
        if (ConsoleTheme::SliderRow("value_mult", "倍率", &mult, 1.0f, 10.0f, "%.1f x", true))
            ProgressFeatures::valueMultiplier.store(mult);

        for (uint32_t i = 0; i < ProgressFeatures::kValueSlotCount; ++i)
        {
            const char* label = ProgressFeatures::GetValueSlotLabel(i);
            const char* rawName = ProgressFeatures::GetValueSlotName(i);
            if (!rawName || rawName[0] == '?')
                continue;

            // 每个槽位一个独立 ID 作用域：内部控件自动互不冲突
            // （之前 9 行都用同一个 "   实读" 标签 → ImGui 报 "5 visible items with conflicting ID"）。
            ImGui::PushID(static_cast<int>(i));

            bool on = ProgressFeatures::valueSlotEnabled[i].load();
            bool ok = false;
            const int32_t live = ProgressFeatures::GetValueSlotLive(i, &ok);
            const int32_t original = ProgressFeatures::GetValueSlotOriginal(i);

            // 一条只占一行：数值状态放进描述文字，避免框高失控
            char desc[160] = {};
            std::snprintf(desc, sizeof(desc), "%s值 %s → %s", ok ? "原" : "读失败·原",
                          original > 0 ? std::to_string(original).c_str() : "—",
                          ok ? std::to_string(live).c_str() : "—");
            if (ConsoleTheme::ToggleRow("slot", label ? label : rawName, desc, &on, true))
                ProgressFeatures::valueSlotEnabled[i].store(on);

            ImGui::PopID();
        }
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxHeight(valueRows));
    }

    {
        const ProgressFeatures::Report report = ProgressFeatures::GetReport();
        col.Place(0);
        ConsoleTheme::BoxBegin("prog_state", 4, "生效状态", col.width);

        char slots[64] = {};
        // 分母写全量（进度页关心的是 20 条 tunable + 3 个进度开关），别再写死 3 让人以为是坏的
        std::snprintf(slots, sizeof(slots), "%d / %d", ProgressFeatures::GetResolvedCount(), 3);
        ConsoleTheme::TextRow("进度开关已定位", slots, ProgressFeatures::GetResolvedCount() == 3, true);

        int valueReady = 0;
        for (uint32_t i = 0; i < ProgressFeatures::kValueSlotCount; ++i)
        {
            const char* rawName = ProgressFeatures::GetValueSlotName(i);
            if (rawName && rawName[0] != '?')
                ++valueReady;
        }
        char values[64] = {};
        std::snprintf(values, sizeof(values), "%d / %d 条可用", valueReady,
                      static_cast<int>(ProgressFeatures::kValueSlotCount));
        ConsoleTheme::TextRow("金额条目", values, valueReady == static_cast<int>(ProgressFeatures::kValueSlotCount), true);

        char applied[48] = {};
        std::snprintf(applied, sizeof(applied), "%d 项已写入", report.appliedSlots);
        ConsoleTheme::TextRow("本轮写入", applied, report.appliedSlots > 0, true);

        char blocked[48] = {};
        std::snprintf(blocked, sizeof(blocked), "%d 次被拒/被覆盖", report.blockedWrites);
        ConsoleTheme::TextRow("写入争用", blocked, report.blockedWrites == 0, false);
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxHeight(4));
    }

    /* ================== 右列：tunable 自检表 ================== */

    {
        col.Place(1);
        ConsoleTheme::BoxBeginPixels("prog_selftest", 420.0f, "tunable 自检", col.width);

        char anchor[64] = {};
        std::snprintf(anchor, sizeof(anchor), "element %d", Tunables::GetAnchorElement());
        ConsoleTheme::TextRow("值锚位置", anchor, Tunables::GetAnchorElement() >= 0, true);

        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("##tunable_selftest", 4, flags, ImVec2(0.0f, 330.0f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            // 列宽自适应框宽：之前 4 列固定宽度合计 450px > 右栏框宽 → 最后一列被截成「状」。
            ImGui::TableSetupColumn("功能", ImGuiTableColumnFlags_WidthStretch, 1.60f);
            ImGui::TableSetupColumn("索引", ImGuiTableColumnFlags_WidthFixed, 66.0f);
            ImGui::TableSetupColumn("实读值", ImGuiTableColumnFlags_WidthFixed, 84.0f);
            ImGui::TableSetupColumn("定位", ImGuiTableColumnFlags_WidthStretch, 0.90f);
            ImGui::TableHeadersRow();

            for (uint32_t i = 0; i < Tunables::kSlotCount; ++i)
            {
                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(i));

                ImGui::TableNextColumn();
                {
                    const char* rawName = Tunables::GetEntryName(i);
                    const char* label = Tunables::GetEntryLabel(i);
                    ImGui::TextUnformatted((label && label[0] && label[0] != '?') ? label : rawName);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", rawName);
                }

                ImGui::TableNextColumn();
                ImGui::Text("0x%X", Tunables::GetGlobalIndex(i));

                ImGui::TableNextColumn();
                bool ok = false;
                const int32_t live = Tunables::ReadLive(i, &ok);
                if (!ok)
                {
                    ImGui::TextDisabled("-");
                }
                else
                {
                    const bool isFloat = std::strstr(Tunables::GetEntryName(i), "XP_MULTIPLIER") != nullptr;
                    if (isFloat)
                    {
                        float f = 0.0f;
                        std::memcpy(&f, &live, sizeof(f));
                        ImGui::Text("%.2f", f);
                    }
                    else
                    {
                        ImGui::Text("%d", live);
                    }
                }

                ImGui::TableNextColumn();
                if (Tunables::IsResolved(i))
                    ImGui::TextColored(ConsoleTheme::Success(), "%s", Tunables::GetLocatedBy(i));
                else
                    ImGui::TextColored(ConsoleTheme::Danger(), "未定位");

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ConsoleTheme::BoxEnd();
        col.Advance(1, TitledBoxPixels(420.0f));
    }

    {
        col.Place(1);
        ConsoleTheme::BoxBegin("prog_note", 3, "说明", col.width);
        ConsoleTheme::TextRow("写入方式", "DMA 直写脚本全局单元", true, true);
        ConsoleTheme::TextRow("定位方式", "四连组校验 / 候选绝对位置（单值邻域搜索已移除）", true, true);
        ConsoleTheme::TextRow("安全阀", "当前值必须等于默认值才允许写", true, false);
        ConsoleTheme::BoxEnd();
        col.Advance(1, TitledBoxHeight(3));
    }

    col.End();
}

/* ---------- 经济与自动化（第17轮）---------- */

void MenuManager::RenderEconomyPageContent()
{
    // 本页动作全部写「脚本全局动作格」：先体检（值必须落在登记表的合法区间内）再写，写完读回校验。
    // 保险箱领取是脉冲写（写 1，1.5 秒后自动还原），所以按钮点一下就够，不需要一直开着。
    TwoColumn col;
    col.Begin();

    /* ================== 左列：动作 ================== */

    {
        col.Place(0);
        ConsoleTheme::BoxBegin("eco_auto", 4, "自动动作", col.width);

        bool autoClaim = EconomyFeatures::bAutoClaimSafeEarnings.load();
        if (ConsoleTheme::ToggleRow("eco_auto_claim", "自动领取保险箱",
                                    "周期触发 7 个产业的保险箱动作格（游戏自己判断有没有钱）", &autoClaim, true))
            EconomyFeatures::bAutoClaimSafeEarnings.store(autoClaim);

        float interval = static_cast<float>(EconomyFeatures::claimIntervalSeconds.load());
        if (ConsoleTheme::SliderRow("eco_auto_claim_interval", "领取间隔", &interval, 5.0f, 120.0f, "%.0f 秒", true))
            EconomyFeatures::claimIntervalSeconds.store(static_cast<int>(interval));

        bool autoSilence = EconomyFeatures::bAutoSilenceCalls.load();
        if (ConsoleTheme::ToggleRow("eco_auto_silence", "自动静音来电",
                                    "读 状态/通话中/来电 三个格，条件成立就把状态写成 6", &autoSilence, true))
            EconomyFeatures::bAutoSilenceCalls.store(autoSilence);

        bool gtaPlus = EconomyFeatures::bUnlockGTAPlus.load();
        if (ConsoleTheme::ToggleRow("eco_gta_plus", "GTA+ 解锁",
                                    "写 GTA_PLUS_ENABLED / 权益位 + 引擎标志（关闭时还原）", &gtaPlus, false))
        {
            EconomyFeatures::bUnlockGTAPlus.store(gtaPlus);
            EconomyFeatures::SetGTAPlus(gtaPlus);
        }
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxHeight(4));
    }

    {
        col.Place(0);
        ConsoleTheme::BoxBegin("eco_safe", 8, "产业保险箱（一键领取）", col.width);

        if (ConsoleTheme::ButtonRow("一键领取全部产业", UiIcon::Refresh, true))
            EconomyFeatures::ClaimAllSafes();

        for (uint32_t i = 0; i < static_cast<uint32_t>(EconomyFeatures::kSafeCount); ++i)
        {
            char label[64] = {};
            std::snprintf(label, sizeof(label), "%s", EconomyFeatures::GetBusinessName(i));
            const bool ready = EconomyFeatures::IsBusinessReady(i);
            if (ConsoleTheme::ButtonRow(label, UiIcon::Save, false, false))
                EconomyFeatures::ClaimSafe(i);
            if (!ready)
                ImGui::TextDisabled("    (未定位：全局索引 0x%X 不在合法值区间内，已跳过)",
                                    EconomyFeatures::GetBusinessGlobalIndex(i));
        }
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxHeight(8));
    }

    {
        col.Place(0);
        ConsoleTheme::BoxBegin("eco_state", 4, "状态", col.width);
        ConsoleTheme::TextRow("动作计数", EconomyFeatures::GetStatusLine(), true, true);
        ConsoleTheme::TextRow("最近动作", EconomyFeatures::GetLastAction(), true, true);

        char gp[64] = {};
        const int gpAddr = EconomyFeatures::GetGtaPlusFlagAddress();
        if (gpAddr)
            std::snprintf(gp, sizeof(gp), "已定位 @ 0x%X", static_cast<unsigned>(gpAddr));
        else
            std::snprintf(gp, sizeof(gp), "未定位");
        ConsoleTheme::TextRow("GTA+ 引擎标志", gp, gpAddr != 0, false);



        if (ConsoleTheme::ButtonRow("导出诊断（写 GTA5_DMA_diag.txt）", UiIcon::Save, false))
            Diagnostics::WriteReport(nullptr);
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxHeight(3));
    }

    /* ================== 右列：只读自检 ================== */

    {
        col.Place(1);
        ConsoleTheme::BoxBeginPixels("eco_threads", 290.0f, "脚本线程（只读）", col.width);

        ConsoleTheme::TextRow("数组", ScriptThreads::GetSummary(), ScriptThreads::IsReady(), true);

        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("##eco_thread_list", 3, flags, ImVec2(0.0f, 190.0f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("脚本", ImGuiTableColumnFlags_WidthFixed, 190.0f);
            ImGui::TableSetupColumn("hash", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("locals 基址（栈）", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            const uint32_t total = ScriptThreads::Count();
            for (uint32_t i = 0; i < total && i < 64; ++i)
            {
                ScriptThreads::ThreadInfo info{};
                if (!ScriptThreads::Get(i, info))
                    continue;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(info.name[0] ? info.name : "(无名)");
                ImGui::TableNextColumn();
                ImGui::Text("0x%08X", info.hash);
                ImGui::TableNextColumn();
                ImGui::Text("0x%llX", static_cast<unsigned long long>(info.stack));
            }
            ImGui::EndTable();
        }
        ConsoleTheme::BoxEnd();
        col.Advance(1, TitledBoxPixels(290.0f));
    }

    {
        col.Place(1);
        ConsoleTheme::BoxBeginPixels("eco_globals", 330.0f, "脚本全局自检", col.width);

        const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        if (ImGui::BeginTable("##eco_global_list", 4, flags, ImVec2(0.0f, 280.0f)))
        {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("动作格", ImGuiTableColumnFlags_WidthFixed, 190.0f);
            ImGui::TableSetupColumn("索引", ImGuiTableColumnFlags_WidthFixed, 78.0f);
            ImGui::TableSetupColumn("实读值", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableHeadersRow();

            for (uint32_t i = 0; i < ScriptGlobals::kSlotCount; ++i)
            {
                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(i));

                ImGui::TableNextColumn();
                const char* rawName = ScriptGlobals::GetEntryName(i);
                const char* purpose = ScriptGlobals::GetEntryPurpose(i);
                ImGui::TextUnformatted((purpose && purpose[0] && purpose[0] != '?') ? purpose : rawName);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", rawName);

                ImGui::TableNextColumn();
                ImGui::Text("0x%X", ScriptGlobals::GetGlobalIndex(i));

                ImGui::TableNextColumn();
                bool ok = false;
                const int32_t live = ScriptGlobals::ReadLive(i, &ok);
                if (ok)
                    ImGui::Text("%d", live);
                else
                    ImGui::TextDisabled("-");

                ImGui::TableNextColumn();
                if (ScriptGlobals::IsResolved(i))
                    ImGui::TextColored(ConsoleTheme::Success(), "已定位");
                else
                    ImGui::TextColored(ConsoleTheme::Danger(), "未定位");

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ConsoleTheme::BoxEnd();
        col.Advance(1, TitledBoxPixels(330.0f));
    }

    col.End();
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
