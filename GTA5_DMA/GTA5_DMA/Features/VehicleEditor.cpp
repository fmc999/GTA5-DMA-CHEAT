#include "pch.h"
#include "ConsoleTheme.h"
#include "VehicleEditor.h"
#include "DMA.h"
#include "Reclass.h"
#include "Offsets.h"
#include <cmath>

// Debug: Print size of CHandlingData structure
static_assert(sizeof(CHandlingData) == 0x87C, "CHandlingData size mismatch");

DWORD VehicleEditor::BytesRead = 0;

// Current vehicle values
float VehicleEditor::currentAcceleration = 0.0f;
float VehicleEditor::currentMass = 0.0f;
float VehicleEditor::currentJetCharge = 0.0f;
float VehicleEditor::currentVehicleHealth = 0.0f;
float VehicleEditor::currentEngineHealth = 0.0f;
float VehicleEditor::currentVehicleMaxHealth = 0.0f;
float VehicleEditor::currentVehicleHealthAlt = 0.0f;  // 车辆健康 (新)
float VehicleEditor::currentBodyHealth = 0.0f;        // 车身健康 (新)
float VehicleEditor::currentTankHealth = 0.0f;        // 车辆油箱健康 (新)
float VehicleEditor::currentDragCoefficient = 0.0f;   // 阻力系数
float VehicleEditor::currentBuoyancy = 0.0f;          // 浮力
float VehicleEditor::currentDriveInertia = 0.0f;      // 驱动惯性
float VehicleEditor::currentInitialDriveForce = 0.0f; // 初始驱动力
float VehicleEditor::currentBrakeForce = 0.0f;        // 制动力
float VehicleEditor::currentHandbrakeForce = 0.0f;    // 手刹力
float VehicleEditor::currentTractionCurveMax = 0.0f;  // 牵引曲线max
float VehicleEditor::currentTractionCurveMin = 0.0f;  // 牵引曲线min
float VehicleEditor::currentCollisionMultiplier = 0.0f; // 碰撞Multi
float VehicleEditor::currentWeaponMultiplier = 0.0f;  // 武器Multi
float VehicleEditor::currentDeformationMultiplier = 0.0f; // 变形Multi
float VehicleEditor::currentEngine = 0.0f;            // 发动机
float VehicleEditor::currentThrust = 0.0f;            // 推力
uint8_t VehicleEditor::currentParachuteStatus = 0;
uint32_t VehicleEditor::currentVehicleModelHash = 0;
uint32_t VehicleEditor::currentVehicleEntityModelHash = 0;
uint8_t VehicleEditor::currentVehicleState = 0;
uint8_t VehicleEditor::currentVehicleFreezeFlag = 0;
uint8_t VehicleEditor::currentVehicleWeaponAmmo = 0;
uintptr_t VehicleEditor::trackedVehicleAddress = 0;
bool VehicleEditor::bFreezeVehicle = false;
int VehicleEditor::desiredVehicleWeaponAmmo = 0;

// 新增变量定义
bool VehicleEditor::bInfiniteJet = false;              // 无限喷气开关
int VehicleEditor::vehicleAddonMode = 0;            // 载具附加功能模式
bool VehicleEditor::bVehicleAddonApply = false;         // 载具附加功能自动应用
bool VehicleEditor::bParachuteEnabled = false;         // 降落伞开关
bool VehicleEditor::bAutoRepair = false;                // 自动修复开关
bool VehicleEditor::bRepairTriggered = false;           // 修复触发标志
bool VehicleEditor::bVehicleRepair18 = false;           // 18修复载具开关
bool VehicleEditor::bSeatBeltEnabled = false;         // 安全带开关

// Desired vehicle values (for editing)

float VehicleEditor::desiredAcceleration = 1.0f;

float VehicleEditor::desiredMass = 1.0f;

float VehicleEditor::desiredDragCoefficient = 1.0f;   // 阻力系数
float VehicleEditor::desiredBuoyancy = 1.0f;          // 浮力
float VehicleEditor::desiredDriveInertia = 1.0f;      // 驱动惯性
float VehicleEditor::desiredInitialDriveForce = 1.0f; // 初始驱动力
float VehicleEditor::desiredBrakeForce = 1.0f;        // 制动力
float VehicleEditor::desiredHandbrakeForce = 1.0f;    // 手刹力
float VehicleEditor::desiredTractionCurveMax = 1.0f;  // 牵引曲线max
float VehicleEditor::desiredTractionCurveMin = 1.0f;  // 牵引曲线min
float VehicleEditor::desiredCollisionMultiplier = 1.0f; // 碰撞Multi
float VehicleEditor::desiredWeaponMultiplier = 1.0f;  // 武器Multi
float VehicleEditor::desiredDeformationMultiplier = 1.0f; // 变形Multi
float VehicleEditor::desiredEngine = 1.0f;            // 发动机
float VehicleEditor::desiredThrust = 1.0f;            // 推力

// 导弹相关变量初始化
float VehicleEditor::currentLockOnRange = 0.0f;
float VehicleEditor::currentWeaponRange = 0.0f;
float VehicleEditor::desiredLockOnRange = 1000.0f;
float VehicleEditor::desiredWeaponRange = 1500.0f;
bool VehicleEditor::bEnableMissileMods = false;
bool VehicleEditor::bAutoApplyMissileMods = false;

// 跳跃恢复速度相关变量初始化
float VehicleEditor::currentJumpRecoverySpeed = 0.0f;
bool VehicleEditor::bLockJumpRecoverySpeed = false;

// 喷气恢复速度相关变量初始化
float VehicleEditor::currentJetRecoverySpeed = 0.0f;
bool VehicleEditor::bLockJetRecoverySpeed = false;

// 更新跳跃恢复速度
bool VehicleEditor::UpdateJumpRecoverySpeed()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置读取跳跃恢复速度:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x3A0
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x3A0
    uintptr_t finalAddr = addr3 + 0x3A0;
    
    // 读取跳跃恢复速度值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentJumpRecoverySpeed = value;
    return true;
}

// 设置跳跃恢复速度
bool VehicleEditor::SetJumpRecoverySpeed(float value)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置设置跳跃恢复速度:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x3A0
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x3A0
    uintptr_t finalAddr = addr3 + 0x3A0;
    
    // 写入跳跃恢复速度值
    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float));
    
    return true;
}



bool VehicleEditor::bNeedsOverwrite = false;
bool VehicleEditor::bRequestedCopyToDesired = false;

bool VehicleEditor::RenderContent() {
    ConsoleTheme::SectionHeader("载具工作区", "属性、附加能力与操控数据");

    // 使用标签页组织功能模块
    if (ImGui::BeginTabBar("VehicleTabs", ImGuiTabBarFlags_None)) {
        // 载具信息标签页
        const bool tabOpen1 = ImGui::BeginTabItem("载具信息");
        {
            const ImVec2 tmin = ImGui::GetItemRectMin();
            const ImVec2 tmax = ImGui::GetItemRectMax();
            char tbuf[128];
            snprintf(tbuf, sizeof(tbuf), "TAB 载具信息 %.0f %.0f %.0f %.0f", tmin.x, tmin.y, tmax.x, tmax.y);
            ConsoleTheme::TraceNote(tbuf);
        }
        if (tabOpen1) {
            const bool hasVehicle = DMA::VehicleAddress != 0;

            const char* modeText = "未知";
            switch (vehicleAddonMode) {
                case 0: modeText = "默认"; break;
                case 40: modeText = "跳跃"; break;
                case 66: modeText = "加速"; break;
                case 96: modeText = "跳跃+加速"; break;
            }
            const char* vehicleStateText = "未知";
            switch (currentVehicleState) {
                case 0: vehicleStateText = "玩家载具"; break;
                case 1: vehicleStateText = "NPC载具"; break;
                case 2: vehicleStateText = "玩家载具（状态2）"; break;
                case 3: vehicleStateText = "已毁坏"; break;
            }
            char va[80] = {};
            char vb[80] = {};

            // ── 2 列卡片：左列 当前载具属性 / 载具操作 ｜ 右列 载具标识与弹药 / 修改载具属性 ──
            // 原来每张卡各占一整行、右半边空着 —— 这里按列并排，卡片内容一字未改。
            ConsoleTheme::Columns col;
            col.Begin(2);

            col.Place(0);
            ConsoleTheme::BoxBegin("veh_stats", 5, "当前载具属性", col.width);
            snprintf(va, sizeof(va), "%.2f", currentAcceleration);
            snprintf(vb, sizeof(vb), "%.2f", currentMass);
            ConsoleTheme::TextRow2("加速度", va, true, "质量", vb, true);
            snprintf(va, sizeof(va), "%.2f / %.2f", currentVehicleHealth, currentVehicleMaxHealth);
            snprintf(vb, sizeof(vb), "%.2f", currentEngineHealth);
            ConsoleTheme::TextRow2("载具血量", va, true, "引擎血量", vb, true);
            snprintf(va, sizeof(va), "%.2f", currentVehicleHealthAlt);
            snprintf(vb, sizeof(vb), "%.2f", currentBodyHealth);
            ConsoleTheme::TextRow2("车辆健康", va, true, "车身健康", vb, true);
            snprintf(va, sizeof(va), "%.2f", currentTankHealth);
            ConsoleTheme::TextRow2("油箱健康", va, true, "降落伞状态", currentParachuteStatus ? "开启" : "关闭", true);
            ConsoleTheme::TextRow2("功能模式", modeText, true, "载具状态", vehicleStateText, false);
            ConsoleTheme::BoxEnd();
            col.Advance(0, ConsoleTheme::TitledBoxHeight(5));

            col.Place(0);
            ConsoleTheme::BoxBeginPixels("veh_ops", layout::box_height(2) + 45.0f, "载具操作", col.width);
            if (!hasVehicle)
                ImGui::BeginDisabled();
            {
                bool freezeVehicle = bFreezeVehicle;
                if (ConsoleTheme::ToggleRow("##freeze_vehicle", "冻结当前载具", "锁定载具位置与物理模拟", &freezeVehicle)) {
                    if (SetVehicleFrozen(freezeVehicle))
                        bFreezeVehicle = freezeVehicle;
                }
                float ammoF = static_cast<float>(desiredVehicleWeaponAmmo);
                if (ConsoleTheme::InputRow("##veh_ammo", "目标载具武器弹药", &ammoF, "%.0f")) {
                    int v = static_cast<int>(ammoF + 0.5f);
                    desiredVehicleWeaponAmmo = v < 0 ? 0 : (v > 255 ? 255 : v);
                }
                if (ConsoleTheme::ButtonRow("应用弹药数值", UiIcon::Check, true))
                    SetVehicleWeaponAmmo(static_cast<uint8_t>(desiredVehicleWeaponAmmo));
            }
            if (!hasVehicle)
                ImGui::EndDisabled();
            ConsoleTheme::BoxEnd();
            col.Advance(0, ConsoleTheme::TitledBoxPixels(layout::box_height(2) + 45.0f));

            col.Place(1);
            ConsoleTheme::BoxBegin("veh_id", 3, "载具标识与弹药", col.width);
            if (hasVehicle) {
                snprintf(va, sizeof(va), "0x%08X", (unsigned)currentVehicleModelHash);
                snprintf(vb, sizeof(vb), "0x%08X", (unsigned)currentVehicleEntityModelHash);
            } else {
                snprintf(va, sizeof(va), "%s", "--");
                snprintf(vb, sizeof(vb), "%s", "--");
            }
            ConsoleTheme::TextRow2("载具模型", va, hasVehicle, "实体哈希", vb, hasVehicle);
            snprintf(va, sizeof(va), "%s", hasVehicle ? vehicleStateText : "--");
            snprintf(vb, sizeof(vb), "%s", hasVehicle ? (currentVehicleFreezeFlag ? "已冻结" : "未冻结") : "--");
            ConsoleTheme::TextRow2("载具状态", va, hasVehicle, "冻结状态", vb, hasVehicle);
            if (hasVehicle)
                snprintf(va, sizeof(va), "%u", (unsigned)currentVehicleWeaponAmmo);
            else
                snprintf(va, sizeof(va), "%s", "--");
            ConsoleTheme::TextRow2("当前载具武器弹药", va, hasVehicle, "", "", hasVehicle, false);
            ConsoleTheme::BoxEnd();
            col.Advance(1, ConsoleTheme::TitledBoxHeight(3));

            col.Place(1);
            ConsoleTheme::BoxBeginPixels("veh_edit", layout::box_height(2) + 90.0f, "修改载具属性", col.width);
            ConsoleTheme::InputRow("##veh_acc", "加速度", &desiredAcceleration, "%.2f");
            ConsoleTheme::InputRow("##veh_mass", "质量", &desiredMass, "%.0f");
            if (ConsoleTheme::ButtonRow("更新", UiIcon::Refresh, true))
                bNeedsOverwrite = true;
            if (ConsoleTheme::ButtonRow("复制当前到目标", UiIcon::Folder, false))
                bRequestedCopyToDesired = true;
            ConsoleTheme::BoxEnd();
            col.Advance(1, ConsoleTheme::TitledBoxPixels(layout::box_height(2) + 90.0f));

            col.End();

            ImGui::EndTabItem();
        }

        
        // 飞行与跳跃标签页
        const bool tabOpen2 = ImGui::BeginTabItem("飞行·修复");
        {
            const ImVec2 tmin = ImGui::GetItemRectMin();
            const ImVec2 tmax = ImGui::GetItemRectMax();
            char tbuf[128];
            snprintf(tbuf, sizeof(tbuf), "TAB 飞行·修复 %.0f %.0f %.0f %.0f", tmin.x, tmin.y, tmax.x, tmax.y);
            ConsoleTheme::TraceNote(tbuf);
        }
        if (tabOpen2) {
            char vbuf[64] = {};
            char va[64] = {};
            char vb[64] = {};
            const float btnBlock = 45.0f;   // 一个按钮行实测占位（34px 按钮 + 行距），与布局自检对齐

            // ── 2 列卡片：左列 喷气 / 跳跃恢复 / 载具附加 / 降落伞 ｜ 右列 喷气恢复 / 安全带 / 修复 / 导弹 ──
            // 原来每张卡各占一整行、右半边空着 —— 这里按列并排，卡片内容一字未改。
            ConsoleTheme::Columns col;
            col.Begin(2);

            col.Place(0);
            ConsoleTheme::BoxBegin("veh_jet", 2, "喷气功能", col.width);
            snprintf(vbuf, sizeof(vbuf), "%.2f", currentJetCharge);
            ConsoleTheme::TextRow("喷气充能进度", vbuf, true);
            {
                bool infiniteJet = bInfiniteJet;
                if (ConsoleTheme::ToggleRow("##infinite_jet", "锁定无限喷气", "开启后喷气充能锁定为 1.25", &infiniteJet, false)) {
                    bInfiniteJet = infiniteJet;
                    if (infiniteJet) {
                        SetJetChargeValue(1.25f);
                    }
                }
            }
            ConsoleTheme::BoxEnd();
            col.Advance(0, ConsoleTheme::TitledBoxHeight(2));

            col.Place(0);
            ConsoleTheme::BoxBegin("veh_jump_recovery", 2, "跳跃恢复", col.width);
            snprintf(vbuf, sizeof(vbuf), "%.2f", currentJumpRecoverySpeed);
            ConsoleTheme::TextRow("当前恢复速度", vbuf, true);
            {
                bool lockJumpRecoverySpeed = bLockJumpRecoverySpeed;
                if (ConsoleTheme::ToggleRow("##lock_jump_recovery", "锁定恢复速度", "锁定后保持当前跳跃恢复速度", &lockJumpRecoverySpeed, false)) {
                    bLockJumpRecoverySpeed = lockJumpRecoverySpeed;
                }
            }
            ConsoleTheme::BoxEnd();
            col.Advance(0, ConsoleTheme::TitledBoxHeight(2));

            // 左列 · 载具附加功能（原「附加功能」标签页并入本页）
            col.Place(0);
            {
                int addonMode = vehicleAddonMode;
                int selectedMode = 0;
                if (addonMode == 40) selectedMode = 1;
                else if (addonMode == 66) selectedMode = 2;
                else if (addonMode == 96) selectedMode = 3;
                static const char* addonItems[] = { "默认(0)", "跳跃(40)", "加速(66)", "二者都(96)" };

                ConsoleTheme::BoxBeginPixels("veh_addon", layout::box_height(2) + btnBlock, "载具附加功能", col.width);
                if (ConsoleTheme::ComboRow("##addon_mode", "附加功能模式", addonItems, 4, &selectedMode, "选择载具附加能力组合")) {
                    int actualValue = 0;
                    switch (selectedMode) {
                        case 0: actualValue = 0; break;
                        case 1: actualValue = 40; break;
                        case 2: actualValue = 66; break;
                        case 3: actualValue = 96; break;
                    }
                    vehicleAddonMode = actualValue;
                    if (bVehicleAddonApply) {
                        SetVehicleAddon(actualValue);
                    }
                }
                {
                    bool autoApply = bVehicleAddonApply;
                    if (ConsoleTheme::ToggleRow("##auto_apply_addon", "立即生效", "切换模式后立即写入载具", &autoApply)) {
                        bVehicleAddonApply = autoApply;
                        if (autoApply) {
                            SetVehicleAddon(vehicleAddonMode);
                        }
                    }
                }
                if (ConsoleTheme::ButtonRow("点击应用", UiIcon::Check, true)) {
                    SetVehicleAddon(vehicleAddonMode);
                }
                ConsoleTheme::BoxEnd();
            }
            col.Advance(0, ConsoleTheme::TitledBoxPixels(layout::box_height(2) + btnBlock));

            col.Place(0);
            ConsoleTheme::BoxBegin("veh_parachute", 1, "降落伞设置", col.width);
            {
                bool parachuteEnabled = bParachuteEnabled;
                if (ConsoleTheme::ToggleRow("##parachute_toggle", "降落伞开关", "为载具启用降落伞能力", &parachuteEnabled, false)) {
                    bParachuteEnabled = parachuteEnabled;
                    SetParachute(parachuteEnabled);
                }
            }
            ConsoleTheme::BoxEnd();
            col.Advance(0, ConsoleTheme::TitledBoxHeight(1));

            col.Place(1);
            ConsoleTheme::BoxBegin("veh_jet_recovery", 2, "喷气恢复", col.width);
            snprintf(vbuf, sizeof(vbuf), "%.2f", currentJetRecoverySpeed);
            ConsoleTheme::TextRow("喷气恢复速度", vbuf, true);
            {
                bool lockJetRecoverySpeed = bLockJetRecoverySpeed;
                if (ConsoleTheme::ToggleRow("##lock_jet_recovery", "锁定喷气恢复速度", "开启后恢复速度锁定为 5.0", &lockJetRecoverySpeed, false)) {
                    bLockJetRecoverySpeed = lockJetRecoverySpeed;
                    if (lockJetRecoverySpeed) {
                        SetJetRecoverySpeedValue(5.0f);
                    }
                }
            }
            ConsoleTheme::BoxEnd();
            col.Advance(1, ConsoleTheme::TitledBoxHeight(2));

            col.Place(1);
            ConsoleTheme::BoxBegin("veh_seatbelt", 1, "安全带设置", col.width);
            {
                int seatBeltMode = bSeatBeltEnabled ? 1 : 0;
                static const char* seatBeltOptions[] = { "关闭", "开启" };
                if (ConsoleTheme::ComboRow("##seat_belt_combo", "安全带模式", seatBeltOptions, IM_ARRAYSIZE(seatBeltOptions), &seatBeltMode,
                                           "开启后按载具类型写入：普通载具201，摩托车0", false)) {
                    bSeatBeltEnabled = seatBeltMode == 1;
                    SetSeatBelt(bSeatBeltEnabled);
                }
            }
            ConsoleTheme::BoxEnd();
            col.Advance(1, ConsoleTheme::TitledBoxHeight(1));

            col.Place(1);
            ConsoleTheme::BoxBeginPixels("veh_repair", layout::box_height(1) + btnBlock, "载具修复选项", col.width);
            if (ConsoleTheme::ButtonRow("一键修复", UiIcon::Shield, true)) {
                bRepairTriggered = true;
            }
            {
                bool autoRepair = bAutoRepair;
                if (ConsoleTheme::ToggleRow("##auto_repair", "自动修复", "持续监测并自动修复载具", &autoRepair, false)) {
                    bAutoRepair = autoRepair;
                }
            }
            ConsoleTheme::BoxEnd();
            col.Advance(1, ConsoleTheme::TitledBoxPixels(layout::box_height(1) + btnBlock));

            col.Place(1);
            ConsoleTheme::BoxBegin("veh_repair_skin", 1, "外观修复", col.width);
            {
                bool vehicleRepair18 = bVehicleRepair18;
                if (ConsoleTheme::ToggleRow("##vehicle_repair_18", "载具外观修复", "启用后将持续执行载具外观修复操作", &vehicleRepair18, false)) {
                    bVehicleRepair18 = vehicleRepair18;
                }
            }
            ConsoleTheme::BoxEnd();
            col.Advance(1, ConsoleTheme::TitledBoxHeight(1));

            col.Place(1);
            ConsoleTheme::BoxBegin("veh_missile_state", 2, "导弹属性设置", col.width);
            snprintf(va, sizeof(va), "%.2f", currentLockOnRange);
            snprintf(vb, sizeof(vb), "%.2f", currentWeaponRange);
            ConsoleTheme::TextRow2("锁定范围", va, true, "有效距离", vb, true);
            {
                bool enableMissileMods = bEnableMissileMods;
                if (ConsoleTheme::ToggleRow("##enable_missile_mods", "启用导弹功能", "解锁下方导弹属性修改", &enableMissileMods, false)) {
                    bEnableMissileMods = enableMissileMods;
                }
            }
            ConsoleTheme::BoxEnd();
            col.Advance(1, ConsoleTheme::TitledBoxHeight(2));

            // 右列 · 修改导弹属性（原「导弹功能」标签页并入本页，启用后展开）
            if (bEnableMissileMods) {
            col.Place(1);
                ConsoleTheme::BoxBeginPixels("veh_missile_edit", layout::box_height(3) + btnBlock, "修改导弹属性", col.width);
                ConsoleTheme::InputRow("##input_lock_range", "锁定范围", &desiredLockOnRange, "%.0f");
                ConsoleTheme::InputRow("##input_weapon_range", "有效距离", &desiredWeaponRange, "%.0f");
                {
                    bool autoApplyMissileMods = bAutoApplyMissileMods;
                    if (ConsoleTheme::ToggleRow("##auto_apply_missile", "立即生效", "修改后立即写入导弹属性", &autoApplyMissileMods)) {
                        bAutoApplyMissileMods = autoApplyMissileMods;
                        if (autoApplyMissileMods) {
                            SetLockOnRange(desiredLockOnRange);
                            SetWeaponRange(desiredWeaponRange);
                        }
                    }
                }
                if (ConsoleTheme::ButtonRow("应用导弹设置", UiIcon::Target, true)) {
                    SetLockOnRange(desiredLockOnRange);
                    SetWeaponRange(desiredWeaponRange);
                }
                ConsoleTheme::BoxEnd();
            col.Advance(1, ConsoleTheme::TitledBoxPixels(layout::box_height(3) + btnBlock));
            }

            (void)btnBlock;
            col.End();

            ImGui::EndTabItem();
        }
        const bool tabOpen4 = ImGui::BeginTabItem("操控数据");
        {
            const ImVec2 tmin = ImGui::GetItemRectMin();
            const ImVec2 tmax = ImGui::GetItemRectMax();
            char tbuf[128];
            snprintf(tbuf, sizeof(tbuf), "TAB 操控数据 %.0f %.0f %.0f %.0f", tmin.x, tmin.y, tmax.x, tmax.y);
            ConsoleTheme::TraceNote(tbuf);
        }
        if (tabOpen4) {
            ConsoleTheme::SectionHeader("操控数据", "实时数值 · 修改后点“更新操控数据”写入");

            char now01[48], now02[48], now03[48], now04[48], now05[48], now06[48], now07[48], now08[48];
            char now09[48], now10[48], now11[48], now12[48], now13[48], now14[48], now15[48];
            snprintf(now01, sizeof(now01), "%.0f", currentMass);
            snprintf(now02, sizeof(now02), "%.6f", currentDragCoefficient);
            snprintf(now03, sizeof(now03), "%.6f", currentBuoyancy);
            snprintf(now04, sizeof(now04), "%.6f", currentAcceleration);
            snprintf(now05, sizeof(now05), "%.6f", currentDriveInertia);
            snprintf(now06, sizeof(now06), "%.6f", currentInitialDriveForce);
            snprintf(now07, sizeof(now07), "%.6f", currentBrakeForce);
            snprintf(now08, sizeof(now08), "%.6f", currentHandbrakeForce);
            snprintf(now09, sizeof(now09), "%.6f", currentTractionCurveMax);
            snprintf(now10, sizeof(now10), "%.6f", currentTractionCurveMin);
            snprintf(now11, sizeof(now11), "%.6f", currentCollisionMultiplier);
            snprintf(now12, sizeof(now12), "%.6f", currentWeaponMultiplier);
            snprintf(now13, sizeof(now13), "%.6f", currentDeformationMultiplier);
            snprintf(now14, sizeof(now14), "%.6f", currentEngine);
            snprintf(now15, sizeof(now15), "%.6f", currentThrust);

            // ── 双列卡片：左「当前操控数据」（8 行 · 每行两组读数）｜右「修改操控数据」（8 行 · 每行两组步进）──
            // 15 项读 + 15 项改原来竖向堆到一屏之外，现在左右并排，写入按钮紧贴两列下方。
            ConsoleTheme::Columns col;
            col.Begin(2);

            // 左列 · 当前操控数据
            col.Place(0);
            ConsoleTheme::BoxBegin("veh_handling_now", 8, "当前操控数据", col.width);
            ConsoleTheme::TextRow2("质量", now01, true, "牵引曲线max", now09, true, true);
            ConsoleTheme::TextRow2("阻力系数", now02, true, "牵引曲线min", now10, true, true);
            ConsoleTheme::TextRow2("浮力", now03, true, "碰撞倍率", now11, true, true);
            ConsoleTheme::TextRow2("加速度", now04, true, "武器倍率", now12, true, true);
            ConsoleTheme::TextRow2("驱动惯性", now05, true, "变形倍率", now13, true, true);
            ConsoleTheme::TextRow2("初始驱动力", now06, true, "发动机", now14, true, true);
            ConsoleTheme::TextRow2("制动力", now07, true, "推力", now15, true, true);
            ConsoleTheme::TextRow2("手刹力", now08, true, nullptr, nullptr, true, false);
            ConsoleTheme::BoxEnd();
            col.Advance(0, ConsoleTheme::TitledBoxHeight(8));

            // 右列 · 修改操控数据（每行两组，可直接键入或按步长微调）
            col.Place(1);
            ConsoleTheme::BoxBegin("veh_handling_edit", 8, "修改操控数据（可直接键入）", col.width);
            ConsoleTheme::StepperRow2("veh_h1",
                                      "质量", &desiredMass, 50.0f, "%.0f",
                                      "阻力系数", &desiredDragCoefficient, 0.0001f, "%.6f");
            ConsoleTheme::StepperRow2("veh_h2",
                                      "浮力", &desiredBuoyancy, 0.1f, "%.6f",
                                      "加速度", &desiredAcceleration, 0.1f, "%.6f");
            ConsoleTheme::StepperRow2("veh_h3",
                                      "驱动惯性", &desiredDriveInertia, 0.1f, "%.6f",
                                      "初始驱动力", &desiredInitialDriveForce, 0.1f, "%.6f");
            ConsoleTheme::StepperRow2("veh_h4",
                                      "制动力", &desiredBrakeForce, 0.1f, "%.6f",
                                      "手刹力", &desiredHandbrakeForce, 0.1f, "%.6f");
            ConsoleTheme::StepperRow2("veh_h5",
                                      "牵引曲线max", &desiredTractionCurveMax, 0.1f, "%.6f",
                                      "牵引曲线min", &desiredTractionCurveMin, 0.1f, "%.6f");
            ConsoleTheme::StepperRow2("veh_h6",
                                      "碰撞倍率", &desiredCollisionMultiplier, 0.1f, "%.6f",
                                      "武器倍率", &desiredWeaponMultiplier, 0.1f, "%.6f");
            ConsoleTheme::StepperRow2("veh_h7",
                                      "变形倍率", &desiredDeformationMultiplier, 0.1f, "%.6f",
                                      "发动机", &desiredEngine, 0.1f, "%.6f");
            ConsoleTheme::StepperRow2("veh_h8",
                                      "推力", &desiredThrust, 0.1f, "%.6f",
                                      nullptr, nullptr, 0.0f, "%.6f", false);
            ConsoleTheme::BoxEnd();
            col.Advance(1, ConsoleTheme::TitledBoxHeight(8));

            col.End();

            // 写入：整行两张按钮，紧贴两列下方（不再沉到一屏之外）
            ConsoleTheme::BoxBeginPixels("veh_handling_ops", layout::box_height(0) + 45.0f * 2.0f, "写入", 0.0f);
            if (ConsoleTheme::ButtonRow("更新操控数据", UiIcon::Check, true)) {
                bNeedsOverwrite = true;
            }
            if (ConsoleTheme::ButtonRow("复制当前到目标", UiIcon::Folder)) {
                desiredAcceleration = currentAcceleration;
                desiredMass = currentMass;
                desiredDragCoefficient = currentDragCoefficient;
                desiredBuoyancy = currentBuoyancy;
                desiredDriveInertia = currentDriveInertia;
                desiredInitialDriveForce = currentInitialDriveForce;
                desiredBrakeForce = currentBrakeForce;
                desiredHandbrakeForce = currentHandbrakeForce;
                desiredTractionCurveMax = currentTractionCurveMax;
                desiredTractionCurveMin = currentTractionCurveMin;
                desiredCollisionMultiplier = currentCollisionMultiplier;
                desiredWeaponMultiplier = currentWeaponMultiplier;
                desiredDeformationMultiplier = currentDeformationMultiplier;
                desiredEngine = currentEngine;
                desiredThrust = currentThrust;
            }
            ConsoleTheme::BoxEnd();

            ImGui::EndTabItem();
        }

        
        ImGui::EndTabBar();
    }
    
    return true;
}

bool VehicleEditor::Render() {
    // 只有当启用时才渲染车辆编辑器窗口
    if (!bEnable)
        return false;

    // 设置窗口样式
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 10));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 3.0f);

    // 设置窗口标题颜色
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.16f, 0.29f, 0.48f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.20f, 0.35f, 0.60f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, ImVec4(0.12f, 0.20f, 0.35f, 1.0f));

    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    ImGui::Begin("载具编辑器", &bEnable, ImGuiWindowFlags_NoCollapse);

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(6);

    // 渲染载具编辑器内容
    RenderContent();

    ImGui::End();

    return true;
}

bool VehicleEditor::OnDMAFrame()

{

    // Handle auto repair first - 优先处理修复逻辑，不依赖于VehicleAddress

    if (bAutoRepair) {

        bool repairResult = RepairAll();
        // 可以在这里添加调试信息，查看修复结果

    }

    

    // Handle triggered repair (one-time repair) - 优先处理一键修复，不依赖于VehicleAddress

    if (bRepairTriggered) {

        bool repairResult = RepairAll();
        // 可以在这里添加调试信息，查看修复结果
        bRepairTriggered = false;

    }
    
    
    // Handle 18 vehicle repair - 单独处理18修复载具，不依赖于VehicleAddress
    
    if (bVehicleRepair18) {
        
        RepairVehiclePart18();
        // 持续执行18修复载具操作
        
    }

    // 其他功能依赖于VehicleAddress
    if (!DMA::VehicleAddress) {
        trackedVehicleAddress = 0;
        currentVehicleModelHash = 0;
        currentVehicleEntityModelHash = 0;
        currentVehicleState = 0;
        currentVehicleFreezeFlag = 0;
        currentVehicleWeaponAmmo = 0;
        bFreezeVehicle = false;
        return false;
    }

        

    // Update vehicle info
    UpdateVehicleInfo();

    if (bFreezeVehicle)
        SetVehicleFrozen(true);
    
    // Update missile lock on range and weapon range
    UpdateLockOnRange();
    UpdateWeaponRange();
    
    // Handle lock jump recovery speed - 先处理锁定逻辑，再更新显示值
    if (bLockJumpRecoverySpeed) {
        SetJumpRecoverySpeed(5.0f);  // 使用固定值5.0进行锁定
        currentJumpRecoverySpeed = 5.0f;  // 同步显示值为锁定值
    } else {
        // 只有在未锁定时才更新跳跃恢复速度
        UpdateJumpRecoverySpeed();
    }

    

    // Handle infinite jet - 先处理无限喷气逻辑，避免currentJetCharge被更新

    if (bInfiniteJet) {

        SetJetChargeValue(1.25f);  // 每一帧都设置为1.25以保持锁定

    }
    
    // Handle lock jet recovery speed - 处理锁定喷气恢复速度逻辑
    if (bLockJetRecoverySpeed) {
        SetJetRecoverySpeedValue(5.0f);  // 每一帧都设置为5.0以保持锁定
        currentJetRecoverySpeed = 5.0f;  // 同步显示值为锁定值
    }

    

    // Update jet charge value and jet recovery speed

    UpdateJetChargeValue();
    UpdateJetRecoverySpeed();
    
    // Handle parachute enabled - 先处理降落伞开关逻辑
    if (bParachuteEnabled) {
        SetParachute(true);  // 每一帧都设置为true以保持锁定
    }
    
    // Handle seat belt enabled - 处理安全带开关逻辑
    if (bSeatBeltEnabled) {
        SetSeatBelt(true);  // 每一帧都设置为true以保持锁定
    } else {
        SetSeatBelt(false);  // 每一帧都设置为false以保持锁定
    }

    // Update vehicle health info

    UpdateVehicleHealth();

    UpdateEngineHealth();

    UpdateVehicleMaxHealth();

    UpdateParachuteStatus();

    

    // Update new vehicle health info

    UpdateVehicleHealthAlt();

    UpdateBodyHealth();

    UpdateTankHealth();

        

    // Handle vehicle editor features

    if (bVehicleEditor) {

        ApplyVehicleMods();

    }

    

    // Handle vehicle addon auto apply
    if (bVehicleAddonApply) {
        int mode = vehicleAddonMode;
        SetVehicleAddon(mode);
    }
    
    // Handle missile mods auto apply
    if (bEnableMissileMods && bAutoApplyMissileMods) {
        SetLockOnRange(desiredLockOnRange);
        SetWeaponRange(desiredWeaponRange);
    }
    
    // Handle overwrite request
    if (bNeedsOverwrite) {
        ApplyVehicleMods();
        bNeedsOverwrite = false;
    }
    
    // Handle copy request
    if (bRequestedCopyToDesired) {
        desiredAcceleration = currentAcceleration;
        desiredMass = currentMass;
        bRequestedCopyToDesired = false;
    }
    

    
    return true;

}



bool VehicleEditor::ApplyVehicleMods()
{
    if (!DMA::VehicleAddress)
        return false;
        
    // Modify vehicle handling data
    uintptr_t handlingAddr = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, 
        DMA::VehicleAddress + offsetof(CVehicle, pCHandlingData), 
        (BYTE*)&handlingAddr, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
        
    if (BytesRead == sizeof(uintptr_t) && handlingAddr) {
        // Modify acceleration
        VMMDLL_MemWrite(DMA::vmh, DMA::PID, 
            handlingAddr + offsetof(CHandlingData, Acceleration), 
            (BYTE*)&desiredAcceleration, sizeof(float));
            
        // Modify mass
        VMMDLL_MemWrite(DMA::vmh, DMA::PID, 
            handlingAddr + offsetof(CHandlingData, Mass), 
            (BYTE*)&desiredMass, sizeof(float));
            
        // Modify drag coefficient
        VMMDLL_MemWrite(DMA::vmh, DMA::PID, 
            handlingAddr + 0x10, 
            (BYTE*)&desiredDragCoefficient, sizeof(float));
            
        // Modify buoyancy
        VMMDLL_MemWrite(DMA::vmh, DMA::PID, 
            handlingAddr + 0x40, 
            (BYTE*)&desiredBuoyancy, sizeof(float));
            
        // Modify drive inertia
        VMMDLL_MemWrite(DMA::vmh, DMA::PID, 
            handlingAddr + 0x54, 
            (BYTE*)&desiredDriveInertia, sizeof(float));
            
        // Modify initial drive force
        VMMDLL_MemWrite(DMA::vmh, DMA::PID, 
            handlingAddr + 0x60, 
            (BYTE*)&desiredInitialDriveForce, sizeof(float));
            
        // Modify brake force
        VMMDLL_MemWrite(DMA::vmh, DMA::PID, 
            handlingAddr + 0x6C, 
            (BYTE*)&desiredBrakeForce, sizeof(float));
            
        // Modify handbrake force
        VMMDLL_MemWrite(DMA::vmh, DMA::PID, 
            handlingAddr + 0x7C, 
            (BYTE*)&desiredHandbrakeForce, sizeof(float));
            
        // 直接从地址写入属性值
        uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
        if (baseAddr) {
            // 读取WorldPtr值
            uintptr_t addr1 = 0;
            VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
            if (BytesRead == sizeof(uintptr_t) && addr1) {
                // +8
                uintptr_t addr2 = 0;
                VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
                if (BytesRead == sizeof(uintptr_t) && addr2) {
                    // +0xD10
                    uintptr_t addr3 = 0;
                    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
                    if (BytesRead == sizeof(uintptr_t) && addr3) {
                        // +0x960
                        uintptr_t addr4 = 0;
                        VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x960, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
                        if (BytesRead == sizeof(uintptr_t) && addr4) {
                            // 写入牵引曲线max
                            VMMDLL_MemWrite(DMA::vmh, DMA::PID, addr4 + 0x88, (BYTE*)&desiredTractionCurveMax, sizeof(float));
                            // 写入牵引曲线min
                            VMMDLL_MemWrite(DMA::vmh, DMA::PID, addr4 + 0x90, (BYTE*)&desiredTractionCurveMin, sizeof(float));
                            // 写入碰撞Mult
                            VMMDLL_MemWrite(DMA::vmh, DMA::PID, addr4 + 0xF0, (BYTE*)&desiredCollisionMultiplier, sizeof(float));
                            // 写入武器Mult
                            VMMDLL_MemWrite(DMA::vmh, DMA::PID, addr4 + 0xF4, (BYTE*)&desiredWeaponMultiplier, sizeof(float));
                            // 写入变形Mult
                            VMMDLL_MemWrite(DMA::vmh, DMA::PID, addr4 + 0xF8, (BYTE*)&desiredDeformationMultiplier, sizeof(float));
                            // 写入发动机
                            VMMDLL_MemWrite(DMA::vmh, DMA::PID, addr4 + 0xFC, (BYTE*)&desiredEngine, sizeof(float));
                            // 写入推力
                            VMMDLL_MemWrite(DMA::vmh, DMA::PID, addr4 + 0x338, (BYTE*)&desiredThrust, sizeof(float));
                        }
                    }
                }
            }
        }

    }

    

    return true;

}



// 载具修复函数实现
bool VehicleEditor::RepairVehicle()

{

    if (!DMA::vmh || !DMA::PID)

        return false;



    // 修复载具血量:

    // "GTA5_Enhanced.exe"+043DBC98 -> 基址

    // 指针链: +8 -> +0xD10 -> +0x300

    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;

    if (!baseAddr) return false;

    

    // 读取基址值

    uintptr_t addr1 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;

    

    // +8

    uintptr_t addr2 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;

    

    // +0xD10

    uintptr_t addr3 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;

    

    // 最终地址: +0x300

    uintptr_t finalAddr = addr3 + 0x300;

    

    // 写入最大血量值 (1000.0f)

    float maxHealth = 1000.0f;

    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&maxHealth, sizeof(float));

    

    return true;

}



bool VehicleEditor::RepairEngine()

{

    if (!DMA::vmh || !DMA::PID)

        return false;



    // 修复引擎血量:

    // "GTA5_Enhanced.exe"+043DBC98 -> 基址

    // 指针链: +8 -> +0xD10 -> +0x910

    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;

    if (!baseAddr) return false;

    

    // 读取基址值

    uintptr_t addr1 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;

    

    // +8

    uintptr_t addr2 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;

    

    // +0xD10

    uintptr_t addr3 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;

    

    // 最终地址: +0x910

    uintptr_t finalAddr = addr3 + 0x910;

    

    // 写入最大血量值 (1000.0f)

    float maxHealth = 1000.0f;

    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&maxHealth, sizeof(float));

    

    return true;

}



bool VehicleEditor::RepairBody()

{

    if (!DMA::vmh || !DMA::PID)

        return false;



    // 修复车身健康:

    // "GTA5_Enhanced.exe"+043DBC98 -> 基址

    // 指针链: +8 -> +0xD10 -> +0x830

    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;

    if (!baseAddr) return false;

    

    // 读取基址值

    uintptr_t addr1 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;

    

    // +8

    uintptr_t addr2 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;

    

    // +0xD10

    uintptr_t addr3 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;

    

    // 最终地址: +0x830

    uintptr_t finalAddr = addr3 + 0x830;

    

    // 写入最大健康值 (1000.0f)

    float maxHealth = 1000.0f;

    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&maxHealth, sizeof(float));

    

    return true;

}



bool VehicleEditor::RepairTank()

{

    if (!DMA::vmh || !DMA::PID)

        return false;



    // 修复油箱健康:

    // "GTA5_Enhanced.exe"+043DBC98 -> 基址

    // 指针链: +8 -> +0xD10 -> +0x834

    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;

    if (!baseAddr) return false;

    

    // 读取基址值

    uintptr_t addr1 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;

    

    // +8

    uintptr_t addr2 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;

    

    // +0xD10

    uintptr_t addr3 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;

    

    // 最终地址: +0x834

    uintptr_t finalAddr = addr3 + 0x834;

    

    // 写入最大健康值 (1000.0f)

    float maxHealth = 1000.0f;

    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&maxHealth, sizeof(float));

    

    return true;

}



// 新增函数：修复车辆健康 (ID 200)

bool VehicleEditor::RepairVehicleHealthAlt()

{

    if (!DMA::vmh || !DMA::PID)

        return false;



    // 修复车辆健康:

    // "GTA5_Enhanced.exe"+043DBC98 -> 基址

    // 指针链: +8 -> +0xD10 -> +0x280

    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;

    if (!baseAddr) return false;

    

    // 读取基址值

    uintptr_t addr1 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;

    

    // +8

    uintptr_t addr2 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;

    

    // +0xD10

    uintptr_t addr3 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;

    

    // 最终地址: +0x280

    uintptr_t finalAddr = addr3 + 0x280;

    

    // 写入最大健康值 (1000.0f)

    float maxHealth = 1000.0f;

    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&maxHealth, sizeof(float));

    

    return true;

}


bool VehicleEditor::RepairVehiclePart18()

{

    if (!DMA::vmh || !DMA::PID)

        return false;



    // 载具外观修复:

    // Cheat Engine指针链: WorldPTR -> 0x8 -> 0xD10 -> 0x972
    // 处理顺序: WorldPTR -> 读取值 -> +0x8 -> 读取值 -> +0xD10 -> 读取值 -> +0x972
    // 与SetJetChargeValue和SetJumpRecoverySpeed保持一致的处理方式

    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;

    if (!baseAddr) return false;

    


    // 读取基址值

    uintptr_t addr1 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &VehicleEditor::BytesRead, VMMDLL_FLAG_NOCACHE);

    if (VehicleEditor::BytesRead != sizeof(uintptr_t) || !addr1) return false;

    


    // +0x8

    uintptr_t addr2 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &VehicleEditor::BytesRead, VMMDLL_FLAG_NOCACHE);

    if (VehicleEditor::BytesRead != sizeof(uintptr_t) || !addr2) return false;

    


    // +0xD10

    uintptr_t addr3 = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &VehicleEditor::BytesRead, VMMDLL_FLAG_NOCACHE);

    if (VehicleEditor::BytesRead != sizeof(uintptr_t) || !addr3) return false;

    


    // 最终地址: +0x972

    uintptr_t finalAddr = addr3 + 0x972;

    


    // 写入修复值 (18)

    BYTE repairValue = 18;

    BOOL writeResult = VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&repairValue, sizeof(BYTE));

    if (!writeResult) {
        return false;
    }

    


    return true;

}


bool VehicleEditor::RepairAll()

{

    // 修复所有载具相关部件

    bool result = true;

    result &= RepairVehicle();      // 修复载具血量 (0x834)

    result &= RepairEngine();       // 修复引擎血量 (0x910)

    result &= RepairBody();         // 修复车身健康 (0x830)

    result &= RepairTank();         // 修复油箱健康 (0x834)

    result &= RepairVehicleHealthAlt(); // 修复车辆健康 (0x280)

    result &= RepairVehiclePart18();   // 载具修复（18修复） (0x972 -> 0xD10 -> 0x8)

    return result;

}

bool VehicleEditor::UpdateJetChargeValue()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置读取喷气充能进度:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x300
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x300
    uintptr_t finalAddr = addr3 + 0x300;
    
    // 读取喷气充能值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentJetCharge = value;
    
    return true;
}

bool VehicleEditor::UpdateJetRecoverySpeed()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置读取喷气恢复速度:
    // "GTA5_Enhanced.exe"+44061E8 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x304
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x304
    uintptr_t finalAddr = addr3 + 0x304;
    
    // 读取喷气恢复速度值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentJetRecoverySpeed = value;
    
    return true;
}

bool VehicleEditor::SetJetRecoverySpeedValue(float value)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置设置喷气恢复速度:
    // "GTA5_Enhanced.exe"+44061E8 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x304
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x304
    uintptr_t finalAddr = addr3 + 0x304;
    
    // 写入喷气恢复速度值
    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float));
    
    return true;
}

bool VehicleEditor::SetJetChargeValue(float value)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置设置喷气充能进度:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x300
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x300
    uintptr_t finalAddr = addr3 + 0x300;
    
    // 写入喷气充能值
    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float));
    
    return true;
}

bool VehicleEditor::UpdateVehicleAddon()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 读取载具附加功能值:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x20 -> +0x58B
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // +0x20
    uintptr_t addr4 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x20, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr4) return false;
    
    // 最终地址: +0x58B
    uintptr_t finalAddr = addr4 + 0x58B;
    
    // 读取载具附加功能值
    uint8_t value = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(uint8_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uint8_t)) return false;
    
    vehicleAddonMode = static_cast<int>(value);
    
    return true;
}

bool VehicleEditor::SetVehicleAddon(int mode)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 设置载具附加功能值:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x20 -> +0x58B
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // +0x20
    uintptr_t addr4 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x20, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr4) return false;
    
    // 最终地址: +0x58B
    uintptr_t finalAddr = addr4 + 0x58B;
    
    // 写入载具附加功能值
    uint8_t byteMode = static_cast<uint8_t>(mode);
    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&byteMode, sizeof(uint8_t));
    
    return true;
}

bool VehicleEditor::SetParachute(bool enabled)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 设置载具降落伞:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x20 -> +0x58C
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // +0x20
    uintptr_t addr4 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x20, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr4) return false;
    
    // 最终地址: +0x58C
    uintptr_t finalAddr = addr4 + 0x58C;
    
    // 写入降落伞状态值
    uint8_t value = enabled ? 1 : 0;
    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(uint8_t));
    
    return true;
}

// 新增函数：读取载具血量
bool VehicleEditor::UpdateVehicleHealth()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 读取载具血量:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x834
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x834
    uintptr_t finalAddr = addr3 + 0x834;
    
    // 读取载具血量值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentVehicleHealth = value;
    return true;
}

// 新增函数：读取引擎血量
bool VehicleEditor::UpdateEngineHealth()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 读取引擎血量:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x910
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x910
    uintptr_t finalAddr = addr3 + 0x910;
    
    // 读取引擎血量值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentEngineHealth = value;
    return true;
}

// 新增函数：读取载具最大血量
bool VehicleEditor::UpdateVehicleMaxHealth()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 读取载具最大血量:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x284
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x284
    uintptr_t finalAddr = addr3 + 0x284;
    
    // 读取载具最大血量值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentVehicleMaxHealth = value;
    return true;
}

// 新增函数：读取降落伞状态
bool VehicleEditor::UpdateParachuteStatus()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 读取降落伞状态:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x20 -> +0x58C
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // +0x20
    uintptr_t addr4 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x20, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr4) return false;
    
    // 最终地址: +0x58C
    uintptr_t finalAddr = addr4 + 0x58C;
    
    // 读取降落伞状态值
    uint8_t value = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(uint8_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uint8_t)) return false;
    
    currentParachuteStatus = value;
    return true;
}

// 新增函数：读取车辆健康 (ID 200)
bool VehicleEditor::UpdateVehicleHealthAlt()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 读取车辆健康:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x280
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x280
    uintptr_t finalAddr = addr3 + 0x280;
    
    // 读取车辆健康值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentVehicleHealthAlt = value;
    return true;
}

// 新增函数：读取车身健康 (ID 201)
bool VehicleEditor::UpdateBodyHealth()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 读取车身健康:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x830
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x830
    uintptr_t finalAddr = addr3 + 0x830;
    
    // 读取车身健康值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentBodyHealth = value;
    return true;
}

// 新增函数：读取车辆油箱健康 (ID 198)
bool VehicleEditor::UpdateTankHealth()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 修复油箱健康:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x834
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // 最终地址: +0x834
    uintptr_t finalAddr = addr3 + 0x834;
    
    // 读取油箱健康值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentTankHealth = value;
    
    return true;
}

// 更新导弹锁定范围
bool VehicleEditor::UpdateLockOnRange()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置读取导弹锁定范围:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x20 -> +0x288
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // +0x20
    uintptr_t addr4 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x20, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr4) return false;
    
    // 最终地址: +0x288
    uintptr_t finalAddr = addr4 + 0x288;
    
    // 读取导弹锁定范围值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentLockOnRange = value;
    
    return true;
}

// 设置导弹锁定范围
bool VehicleEditor::SetLockOnRange(float value)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置设置导弹锁定范围:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x20 -> +0x288
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // +0x20
    uintptr_t addr4 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x20, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr4) return false;
    
    // 最终地址: +0x288
    uintptr_t finalAddr = addr4 + 0x288;
    
    // 写入导弹锁定范围值
    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float));
    
    return true;
}

// 更新导弹有效距离
bool VehicleEditor::UpdateWeaponRange()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置读取导弹有效距离:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x20 -> +0x28C
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // +0x20
    uintptr_t addr4 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x20, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr4) return false;
    
    // 最终地址: +0x28C
    uintptr_t finalAddr = addr4 + 0x28C;
    
    // 读取导弹有效距离值
    float value = 0.0f;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(float)) return false;
    
    currentWeaponRange = value;
    
    return true;
}

// 设置导弹有效距离
bool VehicleEditor::SetWeaponRange(float value)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 根据Cheat Engine配置设置导弹有效距离:
    // "GTA5_Enhanced.exe"+043DBC98 -> 基址
    // 指针链: +8 -> +0xD10 -> +0x20 -> +0x28C
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // +0xD10
    uintptr_t addr3 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr3) return false;
    
    // +0x20
    uintptr_t addr4 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x20, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr4) return false;
    
    // 最终地址: +0x28C
    uintptr_t finalAddr = addr4 + 0x28C;
    
    // 写入导弹有效距离值
    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(float));
    
    return true;
}

// 安全带相关函数
bool VehicleEditor::UpdateSeatBeltStatus()
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 读取安全带状态:
    // "GTA5_Enhanced.exe"+44061E8 -> 基址
    // 指针链: +8 -> +0x143c
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // 最终地址: +0x143c
    uintptr_t finalAddr = addr2 + 0x143c;
    
    // 读取安全带状态值
    uint8_t value = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(uint8_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uint8_t)) return false;
    
    return true;
}

bool VehicleEditor::SetSeatBelt(bool enabled)
{
    if (!DMA::vmh || !DMA::PID)
        return false;

    // 设置安全带状态:
    // "GTA5_Enhanced.exe"+44061E8 -> 基址
    // 指针链: +8 -> +0x143c
    uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
    if (!baseAddr) return false;
    
    // 读取基址值
    uintptr_t addr1 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr1) return false;
    
    // +8
    uintptr_t addr2 = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
    if (BytesRead != sizeof(uintptr_t) || !addr2) return false;
    
    // 最终地址: +0x143c
    uintptr_t finalAddr = addr2 + 0x143c;
    
    // 检测是否为摩托车
    bool isMotorcycle = false;
    if (DMA::VehicleAddress) {
        // 读取载具类型
        uint8_t vehicleClass = 0;
        // 假设vehicleClass字段在CVehicle结构中的偏移是0x14C（根据常见GTA5偏移）
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID, DMA::VehicleAddress + 0x14C, (BYTE*)&vehicleClass, sizeof(uint8_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
        // 摩托车的vehicleClass通常是4
        isMotorcycle = vehicleClass == 4;
    }
    
    // 根据载具类型设置不同的安全带值
    uint8_t value = 0;
    if (enabled) {
        value = isMotorcycle ? 0 : 201;
    } else {
        value = 200;
    }
    
    VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&value, sizeof(uint8_t));
    
    return true;
}


bool VehicleEditor::SetVehicleFrozen(bool enabled)
{
    if (!DMA::IsValidAddress(DMA::VehicleAddress))
        return false;

    const uint8_t value = enabled ? 1 : 0;
    if (!VMMDLL_MemWrite(DMA::vmh, DMA::PID,
        DMA::VehicleAddress + offsetof(CVehicle, FreezeFlag),
        (BYTE*)&value, sizeof(value)))
        return false;

    currentVehicleFreezeFlag = value;
    return true;
}

bool VehicleEditor::SetVehicleWeaponAmmo(uint8_t value)
{
    if (!DMA::IsValidAddress(DMA::VehicleAddress))
        return false;

    if (!VMMDLL_MemWrite(DMA::vmh, DMA::PID,
        DMA::VehicleAddress + offsetof(CVehicle, VehicleWeaponAmmo),
        (BYTE*)&value, sizeof(value)))
        return false;

    currentVehicleWeaponAmmo = value;
    desiredVehicleWeaponAmmo = value;
    return true;
}

bool VehicleEditor::UpdateVehicleInfo()
{
    if (!DMA::VehicleAddress)
        return false;

    const bool vehicleChanged = trackedVehicleAddress != DMA::VehicleAddress;
    DWORD diagnosticBytesRead = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
        DMA::VehicleAddress + offsetof(CVehicle, EntityModelHash),
        (BYTE*)&currentVehicleEntityModelHash, sizeof(currentVehicleEntityModelHash),
        &diagnosticBytesRead, VMMDLL_FLAG_NOCACHE);
    if (diagnosticBytesRead != sizeof(currentVehicleEntityModelHash))
        currentVehicleEntityModelHash = 0;

    uintptr_t modelInfoAddress = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
        DMA::VehicleAddress + offsetof(CVehicle, pCModelInfo),
        (BYTE*)&modelInfoAddress, sizeof(modelInfoAddress),
        &diagnosticBytesRead, VMMDLL_FLAG_NOCACHE);
    currentVehicleModelHash = 0;
    if (diagnosticBytesRead == sizeof(modelInfoAddress) && modelInfoAddress) {
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
            modelInfoAddress + offsetof(CModelInfo, ModelHash),
            (BYTE*)&currentVehicleModelHash, sizeof(currentVehicleModelHash),
            &diagnosticBytesRead, VMMDLL_FLAG_NOCACHE);
        if (diagnosticBytesRead != sizeof(currentVehicleModelHash))
            currentVehicleModelHash = 0;
    }

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
        DMA::VehicleAddress + offsetof(CVehicle, VehicleState),
        (BYTE*)&currentVehicleState, sizeof(currentVehicleState),
        &diagnosticBytesRead, VMMDLL_FLAG_NOCACHE);
    if (diagnosticBytesRead != sizeof(currentVehicleState))
        currentVehicleState = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
        DMA::VehicleAddress + offsetof(CVehicle, FreezeFlag),
        (BYTE*)&currentVehicleFreezeFlag, sizeof(currentVehicleFreezeFlag),
        &diagnosticBytesRead, VMMDLL_FLAG_NOCACHE);
    if (diagnosticBytesRead != sizeof(currentVehicleFreezeFlag))
        currentVehicleFreezeFlag = 0;

    VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
        DMA::VehicleAddress + offsetof(CVehicle, VehicleWeaponAmmo),
        (BYTE*)&currentVehicleWeaponAmmo, sizeof(currentVehicleWeaponAmmo),
        &diagnosticBytesRead, VMMDLL_FLAG_NOCACHE);
    if (diagnosticBytesRead != sizeof(currentVehicleWeaponAmmo))
        currentVehicleWeaponAmmo = 0;

    if (vehicleChanged) {
        trackedVehicleAddress = DMA::VehicleAddress;
        bFreezeVehicle = currentVehicleFreezeFlag != 0;
        desiredVehicleWeaponAmmo = currentVehicleWeaponAmmo;
    }
        
    // Read current vehicle handling data
    uintptr_t handlingAddr = 0;
    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, 
        DMA::VehicleAddress + offsetof(CVehicle, pCHandlingData), 
        (BYTE*)&handlingAddr, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
        
    if (BytesRead == sizeof(uintptr_t) && handlingAddr) {
        // Read acceleration
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
            handlingAddr + offsetof(CHandlingData, Acceleration),
            (BYTE*)&currentAcceleration, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
            
        // Read mass
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
            handlingAddr + offsetof(CHandlingData, Mass),
            (BYTE*)&currentMass, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
            
        // Read drag coefficient
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
            handlingAddr + 0x10,
            (BYTE*)&currentDragCoefficient, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
            
        // Read buoyancy
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
            handlingAddr + 0x40,
            (BYTE*)&currentBuoyancy, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
            
        // Read drive inertia
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
            handlingAddr + 0x54,
            (BYTE*)&currentDriveInertia, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
            
        // Read initial drive force
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
            handlingAddr + 0x60,
            (BYTE*)&currentInitialDriveForce, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
            
        // Read brake force
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
            handlingAddr + 0x6C,
            (BYTE*)&currentBrakeForce, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
            
        // Read handbrake force
        VMMDLL_MemReadEx(DMA::vmh, DMA::PID,
            handlingAddr + 0x7C,
            (BYTE*)&currentHandbrakeForce, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
            
        // 直接从地址读取属性值
        uintptr_t baseAddr = DMA::BaseAddress + Offsets::WorldPtr;
        if (baseAddr) {
            // 读取WorldPtr值
            uintptr_t addr1 = 0;
            VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
            if (BytesRead == sizeof(uintptr_t) && addr1) {
                // +8
                uintptr_t addr2 = 0;
                VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x8, (BYTE*)&addr2, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
                if (BytesRead == sizeof(uintptr_t) && addr2) {
                    // +0xD10
                    uintptr_t addr3 = 0;
                    VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr2 + 0xD10, (BYTE*)&addr3, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
                    if (BytesRead == sizeof(uintptr_t) && addr3) {
                        // +0x960
                        uintptr_t addr4 = 0;
                        VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr3 + 0x960, (BYTE*)&addr4, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);
                        if (BytesRead == sizeof(uintptr_t) && addr4) {
                            // 读取牵引曲线max
                            VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr4 + 0x88, (BYTE*)&currentTractionCurveMax, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
                            // 读取牵引曲线min
                            VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr4 + 0x90, (BYTE*)&currentTractionCurveMin, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
                            // 读取碰撞Mult
                            VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr4 + 0xF0, (BYTE*)&currentCollisionMultiplier, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
                            // 读取武器Mult
                            VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr4 + 0xF4, (BYTE*)&currentWeaponMultiplier, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
                            // 读取变形Mult
                            VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr4 + 0xF8, (BYTE*)&currentDeformationMultiplier, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
                            // 读取发动机
                            VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr4 + 0xFC, (BYTE*)&currentEngine, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
                            // 读取推力
                            VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr4 + 0x338, (BYTE*)&currentThrust, sizeof(float), &BytesRead, VMMDLL_FLAG_NOCACHE);
                        }
                    }
                }
            }
        }
    }
    
    return true;
}
