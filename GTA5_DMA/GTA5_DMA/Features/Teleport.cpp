#include "pch.h"

#include "ConsoleTheme.h"
#include "Teleport.h"

#include "Locations.h"

#include "Offsets.h"

bool Teleport::UpdatePlayerStartingLocation()
{
	puts(__FUNCTION__);

	// Check if NavigationAddress is valid
	if (DMA::NavigationAddress == 0)
	{
		puts("[ERROR] NavigationAddress is invalid");
		ZeroMemory(&StartingLocation, sizeof(Vec3));
		return false;
	}

	uintptr_t LocationAddress = DMA::NavigationAddress + offsetof(CNavigation, Position);
	DWORD BytesRead = 0;
	int RetryCount = 0;
	const int MaxRetries = 3;

	// Add retry mechanism
	while (RetryCount < MaxRetries)
	{
		VMMDLL_MemReadEx(DMA::vmh, DMA::PID, LocationAddress, (BYTE*)&StartingLocation, sizeof(Vec3), &BytesRead, VMMDLL_FLAG_NOCACHE);

		if (BytesRead == sizeof(Vec3))
		{
			// Check if coordinates are valid (not zero or extremely large values)
			if (StartingLocation.x != 0.0f || StartingLocation.y != 0.0f || StartingLocation.z != 0.0f)
			{
				return true;
			}
		}

		RetryCount++;
		Sleep(5); // Short delay between retries
	}

	puts("[ERROR] Failed to read player starting location after multiple attempts");
	ZeroMemory(&StartingLocation, sizeof(Vec3));
	return false;
}

void Teleport::OverwriteLocation(Vec3 Location)
{
	/* save starting location for later comparison */
	UpdatePlayerStartingLocation();

	// Create a local copy of the target location to avoid modification during teleport
	Vec3 TargetLocation = Location;

	BYTE InVehicleBits = 0x0;
	bool IsInVehicle = false;

	// Check if LocalPlayerAddress is valid
	if (DMA::LocalPlayerAddress != 0)
	{
		uintptr_t InVehicleAddress = DMA::LocalPlayerAddress + offsetof(PED, InVehicleBits);
		DWORD BytesRead = 0;
		VMMDLL_MemReadEx(DMA::vmh, DMA::PID, InVehicleAddress, (BYTE*)&InVehicleBits, sizeof(BYTE), &BytesRead, VMMDLL_FLAG_NOCACHE);
		
		if (BytesRead == sizeof(BYTE))
		{
			IsInVehicle = (InVehicleBits & 0x1) != 0;
		}
		else
		{
			puts("[ERROR] Failed to read InVehicleBits");
		}
	}

	VMMDLL_SCATTER_HANDLE vmsh = VMMDLL_Scatter_Initialize(DMA::vmh, DMA::PID, VMMDLL_FLAG_NOCACHE);

	// Create a temporary scatter write structure with the target location
	// This prevents the DesiredLocation static variable from affecting the current teleport
	struct TeleportWriteInfo
	{
		VMMDLL_SCATTER_HANDLE handle;
		Vec3 targetLoc;
	};

	TeleportWriteInfo WriteInfo;
	WriteInfo.handle = vmsh;
	WriteInfo.targetLoc = TargetLocation;

	// Write the target location directly to the navigation address instead of using DesiredLocation
	if (vmsh)
	{
		// Prepare player writes with local target location
		if (DMA::NavigationAddress != 0)
		{
			uintptr_t PlayerLocationAddress = DMA::NavigationAddress + offsetof(CNavigation, Position);
			VMMDLL_Scatter_PrepareWrite(vmsh, PlayerLocationAddress, (BYTE*)&TargetLocation, sizeof(Vec3));
		}

		// Prepare vehicle writes if in vehicle
		if (IsInVehicle && DMA::VehicleNavigationAddress != 0)
		{
			uintptr_t VehicleLocationAddress = DMA::VehicleNavigationAddress + offsetof(CNavigation, Position);
			VMMDLL_Scatter_PrepareWrite(vmsh, VehicleLocationAddress, (BYTE*)&TargetLocation, sizeof(Vec3));
		}
	}

	bool bSuccessfullyTeleported = false;
	int SuccessAttempts = 0;
	const int RequiredSuccessAttempts = 3;

	for (int i = 0; i < 200; i++)
	{
		// Execute scatter writes with local target location
		VMMDLL_Scatter_Execute(vmsh);

		Sleep(10);

		if (!DMA::UpdatePlayerCurrentLocation())
			continue;

		// Calculate distances using local target location
		float DistanceToTarget = DMA::LocalPlayerLocation.Distance(TargetLocation);
		float DistanceFromStart = DMA::LocalPlayerLocation.Distance(StartingLocation);

		// Improved teleport success condition
		if (DistanceFromStart > 5.0f && DistanceToTarget < 50.0f)
		{
			if (DistanceToTarget < 10.0f)
			{
				SuccessAttempts++;
				// Require multiple consecutive successful checks to confirm teleport
				if (SuccessAttempts >= RequiredSuccessAttempts)
				{
					puts("Successfully teleported.");
					bSuccessfullyTeleported = true;
					break;
				}
			}
		}
	}

	VMMDLL_Scatter_CloseHandle(vmsh);

	if (!bSuccessfullyTeleported)
		puts("Teleport failed.");
}

bool Teleport::OnDMAFrame()
{
	static bool bTeleportInProgress = false;

	if (bRequestedTeleport && !bTeleportInProgress)
	{
		// Set teleport in progress flag to prevent multiple triggers
		bTeleportInProgress = true;
		
		// Create a local copy of DesiredLocation to avoid race conditions
		Vec3 TargetLocation = DesiredLocation;
		
		// Reset request flag immediately
		bRequestedTeleport = false;
		
		// Execute teleport immediately
		OverwriteLocation(TargetLocation);
		
		// Clear in progress flag immediately to allow next teleport request
		// This ensures teleport requests are handled on the next frame
		bTeleportInProgress = false;
	}

	return true;
}

static const std::vector<std::string>CayoSecondaryLocationStrings = { "可卡因田","北部码头1","北部码头2","北部码头3","中部","主区1","主区2","主区3","机库1","机库2","出生点" };
static const std::vector<std::string>GeneralLocationStrings = { "游戏厅","军事基地","桑迪海岸","地堡","夜总会","赌场","赌场警察楼顶" };

bool Teleport::Render()
{
    // 只有当启用时才渲染传送窗口
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
    ImGui::Begin("传送", &bEnable, ImGuiWindowFlags_NoCollapse);

    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(6);

    RenderContent();

    ImGui::End();

    return true;
}

/* ---------- 传送页：卡片 + 芯片网格 + 坐标去重 ---------- */

namespace
{
struct TpChip
{
    std::string label;
    Vec3 pos;
    bool dropped = false;   // 被 8 米内重复项合并掉，不再铺出按钮
};

struct TpGroup
{
    const char* id;
    const char* title;
    const char* note;
    std::vector<TpChip> chips;
};

Vec3 TpLookup(const char* name)
{
    auto it = LocationMap.find(name);
    return it == LocationMap.end() ? Vec3{ 0.0f, 0.0f, 0.0f } : it->second;
}

// 用户反馈“传送点有些基本上都重复了”：相距 8 米内的预设视为同一地点，
// 只铺一个芯片，别名并进同一个标签（信息不丢，按钮不再重复）。
const float kTpSamePlace = 8.0f;

std::vector<TpGroup> TpBuildGroups()
{
    struct RawGroup
    {
        const char* id;
        const char* title;
        const char* note;
        std::vector<std::string> names;
    };

    std::vector<RawGroup> raw;
    raw.push_back({ "tp_g_general", "通用传送点", nullptr, GeneralLocationStrings });
    raw.push_back({ "tp_g_vault", "赌场金库", "此选项只是卡金库门 满收益400w",
                    { "赌场金库门前", "赌场金库大厅" } });
    raw.push_back({ "tp_g_casinoprep", "赌场前置传送点", nullptr,
                    { "游戏厅(新)", "天文台", "赌场大门", "FIB电梯", "FIB", "戴维斯市政厅", "国安局",
                      "监狱正中心", "克里福德金库激光器", "保安证" } });
    raw.push_back({ "tp_g_casinomission", "赌场任务", nullptr,
                    { "下水道", "金库", "金库门", "金库门禁", "保安室1", "保安室2", "下层楼梯下",
                      "小金库", "下层楼梯上", "洗衣房", "办公室", "员工大厅" } });
    raw.push_back({ "tp_g_cayoprep", "佩里克岛前置", nullptr,
                    { "信号箱 1F", "信号箱 2F", "信号箱 3F", "信号箱 4F", "撤离", "武器梅利威瑟",
                      "等离子切割枪(藏身处)", "等离子切割枪1", "等离子切割枪2", "等离子切割枪3",
                      "等离子切割枪4", "指纹验证器", "撤离岛满载回归" } });
    raw.push_back({ "tp_g_cayovilla", "佩里克岛 · 别墅外", "侦察可正常用，上岛不建议容易直接死",
                    { "无线电塔", "上层无线电塔", "第一房间", "主出口", "佩里克岛主目标",
                      "佩里克岛大门别墅入口" } });
    raw.push_back({ "tp_g_cayoloot", "佩里克岛 · 次要战利品", nullptr, CayoSecondaryLocationStrings });
    raw.push_back({ "tp_g_cayowater", "佩里克岛 · 撤离", "不建议用传送很容易直接死",
                    { "佩里克岛传送到水里撤离" } });

    // 第一遍：按组把预设铺平（此时索引已全部确定，后面去重只改标签/标记，不再增删元素）
    std::vector<TpGroup> groups;
    for (const RawGroup& r : raw)
    {
        TpGroup g;
        g.id = r.id;
        g.title = r.title;
        g.note = r.note;
        for (const std::string& name : r.names)
            g.chips.push_back({ name, TpLookup(name.c_str()), false });
        if (!g.chips.empty())
            groups.push_back(g);
    }

    // 第二遍：相距 8 米内视为同一地点（用户反馈“有些传送点基本上都重复了”）：
    // 只保留先出现的那一个按钮，后面的名字并入它的标签，并标记为不显示。
    struct Seen
    {
        int group;
        int chip;
        Vec3 pos;
    };
    std::vector<Seen> seen;

    for (size_t gi = 0; gi < groups.size(); ++gi)
    {
        for (size_t ci = 0; ci < groups[gi].chips.size(); ++ci)
        {
            const Vec3 p = groups[gi].chips[ci].pos;
            int found = -1;
            for (size_t i = 0; i < seen.size(); ++i)
            {
                const float dx = seen[i].pos.x - p.x;
                const float dy = seen[i].pos.y - p.y;
                const float dz = seen[i].pos.z - p.z;
                if (dx * dx + dy * dy + dz * dz <= kTpSamePlace * kTpSamePlace)
                {
                    found = static_cast<int>(i);
                    break;
                }
            }

            if (found >= 0)
            {
                std::string& label = groups[seen[found].group].chips[seen[found].chip].label;
                const std::string& alias = groups[gi].chips[ci].label;
                if (label.find(alias) == std::string::npos)
                    label += " / " + alias;
                groups[gi].chips[ci].dropped = true;
                continue;
            }

            seen.push_back({ static_cast<int>(gi), static_cast<int>(ci), p });
        }
    }

    // 第三遍：只留下未被合并的按钮，空组丢弃
    std::vector<TpGroup> out;
    for (TpGroup& g : groups)
    {
        std::vector<TpChip> keep;
        for (TpChip& c : g.chips)
        {
            if (!c.dropped)
                keep.push_back(c);
        }
        if (keep.empty())
            continue;
        g.chips.swap(keep);
        out.push_back(std::move(g));
    }

    return out;
}
} // namespace

bool Teleport::RenderContent()
{
    static float adjustmentValue = 1.0f;   // X/Y/Z 步进步长（米）

    ConsoleTheme::SectionHeader("坐标传送", "X / Y / Z 可直接键入或按步长微调");

    char buf[96];

    // ── 三列瀑布流：目标坐标 / 传送操作 / 8 组预设点 一起排 ──
    // 原来坐标与操作各占一整行（右半边空着），预设点再单独铺一遍 → 实测内容 1460px 以上，
    // 两列怎么排都超出一屏（曾量到盒底 1052，被视口裁掉）。三列 + 短列优先落位后整页铺满且不裁切。
    ConsoleTheme::Columns col;
    col.Begin(3);

    col.Place(0);
    ConsoleTheme::BoxBegin("tp_target", 5, "目标坐标", col.width);
    snprintf(buf, sizeof(buf), "%.2f, %.2f, %.2f", StartingLocation.x, StartingLocation.y, StartingLocation.z);
    ConsoleTheme::TextRow("当前位置", buf, true);
    snprintf(buf, sizeof(buf), "%.2f, %.2f, %.2f", DesiredLocation.x, DesiredLocation.y, DesiredLocation.z);
    ConsoleTheme::TextRow("目标坐标", buf, true);
    ConsoleTheme::StepperRow("tp_x", "X", &DesiredLocation.x, adjustmentValue, "%.2f");
    ConsoleTheme::StepperRow("tp_y", "Y", &DesiredLocation.y, adjustmentValue, "%.2f");
    ConsoleTheme::StepperRow("tp_z", "Z", &DesiredLocation.z, adjustmentValue, "%.2f");
    ConsoleTheme::BoxEnd();
    col.Advance(0, ConsoleTheme::TitledBoxHeight(5));

    col.Place(1);
    ConsoleTheme::BoxBeginPixels("tp_actions", layout::box_height(0) + 45.0f * 4.0f, "传送操作", col.width);
    if (ConsoleTheme::ButtonRow("复制当前位置到目标", UiIcon::Folder))
    {
        DesiredLocation = StartingLocation;
    }
    if (ConsoleTheme::ButtonRow("传送到目标坐标", UiIcon::Target, true))
    {
        bRequestedTeleport = true;
    }
    if (ConsoleTheme::ButtonRow("传送到标记点 (F5)", UiIcon::Pin))
    {
        RequestWaypointTeleport();
    }
    if (ConsoleTheme::ButtonRow("传送到任务点 (F6)", UiIcon::Map))
    {
        RequestObjectiveTeleport();
    }
    ConsoleTheme::BoxEnd();
    col.Advance(1, ConsoleTheme::TitledBoxPixels(layout::box_height(0) + 45.0f * 4.0f));

    col.End();

    static const std::vector<TpGroup> groups = TpBuildGroups();
    for (const TpGroup& g : groups)
    {
        const float avail = col.width - layout::box_pad_x * 2.0f;
        std::vector<const char*> labels;
        labels.reserve(g.chips.size());
        for (const TpChip& c : g.chips)
            labels.push_back(c.label.c_str());

        const bool hasNote = g.note && g.note[0] != '\0';
        const int chipCount = static_cast<int>(labels.size());
        const float gridH = ConsoleTheme::ChipGridHeight(labels.data(), chipCount, avail, 6.0f);
        const float noteH = hasNote ? layout::row_h + layout::separator_h : 0.0f;
        const float boxH = layout::box_pad_y * 2.0f + gridH + noteH;

        const int slot = col.Shortest();
        col.Place(slot);
        ConsoleTheme::BoxBeginPixels(g.id, boxH, g.title, col.width);
        if (hasNote)
            ConsoleTheme::NoteRow(g.note, true, true);
        const int hit = ConsoleTheme::ChipGrid(g.id, labels.data(), chipCount, avail, 6.0f);
        if (hit >= 0)
        {
            DesiredLocation = g.chips[hit].pos;
            bRequestedTeleport = true;
            // 自检：每次真正命中追加一行 → 用来验证“点一个芯片只触发一个动作”
            char chipLog[192];
            snprintf(chipLog, sizeof(chipLog), "CHIPHIT %s i=%d label=%s\r\n",
                     g.id, hit, g.chips[hit].label.c_str());
            FILE* cf = nullptr;
            if (fopen_s(&cf, "chip_hits.log", "a") == 0 && cf)
            {
                fputs(chipLog, cf);
                fclose(cf);
            }
        }
        ConsoleTheme::BoxEnd();
        col.Advance(slot, ConsoleTheme::TitledBoxPixels(boxH));
    }
    col.End();

    return true;
}
bool Teleport::RequestWaypointTeleport()
{
    const Vec3 waypointCoords = GetWaypointCoords();
    if (waypointCoords.x == 0.0f && waypointCoords.y == 0.0f)
        return false;

    DesiredLocation = waypointCoords;
    bRequestedTeleport = true;
    return true;
}

bool Teleport::RequestObjectiveTeleport()
{
    if (currentGameType != GameType::GTA5_Enhanced)
        return false;

    const Vec3 objectiveCoords = GetObjectiveCoords();
    if (objectiveCoords.x == 0.0f && objectiveCoords.y == 0.0f)
        return false;

    DesiredLocation = objectiveCoords;
    bRequestedTeleport = true;
    return true;
}

void Teleport::PreparePlayerWrites(VMMDLL_SCATTER_HANDLE vmsh)
{
	// Check if scatter handle is valid
	if (!vmsh)
	{
		puts("[ERROR] Invalid scatter handle in PreparePlayerWrites");
		return;
	}

	// Check if NavigationAddress is valid
	if (DMA::NavigationAddress == 0)
	{
		puts("[ERROR] NavigationAddress is invalid in PreparePlayerWrites");
		return;
	}

	uintptr_t LocationAddress = DMA::NavigationAddress + offsetof(CNavigation, Position);
	VMMDLL_Scatter_PrepareWrite(vmsh, LocationAddress, (BYTE*)&DesiredLocation, sizeof(Vec3));
}

void Teleport::PrepareVehicleWrites(VMMDLL_SCATTER_HANDLE vmsh)
{
	// Check if scatter handle is valid
	if (!vmsh)
	{
		puts("[ERROR] Invalid scatter handle in PrepareVehicleWrites");
		return;
	}

	// Check if VehicleNavigationAddress is valid
	if (DMA::VehicleNavigationAddress == 0)
	{
		puts("[ERROR] VehicleNavigationAddress is invalid in PrepareVehicleWrites");
		return;
	}

	uintptr_t LocationAddress = DMA::VehicleNavigationAddress + offsetof(CNavigation, Position);
	VMMDLL_Scatter_PrepareWrite(vmsh, LocationAddress, (BYTE*)&DesiredLocation, sizeof(Vec3));
}

#define BLIP_NUM 1000

Vec3 Teleport::GetWaypointCoords()
{
	puts("Getting waypoint coords...");

	Vec3 WaypointCoordinates = { 0.0f,0.0f,0.0f };

	// Check if BaseAddress is valid
	if (DMA::BaseAddress == 0)
	{
		puts("[ERROR] BaseAddress is invalid");
		return WaypointCoordinates;
	}

	// 优先路径：动态解析的导航点基址（基址+0x20 处直接存放 vec3 坐标）
	// 参考 CT 表: WaypointPTR 反编译基址 + 0x20 = X (+4=Y, +4=Z)
	if (Offsets::WaypointPtr != 0)
	{
		Vec3 direct = { 0.0f, 0.0f, 0.0f };
		if (DMA::Memory().Read(DMA::BaseAddress + Offsets::WaypointPtr + 0x20, &direct, sizeof(direct)))
		{
			if (direct.x != 0.0f || direct.y != 0.0f)
			{
				printf("Waypoint from resolved base: %.2f %.2f %.2f\n", direct.x, direct.y, direct.z);
				return direct;
			}
		}
	}

	// 回退路径：Blip 数组扫描（ID 8 = 导航点）
	uintptr_t BlipsArrayAddress = DMA::BaseAddress + Offsets::BlipPtr;
	const int MaxRetries = 3;
	int RetryCount = 0;
	bool bFound = false;

	// Add retry mechanism for waypoint detection
	while (RetryCount < MaxRetries && !bFound)
	{
		uintptr_t BlipAddresses[BLIP_NUM] = { 0 };
		DWORD BytesRead = 0x0;

		// Read blips array with error checking
		if (!VMMDLL_MemReadEx(DMA::vmh, DMA::PID, BlipsArrayAddress, (BYTE*)&BlipAddresses, sizeof(uintptr_t) * BLIP_NUM, &BytesRead, VMMDLL_FLAG_NOCACHE))
		{
			puts("[ERROR] Failed to read blips array");
			RetryCount++;
			Sleep(10);
			continue;
		}

		// Check if we read at least some data
		if (BytesRead == 0 || BytesRead % sizeof(uintptr_t) != 0)
		{
			puts("[ERROR] Invalid blips array read");
			RetryCount++;
			Sleep(10);
			continue;
		}

		// Calculate how many blip addresses we actually read
		int ValidBlipCount = BytesRead / sizeof(uintptr_t);
		if (ValidBlipCount > BLIP_NUM)
			ValidBlipCount = BLIP_NUM;

		// Create scatter handle with error checking
		VMMDLL_SCATTER_HANDLE vmsh = VMMDLL_Scatter_Initialize(DMA::vmh, DMA::PID, VMMDLL_FLAG_NOCACHE);
		if (!vmsh)
		{
			puts("[ERROR] Failed to initialize scatter handle");
			RetryCount++;
			Sleep(10);
			continue;
		}

		// Allocate memory for blips
		auto pBlips = std::make_unique<Blip[]>(BLIP_NUM);
		ZeroMemory(pBlips.get(), sizeof(Blip) * BLIP_NUM);

		// Prepare scatter writes for valid blip addresses only
		for (int i = 0; i < ValidBlipCount; i++)
		{
			if (!BlipAddresses[i])
				continue;

			// Prepare scatter read with proper error checking
			if (!VMMDLL_Scatter_PrepareEx(vmsh, BlipAddresses[i], sizeof(Blip), (BYTE*)&pBlips[i], nullptr))
			{
				continue; // Skip invalid blips
			}
		}

		// Execute scatter reads
		if (!VMMDLL_Scatter_Execute(vmsh))
		{
			puts("[ERROR] Failed to execute scatter reads");
			VMMDLL_Scatter_CloseHandle(vmsh);
			RetryCount++;
			Sleep(10);
			continue;
		}

		// Search for waypoint blip (ID 8)
		for (int i = 0; i < ValidBlipCount; i++)
		{
			if (pBlips[i].ID == 8)
			{
				// Check if coordinates are valid (not zero)
				if (pBlips[i].Position.x != 0.0f && pBlips[i].Position.y != 0.0f)
				{
					printf("Found waypoint blip!\n");
					printf("%.2f %.2f %.2f\n", pBlips[i].Position.x, pBlips[i].Position.y, pBlips[i].Position.z);

					WaypointCoordinates = pBlips[i].Position;

					// Adjust Z coordinate with more robust logic
					if (WaypointCoordinates.z == 20)
						WaypointCoordinates.z = -255;
					else
						WaypointCoordinates.z += 2;

					bFound = true;
					break;
				}
			}
		}

		// Close scatter handle
		VMMDLL_Scatter_CloseHandle(vmsh);

		if (!bFound)
		{
			RetryCount++;
			Sleep(10);
		}
	}

	if (!bFound)
		puts("Couldn't find waypoint coords after multiple attempts.");

	// Optimize teleport waypoint: if Z axis is -255, replace with 50
	if (WaypointCoordinates.z == -255.0f)
		WaypointCoordinates.z = 50.0f;

	return WaypointCoordinates;
}



Vec3 Teleport::GetObjectiveCoords()
{
	Vec3 objectiveCoordinates = { 0.0f, 0.0f, 0.0f };
	if (currentGameType != GameType::GTA5_Enhanced || DMA::BaseAddress == 0)
		return objectiveCoordinates;

	struct ObjectiveBlip
	{
		char pad_0000[16];
		Vec3 Position;
		char pad_001C[36];
		int32_t ID;
		char pad_0044[4];
		uint8_t Color; // GTA5 Enhanced Blip color at offset 0x48.
	};
	static_assert(offsetof(ObjectiveBlip, Color) == 0x48);

	struct ObjectiveType
	{
		const int* IDs;
		size_t IDCount;
		const uint8_t* Colors;
		size_t ColorCount;
	};

	static const int primaryIDs[] = { 1 };
	static const uint8_t primaryColors[] = { 5, 60, 66 };
	static const int missionIDs[] = { 1, 225, 427, 478, 501, 523, 556 };
	static const uint8_t missionColors[] = { 1, 2, 3, 54, 78 };
	static const int specialIDs[] = { 432, 443 };
	static const uint8_t specialColors[] = { 59 };
	static const ObjectiveType objectiveTypes[] = {
		{ primaryIDs, _countof(primaryIDs), primaryColors, _countof(primaryColors) },
		{ missionIDs, _countof(missionIDs), missionColors, _countof(missionColors) },
		{ specialIDs, _countof(specialIDs), specialColors, _countof(specialColors) }
	};

	uintptr_t blipAddresses[BLIP_NUM] = { 0 };
	DWORD bytesRead = 0;
	const uintptr_t blipsArrayAddress = DMA::BaseAddress + Offsets::BlipPtr;
	if (!VMMDLL_MemReadEx(DMA::vmh, DMA::PID, blipsArrayAddress, reinterpret_cast<BYTE*>(blipAddresses), sizeof(blipAddresses), &bytesRead, VMMDLL_FLAG_NOCACHE))
		return objectiveCoordinates;

	int validBlipCount = static_cast<int>(bytesRead / sizeof(uintptr_t));
	if (validBlipCount > BLIP_NUM)
		validBlipCount = BLIP_NUM;

	auto blips = std::make_unique<ObjectiveBlip[]>(validBlipCount);
	ZeroMemory(blips.get(), sizeof(ObjectiveBlip) * validBlipCount);
	VMMDLL_SCATTER_HANDLE vmsh = VMMDLL_Scatter_Initialize(DMA::vmh, DMA::PID, VMMDLL_FLAG_NOCACHE);
	if (!vmsh)
		return objectiveCoordinates;

	for (int index = 0; index < validBlipCount; ++index)
	{
		if (blipAddresses[index])
			VMMDLL_Scatter_PrepareEx(vmsh, blipAddresses[index], sizeof(ObjectiveBlip), reinterpret_cast<BYTE*>(&blips[index]), nullptr);
	}

	if (!VMMDLL_Scatter_Execute(vmsh))
	{
		VMMDLL_Scatter_CloseHandle(vmsh);
		return objectiveCoordinates;
	}

	for (const ObjectiveType& objectiveType : objectiveTypes)
	{
		for (int index = 0; index < validBlipCount; ++index)
		{
			const ObjectiveBlip& blip = blips[index];
			bool idMatches = false;
			bool colorMatches = false;
			for (size_t idIndex = 0; idIndex < objectiveType.IDCount; ++idIndex)
				idMatches = idMatches || blip.ID == objectiveType.IDs[idIndex];
			for (size_t colorIndex = 0; colorIndex < objectiveType.ColorCount; ++colorIndex)
				colorMatches = colorMatches || blip.Color == objectiveType.Colors[colorIndex];

			if (idMatches && colorMatches && (blip.Position.x != 0.0f || blip.Position.y != 0.0f))
			{
				objectiveCoordinates = blip.Position;
				objectiveCoordinates.z = objectiveCoordinates.z == 20.0f ? 50.0f : objectiveCoordinates.z + 1.0f;
				VMMDLL_Scatter_CloseHandle(vmsh);
				return objectiveCoordinates;
			}
		}
	}

	VMMDLL_Scatter_CloseHandle(vmsh);
	return objectiveCoordinates;
}
