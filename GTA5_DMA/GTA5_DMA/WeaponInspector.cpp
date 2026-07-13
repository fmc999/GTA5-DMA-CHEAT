#include "pch.h"

#include "ConsoleTheme.h"
#include "WeaponInspector.h"

#include "Offsets.h"

bool WeaponInspector::RenderContent() {
    ConsoleTheme::SectionHeader("武器工作区", "当前武器属性与写入参数");

	// 创建两列布局
	ImGui::BeginTable("weapon_table", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_Borders);

	// 左侧列：当前武器信息和属性修改
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	// 当前武器信息
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
	ImGui::SeparatorText("当前武器信息");
	ImGui::PopStyleColor();

	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.80f, 0.80f, 1.0f));
	ImGui::Text("伤害: %.2f", WepInfo.WeaponDamage);
	ImGui::Text("射速: %.2f", WepInfo.WeaponFireRate);
	ImGui::Text("射程: %.2f", WepInfo.WeaponRange);
	ImGui::Text("穿透: %.2f", WepInfo.WeaponPenetration);
	ImGui::Text("后坐力: %.2f", WepInfo.RecoilAmplitude);
	ImGui::Text("冲击类型: %d", WepInfo.ImpactType);
	ImGui::Text("冲击爆炸: %d", WepInfo.ImpactExplosion);
	
	// 读取并显示子弹飞行速度
	float bulletSpeed = 0.0f;
	bool bulletSpeedValid = ReadBulletSpeed(bulletSpeed);
	
	if (bulletSpeedValid) {
		ImGui::Text("子弹飞行速度: %.2f", bulletSpeed);
	} else {
		ImGui::Text("子弹飞行速度: 读取失败");
	}
	
	// 读取并显示冲击力相关数值
	float objectImpactForce = 0.0f;
	float pedImpactForce = 0.0f;
	float vehicleImpactForce = 0.0f;
	float aircraftImpactForce = 0.0f;
	
	bool objectImpactValid = ReadImpactForce(0xD8, objectImpactForce);
	bool pedImpactValid = ReadImpactForce(0xDC, pedImpactForce);
	bool vehicleImpactValid = ReadImpactForce(0xE0, vehicleImpactForce);
	bool aircraftImpactValid = ReadImpactForce(0xE4, aircraftImpactForce);
	
	if (objectImpactValid) {
		ImGui::Text("武器命中普通物体冲击力: %.2f", objectImpactForce);
	} else {
		ImGui::Text("武器命中普通物体冲击力: 读取失败");
	}
	
	if (pedImpactValid) {
		ImGui::Text("武器对行人冲击力: %.2f", pedImpactForce);
	} else {
		ImGui::Text("武器对行人冲击力: 读取失败");
	}
	
	if (vehicleImpactValid) {
		ImGui::Text("武器对载具冲击力: %.2f", vehicleImpactForce);
	} else {
		ImGui::Text("武器对载具冲击力: 读取失败");
	}
	
	if (aircraftImpactValid) {
		ImGui::Text("武器对飞行目标冲击力: %.2f", aircraftImpactForce);
	} else {
		ImGui::Text("武器对飞行目标冲击力: 读取失败");
	}
	
	ImGui::PopStyleColor();

	ImGui::Spacing();
	ImGui::Spacing();

	// 武器属性修改
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
	ImGui::SeparatorText("覆盖数值");
	ImGui::PopStyleColor();

	// 覆盖数值部分，使用双列布局
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.13f, 0.14f, 0.17f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.22f, 0.27f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.16f, 0.18f, 0.22f, 1.0f));

	// 设置双列布局
	ImGui::BeginTable("weapon_properties_table", 2, ImGuiTableFlags_Resizable);

	// 武器属性输入框 - 双列布局
	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::InputFloat("武器伤害", &DesiredWepInfo.WeaponDamage, 0.0f, 0.0f, "%.3f");
	ImGui::TableNextColumn();
	ImGui::InputFloat("武器射速", &DesiredWepInfo.WeaponFireRate, 0.0f, 0.0f, "%.3f");

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::InputFloat("武器射程", &DesiredWepInfo.WeaponRange, 0.0f, 0.0f, "%.3f");
	ImGui::TableNextColumn();
	ImGui::InputFloat("武器穿透", &DesiredWepInfo.WeaponPenetration, 0.0f, 0.0f, "%.3f");

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::InputFloat("后坐力幅度", &DesiredWepInfo.RecoilAmplitude, 0.0f, 0.0f, "%.3f");
	ImGui::TableNextColumn();
	ImGui::InputInt("冲击类型", (int*)&DesiredWepInfo.ImpactType);

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::InputInt("冲击爆炸", (int*)&DesiredWepInfo.ImpactExplosion);
	ImGui::TableNextColumn();
	// 占位，保持布局平衡
	ImGui::Dummy(ImVec2(0.0f, 0.0f));

	ImGui::EndTable();

	ImGui::PopStyleColor(3);
	
	// 武器属性的应用和复制按钮
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.40f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.60f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.80f));

	if (ImGui::Button("更新"))
		bNeedsOverwrite = true;

	ImGui::SameLine();

	if (ImGui::Button("复制当前到目标"))
		bRequestedCopyToDesired = true;

	ImGui::PopStyleColor(3);
	
	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();
	
	// 冲击力设置部分
	ImGui::Text("让他们飞起来冲击力设置");
	ImGui::Spacing();
	
	// 冲击力输入框
	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.13f, 0.14f, 0.17f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.22f, 0.27f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.16f, 0.18f, 0.22f, 1.0f));

	// 使用双列布局显示冲击力输入框
	ImGui::BeginTable("impact_forces_table", 2, ImGuiTableFlags_Resizable);

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::InputFloat("普通物体", &DesiredObjectImpactForce, 0.0f, 0.0f, "%.3f");
	ImGui::TableNextColumn();
	ImGui::InputFloat("行人", &DesiredPedImpactForce, 0.0f, 0.0f, "%.3f");

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	ImGui::InputFloat("陆地载具", &DesiredVehicleImpactForce, 0.0f, 0.0f, "%.3f");
	ImGui::TableNextColumn();
	ImGui::InputFloat("飞行载具", &DesiredAircraftImpactForce, 0.0f, 0.0f, "%.3f");

	ImGui::EndTable();

	ImGui::PopStyleColor(3);
	
	// 冲击力设置的应用和复制按钮
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.40f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.60f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.80f));

	// 冲击力设置的更新按钮 - 单次更新
	if (ImGui::Button("更新冲击力")) {
		// 单次应用冲击力修改
		WriteImpactForce(0xD8, DesiredObjectImpactForce); // 武器命中普通物体冲击力
		WriteImpactForce(0xDC, DesiredPedImpactForce); // 武器对行人冲击力
		WriteImpactForce(0xE0, DesiredVehicleImpactForce); // 武器对载具冲击力
		WriteImpactForce(0xE4, DesiredAircraftImpactForce); // 武器对飞行目标冲击力
	}

	ImGui::SameLine();

	// 冲击力设置的复制按钮
	if (ImGui::Button("复制当前冲击力到目标")) {
		// 读取当前冲击力数值
		float objectImpactForce = 0.0f;
		float pedImpactForce = 0.0f;
		float vehicleImpactForce = 0.0f;
		float aircraftImpactForce = 0.0f;
		
		// 读取各种冲击力数值
		ReadImpactForce(0xD8, objectImpactForce);
		ReadImpactForce(0xDC, pedImpactForce);
		ReadImpactForce(0xE0, vehicleImpactForce);
		ReadImpactForce(0xE4, aircraftImpactForce);
		
		// 赋值给目标变量
		DesiredObjectImpactForce = objectImpactForce;
		DesiredPedImpactForce = pedImpactForce;
		DesiredVehicleImpactForce = vehicleImpactForce;
		DesiredAircraftImpactForce = aircraftImpactForce;
	}

	ImGui::PopStyleColor(3);
	
	// 冲击力修改复选框 - 直接应用数值
	ImGui::Checkbox("直接应用数值", &bApplyImpactForces);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("勾选后将持续应用设置的冲击力数值");
		ImGui::EndTooltip();
	}

	// 右侧列：武器功能选项
	ImGui::TableNextColumn();

	// 武器功能选项
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
	ImGui::SeparatorText("武器功能选项");
	ImGui::PopStyleColor();

	// 复选框样式
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
	ImGui::Checkbox("无限弹药", &bInfiniteAmmo);
	ImGui::Checkbox("无需装弹", &bNoReload);
	
	// 禁用其他人武器复选框
	ImGui::Checkbox("禁用其他人武器", &bDisableOthersWeapons);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("使用说明：");
		ImGui::BulletText("瞄准敌人生效");
		ImGui::BulletText("缺点：对自己也生效");
		ImGui::BulletText("取消需要重新瞄准敌方");
		ImGui::EndTooltip();
	}
	
	// 其他复选框
	ImGui::Checkbox("瞄准敌人修改血量为-1", &bSetAimTargetHealthToMinusOne);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("将瞄准的敌人血量修改为-1，使其立即死亡");
		ImGui::EndTooltip();
	}
	
	// 瞄准敌人修改防弹衣复选框
	ImGui::Checkbox("瞄准敌人修改防弹衣为-1", &bSetAimTargetArmorToMinusOne);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("将瞄准的敌人防弹衣修改为-1");
		ImGui::EndTooltip();
	}
	
	ImGui::Checkbox("修改其他人移动速度为10", &bModifyOthersMoveSpeed);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("将瞄准的敌人移动速度修改为10");
		ImGui::EndTooltip();
	}
	
	// 百万瞬击复选框
	ImGui::Checkbox("百万瞬击", &bMillionInstantHit);
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("子弹飞行速度设置为99999999");
		ImGui::EndTooltip();
	}
	
	ImGui::PopStyleColor();

	ImGui::Separator();

	// 一键魔改部分
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
	ImGui::SeparatorText("一键教训智障儿童（天机炮版本半无敌也死)");
	ImGui::PopStyleColor();

	// 一键魔改按钮
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.95f, 0.45f, 0.15f, 0.40f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.95f, 0.45f, 0.15f, 0.60f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.95f, 0.45f, 0.15f, 0.80f));

	if (ImGui::Button("一键魔改")) {
		// Set the desired weapon properties for one-click modification
		DesiredWepInfo.WeaponDamage = 99999.0f;
		DesiredWepInfo.WeaponFireRate = 0.0f;
		DesiredWepInfo.WeaponRange = 99999.0f;
		DesiredWepInfo.WeaponPenetration = 99999.0f;
		DesiredWepInfo.RecoilAmplitude = 0.0f;
		DesiredWepInfo.ImpactType = IT_EXPLOSION; // 5
		DesiredWepInfo.ImpactExplosion = IE_ORBITAL_CANNON; // 59 轨道炮
		
		// If the checkbox is checked, apply immediately
		if (bApplyOneClickMod) {
			bNeedsOverwrite = true;
		}
	}

	ImGui::PopStyleColor(3);
	
	ImGui::SameLine();
	
	// 复选框样式
	ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
	ImGui::Checkbox("勾选直接应用", &bApplyOneClickMod);
	ImGui::PopStyleColor();
	
	ImGui::SameLine();
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::Text("勾选后点击一键魔改按钮将直接应用修改到当前武器");
		ImGui::EndTooltip();
	}

	// 结束表格布局
	ImGui::EndTable();

	return 1;
}

bool WeaponInspector::Render() {
	// 只有当启用时才渲染武器检查器窗口
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

	ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
	ImGui::Begin("武器检查器", &bEnable, ImGuiWindowFlags_NoCollapse);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(6);

	// 渲染武器检查器内容
	RenderContent();

	ImGui::End();

	return 1;
}

bool WeaponInspector::OnDMAFrame()
{
	if (!UpdateEssentials())
	{
		std::println("{} failed!",__FUNCTION__);
		return 0;
	}

	if (bNeedsOverwrite)
	{
		UpdateCurrentWeapon();
		bNeedsOverwrite = false;
	}

	if (bRequestedCopyToDesired)
	{
		DesiredWepInfo = WepInfo;
		bRequestedCopyToDesired = false;
	}

	// Always apply ammo settings on each frame to ensure they persist after map changes
	if (bNoReload)
		EnableNoReload();
	else if (bPrevNoReload)
		DisableNoReload();

	if (bInfiniteAmmo)
		EnableInfAmmo();
	else if (bPrevInfiniteAmmo)
		DisableInfAmmo();
	
	// 一键魔改功能 - 只有勾选时才进行修改，并且检查是否已经是目标数值
	if (bApplyOneClickMod)
	{
		// 检查当前武器属性是否已经是一键魔改的数值
		bool isAlreadyModified = 
			WepInfo.WeaponDamage == 99999.0f &&
			WepInfo.WeaponFireRate == 0.0f &&
			WepInfo.WeaponRange == 99999.0f &&
			WepInfo.WeaponPenetration == 99999.0f &&
			WepInfo.RecoilAmplitude == 0.0f &&
			WepInfo.ImpactType == IT_EXPLOSION &&
			WepInfo.ImpactExplosion == IE_ORBITAL_CANNON;
		
		// 如果不是一键魔改的数值，则应用修改
		if (!isAlreadyModified)
		{
			// 设置一键魔改的武器属性
			DesiredWepInfo.WeaponDamage = 99999.0f;
			DesiredWepInfo.WeaponFireRate = 0.0f;
			DesiredWepInfo.WeaponRange = 99999.0f;
			DesiredWepInfo.WeaponPenetration = 99999.0f;
			DesiredWepInfo.RecoilAmplitude = 0.0f;
			DesiredWepInfo.ImpactType = IT_EXPLOSION; // 5
			DesiredWepInfo.ImpactExplosion = IE_ORBITAL_CANNON; // 59 轨道炮
			
			// 应用修改
			UpdateCurrentWeapon();
		}
	}
	else if (bPrevApplyOneClickMod) // 如果当前未勾选但之前勾选了，说明是刚取消勾选
	{
		// 取消勾选时，恢复默认武器属性（从当前武器读取）
		DesiredWepInfo = WepInfo;
	}

	// 禁用其他人武器逻辑 - 功能开启时持续执行
	// 多级指针解引用：基址 -> 0x10B8 -> 0x20 -> +0x54
	uintptr_t baseAddr = DMA::BaseAddress + Offsets::AimCPedPtr;
	uintptr_t level1Addr = 0;
	uintptr_t level2Addr = 0;
	uintptr_t level3Addr = 0;
	DWORD bytesRead = 0;
	bool addressValid = false;
	uintptr_t finalAddr = 0;
	
	// 读取第一级指针
	if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, baseAddr, (BYTE*)&level1Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t))
	{
		// 第一级指针 + 0x10B8
		level1Addr += 0x10B8;
		
		// 读取第二级指针
		if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, level1Addr, (BYTE*)&level2Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t))
		{
			// 第二级指针 + 0x20
			level2Addr += 0x20;
			
			// 读取第三级指针
			if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, level2Addr, (BYTE*)&level3Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t))
			{
				// 检查第三级指针是否有效（不是空指针或无效地址）
				if (level3Addr != 0 && level3Addr != 0xCCCCCCCCCCCCCCCC && level3Addr != 0xFFFFFFFFFFFFFFFF)
				{
					// 第三级指针 + 0x54 = 最终地址
					finalAddr = level3Addr + 0x54;
					addressValid = true;
				}
			}
		}
	}
	
	// 只有当地址有效时才进行写入操作
	if (addressValid)
	{
		if (bDisableOthersWeapons)
		{
			// 功能开启时，持续禁用武器（修改为0）
			uint32_t disableValue = 0;
			VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&disableValue, sizeof(uint32_t));
		}
		else
		{
			// 功能关闭时，检查当前值是否为0，如果是则修改为2
			uint32_t currentValue = 0;
			if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&currentValue, sizeof(uint32_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uint32_t))
			{
				if (currentValue == 0)
				{
					uint32_t enableValue = 2;
					VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&enableValue, sizeof(uint32_t));
				}
			}
		}
	}
	
	// 更新状态
	bPrevDisableOthersWeapons = bDisableOthersWeapons;
	bPrevApplyOneClickMod = bApplyOneClickMod;

	// 修改其他人移动速度逻辑
	// 从 CT 文件中获取的正确偏移：AimCPedPTR -> +0x5E4
	if (bModifyOthersMoveSpeed)
	{
		// 读取 AimCPedPTR
		uintptr_t aimCPedBaseAddr = DMA::BaseAddress + Offsets::AimCPedPtr;
		uintptr_t aimCPedPtr = 0;
		DWORD bytesRead = 0;
		
		// 读取瞄准对象指针
		if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, aimCPedBaseAddr, (BYTE*)&aimCPedPtr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t) && aimCPedPtr != 0)
		{
			// 移动速度地址 = AimCPedPtr + 0x5E4
			uintptr_t moveSpeedAddr = aimCPedPtr + 0x5E4;
			// 设置移动速度为10
			float moveSpeedValue = 10.0f;
			VMMDLL_MemWrite(DMA::vmh, DMA::PID, moveSpeedAddr, (BYTE*)&moveSpeedValue, sizeof(float));
		}
	}

	// 将瞄准敌人血量修改为 -1 逻辑
	if (bSetAimTargetHealthToMinusOne) {
        // 读取 AimCPedPTR
        uintptr_t aimCPedBaseAddr = DMA::BaseAddress + Offsets::AimCPedPtr;
        uintptr_t aimCPedPtr = 0;
        DWORD bytesRead = 0;
        
        // 读取瞄准对象指针
        if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, aimCPedBaseAddr, (BYTE*)&aimCPedPtr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t) && aimCPedPtr != 0) {
            // 读取AimCPed 血量 (+280, Float)
            float health = 0.0f;
            if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, aimCPedPtr + 0x280, (BYTE*)&health, sizeof(float), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(float)) {
                // 将血量修改为-1
                float newHealth = -1.0f;
                VMMDLL_MemWrite(DMA::vmh, DMA::PID, aimCPedPtr + 0x280, (BYTE*)&newHealth, sizeof(float));
            }
        }
    }

	// 将瞄准敌人防弹衣修改为 -1 逻辑
	if (bSetAimTargetArmorToMinusOne) {
        // 读取 AimCPedPTR
        uintptr_t aimCPedBaseAddr = DMA::BaseAddress + Offsets::AimCPedPtr;
        uintptr_t aimCPedPtr = 0;
        DWORD bytesRead = 0;
        
        // 读取瞄准对象指针
        if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, aimCPedBaseAddr, (BYTE*)&aimCPedPtr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t) && aimCPedPtr != 0) {
            // 读取AimCPed 防弹衣 (+150C, Float) - 从CT文件中获取的偏移
            float armor = 0.0f;
            if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, aimCPedPtr + 0x150C, (BYTE*)&armor, sizeof(float), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(float)) {
                // 将防弹衣修改为-1
                float newArmor = -1.0f;
                VMMDLL_MemWrite(DMA::vmh, DMA::PID, aimCPedPtr + 0x150C, (BYTE*)&newArmor, sizeof(float));
            }
        }
    }

	// 百万瞬击逻辑 - 子弹飞行速度设置为9999
	if (bMillionInstantHit) {
        // 设置子弹飞行速度为9999
        WriteBulletSpeed(9999999999.0f);
    }

	// 应用冲击力修改逻辑
	if (bApplyImpactForces) {
        // 写入各种冲击力数值
        WriteImpactForce(0xD8, DesiredObjectImpactForce); // 武器命中普通物体冲击力
        WriteImpactForce(0xDC, DesiredPedImpactForce); // 武器对行人冲击力
        WriteImpactForce(0xE0, DesiredVehicleImpactForce); // 武器对载具冲击力
        WriteImpactForce(0xE4, DesiredAircraftImpactForce); // 武器对飞行目标冲击力
    }

	return 1;
}

bool WeaponInspector::UpdateEssentials()
{
	if (!UpdateWeaponInfo())
		return 0;

	if (!UpdateWeaponInventory())
		return 0;

	return 1;
}

bool WeaponInspector::UpdateCurrentWeapon()
{
	if (!DMA::WeaponInfoAddress)
	{
		std::println("Cannot update weapon info when in vehicle.");
		return 0;
	}

	DWORD BytesRead = 0x0;

	WeaponInfo LocalWeaponInfo;
	ZeroMemory(&LocalWeaponInfo, sizeof(WeaponInfo));

	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, DMA::WeaponInfoAddress, (BYTE*)&LocalWeaponInfo, sizeof(WeaponInfo), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(WeaponInfo))
		return 0;

	LocalWeaponInfo.WeaponDamage = DesiredWepInfo.WeaponDamage;
	LocalWeaponInfo.WeaponFireRate = DesiredWepInfo.WeaponFireRate;
	LocalWeaponInfo.WeaponRange = DesiredWepInfo.WeaponRange;
	LocalWeaponInfo.WeaponPenetration = DesiredWepInfo.WeaponPenetration;
	LocalWeaponInfo.ImpactType = DesiredWepInfo.ImpactType;
	LocalWeaponInfo.ImpactExplosion = DesiredWepInfo.ImpactExplosion;
	LocalWeaponInfo.RecoilAmplitude = DesiredWepInfo.RecoilAmplitude;

	VMMDLL_MemWrite(DMA::vmh, DMA::PID, DMA::WeaponInfoAddress, (BYTE*)&LocalWeaponInfo, sizeof(WeaponInfo));

	return 1;
}

bool WeaponInspector::UpdateWeaponInventory()
{
	uintptr_t WeaponInvPtr = DMA::LocalPlayerAddress + offsetof(PED, pCWeaponInventory);

	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, WeaponInvPtr, (BYTE*)&WeaponInvAddress, sizeof(uintptr_t), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(uintptr_t))
	{
		std::println("{} failed reading WeaponInvPtr!", __FUNCTION__);
		return 0;
	}

	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, WeaponInvAddress, (BYTE*)&WepInv, sizeof(WeaponInventory), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(WeaponInventory))
	{
		std::println("{} failed reading WeaponInventory!", __FUNCTION__);
		return 0;
	}

	return 1;
}

bool WeaponInspector::UpdateWeaponInfo()
{
	if (!DMA::WeaponInfoAddress)
	{
		ZeroMemory(&WepInfo, sizeof(WeaponInfo));
		return 1;
	}

	DWORD BytesRead = 0x0;

	VMMDLL_MemReadEx(DMA::vmh, DMA::PID, DMA::WeaponInfoAddress, (BYTE*)&WepInfo, sizeof(WeaponInfo), &BytesRead, VMMDLL_FLAG_NOCACHE);

	if (BytesRead != sizeof(WeaponInfo))
	{
		std::println("{} failed reading WeaponInfo!", __FUNCTION__);
		return 0;
	}

	return 1;
}

// 读取子弹飞行速度
bool WeaponInspector::ReadBulletSpeed(float& outBulletSpeed)
{
	DWORD bytesRead = 0;
	
	// 多级指针解引用：BaseAddress + Offsets::WorldPtr -> 0x8 -> 0x10B8 -> 0x20 -> 0x11C
	uintptr_t worldPtrBase = DMA::BaseAddress + Offsets::WorldPtr;
	uintptr_t worldPtr = 0;
	
	if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, worldPtrBase, (BYTE*)&worldPtr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t) && worldPtr != 0) {
		uintptr_t level1Addr = 0;
		if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, worldPtr + 0x8, (BYTE*)&level1Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t)) {
			uintptr_t level2Addr = 0;
			if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, level1Addr + 0x10B8, (BYTE*)&level2Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t)) {
				uintptr_t level3Addr = 0;
				if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, level2Addr + 0x20, (BYTE*)&level3Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t)) {
					uintptr_t finalAddr = level3Addr + 0x11C;
					if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&outBulletSpeed, sizeof(float), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(float)) {
						return true;
					}
				}
			}
		}
	}
	
	return false;
}

// 写入子弹飞行速度
bool WeaponInspector::WriteBulletSpeed(float bulletSpeed)
{
	return WriteImpactForce(0x11C, bulletSpeed);
}

// 读取冲击力相关数值
bool WeaponInspector::ReadImpactForce(uintptr_t offset, float& outImpactForce)
{
	DWORD bytesRead = 0;
	
	// 多级指针解引用：BaseAddress + Offsets::WorldPtr -> 0x8 -> 0x10B8 -> 0x20 -> +offset
	uintptr_t worldPtrBase = DMA::BaseAddress + Offsets::WorldPtr;
	uintptr_t worldPtr = 0;
	
	if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, worldPtrBase, (BYTE*)&worldPtr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t) && worldPtr != 0) {
		uintptr_t level1Addr = 0;
		if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, worldPtr + 0x8, (BYTE*)&level1Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t)) {
			uintptr_t level2Addr = 0;
			if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, level1Addr + 0x10B8, (BYTE*)&level2Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t)) {
				uintptr_t level3Addr = 0;
				if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, level2Addr + 0x20, (BYTE*)&level3Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t)) {
					uintptr_t finalAddr = level3Addr + offset;
					if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&outImpactForce, sizeof(float), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(float)) {
						return true;
					}
				}
			}
		}
	}
	
	return false;
}

// 写入冲击力相关数值
bool WeaponInspector::WriteImpactForce(uintptr_t offset, float impactForce)
{
	DWORD bytesRead = 0;
	
	// 多级指针解引用：BaseAddress + Offsets::WorldPtr -> 0x8 -> 0x10B8 -> 0x20 -> +offset
	uintptr_t worldPtrBase = DMA::BaseAddress + Offsets::WorldPtr;
	uintptr_t worldPtr = 0;
	
	if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, worldPtrBase, (BYTE*)&worldPtr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t) && worldPtr != 0) {
		uintptr_t level1Addr = 0;
		if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, worldPtr + 0x8, (BYTE*)&level1Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t)) {
			uintptr_t level2Addr = 0;
			if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, level1Addr + 0x10B8, (BYTE*)&level2Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t)) {
				uintptr_t level3Addr = 0;
				if (VMMDLL_MemReadEx(DMA::vmh, DMA::PID, level2Addr + 0x20, (BYTE*)&level3Addr, sizeof(uintptr_t), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(uintptr_t)) {
					uintptr_t finalAddr = level3Addr + offset;
					VMMDLL_MemWrite(DMA::vmh, DMA::PID, finalAddr, (BYTE*)&impactForce, sizeof(float));
					return true;
				}
			}
		}
	}
	
	return false;
}

bool WeaponInspector::EnableInfAmmo()
{
	// Check if infinite ammo is already enabled
	std::bitset<8>Bits(WepInv.AmmoModifier);
	if (Bits.test(0))
	{
		// Already enabled, no need to write to memory
		bPrevInfiniteAmmo = true;
		return 1;
	}
	
	Bits.set(0);

	uint8_t NewBits = static_cast<uint8_t>(Bits.to_ulong());

	uintptr_t AmmoModifierAddress = WeaponInvAddress + offsetof(WeaponInventory, AmmoModifier);

	VMMDLL_MemWrite(DMA::vmh, DMA::PID, AmmoModifierAddress, (BYTE*)&NewBits, sizeof(uint8_t));

	bPrevInfiniteAmmo = true;

	return 1;
}

bool WeaponInspector::DisableInfAmmo()
{
	// Check if infinite ammo is already disabled
	std::bitset<8>Bits(WepInv.AmmoModifier);
	if (!Bits.test(0))
	{
		// Already disabled, no need to write to memory
		bPrevInfiniteAmmo = false;
		return 1;
	}
	
	Bits.reset(0);

	uint8_t NewBits = static_cast<uint8_t>(Bits.to_ulong());

	uintptr_t AmmoModifierAddress = WeaponInvAddress + offsetof(WeaponInventory, AmmoModifier);

	VMMDLL_MemWrite(DMA::vmh, DMA::PID, AmmoModifierAddress, (BYTE*)&NewBits, sizeof(uint8_t));

	bPrevInfiniteAmmo = false;

	return 1;
}

bool WeaponInspector::EnableNoReload()
{
	// Check if no reload is already enabled
	std::bitset<8>Bits(WepInv.AmmoModifier);
	if (Bits.test(1))
	{
		// Already enabled, no need to write to memory
		bPrevNoReload = true;
		return 1;
	}
	
	Bits.set(1);

	uint8_t NewBits = static_cast<uint8_t>(Bits.to_ulong());

	uintptr_t AmmoModifierAddress = WeaponInvAddress + offsetof(WeaponInventory, AmmoModifier);

	VMMDLL_MemWrite(DMA::vmh, DMA::PID, AmmoModifierAddress, (BYTE*)&NewBits, sizeof(uint8_t));

	bPrevNoReload = true;

	return 1;
}

bool WeaponInspector::DisableNoReload()
{
	// Check if no reload is already disabled
	std::bitset<8>Bits(WepInv.AmmoModifier);
	if (!Bits.test(1))
	{
		// Already disabled, no need to write to memory
		bPrevNoReload = false;
		return 1;
	}
	
	Bits.reset(1);

	uint8_t NewBits = static_cast<uint8_t>(Bits.to_ulong());

	uintptr_t AmmoModifierAddress = WeaponInvAddress + offsetof(WeaponInventory, AmmoModifier);

	VMMDLL_MemWrite(DMA::vmh, DMA::PID, AmmoModifierAddress, (BYTE*)&NewBits, sizeof(uint8_t));

	bPrevNoReload = false;

	return 1;
}
