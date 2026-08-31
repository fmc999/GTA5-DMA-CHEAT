#include "pch.h"

#include "Dev.h"

bool Dev::Render()
{
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
	ImGui::Begin("开发工具面板", nullptr, ImGuiWindowFlags_NoCollapse);

	ImGui::SeparatorText("全局变量查询");
	
	ImGui::InputInt("全局变量索引", &Dev::DesiredGlobalIndex);

	if (ImGui::Button("获取全局浮点数"))
	{
		float Output = 0.0f;
		DMA::GetGlobalValue<float>(static_cast<DWORD>(DesiredGlobalIndex), Output);
		std::println("全局变量({0:d}) {1:f}", DesiredGlobalIndex, Output);
	}	

	if (ImGui::Button("获取全局DWORD"))
	{
		DWORD Output = 0;
		DMA::GetGlobalValue<DWORD>(static_cast<DWORD>(DesiredGlobalIndex), Output);
		std::println("全局变量({0:d}) {0:d}", DesiredGlobalIndex, Output);
	}

	ImGui::SeparatorText("菜单控制");
	if (ImGui::Button(Dev::bShowAllMenus ? "隐藏主菜单" : "显示主菜单"))
	{
		Dev::bShowAllMenus = !Dev::bShowAllMenus;
	}
	
	ImGui::SeparatorText("请提前关闭BE反作弊(主菜单选择已开启BE进入游戏无效)");
	if (ImGui::Button("关闭(伪装)BE反作弊"))
	{
		// 写入未开启BE的值
		uint64_t value = 0x480100000A92B980;
		uintptr_t address = DMA::BaseAddress + 0x5390319; // 使用基地址+偏移量
		VMMDLL_MemWrite(DMA::vmh, DMA::PID, address, (BYTE*)&value, sizeof(value));
		std::println("已关闭(伪装)BE反作弊");
	}
	
	if (ImGui::Button("开启(伪装)BE反作弊"))
	{
		// 写入已开启(伪装)BE的值
		uint64_t value = 0x48C35E5F38C48348;
		uintptr_t address = DMA::BaseAddress + 0x5390319; // 使用基地址+偏移量
		VMMDLL_MemWrite(DMA::vmh, DMA::PID, address, (BYTE*)&value, sizeof(value));
		std::println("已开启(伪装)BE");
	}
	
	ImGui::End();

	return 1;
}