#include "pch.h"

#include "ConsoleTheme.h"
#include "WeaponInspector.h"

#include "Offsets.h"

#include <cstdio>

namespace
{
// 盒子间距（与 MenuManager 的 kBoxGap 一致）
constexpr float kBoxGap = 10.0f;
// 盒内「按钮行」占位：行间距 + 按钮高度
// 按钮块高度：ItemSpacing(10) + 按钮(button_h) + 分隔(1)，实测比原 8.0f 多 3px，
// 少算会让盒内容超出下边框 3px（自检里的 slack=-3）。
constexpr float kButtonBlock = 11.0f + layout::button_h;

// 内容盒是子窗口，不参与 ImGui 的组布局，必须显式定位（与 MenuManager::TwoColumn 同构）
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

// 带标题的盒子占位 = 标题行(section_h) + 盒子本体
float TitledBoxHeight(int rows) { return layout::box_height(rows) + layout::section_h; }
float TitledBoxPixels(float height) { return height + layout::section_h; }
} // namespace

bool WeaponInspector::RenderContent() {
    // 与其它页面共用同一套设计语言：两列 + 玻璃内容盒 + 行式控件。
    // 旧实现用 ImGui::BeginTable / SeparatorText / Checkbox 自成一套视觉，
    // 与外壳脱节，且输入框宽度随表格列宽漂移、标签忽左忽右、tooltip 靠游离的 "(?)"——
    // 这里全部改成盒内行：标签左对齐、控件列宽固定、说明并入行描述。
    float bulletSpeed = 0.0f;
    const bool bulletSpeedValid = ReadBulletSpeed(bulletSpeed);

    float objectImpactForce = 0.0f;
    float pedImpactForce = 0.0f;
    float vehicleImpactForce = 0.0f;
    float aircraftImpactForce = 0.0f;
    const bool objectImpactValid = ReadImpactForce(0xD8, objectImpactForce);
    const bool pedImpactValid = ReadImpactForce(0xDC, pedImpactForce);
    const bool vehicleImpactValid = ReadImpactForce(0xE0, vehicleImpactForce);
    const bool aircraftImpactValid = ReadImpactForce(0xE4, aircraftImpactForce);

    char damageText[32], fireRateText[32], rangeText[32], penetrationText[32];
    char accuracyText[32], moveAccuracyText[32], lockRangeText[32], recoilText[32];
    char impactTypeText[32], impactExplosionText[32];
    char bulletSpeedText[32], objectForceText[32], pedForceText[32], vehicleForceText[32], aircraftForceText[32];
    std::snprintf(damageText, sizeof(damageText), "%.2f", WepInfo.WeaponDamage);
    std::snprintf(fireRateText, sizeof(fireRateText), "%.2f", WepInfo.WeaponFireRate);
    std::snprintf(rangeText, sizeof(rangeText), "%.2f", WepInfo.WeaponRange);
    std::snprintf(penetrationText, sizeof(penetrationText), "%.2f", WepInfo.WeaponPenetration);
    std::snprintf(accuracyText, sizeof(accuracyText), "%.2f", WepInfo.WeaponAccuracy);
    std::snprintf(moveAccuracyText, sizeof(moveAccuracyText), "%.2f", WepInfo.WeaponMoveAccuracy);
    std::snprintf(lockRangeText, sizeof(lockRangeText), "%.2f", WepInfo.WeaponLockRange);
    std::snprintf(recoilText, sizeof(recoilText), "%.2f", WepInfo.RecoilAmplitude);
    std::snprintf(impactTypeText, sizeof(impactTypeText), "%d", static_cast<int>(WepInfo.ImpactType));
    std::snprintf(impactExplosionText, sizeof(impactExplosionText), "%d", static_cast<int>(WepInfo.ImpactExplosion));
    if (bulletSpeedValid)
        std::snprintf(bulletSpeedText, sizeof(bulletSpeedText), "%.2f", bulletSpeed);
    else
        std::snprintf(bulletSpeedText, sizeof(bulletSpeedText), "读取失败");
    if (objectImpactValid) std::snprintf(objectForceText, sizeof(objectForceText), "%.2f", objectImpactForce);
    else std::snprintf(objectForceText, sizeof(objectForceText), "读取失败");
    if (pedImpactValid) std::snprintf(pedForceText, sizeof(pedForceText), "%.2f", pedImpactForce);
    else std::snprintf(pedForceText, sizeof(pedForceText), "读取失败");
    if (vehicleImpactValid) std::snprintf(vehicleForceText, sizeof(vehicleForceText), "%.2f", vehicleImpactForce);
    else std::snprintf(vehicleForceText, sizeof(vehicleForceText), "读取失败");
    if (aircraftImpactValid) std::snprintf(aircraftForceText, sizeof(aircraftForceText), "%.2f", aircraftImpactForce);
    else std::snprintf(aircraftForceText, sizeof(aircraftForceText), "读取失败");

    TwoColumn col;
    col.Begin();

    /* ================= 左列 ================= */

    // 1) 当前武器信息：读数两两成行（8 项属性 + 2 项冲击参数），不再每项独占一行留下大片空档
    {
        col.Place(0);
        ConsoleTheme::BoxBeginPixels("weapon_current", layout::box_height(5), "当前武器信息", col.width);
        ConsoleTheme::TextRow2("伤害", damageText, true, "射速", fireRateText, true);
        ConsoleTheme::TextRow2("射程", rangeText, true, "穿透", penetrationText, true);
        ConsoleTheme::TextRow2("射击精准度", accuracyText, true, "移动精准度", moveAccuracyText, true);
        ConsoleTheme::TextRow2("射击锁定范围", lockRangeText, true, "后坐力幅度", recoilText, true);
        ConsoleTheme::TextRow2("冲击类型", impactTypeText, true, "冲击爆炸", impactExplosionText, true, false);
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxPixels(layout::box_height(5)));
    }

    // 2) 武器功能选项：开关成组，原先挂在 "(?)" tooltip 上的说明直接写在行下
    {
        col.Place(0);
        ConsoleTheme::BoxBegin("weapon_options", 7, "武器功能选项", col.width);
        ConsoleTheme::ToggleRow("wi_inf_ammo", "无限弹药", "弹匣容量不再减少", &bInfiniteAmmo);
        ConsoleTheme::ToggleRow("wi_no_reload", "无需装弹", "跳过换弹动作直接射击", &bNoReload);
        ConsoleTheme::ToggleRow("wi_disable_others", "禁用其他人武器", "瞄准范围内其他玩家无法开火", &bDisableOthersWeapons);
        ConsoleTheme::ToggleRow("wi_aim_health", "瞄准敌人修改血量为-1", "瞄准敌人生效；对自己同样生效，取消需重新瞄准敌方", &bSetAimTargetHealthToMinusOne);
        ConsoleTheme::ToggleRow("wi_aim_armor", "瞄准敌人修改防弹衣为-1", "将瞄准敌人的防弹衣修改为 -1", &bSetAimTargetArmorToMinusOne);
        ConsoleTheme::ToggleRow("wi_aim_speed", "修改其他人移动速度为10", "将瞄准敌人的移动速度修改为 10", &bModifyOthersMoveSpeed);
        ConsoleTheme::ToggleRow("wi_million_hit", "百万瞬击", "子弹飞行速度设置为 99999999", &bMillionInstantHit, false);
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxHeight(7));
    }

    // 3) 一键魔改：写入一组预设数值；勾选后同时立即应用
    {
        const float boxH = layout::box_height(1) + kButtonBlock;
        col.Place(0);
        ConsoleTheme::BoxBeginPixels("weapon_one_click", boxH, "一键教训智障儿童（天机炮版本半无敌也死）", col.width);
        ConsoleTheme::ToggleRow("wi_apply_one_click", "勾选直接应用", "勾选后点击一键魔改将直接写入当前武器", &bApplyOneClickMod);
        if (ConsoleTheme::ButtonRow("一键魔改", UiIcon::Zap, true))
        {
            DesiredWepInfo.WeaponDamage = 99999.0f;
            DesiredWepInfo.WeaponFireRate = 0.0f;
            DesiredWepInfo.WeaponRange = 99999.0f;
            DesiredWepInfo.WeaponPenetration = 99999.0f;
            DesiredWepInfo.WeaponAccuracy = 0.0f;
            DesiredWepInfo.WeaponMoveAccuracy = 0.0f;
            DesiredWepInfo.WeaponLockRange = 99999.0f;
            DesiredWepInfo.RecoilAmplitude = 0.0f;
            DesiredWepInfo.ImpactType = IT_EXPLOSION;      // 5
            DesiredWepInfo.ImpactExplosion = IE_ORBITAL_CANNON; // 59 轨道炮

            if (bApplyOneClickMod)
                bNeedsOverwrite = true;
        }
        ConsoleTheme::BoxEnd();
        col.Advance(0, TitledBoxPixels(boxH));
    }

    /* ================= 右列 ================= */

    // 4) 覆盖数值：10 个可编辑数值两两成行，输入框宽度固定，标签严格对齐
    {
        const float boxH = layout::box_height(5) + kButtonBlock;
        int impactType = static_cast<int>(DesiredWepInfo.ImpactType);
        int impactExplosion = static_cast<int>(DesiredWepInfo.ImpactExplosion);

        col.Place(1);
        ConsoleTheme::BoxBeginPixels("weapon_overwrite", boxH, "覆盖数值", col.width);
        ConsoleTheme::InputRow2("wi_damage", "武器伤害", &DesiredWepInfo.WeaponDamage, "武器射速", &DesiredWepInfo.WeaponFireRate, "%.3f");
        ConsoleTheme::InputRow2("wi_range", "武器射程", &DesiredWepInfo.WeaponRange, "武器穿透", &DesiredWepInfo.WeaponPenetration, "%.3f");
        ConsoleTheme::InputRow2("wi_accuracy", "射击精准度", &DesiredWepInfo.WeaponAccuracy, "移动精准度", &DesiredWepInfo.WeaponMoveAccuracy, "%.3f");
        ConsoleTheme::InputRow2("wi_lock_range", "射击锁定范围", &DesiredWepInfo.WeaponLockRange, "后坐力幅度", &DesiredWepInfo.RecoilAmplitude, "%.3f");
        if (ConsoleTheme::IntRow2("wi_impact", "冲击类型", &impactType, "冲击爆炸", &impactExplosion, false))
        {
            DesiredWepInfo.ImpactType = static_cast<eImpactType>(impactType);
            DesiredWepInfo.ImpactExplosion = static_cast<eImpactExplosion>(impactExplosion);
        }

        const float btnW = (ConsoleTheme::RowWidth() - 10.0f) * 0.5f;
        if (ConsoleTheme::AccentButton("更新", UiIcon::Check, ImVec2(btnW, layout::button_h)))
            bNeedsOverwrite = true;
        ImGui::SameLine(0.0f, 10.0f);
        if (ConsoleTheme::GhostButton("复制当前到目标", UiIcon::Chevron, ImVec2(btnW, layout::button_h)))
            bRequestedCopyToDesired = true;
        ConsoleTheme::BoxEnd();
        col.Advance(1, TitledBoxPixels(boxH));
    }

    // 5) 命中参数：游戏内实时读数（读取失败用警示色）
    {
        col.Place(1);
        ConsoleTheme::BoxBegin("weapon_hit_readout", 3, "命中参数", col.width);
        ConsoleTheme::TextRow2("子弹飞行速度", bulletSpeedText, bulletSpeedValid, "普通物体冲击力", objectForceText, objectImpactValid);
        ConsoleTheme::TextRow2("行人冲击力", pedForceText, pedImpactValid, "载具冲击力", vehicleForceText, vehicleImpactValid);
        ConsoleTheme::TextRow2("飞行载具冲击力", aircraftForceText, aircraftImpactValid, nullptr, nullptr, true, false);
        ConsoleTheme::BoxEnd();
        col.Advance(1, TitledBoxHeight(3));
    }

    // 6) 冲击力覆盖：4 个可编辑冲击力 + 持续写入开关
    {
        const float boxH = layout::box_height(3) + kButtonBlock;
        col.Place(1);
        ConsoleTheme::BoxBeginPixels("weapon_impact_write", boxH, "让他们飞起来冲击力", col.width);
        ConsoleTheme::InputRow2("wi_force_object", "普通物体", &DesiredObjectImpactForce, "行人", &DesiredPedImpactForce, "%.0f");
        ConsoleTheme::InputRow2("wi_force_vehicle", "陆地载具", &DesiredVehicleImpactForce, "飞行载具", &DesiredAircraftImpactForce, "%.0f");
        ConsoleTheme::ToggleRow("wi_apply_impact", "直接应用数值", "勾选后持续写入设置的冲击力数值", &bApplyImpactForces);

        const float btnW = (ConsoleTheme::RowWidth() - 10.0f) * 0.5f;
        if (ConsoleTheme::AccentButton("更新冲击力", UiIcon::Zap, ImVec2(btnW, layout::button_h)))
        {
            WriteImpactForce(0xD8, DesiredObjectImpactForce);
            WriteImpactForce(0xDC, DesiredPedImpactForce);
            WriteImpactForce(0xE0, DesiredVehicleImpactForce);
            WriteImpactForce(0xE4, DesiredAircraftImpactForce);
        }
        ImGui::SameLine(0.0f, 10.0f);
        if (ConsoleTheme::GhostButton("复制当前冲击力", UiIcon::Chevron, ImVec2(btnW, layout::button_h)))
        {
            float objectForce = 0.0f, pedForce = 0.0f, vehicleForce = 0.0f, aircraftForce = 0.0f;
            ReadImpactForce(0xD8, objectForce);
            ReadImpactForce(0xDC, pedForce);
            ReadImpactForce(0xE0, vehicleForce);
            ReadImpactForce(0xE4, aircraftForce);
            DesiredObjectImpactForce = objectForce;
            DesiredPedImpactForce = pedForce;
            DesiredVehicleImpactForce = vehicleForce;
            DesiredAircraftImpactForce = aircraftForce;
        }
        ConsoleTheme::BoxEnd();
        col.Advance(1, TitledBoxPixels(boxH));
    }

    col.End();
    return true;
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
			WepInfo.WeaponAccuracy == 0.0f &&
			WepInfo.WeaponMoveAccuracy == 0.0f &&
			WepInfo.WeaponLockRange == 99999.0f &&
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
			DesiredWepInfo.WeaponAccuracy = 0.0f;
			DesiredWepInfo.WeaponMoveAccuracy = 0.0f;
			DesiredWepInfo.WeaponLockRange = 99999.0f;
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
	LocalWeaponInfo.WeaponAccuracy = DesiredWepInfo.WeaponAccuracy;
	LocalWeaponInfo.WeaponMoveAccuracy = DesiredWepInfo.WeaponMoveAccuracy;
	LocalWeaponInfo.WeaponLockRange = DesiredWepInfo.WeaponLockRange;
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
