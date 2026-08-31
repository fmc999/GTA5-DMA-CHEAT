#include "pch.h"
#include "PlayerChaserMenu.h"
#include "PlayerChaser.h"
#include "Dev.h"
#include <iostream>
#include <sstream>
#include <iomanip>

// Keep track of menu visibility
static bool bChaserMenuVisible = false;

bool PlayerChaserMenu::Render()
{
	// Show/hide chaser menu based on UI enable flag - direct check for immediate response
	if (PlayerChaser::bEnableUI) {
		bChaserMenuVisible = true;
	} else {
		bChaserMenuVisible = false;
	}

	// Only render when visible
	if (bChaserMenuVisible && Dev::bShowAllMenus) {
		ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
		ImGui::Begin("追战局功能", nullptr, ImGuiWindowFlags_NoCollapse);

		// Add warning text in yellow
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "此功能只是一个测试版本");
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "需要输入一个工具人的RID(帮会好友都行)");
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "搜索RID并替换RID即可");
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "之后加入帮会/好友战局即可追到人");
		
		// Input fields for toolman RID and target RID
		ImGui::Text("工具人RID:");
		ImGui::InputText("##ToolmanRid", PlayerChaser::toolmanRid, sizeof(PlayerChaser::toolmanRid));
		
		ImGui::Text("目标RID:");
		ImGui::InputText("##TargetRid", PlayerChaser::targetId, sizeof(PlayerChaser::targetId));
		
		// Search button
		if (ImGui::Button("搜索")) {
			if (strlen(PlayerChaser::toolmanRid) > 0) {
				PlayerChaser::bSearchRequested = true;
			}
		}
		
		ImGui::SameLine();
		
		// Next scan value input
		ImGui::Text("下次扫描值:");
		ImGui::InputText("##NextScanValue", PlayerChaser::nextScanValue, sizeof(PlayerChaser::nextScanValue));
		
		// Next scan button
		if (ImGui::Button("下次扫描")) {
			PlayerChaser::bNextScanRequested = true;
		}
		
		// Action buttons
		if (!PlayerChaser::searchResults.empty()) {
			ImGui::PushItemWidth(120);
			
			// Select all button
			if (ImGui::Button("全选")) {
				PlayerChaser::bSelectAllRequested = true;
			}
			
			ImGui::SameLine();
			
			// Deselect all button
			if (ImGui::Button("取消全选")) {
				PlayerChaser::bDeselectAllRequested = true;
			}
			
			ImGui::SameLine();
			
			// Replace selected button
			if (ImGui::Button("修改选中项")) {
				if (strlen(PlayerChaser::targetId) > 0) {
					PlayerChaser::bReplaceAllRequested = true;
				}
			}
			
			ImGui::SameLine();
			
			// Replace all button
			if (ImGui::Button("修改所有")) {
				if (strlen(PlayerChaser::targetId) > 0) {
					// Select all first
					PlayerChaser::SelectAllResults();
					PlayerChaser::bReplaceAllRequested = true;
				}
			}
			
			ImGui::PopItemWidth();
		} else {
			ImGui::PushItemWidth(120);
			ImGui::BeginDisabled();
			ImGui::Button("全选");
			ImGui::SameLine();
			ImGui::Button("取消全选");
			ImGui::SameLine();
			ImGui::Button("修改选中项");
			ImGui::SameLine();
			ImGui::Button("修改所有");
			ImGui::EndDisabled();
			ImGui::PopItemWidth();
		}
		
		// Display search results
		if (!PlayerChaser::searchResults.empty()) {
			ImGui::SeparatorText("搜索结果");
			
			// Show result count
			ImGui::Text("共找到 %d 个结果", (int)PlayerChaser::searchResults.size());
			
			// Result list with better scrolling and visual effects
			ImGui::BeginChild("ResultsList", ImVec2(0, 250), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
			
			// Setup table for better result display
			ImGui::Columns(3, "resultTable", false);
			ImGui::SetColumnWidth(0, 250); // Address column
			ImGui::SetColumnWidth(1, 100); // Value column
			ImGui::SetColumnWidth(2, 100); // Select column
			
			// Table headers
			ImGui::Text("地址");
			ImGui::NextColumn();
			ImGui::Text("值");
			ImGui::NextColumn();
			ImGui::Text("选择");
			ImGui::NextColumn();
			ImGui::Separator();
			
			for (size_t i = 0; i < PlayerChaser::searchResults.size(); i++) {
				auto& result = PlayerChaser::searchResults[i];
				
				// Address column with highlighting for changed values
				if (result.changed) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
				}
				ImGui::Text("0x%llx", result.address);
				if (result.changed) {
					ImGui::PopStyleColor();
				}
				ImGui::NextColumn();
				
				// Value column
				ImGui::Text("%u", result.value);
				ImGui::NextColumn();
				
				// Selection checkbox
				ImGui::Checkbox("", &result.selected);
				ImGui::NextColumn();
			}
			
			ImGui::Columns(1); // Reset columns
			ImGui::EndChild();
			
			// Selected count with visual feedback
			int selectedCount = 0;
			for (const auto& result : PlayerChaser::searchResults) {
				if (result.selected) {
					selectedCount++;
				}
			}
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "已选择 %d 个结果", selectedCount);
		} else if (PlayerChaser::searchInProgress) {
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "搜索中...");
		} else {
			ImGui::Text("暂无搜索结果");
		}
		
		// Status message
		if (!PlayerChaser::statusMessage.empty()) {
			ImGui::TextColored(PlayerChaser::statusColor, PlayerChaser::statusMessage.c_str());
		}
		
		// Show search progress
		if (PlayerChaser::searchInProgress) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "搜索中...");
		}

		ImGui::End();
	}

	return true;
}