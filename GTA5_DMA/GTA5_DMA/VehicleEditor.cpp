#include "pch.h"
#include "ConsoleTheme.h"
#include "VehicleEditor.h"
#include "DMA.h"
#include "Reclass.h"
#include "MyMenu.h"
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
        if (ImGui::BeginTabItem("载具信息")) {
            // 当前载具信息
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("当前载具属性");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.80f, 0.80f, 1.0f));
            
            // 使用两列布局显示属性
            ImGui::Columns(2, NULL, false);
            ImGui::Text("加速度: %.2f", currentAcceleration);
            ImGui::Text("质量: %.2f", currentMass);
            ImGui::Text("载具血量: %.2f / %.2f", currentVehicleHealth, currentVehicleMaxHealth);
            ImGui::Text("引擎血量: %.2f", currentEngineHealth);
            ImGui::NextColumn();
            ImGui::Text("车辆健康: %.2f", currentVehicleHealthAlt);
            ImGui::Text("车身健康: %.2f", currentBodyHealth);
            ImGui::Text("油箱健康: %.2f", currentTankHealth);
            ImGui::Text("降落伞状态: %s", currentParachuteStatus ? "开启" : "关闭");
            
            // 显示当前功能模式
            int currentAddonMode = vehicleAddonMode;
            const char* modeText = "未知";
            switch(currentAddonMode) {
                case 0: modeText = "默认"; break;
                case 40: modeText = "跳跃"; break;
                case 66: modeText = "加速"; break;
                case 96: modeText = "跳跃+加速"; break;
            }
            ImGui::Text("功能模式: %s", modeText);
            
            ImGui::Columns(1);
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Separator();
            
            // 覆盖数值
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("修改载具属性");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::InputFloat("加速度##acceleration", &desiredAcceleration, 0.1f, 1.0f, "%.2f");
            ImGui::InputFloat("质量##mass", &desiredMass, 50.0f, 500.0f, "%.0f");
            
            // 按钮行
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::Button("更新##update_values", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 0))) {
                bNeedsOverwrite = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("复制当前到目标##copy_current", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
                bRequestedCopyToDesired = true;
            }
            ImGui::PopStyleVar();
            
            ImGui::EndTabItem();
        }
        
        // 飞行与跳跃标签页
        if (ImGui::BeginTabItem("飞行与跳跃")) {
            // 喷气功能
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("喷气功能");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::Text("喷气充能进度: %.2f", currentJetCharge);
            bool infiniteJet = bInfiniteJet;
            if (ImGui::Checkbox("锁定无限喷气##infinite_jet", &infiniteJet)) {
                bInfiniteJet = infiniteJet;
                if (infiniteJet) {
                    SetJetChargeValue(1.25f);
                }
            }
            
            ImGui::Text("喷气恢复速度: %.2f", currentJetRecoverySpeed);
            bool lockJetRecoverySpeed = bLockJetRecoverySpeed;
            if (ImGui::Checkbox("锁定喷气恢复速度##lock_jet_recovery", &lockJetRecoverySpeed)) {
                bLockJetRecoverySpeed = lockJetRecoverySpeed;
                if (lockJetRecoverySpeed) {
                    SetJetRecoverySpeedValue(5.0f);
                }
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // 跳跃恢复
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("跳跃恢复");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::Text("当前恢复速度: %.2f##current_jump_recovery", currentJumpRecoverySpeed);
            bool lockJumpRecoverySpeed = bLockJumpRecoverySpeed;
            if (ImGui::Checkbox("锁定恢复速度##lock_jump_recovery", &lockJumpRecoverySpeed)) {
                bLockJumpRecoverySpeed = lockJumpRecoverySpeed;
            }
            
            ImGui::EndTabItem();
        }
        
        // 附加功能标签页
        if (ImGui::BeginTabItem("附加功能")) {
            // 载具附加功能模式选择
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("载具附加功能");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            int addonMode = vehicleAddonMode;
            int selectedMode = 0;
            if (addonMode == 0) selectedMode = 0;
            else if (addonMode == 40) selectedMode = 1;
            else if (addonMode == 66) selectedMode = 2;
            else if (addonMode == 96) selectedMode = 3;
            
            const char* addonItems[] = { "默认(0)", "跳跃(40)", "加速(66)", "二者都(96)" };
            if (ImGui::Combo("附加功能模式", &selectedMode, addonItems, 4)) {
                int actualValue = 0;
                switch(selectedMode) {
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
            
            bool autoApply = bVehicleAddonApply;
            if (ImGui::Checkbox("立即生效##auto_apply_addon", &autoApply)) {
                bVehicleAddonApply = autoApply;
                if (autoApply) {
                    SetVehicleAddon(addonMode);
                }
            }
            
            if (ImGui::Button("点击应用##apply_addon")) {
                SetVehicleAddon(addonMode);
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // 降落伞
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("降落伞设置");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            bool parachuteEnabled = bParachuteEnabled;
            if (ImGui::Checkbox("降落伞开关##parachute_toggle", &parachuteEnabled)) {
                bParachuteEnabled = parachuteEnabled;
                SetParachute(parachuteEnabled);
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // 安全带
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("安全带设置");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            int seatBeltMode = bSeatBeltEnabled ? 1 : 0;
            const char* seatBeltOptions[] = { "关闭", "开启" };
            if (ImGui::Combo("安全带模式##seat_belt_combo", &seatBeltMode, seatBeltOptions, IM_ARRAYSIZE(seatBeltOptions))) {
                bSeatBeltEnabled = seatBeltMode == 1;
                SetSeatBelt(bSeatBeltEnabled);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.60f, 0.90f));
            ImGui::TextWrapped("开启后将根据载具类型设置安全带值：普通载具201，摩托车0");
            ImGui::PopStyleColor();
            
            ImGui::EndTabItem();
        }
        
        // 修复功能标签页
        if (ImGui::BeginTabItem("修复功能")) {
            // 载具修复
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("载具修复选项");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::Button("一键修复##repair_all", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 0))) {
                bRepairTriggered = true;
            }
            ImGui::SameLine();
            bool autoRepair = bAutoRepair;
            if (ImGui::Checkbox("自动修复##auto_repair", &autoRepair)) {
                bAutoRepair = autoRepair;
            }
            ImGui::PopStyleVar();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // 载具外观修复
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("外观修复");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            bool vehicleRepair18 = bVehicleRepair18;
            if (ImGui::Checkbox("载具外观修复##vehicle_repair_18", &vehicleRepair18)) {
                bVehicleRepair18 = vehicleRepair18;
            }
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.60f, 1.0f));
            ImGui::TextWrapped("启用后将持续执行载具外观修复操作");
            ImGui::PopStyleColor();
            
            ImGui::EndTabItem();
        }
        
        // 导弹功能标签页
        if (ImGui::BeginTabItem("导弹功能")) {
            // 导弹功能
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("导弹属性设置");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            // 显示当前导弹锁定范围和有效距离
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.80f, 0.80f, 1.0f));
            ImGui::Text("当前导弹属性");
            ImGui::Text("锁定范围: %.2f##current_lock_range", currentLockOnRange);
            ImGui::Text("有效距离: %.2f##current_weapon_range", currentWeaponRange);
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            
            // 导弹功能启用开关
            bool enableMissileMods = bEnableMissileMods;
            if (ImGui::Checkbox("启用导弹功能##enable_missile_mods", &enableMissileMods)) {
                bEnableMissileMods = enableMissileMods;
            }
            
            // 当导弹功能启用时，显示输入框和应用选项
            if (bEnableMissileMods) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
                ImGui::Text("修改导弹属性");
                ImGui::PopStyleColor();
                ImGui::Spacing();
                
                ImGui::InputFloat("锁定范围##input_lock_range", &desiredLockOnRange, 100.0f, 500.0f, "%.0f");
                ImGui::InputFloat("有效距离##input_weapon_range", &desiredWeaponRange, 100.0f, 500.0f, "%.0f");
                
                // 自动应用开关
                bool autoApplyMissileMods = bAutoApplyMissileMods;
                if (ImGui::Checkbox("立即生效##auto_apply_missile", &autoApplyMissileMods)) {
                    bAutoApplyMissileMods = autoApplyMissileMods;
                    if (autoApplyMissileMods) {
                        SetLockOnRange(desiredLockOnRange);
                        SetWeaponRange(desiredWeaponRange);
                    }
                }
                
                // 手动应用按钮
                if (ImGui::Button("应用导弹设置##apply_missile_settings")) {
                    SetLockOnRange(desiredLockOnRange);
                    SetWeaponRange(desiredWeaponRange);
                }
            }
            
            ImGui::EndTabItem();
        }
        
        // 操控数据标签页
        if (ImGui::BeginTabItem("操控数据")) {
            // 当前操控数据属性
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("当前操控数据属性");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.80f, 0.80f, 1.0f));
            
            // 使用两列布局显示属性
            ImGui::Columns(2, NULL, false);
            ImGui::Text("质量: %.6f", currentMass);
            ImGui::Text("阻力系数: %.6f", currentDragCoefficient);
            ImGui::Text("浮力: %.6f", currentBuoyancy);
            ImGui::Text("加速度: %.6f", currentAcceleration);
            ImGui::Text("驱动惯性: %.6f", currentDriveInertia);
            ImGui::Text("初始驱动力: %.6f", currentInitialDriveForce);
            ImGui::Text("制动力: %.6f", currentBrakeForce);
            ImGui::Text("手刹力: %.6f", currentHandbrakeForce);
            ImGui::NextColumn();
            ImGui::Text("牵引曲线max: %.6f", currentTractionCurveMax);
            ImGui::Text("牵引曲线min: %.6f", currentTractionCurveMin);
            ImGui::Text("碰撞Multi: %.6f", currentCollisionMultiplier);
            ImGui::Text("武器Multi: %.6f", currentWeaponMultiplier);
            ImGui::Text("变形Multi: %.6f", currentDeformationMultiplier);
            ImGui::Text("发动机: %.6f", currentEngine);
            ImGui::Text("推力: %.6f", currentThrust);
            ImGui::Columns(1);
            
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            // 修改操控数据属性
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
            ImGui::Text("修改操控数据属性");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            
            // 使用两列布局显示输入框和按钮
            ImGui::Columns(2, NULL, false);
            
            // 第一列
            ImGui::PushItemWidth(ImGui::GetColumnWidth() * 0.6f);
            
            // 质量
            ImGui::InputFloat("质量##mass", &desiredMass, 0.0f, 0.0f, "%.0f");
            ImGui::SameLine();
            if (ImGui::Button("-##mass_minus", ImVec2(24, 20))) { desiredMass -= 50.0f; }
            ImGui::SameLine();
            if (ImGui::Button("+##mass_plus", ImVec2(24, 20))) { desiredMass += 50.0f; }
            
            // 阻力系数
            ImGui::InputFloat("阻力系数##drag_coefficient", &desiredDragCoefficient, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##drag_minus", ImVec2(24, 20))) { desiredDragCoefficient -= 0.0001f; }
            ImGui::SameLine();
            if (ImGui::Button("+##drag_plus", ImVec2(24, 20))) { desiredDragCoefficient += 0.0001f; }
            
            // 浮力
            ImGui::InputFloat("浮力##buoyancy", &desiredBuoyancy, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##buoyancy_minus", ImVec2(24, 20))) { desiredBuoyancy -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##buoyancy_plus", ImVec2(24, 20))) { desiredBuoyancy += 0.1f; }
            
            // 加速度
            ImGui::InputFloat("加速度##acceleration", &desiredAcceleration, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##acceleration_minus", ImVec2(24, 20))) { desiredAcceleration -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##acceleration_plus", ImVec2(24, 20))) { desiredAcceleration += 0.1f; }
            
            // 驱动惯性
            ImGui::InputFloat("驱动惯性##drive_inertia", &desiredDriveInertia, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##inertia_minus", ImVec2(24, 20))) { desiredDriveInertia -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##inertia_plus", ImVec2(24, 20))) { desiredDriveInertia += 0.1f; }
            
            // 初始驱动力
            ImGui::InputFloat("初始驱动力##initial_drive_force", &desiredInitialDriveForce, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##drive_force_minus", ImVec2(24, 20))) { desiredInitialDriveForce -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##drive_force_plus", ImVec2(24, 20))) { desiredInitialDriveForce += 0.1f; }
            
            // 制动力
            ImGui::InputFloat("制动力##brake_force", &desiredBrakeForce, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##brake_minus", ImVec2(24, 20))) { desiredBrakeForce -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##brake_plus", ImVec2(24, 20))) { desiredBrakeForce += 0.1f; }
            
            // 手刹力
            ImGui::InputFloat("手刹力##handbrake_force", &desiredHandbrakeForce, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##handbrake_minus", ImVec2(24, 20))) { desiredHandbrakeForce -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##handbrake_plus", ImVec2(24, 20))) { desiredHandbrakeForce += 0.1f; }
            
            ImGui::NextColumn();
            
            // 第二列
            ImGui::PushItemWidth(ImGui::GetColumnWidth() * 0.6f);
            
            // 牵引曲线max
            ImGui::InputFloat("牵引曲线max##traction_curve_max", &desiredTractionCurveMax, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##traction_max_minus", ImVec2(24, 20))) { desiredTractionCurveMax -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##traction_max_plus", ImVec2(24, 20))) { desiredTractionCurveMax += 0.1f; }
            
            // 牵引曲线min
            ImGui::InputFloat("牵引曲线min##traction_curve_min", &desiredTractionCurveMin, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##traction_min_minus", ImVec2(24, 20))) { desiredTractionCurveMin -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##traction_min_plus", ImVec2(24, 20))) { desiredTractionCurveMin += 0.1f; }
            
            // 碰撞Multi
            ImGui::InputFloat("碰撞Multi##collision_multiplier", &desiredCollisionMultiplier, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##collision_minus", ImVec2(24, 20))) { desiredCollisionMultiplier -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##collision_plus", ImVec2(24, 20))) { desiredCollisionMultiplier += 0.1f; }
            
            // 武器Multi
            ImGui::InputFloat("武器Multi##weapon_multiplier", &desiredWeaponMultiplier, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##weapon_minus", ImVec2(24, 20))) { desiredWeaponMultiplier -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##weapon_plus", ImVec2(24, 20))) { desiredWeaponMultiplier += 0.1f; }
            
            // 变形Multi
            ImGui::InputFloat("变形Multi##deformation_multiplier", &desiredDeformationMultiplier, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##deformation_minus", ImVec2(24, 20))) { desiredDeformationMultiplier -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##deformation_plus", ImVec2(24, 20))) { desiredDeformationMultiplier += 0.1f; }
            
            // 发动机
            ImGui::InputFloat("发动机##engine", &desiredEngine, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##engine_minus", ImVec2(24, 20))) { desiredEngine -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##engine_plus", ImVec2(24, 20))) { desiredEngine += 0.1f; }
            
            // 推力
            ImGui::InputFloat("推力##thrust", &desiredThrust, 0.0f, 0.0f, "%.6f");
            ImGui::SameLine();
            if (ImGui::Button("-##thrust_minus", ImVec2(24, 20))) { desiredThrust -= 0.1f; }
            ImGui::SameLine();
            if (ImGui::Button("+##thrust_plus", ImVec2(24, 20))) { desiredThrust += 0.1f; }
            
            ImGui::Columns(1);
            
            // 按钮行
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
            if (ImGui::Button("更新操控数据##update_handling_data", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 0))) {
                bNeedsOverwrite = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("复制当前到目标##copy_current_handling", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
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
            ImGui::PopStyleVar();
            
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
    if (!DMA::VehicleAddress)

        return false;

        

    // Update vehicle info
    UpdateVehicleInfo();
    
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


bool VehicleEditor::UpdateVehicleInfo()
{
    if (!DMA::VehicleAddress)
        return false;
        
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
