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

bool Teleport::RenderContent()
{
    ConsoleTheme::SectionHeader("坐标工作区", "编辑目标坐标或选择预设位置");

    // 当前位置
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
    ImGui::Text("当前位置: %.2f, %.2f, %.2f", DMA::LocalPlayerLocation.x, DMA::LocalPlayerLocation.y, DMA::LocalPlayerLocation.z);
    ImGui::PopStyleColor();

    // Custom adjustment value
    static float adjustmentValue = 1.0f;
    ImGui::InputFloat("调整值", &adjustmentValue, 0.1f, 1.0f, "%.3f");

    // X coordinate with +/- buttons
    ImGui::InputFloat("X", &Teleport::DesiredLocation.x);
    ImGui::SameLine();
    if (ImGui::Button("-##X")) {
        Teleport::DesiredLocation.x -= adjustmentValue;
    }
    ImGui::SameLine();
    if (ImGui::Button("+##X")) {
        Teleport::DesiredLocation.x += adjustmentValue;
    }

    // Y coordinate with +/- buttons
    ImGui::InputFloat("Y", &Teleport::DesiredLocation.y);
    ImGui::SameLine();
    if (ImGui::Button("-##Y")) {
        Teleport::DesiredLocation.y -= adjustmentValue;
    }
    ImGui::SameLine();
    if (ImGui::Button("+##Y")) {
        Teleport::DesiredLocation.y += adjustmentValue;
    }

    // Z coordinate with +/- buttons
    ImGui::InputFloat("Z", &Teleport::DesiredLocation.z);
    ImGui::SameLine();
    if (ImGui::Button("-##Z")) {
        Teleport::DesiredLocation.z -= adjustmentValue;
    }
    ImGui::SameLine();
    if (ImGui::Button("+##Z")) {
        Teleport::DesiredLocation.z += adjustmentValue;
    }

    if (ImGui::Button("复制当前位置"))
    {
        Teleport::DesiredLocation = DMA::LocalPlayerLocation;
    }
    ImGui::SameLine();

    if (ImGui::Button("传送玩家"))
    {
        bRequestedTeleport = true;
    }

    if (ImGui::Button("传送到标记点 (F5)"))
        RequestWaypointTeleport();
    ImGui::SameLine();
    if (ImGui::Button("传送到任务点 (F6)"))
        RequestObjectiveTeleport();

    if (ImGui::CollapsingHeader("通用传送点"))
    {
        for (auto Name : GeneralLocationStrings)
        {
            if (ImGui::Button(Name.c_str()))
            {
                DesiredLocation = LocationMap[Name];
                bRequestedTeleport = true;
            }
        }
    }

    if (ImGui::CollapsingHeader("赌场金库传送点"))
    {
        ImGui::Indent();
        
        if (ImGui::Button("赌场金库门前(兵不厌诈别开古贝科技运输车)"))
        {
            DesiredLocation = LocationMap["赌场金库门前"];
            bRequestedTeleport = true;
        }

        if (ImGui::Button("赌场金库大厅(需要开任务到金库门)"))
        {
            DesiredLocation = LocationMap["赌场金库大厅"];
            bRequestedTeleport = true;
        }
        
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "此选项只是卡金库门 满收益400w");
        
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("赌场前置传送点"))
    {
        ImGui::Indent();
        
        if (ImGui::Button("游戏厅##casino_prep")) { DesiredLocation = LocationMap["游戏厅(新)"]; bRequestedTeleport = true; }
        if (ImGui::Button("天文台")) { DesiredLocation = LocationMap["天文台"]; bRequestedTeleport = true; }
        if (ImGui::Button("赌场大门")) { DesiredLocation = LocationMap["赌场大门"]; bRequestedTeleport = true; }
        if (ImGui::Button("FIB电梯")) { DesiredLocation = LocationMap["FIB电梯"]; bRequestedTeleport = true; }
        if (ImGui::Button("FIB")) { DesiredLocation = LocationMap["FIB"]; bRequestedTeleport = true; }
        if (ImGui::Button("戴维斯市政厅")) { DesiredLocation = LocationMap["戴维斯市政厅"]; bRequestedTeleport = true; }
        if (ImGui::Button("国安局")) { DesiredLocation = LocationMap["国安局"]; bRequestedTeleport = true; }
        if (ImGui::Button("监狱正中心")) { DesiredLocation = LocationMap["监狱正中心"]; bRequestedTeleport = true; }
        if (ImGui::Button("克里福德金库激光器")) { DesiredLocation = LocationMap["克里福德金库激光器"]; bRequestedTeleport = true; }
        if (ImGui::Button("保安证")) { DesiredLocation = LocationMap["保安证"]; bRequestedTeleport = true; }
        
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("赌场任务")) {
        if (ImGui::Button("下水道")) { DesiredLocation = LocationMap["下水道"]; bRequestedTeleport = true; }
        if (ImGui::Button("金库")) { DesiredLocation = LocationMap["金库"]; bRequestedTeleport = true; }
        if (ImGui::Button("金库门")) { DesiredLocation = LocationMap["金库门"]; bRequestedTeleport = true; }
        if (ImGui::Button("金库门禁")) { DesiredLocation = LocationMap["金库门禁"]; bRequestedTeleport = true; }
        if (ImGui::Button("保安室1")) { DesiredLocation = LocationMap["保安室1"]; bRequestedTeleport = true; }
        if (ImGui::Button("保安室2")) { DesiredLocation = LocationMap["保安室2"]; bRequestedTeleport = true; }
        if (ImGui::Button("下层楼梯下")) { DesiredLocation = LocationMap["下层楼梯下"]; bRequestedTeleport = true; }
        if (ImGui::Button("小金库")) { DesiredLocation = LocationMap["小金库"]; bRequestedTeleport = true; }
        if (ImGui::Button("下层楼梯上")) { DesiredLocation = LocationMap["下层楼梯上"]; bRequestedTeleport = true; }
        if (ImGui::Button("洗衣房")) { DesiredLocation = LocationMap["洗衣房"]; bRequestedTeleport = true; }
        if (ImGui::Button("办公室")) { DesiredLocation = LocationMap["办公室"]; bRequestedTeleport = true; }
        if (ImGui::Button("员工大厅")) { DesiredLocation = LocationMap["员工大厅"]; bRequestedTeleport = true; }
    }

    if (ImGui::CollapsingHeader("佩里克岛前置"))
    {
        if (ImGui::Button("信号箱 1F")) { DesiredLocation = LocationMap["信号箱 1F"]; bRequestedTeleport = true; }
        ImGui::SameLine();
        if (ImGui::Button("信号箱 2F")) { DesiredLocation = LocationMap["信号箱 2F"]; bRequestedTeleport = true; }
        ImGui::SameLine();
        if (ImGui::Button("信号箱 3F")) { DesiredLocation = LocationMap["信号箱 3F"]; bRequestedTeleport = true; }
        ImGui::SameLine();
        if (ImGui::Button("信号箱 4F")) { DesiredLocation = LocationMap["信号箱 4F"]; bRequestedTeleport = true; }
        if (ImGui::Button("撤离")) { DesiredLocation = LocationMap["撤离"]; bRequestedTeleport = true; }
        ImGui::SameLine();
        if (ImGui::Button("武器梅利威瑟")) { DesiredLocation = LocationMap["武器梅利威瑟"]; bRequestedTeleport = true; }
        if (ImGui::Button("等离子切割枪(藏身处)")) { DesiredLocation = LocationMap["等离子切割枪(藏身处)"]; bRequestedTeleport = true; }
        ImGui::SameLine();
        if (ImGui::Button("等离子切割枪1")) { DesiredLocation = LocationMap["等离子切割枪1"]; bRequestedTeleport = true; }
        if (ImGui::Button("等离子切割枪2")) { DesiredLocation = LocationMap["等离子切割枪2"]; bRequestedTeleport = true; }
        ImGui::SameLine();
        if (ImGui::Button("等离子切割枪3")) { DesiredLocation = LocationMap["等离子切割枪3"]; bRequestedTeleport = true; }
        if (ImGui::Button("等离子切割枪4")) { DesiredLocation = LocationMap["等离子切割枪4"]; bRequestedTeleport = true; }
        ImGui::SameLine();
        if (ImGui::Button("指纹验证器")) { DesiredLocation = LocationMap["指纹验证器"]; bRequestedTeleport = true; }
        if (ImGui::Button("撤离岛满载回归")) { DesiredLocation = LocationMap["撤离岛满载回归"]; bRequestedTeleport = true; }
    }

    if (ImGui::CollapsingHeader("佩里克岛传送点"))
    {
        ImGui::Indent();

        if (ImGui::CollapsingHeader(" 别墅外侦察用"))
        {

            if (ImGui::Button("无线电塔"))
            {
                DesiredLocation = LocationMap["无线电塔"];
                bRequestedTeleport = true;
            }

            if (ImGui::Button("上层无线电塔"))
            {
                DesiredLocation = LocationMap["上层无线电塔"];
                bRequestedTeleport = true;
            }
        }

        if (ImGui::CollapsingHeader(" 别墅内"))
        {


            if (ImGui::Button("第一房间"))
            {
                DesiredLocation = LocationMap["第一房间"];
                bRequestedTeleport = true;
            }

            if (ImGui::Button("主出口"))
            {
                DesiredLocation = LocationMap["主出口"];
                bRequestedTeleport = true;
            }
            
            if (ImGui::Button("佩里克岛主目标"))
            {
                DesiredLocation = LocationMap["佩里克岛主目标"];
                bRequestedTeleport = true;
            }
            
            if (ImGui::Button("佩里克岛大门别墅入口"))
            {
                DesiredLocation = LocationMap["佩里克岛大门别墅入口"];
                bRequestedTeleport = true;
            }
            
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "不建议用传送很容易直接死");
        }

        if (ImGui::CollapsingHeader("次要战利品"))
        {
            for (auto Name : CayoSecondaryLocationStrings)
            {
                if (ImGui::Button(Name.c_str()))
                {
                    DesiredLocation = LocationMap[Name.c_str()];
                    bRequestedTeleport = true;
                }
            }
        }
        
        // 添加撤离点按钮
        if (ImGui::Button("佩里克岛传送到水里撤离"))
        {
            DesiredLocation = LocationMap["佩里克岛传送到水里撤离"];
            bRequestedTeleport = true;
        }
        
        // 添加黄色警告文字
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "侦察可正常用，上岛不建议容易直接死");

        ImGui::Unindent();
    }


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
