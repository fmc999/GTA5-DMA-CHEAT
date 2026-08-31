// ============================================================================
// LegacyPages.cpp — 已停用功能的页面 UI（保留源码，便于后续恢复）
//
// 这些页面对应的 DMA 循环入口已在 Core/DMA.cpp 中注释停用：
//   - 时间控制（TimeControl）
//   - 抢劫任务分红（HeistDividend）
// 恢复步骤：在 UI/ConsoleShell.cpp 的 RenderNavigation()/RenderPage() 中
// 重新启用对应导航项与路由，并在 Core/DMA.cpp 恢复 OnDMAFrame 调用。
// ============================================================================

#include "pch.h"

#include "TimeControl.h"
#include "HeistDividend.h"
#include "MenuManager.h"
#include "AppRuntime.h"

#include <string>
#include <vector>


namespace LegacyUI
{

static void RenderPageTitle__(const std::string& title)
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
    ImGui::Text("%s", title.c_str());
    ImGui::PopStyleColor();
}

static void RenderBackButton__()
{
    ImGui::Separator();
    
    // 返回按钮
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.60f, 0.20f, 0.20f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.30f, 0.30f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.10f, 0.10f, 1.0f));
    
    if (ImGui::Button("返回", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        MenuManager::GetInstance().GoBack();
    }
    
    ImGui::PopStyleColor(3);
}


// ---------- 时间控制页面 ----------

static void RenderTimePageContent()
{
    // 标题
    RenderPageTitle__("时间控制");
    
    ImGui::Separator();
    
    // 时间控制
    ImGui::Checkbox("启用时间控制", &TimeControl::bEnableUI);
    if (TimeControl::bEnableUI) {
        ImGui::Indent();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "时间设置");
        ImGui::SliderInt("天", &TimeControl::dayUI, 0, 30);
        ImGui::SliderInt("时", &TimeControl::hourUI, 0, 23);
        ImGui::SliderInt("分", &TimeControl::minuteUI, 0, 59);
        ImGui::SliderInt("秒", &TimeControl::secondUI, 0, 59);
        ImGui::Spacing();
        ImGui::Unindent();
    }
}

static void RenderTimePage()
{
    /* legacy window helper removed */
    ImGui::Begin("时间控制", nullptr, ImGuiWindowFlags_NoCollapse);
    RenderTimePageContent();
    RenderBackButton__();
    ImGui::End();
}


// ---------- 抢劫任务分红页面 ----------

static void RenderHeistDividendPageContent()
{
    // 标题
    RenderPageTitle__("抢劫任务分红");
    
    ImGui::Separator();
    
    // 启用开关
    if (ImGui::Checkbox("启用抢劫分红修改##heist_enable", &HeistDividend::bEnableUI)) {
        // 状态改变时同步到原子变量
        HeistDividend::bEnable.store(HeistDividend::bEnableUI);
    }
    
    // 分红设置区域
    if (HeistDividend::bEnableUI) {
        // 实时读取并显示分红值
        static std::vector<int> realtimeDividends(8, 0);
        static double lastUpdateTime = 0.0;
        double currentTime = ImGui::GetTime();
        
        // 每0.5秒更新一次实时分红值
        if (currentTime - lastUpdateTime > 0.5f) {
            lastUpdateTime = currentTime;
            
            // 读取当前分红值
            uintptr_t baseAddr = DMA::BaseAddress + 0x03F8E970;
            if (baseAddr) {
                DWORD BytesRead = 0;
                uintptr_t addr1 = 0;
                if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&addr1, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(uintptr_t) && addr1) {
                    // 赌场豪劫分红
                    int value = 0;
                    if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x112828, (BYTE*)&value, sizeof(int), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(int)) {
                        realtimeDividends[0] = value;
                    }
                    if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x112830, (BYTE*)&value, sizeof(int), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(int)) {
                        realtimeDividends[1] = value;
                    }
                    if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x112838, (BYTE*)&value, sizeof(int), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(int)) {
                        realtimeDividends[2] = value;
                    }
                    if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x112840, (BYTE*)&value, sizeof(int), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(int)) {
                        realtimeDividends[3] = value;
                    }
                    
                    // 佩里科岛分红
                    if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x11CFD8, (BYTE*)&value, sizeof(int), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(int)) {
                        realtimeDividends[4] = value;
                    }
                    if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x11CFE0, (BYTE*)&value, sizeof(int), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(int)) {
                        realtimeDividends[5] = value;
                    }
                    if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x11CFE8, (BYTE*)&value, sizeof(int), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(int)) {
                        realtimeDividends[6] = value;
                    }
                    if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, addr1 + 0x11CFF0, (BYTE*)&value, sizeof(int), &BytesRead, VMMDLL_FLAG_NOCACHE) && BytesRead == sizeof(int)) {
                        realtimeDividends[7] = value;
                    }
                }
            }
        }
        
        // 左右布局
        ImGui::Columns(2, "main_layout", false);
        
        // 左侧：预设按钮（自适应宽度）
        float availableWidth = ImGui::GetContentRegionAvail().x;
        // 增加左侧列的最小宽度，确保按钮文字不会被截断
        float leftColumnWidth = std::max(450.0f, availableWidth * 0.7f);
        ImGui::SetColumnWidth(0, leftColumnWidth);
        
        // 按钮区域
        ImGui::Spacing();
        
        // 全员85按钮
        if (ImGui::Button("赌场全员85##set_all_85", ImVec2(150, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置赌场豪劫分红为85%
            HeistDividend::casinoDividend1PUI = 85;
            HeistDividend::casinoDividend2PUI = 85;
            HeistDividend::casinoDividend3PUI = 85;
            HeistDividend::casinoDividend4PUI = 85;
            
            HeistDividend::casinoDividend1P.store(85);
            HeistDividend::casinoDividend2P.store(85);
            HeistDividend::casinoDividend3P.store(85);
            HeistDividend::casinoDividend4P.store(85);
            
            // 调用UpdateDividends写入游戏内存，与手动输入框逻辑一致
            HeistDividend::UpdateDividends();
        }
        
        ImGui::SameLine();
        
        // 佩里科岛猎豹全员135按钮
        if (ImGui::Button("🐆 佩里科岛猎豹全员135##set_cayo_leopard_135", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置佩里科岛分红为135%
            HeistDividend::cayoDividend1PUI = 135;
            HeistDividend::cayoDividend2PUI = 135;
            HeistDividend::cayoDividend3PUI = 135;
            HeistDividend::cayoDividend4PUI = 135;
            
            HeistDividend::cayoDividend1P.store(135);
            HeistDividend::cayoDividend2P.store(135);
            HeistDividend::cayoDividend3P.store(135);
            HeistDividend::cayoDividend4P.store(135);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        // 新行
        ImGui::Spacing();
        
        // 佩里科岛粉钻全员198按钮
        if (ImGui::Button("💎 佩里科岛粉钻全员198##set_cayo_diamond_198", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置佩里科岛分红为198%
            HeistDividend::cayoDividend1PUI = 198;
            HeistDividend::cayoDividend2PUI = 198;
            HeistDividend::cayoDividend3PUI = 198;
            HeistDividend::cayoDividend4PUI = 198;
            
            HeistDividend::cayoDividend1P.store(198);
            HeistDividend::cayoDividend2P.store(198);
            HeistDividend::cayoDividend3P.store(198);
            HeistDividend::cayoDividend4P.store(198);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        // 新行
        ImGui::Spacing();
        
        // 赌场豪劫预设分红按钮
        if (ImGui::Button("🎰 赌场豪劫现金全员75##set_casino_cash_75", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置赌场豪劫分红为75%
            HeistDividend::casinoDividend1PUI = 75;
            HeistDividend::casinoDividend2PUI = 75;
            HeistDividend::casinoDividend3PUI = 75;
            HeistDividend::casinoDividend4PUI = 75;
            
            HeistDividend::casinoDividend1P.store(75);
            HeistDividend::casinoDividend2P.store(75);
            HeistDividend::casinoDividend3P.store(75);
            HeistDividend::casinoDividend4P.store(75);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        ImGui::SameLine();
        
        // 赌场豪劫预设分红按钮
        if (ImGui::Button("🎰 赌场豪劫黄金全员90##set_casino_gold_90", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置赌场豪劫分红为90%
            HeistDividend::casinoDividend1PUI = 90;
            HeistDividend::casinoDividend2PUI = 90;
            HeistDividend::casinoDividend3PUI = 90;
            HeistDividend::casinoDividend4PUI = 90;
            
            HeistDividend::casinoDividend1P.store(90);
            HeistDividend::casinoDividend2P.store(90);
            HeistDividend::casinoDividend3P.store(90);
            HeistDividend::casinoDividend4P.store(90);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        // 新行
        ImGui::Spacing();
        
        // 佩里科岛预设分红按钮
        if (ImGui::Button("🏝️ 佩里科岛现金全员110##set_cayo_cash_110", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置佩里科岛分红为110%
            HeistDividend::cayoDividend1PUI = 110;
            HeistDividend::cayoDividend2PUI = 110;
            HeistDividend::cayoDividend3PUI = 110;
            HeistDividend::cayoDividend4PUI = 110;
            
            HeistDividend::cayoDividend1P.store(110);
            HeistDividend::cayoDividend2P.store(110);
            HeistDividend::cayoDividend3P.store(110);
            HeistDividend::cayoDividend4P.store(110);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        ImGui::SameLine();
        
        // 佩里科岛预设分红按钮
        if (ImGui::Button("🏝️ 佩里科岛艺术品全员150##set_cayo_art_150", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置佩里科岛分红为150%
            HeistDividend::cayoDividend1PUI = 150;
            HeistDividend::cayoDividend2PUI = 150;
            HeistDividend::cayoDividend3PUI = 150;
            HeistDividend::cayoDividend4PUI = 150;
            
            HeistDividend::cayoDividend1P.store(150);
            HeistDividend::cayoDividend2P.store(150);
            HeistDividend::cayoDividend3P.store(150);
            HeistDividend::cayoDividend4P.store(150);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        // 新行
        ImGui::Spacing();
        
        // 赌场豪劫更多预设
        if (ImGui::Button("🎰 赌场豪劫现金全员95##set_casino_cash_95", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置赌场豪劫分红为95%
            HeistDividend::casinoDividend1PUI = 95;
            HeistDividend::casinoDividend2PUI = 95;
            HeistDividend::casinoDividend3PUI = 95;
            HeistDividend::casinoDividend4PUI = 95;
            
            HeistDividend::casinoDividend1P.store(95);
            HeistDividend::casinoDividend2P.store(95);
            HeistDividend::casinoDividend3P.store(95);
            HeistDividend::casinoDividend4P.store(95);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        ImGui::SameLine();
        
        // 赌场豪劫更多预设
        if (ImGui::Button("🎰 赌场豪劫黄金全员80##set_casino_gold_80", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置赌场豪劫分红为80%
            HeistDividend::casinoDividend1PUI = 80;
            HeistDividend::casinoDividend2PUI = 80;
            HeistDividend::casinoDividend3PUI = 80;
            HeistDividend::casinoDividend4PUI = 80;
            
            HeistDividend::casinoDividend1P.store(80);
            HeistDividend::casinoDividend2P.store(80);
            HeistDividend::casinoDividend3P.store(80);
            HeistDividend::casinoDividend4P.store(80);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        // 新行
        ImGui::Spacing();
        
        // 佩里科岛更多预设
        if (ImGui::Button("🏝️ 佩里科岛现金全员120##set_cayo_cash_120", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置佩里科岛分红为120%
            HeistDividend::cayoDividend1PUI = 120;
            HeistDividend::cayoDividend2PUI = 120;
            HeistDividend::cayoDividend3PUI = 120;
            HeistDividend::cayoDividend4PUI = 120;
            
            HeistDividend::cayoDividend1P.store(120);
            HeistDividend::cayoDividend2P.store(120);
            HeistDividend::cayoDividend3P.store(120);
            HeistDividend::cayoDividend4P.store(120);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        ImGui::SameLine();
        
        // 佩里科岛更多预设
        if (ImGui::Button("🏝️ 佩里科岛艺术品全员180##set_cayo_art_180", ImVec2(180, 30))) {
            // 确保功能已启用
            HeistDividend::bEnableUI = true;
            HeistDividend::bEnable.store(true);
            
            // 设置佩里科岛分红为180%
            HeistDividend::cayoDividend1PUI = 180;
            HeistDividend::cayoDividend2PUI = 180;
            HeistDividend::cayoDividend3PUI = 180;
            HeistDividend::cayoDividend4PUI = 180;
            
            HeistDividend::cayoDividend1P.store(180);
            HeistDividend::cayoDividend2P.store(180);
            HeistDividend::cayoDividend3P.store(180);
            HeistDividend::cayoDividend4P.store(180);
            
            // 调用UpdateDividends写入游戏内存
            HeistDividend::UpdateDividends();
        }
        
        // 右侧：实时分红值显示（移动到预设按钮旁边）
        ImGui::NextColumn();
        
        // 实时分红值显示
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "实时分红值：");
        ImGui::Spacing();
        
        // 为实时分红值区域添加更多空间
        ImGui::Spacing();
        
        // 赌场豪劫
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "🎰 赌场豪劫：");
        ImGui::Text("1P: %d%%", realtimeDividends[0]);
        ImGui::Text("2P: %d%%", realtimeDividends[1]);
        ImGui::Text("3P: %d%%", realtimeDividends[2]);
        ImGui::Text("4P: %d%%", realtimeDividends[3]);
        
        // 增加间距
        ImGui::Spacing();
        ImGui::Spacing();
        
        // 佩里科岛
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "🏝️ 佩里科岛：");
        ImGui::Text("1P: %d%%", realtimeDividends[4]);
        ImGui::Text("2P: %d%%", realtimeDividends[5]);
        ImGui::Text("3P: %d%%", realtimeDividends[6]);
        ImGui::Text("4P: %d%%", realtimeDividends[7]);
        
        // 为后期扩展预留空间
        ImGui::Spacing();
        ImGui::Spacing();
        
        // 结束列布局
        ImGui::Columns(1);
        
        // 增加更大的间距
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Spacing();
        
        // 分红操作
        ImGui::TextColored(ImVec4(0.26f, 0.59f, 0.98f, 1.0f), "分红操作：");
        ImGui::Spacing();
        
        // 复制实时值到输入框按钮（自适应宽度）
        if (ImGui::Button("复制实时值到输入框", ImVec2(-1, 30))) {
            // 复制实时分红值到所有玩家的输入框
            // 赌场豪劫分红
            HeistDividend::casinoDividend1PUI = realtimeDividends[0];
            HeistDividend::casinoDividend2PUI = realtimeDividends[1];
            HeistDividend::casinoDividend3PUI = realtimeDividends[2];
            HeistDividend::casinoDividend4PUI = realtimeDividends[3];
            
            // 佩里科岛分红
            HeistDividend::cayoDividend1PUI = realtimeDividends[4];
            HeistDividend::cayoDividend2PUI = realtimeDividends[5];
            HeistDividend::cayoDividend3PUI = realtimeDividends[6];
            HeistDividend::cayoDividend4PUI = realtimeDividends[7];
            
            // 同步到原子变量
            HeistDividend::casinoDividend1P.store(realtimeDividends[0]);
            HeistDividend::casinoDividend2P.store(realtimeDividends[1]);
            HeistDividend::casinoDividend3P.store(realtimeDividends[2]);
            HeistDividend::casinoDividend4P.store(realtimeDividends[3]);
            HeistDividend::cayoDividend1P.store(realtimeDividends[4]);
            HeistDividend::cayoDividend2P.store(realtimeDividends[5]);
            HeistDividend::cayoDividend3P.store(realtimeDividends[6]);
            HeistDividend::cayoDividend4P.store(realtimeDividends[7]);
            
            std::println("已复制实时分红值到所有输入框");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // 左右布局 - 赌场豪劫和佩里科岛分红并列
        ImGui::Columns(2, "dividend_layout", false);
        
        // 左侧：赌场豪劫分红（包含所有4个玩家）
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "🎰 赌场豪劫分红设置：");
        ImGui::Spacing();
        
        // 赌场1P - 当前值、输入框和应用按钮
        ImGui::Text("赌场1P当前分红: %d%%", HeistDividend::casinoDividend1PUI);
        int newCasinoDividend1P = HeistDividend::casinoDividend1PUI;
        if (ImGui::InputInt("##casino_p1_input", &newCasinoDividend1P, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
            // 限制输入范围 - 允许超过100
            newCasinoDividend1P = std::max(0, newCasinoDividend1P);
            HeistDividend::casinoDividend1PUI = newCasinoDividend1P;
            HeistDividend::casinoDividend1P.store(newCasinoDividend1P);
        }
        if (ImGui::Button("应用##casino_p1_apply", ImVec2(-1, 25))) {
            // 写入赌场1P分红值到游戏内存
            HeistDividend::WriteCasinoDividend(1, newCasinoDividend1P);
            std::println("已应用赌场1P分红值: {}%", newCasinoDividend1P);
        }
        
        ImGui::Spacing();
        
        // 赌场2P - 当前值、输入框和应用按钮
        ImGui::Text("赌场2P当前分红: %d%%", HeistDividend::casinoDividend2PUI);
        int newCasinoDividend2P = HeistDividend::casinoDividend2PUI;
        if (ImGui::InputInt("##casino_p2_input", &newCasinoDividend2P, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
            // 限制输入范围 - 允许超过100
            newCasinoDividend2P = std::max(0, newCasinoDividend2P);
            HeistDividend::casinoDividend2PUI = newCasinoDividend2P;
            HeistDividend::casinoDividend2P.store(newCasinoDividend2P);
        }
        if (ImGui::Button("应用##casino_p2_apply", ImVec2(-1, 25))) {
            // 写入赌场2P分红值到游戏内存
            HeistDividend::WriteCasinoDividend(2, newCasinoDividend2P);
            std::println("已应用赌场2P分红值: {}%", newCasinoDividend2P);
        }
        
        ImGui::Spacing();
        
        // 赌场3P - 当前值、输入框和应用按钮
        ImGui::Text("赌场3P当前分红: %d%%", HeistDividend::casinoDividend3PUI);
        int newCasinoDividend3P = HeistDividend::casinoDividend3PUI;
        if (ImGui::InputInt("##casino_p3_input", &newCasinoDividend3P, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
            // 限制输入范围 - 允许超过100
            newCasinoDividend3P = std::max(0, newCasinoDividend3P);
            HeistDividend::casinoDividend3PUI = newCasinoDividend3P;
            HeistDividend::casinoDividend3P.store(newCasinoDividend3P);
        }
        if (ImGui::Button("应用##casino_p3_apply", ImVec2(-1, 25))) {
            // 写入赌场3P分红值到游戏内存
            HeistDividend::WriteCasinoDividend(3, newCasinoDividend3P);
            std::println("已应用赌场3P分红值: {}%", newCasinoDividend3P);
        }
        
        ImGui::Spacing();
        
        // 赌场4P - 当前值、输入框和应用按钮
        ImGui::Text("赌场4P当前分红: %d%%", HeistDividend::casinoDividend4PUI);
        int newCasinoDividend4P = HeistDividend::casinoDividend4PUI;
        if (ImGui::InputInt("##casino_p4_input", &newCasinoDividend4P, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
            // 限制输入范围 - 允许超过100
            newCasinoDividend4P = std::max(0, newCasinoDividend4P);
            HeistDividend::casinoDividend4PUI = newCasinoDividend4P;
            HeistDividend::casinoDividend4P.store(newCasinoDividend4P);
        }
        if (ImGui::Button("应用##casino_p4_apply", ImVec2(-1, 25))) {
            // 写入赌场4P分红值到游戏内存
            HeistDividend::WriteCasinoDividend(4, newCasinoDividend4P);
            std::println("已应用赌场4P分红值: {}%", newCasinoDividend4P);
        }
        
        ImGui::Spacing();
        
        // 计算赌场豪劫分红总和 (不再强制要求)
        int casinoTotal = HeistDividend::casinoDividend1PUI + HeistDividend::casinoDividend2PUI + 
                         HeistDividend::casinoDividend3PUI + HeistDividend::casinoDividend4PUI;
        
        if (casinoTotal == 100) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "赌场豪劫分红总和: %d%% (正确)##casino_total", casinoTotal);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "赌场豪劫分红总和: %d%% (注意: 不为100%%)##casino_total", casinoTotal);
            if (casinoTotal > 100) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "⚠️ 警告: 超出100%%是黑钱##casino_warning");
            }
        }
        
        // 右侧：佩里科岛分红（包含所有4个玩家）
        ImGui::NextColumn();
        
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "🏝️ 佩里科岛分红设置：");
        ImGui::Spacing();
        
        // 佩岛1P - 当前值、输入框和应用按钮
        ImGui::Text("佩岛1P当前分红: %d%%", HeistDividend::cayoDividend1PUI);
        int newCayoDividend1P = HeistDividend::cayoDividend1PUI;
        if (ImGui::InputInt("##cayo_p1_input", &newCayoDividend1P, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
            // 限制输入范围 - 允许超过100
            newCayoDividend1P = std::max(0, newCayoDividend1P);
            HeistDividend::cayoDividend1PUI = newCayoDividend1P;
            HeistDividend::cayoDividend1P.store(newCayoDividend1P);
        }
        if (ImGui::Button("应用##cayo_p1_apply", ImVec2(-1, 25))) {
            // 写入佩岛1P分红值到游戏内存
            HeistDividend::WriteCayoDividend(1, newCayoDividend1P);
            std::println("已应用佩岛1P分红值: {}%", newCayoDividend1P);
        }
        
        ImGui::Spacing();
        
        // 佩岛2P - 当前值、输入框和应用按钮
        ImGui::Text("佩岛2P当前分红: %d%%", HeistDividend::cayoDividend2PUI);
        int newCayoDividend2P = HeistDividend::cayoDividend2PUI;
        if (ImGui::InputInt("##cayo_p2_input", &newCayoDividend2P, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
            // 限制输入范围 - 允许超过100
            newCayoDividend2P = std::max(0, newCayoDividend2P);
            HeistDividend::cayoDividend2PUI = newCayoDividend2P;
            HeistDividend::cayoDividend2P.store(newCayoDividend2P);
        }
        if (ImGui::Button("应用##cayo_p2_apply", ImVec2(-1, 25))) {
            // 写入佩岛2P分红值到游戏内存
            HeistDividend::WriteCayoDividend(2, newCayoDividend2P);
            std::println("已应用佩岛2P分红值: {}%", newCayoDividend2P);
        }
        
        ImGui::Spacing();
        
        // 佩岛3P - 当前值、输入框和应用按钮
        ImGui::Text("佩岛3P当前分红: %d%%", HeistDividend::cayoDividend3PUI);
        int newCayoDividend3P = HeistDividend::cayoDividend3PUI;
        if (ImGui::InputInt("##cayo_p3_input", &newCayoDividend3P, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
            // 限制输入范围 - 允许超过100
            newCayoDividend3P = std::max(0, newCayoDividend3P);
            HeistDividend::cayoDividend3PUI = newCayoDividend3P;
            HeistDividend::cayoDividend3P.store(newCayoDividend3P);
        }
        if (ImGui::Button("应用##cayo_p3_apply", ImVec2(-1, 25))) {
            // 写入佩岛3P分红值到游戏内存
            HeistDividend::WriteCayoDividend(3, newCayoDividend3P);
            std::println("已应用佩岛3P分红值: {}%", newCayoDividend3P);
        }
        
        ImGui::Spacing();
        
        // 佩岛4P - 当前值、输入框和应用按钮
        ImGui::Text("佩岛4P当前分红: %d%%", HeistDividend::cayoDividend4PUI);
        int newCayoDividend4P = HeistDividend::cayoDividend4PUI;
        if (ImGui::InputInt("##cayo_p4_input", &newCayoDividend4P, 0, 0, ImGuiInputTextFlags_CharsDecimal)) {
            // 限制输入范围 - 允许超过100
            newCayoDividend4P = std::max(0, newCayoDividend4P);
            HeistDividend::cayoDividend4PUI = newCayoDividend4P;
            HeistDividend::cayoDividend4P.store(newCayoDividend4P);
        }
        if (ImGui::Button("应用##cayo_p4_apply", ImVec2(-1, 25))) {
            // 写入佩岛4P分红值到游戏内存
            HeistDividend::WriteCayoDividend(4, newCayoDividend4P);
            std::println("已应用佩岛4P分红值: {}%", newCayoDividend4P);
        }
        
        ImGui::Spacing();
        
        // 计算佩里科岛分红总和 (不再强制要求)
        int cayoTotal = HeistDividend::cayoDividend1PUI + HeistDividend::cayoDividend2PUI + 
                       HeistDividend::cayoDividend3PUI + HeistDividend::cayoDividend4PUI;
        
        if (cayoTotal == 100) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "佩里科岛分红总和: %d%% (正确)##cayo_total", cayoTotal);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "佩里科岛分红总和: %d%% (注意: 不为100%%)##cayo_total", cayoTotal);
            if (cayoTotal > 100) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "⚠️ 警告: 超出100%%是黑钱##cayo_warning");
            }
        }
        
        // 结束列布局
        ImGui::Columns(1);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    } else {
        // 功能未启用时的提示
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "请启用抢劫分红修改功能以使用分红设置##disabled_note");
    }
}

static void RenderHeistDividendPage()
{
    // 设置窗口大小
    /* legacy window helper removed */
    // 开始窗口
    if (!ImGui::Begin("抢劫任务分红", nullptr, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    
    RenderHeistDividendPageContent();
    
    // 返回按钮
    RenderBackButton__();
    
    // 结束窗口
    ImGui::End();
}





} // namespace LegacyUI
