#include "pch.h"

#include <fstream>
#include <ctime>
#include <string>

#include "MyMenu.h"
#include "Dev.h"
#include "InputManager.h"
#include "DMA.h"
#include "Offsets.h"

#include "PlayerChaserMenu.h"
#include "Features.h"
// 移除NPCTeleport.h引用

#include "MenuManager.h"

// HWND declaration
extern HWND hwnd;

// 静态成员变量定义
bool MyMenu::bMenuVisible = true;
bool MyMenu::bFusionMode = false;
bool MyMenu::bFullscreenMode = false;

// Function to toggle fullscreen mode
void ToggleFullscreen(HWND hwnd, bool fullscreen) {
	static LONG prevStyle = 0;
	static RECT prevRect = { 0, 0, 0, 0 };

	if (fullscreen) {
		// Save current window style and position
		prevStyle = GetWindowLong(hwnd, GWL_STYLE);
		GetWindowRect(hwnd, &prevRect);

		// Set fullscreen
		SetWindowLong(hwnd, GWL_STYLE, prevStyle & ~WS_OVERLAPPEDWINDOW);
		SetWindowPos(hwnd, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_FRAMECHANGED);
	} else {
		// Restore window
		SetWindowLong(hwnd, GWL_STYLE, prevStyle);
		SetWindowPos(hwnd, HWND_TOP, prevRect.left, prevRect.top, 
			prevRect.right - prevRect.left, prevRect.bottom - prevRect.top, SWP_FRAMECHANGED);
	}
}

bool MyMenu::Render()
{
	// 获取MenuManager实例并渲染当前页面
	MenuManager::GetInstance().RenderCurrentPage();

	// Render the player chaser menu if enabled
	PlayerChaserMenu::Render();
	
	// 移除NPC载具传送功能处理代码

	return true;
}

// 模板函数：安全读取内存
// 简化重复的内存读取逻辑
template<typename T>
bool SafeMemRead(uintptr_t address, T& value) {
    DWORD bytesRead = 0;
    return VMMDLL_MemReadEx(DMA::vmh, DMA::PID, address, (BYTE*)&value, sizeof(T), &bytesRead, VMMDLL_FLAG_NOCACHE) && bytesRead == sizeof(T);
}

// 渲染融合模式窗口
void MyMenu::RenderFusionMode()
{
	constexpr uintptr_t OFFSET_MAX_PLAYERS = 0x288;
	constexpr uintptr_t OFFSET_CURRENT_PLAYERS = 0x294;
	constexpr uintptr_t OFFSET_HEALTH = 0x280;
	constexpr uintptr_t OFFSET_ARMOR = 0x150C;
	constexpr uintptr_t OFFSET_SPEED = 0x5E4;
	constexpr uintptr_t OFFSET_PLAYER_INFO = 0x10A8;
	constexpr uintptr_t OFFSET_NAME_PTR = 0xFC;
	constexpr uintptr_t OFFSET_POS_PTR = 0x30;
	constexpr uintptr_t OFFSET_POS_X = 0x50;
	constexpr uintptr_t OFFSET_POS_Y = 0x54;
	constexpr uintptr_t OFFSET_POS_Z = 0x58;
	constexpr uintptr_t OFFSET_VISUAL_X = 0x90;
	constexpr uintptr_t OFFSET_VISUAL_Y = 0x94;
	constexpr uintptr_t OFFSET_VISUAL_Z = 0x98;
    
    // 性能优化：限制数据更新频率为每100ms一次，减少内存读取开销
    static float lastUpdateTime = 0.0f;
    const float updateInterval = 0.1f; // 100ms
    
    float currentTime = static_cast<float>(ImGui::GetTime());
    bool shouldUpdateData = (currentTime - lastUpdateTime) >= updateInterval;
    
    // 静态变量：缓存玩家数据，避免每帧重新读取
    static uint32_t cachedMaxPlayers = 0;
    static uint32_t cachedCurrentPlayers = 0;
    static float cachedHealth = 0.0f;
    static float cachedArmor = 0.0f;
    static float cachedSpeed = 0.0f;
    static char cachedPlayerName[11] = {0};
    static char lastWrittenPlayerName[11] = {0}; // 存储上一次写入的玩家名称
    static float cachedPosX = 0.0f, cachedPosY = 0.0f, cachedPosZ = 0.0f;
    static float cachedVisualX = 0.0f, cachedVisualY = 0.0f, cachedVisualZ = 0.0f;
    static bool cachedHasAimTarget = false;
    static bool cachedPlayerMgrValid = false;

	// 读取 CNetworkPlayerMgrPtr
	uintptr_t playerMgrAddr = DMA::BaseAddress + Offsets::PlayerMgrPtr;
	
	// 读取 AimCPedPTR
	uintptr_t aimCPedBaseAddr = DMA::BaseAddress + Offsets::AimCPedPtr;
	
	// 只在需要更新数据时执行内存读取
	if (shouldUpdateData) {
        lastUpdateTime = currentTime;
        
        // 重置缓存状态
        cachedPlayerMgrValid = false;
        cachedHasAimTarget = false;
        
        uintptr_t networkPlayerMgrPtr = 0;
        uintptr_t aimCPedPtr = 0;
        
        // 读取玩家管理器指针
        if (SafeMemRead(playerMgrAddr, networkPlayerMgrPtr)) {
            cachedPlayerMgrValid = true;
            
            // 读取最大玩家数量
            SafeMemRead(networkPlayerMgrPtr + OFFSET_MAX_PLAYERS, cachedMaxPlayers);
            
            // 读取当前玩家数量
            SafeMemRead(networkPlayerMgrPtr + OFFSET_CURRENT_PLAYERS, cachedCurrentPlayers);
        }
        
        // 读取AimCPedPTR
        if (SafeMemRead(aimCPedBaseAddr, aimCPedPtr) && aimCPedPtr != 0) {
            cachedHasAimTarget = true;
            
            // 读取AimCPed 血量
            SafeMemRead(aimCPedPtr + OFFSET_HEALTH, cachedHealth);
            
            // 读取AimCPed 防弹衣
            SafeMemRead(aimCPedPtr + OFFSET_ARMOR, cachedArmor);
            
            // 读取AimCPed 移速
            SafeMemRead(aimCPedPtr + OFFSET_SPEED, cachedSpeed);
            
            // 读取AimCPed 玩家Name
            uintptr_t namePtr1 = 0;
            if (SafeMemRead(aimCPedPtr + OFFSET_PLAYER_INFO, namePtr1) && namePtr1 != 0) {
                SafeMemRead(namePtr1 + OFFSET_NAME_PTR, cachedPlayerName);
                
                // 只有当玩家名称不为空且与上一次写入的名称不同时，才写入文件
                if (cachedPlayerName[0] != '\0' && strcmp(cachedPlayerName, lastWrittenPlayerName) != 0) {
                    // 获取当前时间
                    time_t now = time(0);
                    char timeStr[20];
                    tm tm_local;
                    localtime_s(&tm_local, &now);
                    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_local);
                    
                    // 简化文件操作，避免可能的崩溃
                    try {
                        // 打开文件，追加模式
                        std::ofstream outFile("AIMSHOOTNAME.txt", std::ios::app);
                        if (outFile.is_open()) {
                            // 写入时间戳和玩家名称
                            outFile << "[" << timeStr << "] 瞄准玩家: " << cachedPlayerName << std::endl;
                            outFile.close();
                            
                            // 更新上一次写入的玩家名称
                            strncpy_s(lastWrittenPlayerName, sizeof(lastWrittenPlayerName), cachedPlayerName, _TRUNCATE);
                        }
                    } catch (const std::exception&) {
                        // 忽略文件操作异常，避免程序崩溃
                    }
                }
            } else {
                cachedPlayerName[0] = '\0';
            }
            
            // 读取AimCPed Position
            uintptr_t posPtr = 0;
            if (SafeMemRead(aimCPedPtr + OFFSET_POS_PTR, posPtr) && posPtr != 0) {
                SafeMemRead(posPtr + OFFSET_POS_X, cachedPosX);
                SafeMemRead(posPtr + OFFSET_POS_Y, cachedPosY);
                SafeMemRead(posPtr + OFFSET_POS_Z, cachedPosZ);
            }
            
            // 读取AimCPed Visual
            SafeMemRead(aimCPedPtr + OFFSET_VISUAL_X, cachedVisualX);
            SafeMemRead(aimCPedPtr + OFFSET_VISUAL_Y, cachedVisualY);
            SafeMemRead(aimCPedPtr + OFFSET_VISUAL_Z, cachedVisualZ);
        }
    }
    
	// 计算RGB彩色变换（保留每帧计算，以保持视觉效果流畅）
	float time = static_cast<float>(ImGui::GetTime());
	float r = sin(time * 1.0f) * 0.4f + 0.8f;
	float g = sin(time * 1.3f) * 0.4f + 0.8f;
	float b = sin(time * 1.7f) * 0.4f + 0.8f;
	
	// 确保颜色不会太暗，所有通道最小值为0.6
	r = std::max(r, 0.6f);
	g = std::max(g, 0.6f);
	b = std::max(b, 0.6f);
	
	// 确保颜色不会超过1.0
	r = std::min(r, 1.0f);
	g = std::min(g, 1.0f);
	b = std::min(b, 1.0f);
	
	ImVec4 rainbowColor = ImVec4(r, g, b, 1.0f);
	
	// 设置窗口属性
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_Always);
	
	// 渲染文字在最底层
	ImGui::Begin("FusionModeOverlay", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);
	
	// 服务器玩家信息标题
	ImGui::SetWindowFontScale(1.3f);
	ImGui::TextColored(rainbowColor, "服务器玩家信息");
	ImGui::SetWindowFontScale(1.0f);
	ImGui::Separator();
	
	// 使用缓存的玩家管理器数据
	if (cachedPlayerMgrValid) {
		ImGui::TextColored(rainbowColor, "最大对局人数: %d", cachedMaxPlayers);
		ImGui::TextColored(rainbowColor, "当前对局人数: %d", cachedCurrentPlayers);
	} else {
		ImGui::TextColored(rainbowColor, "CNetworkPlayerMgrPtr: 读取失败");
	}
	
	ImGui::Separator();
	
	// 瞄准对象信息
	ImGui::TextColored(rainbowColor, "瞄准对象信息");
	
	// 使用缓存的瞄准对象数据
	if (cachedHasAimTarget) {
		ImGui::TextColored(rainbowColor, "血量: %.1f", cachedHealth);
		ImGui::TextColored(rainbowColor, "防弹衣: %.1f", cachedArmor);
		ImGui::TextColored(rainbowColor, "移速: %.2f", cachedSpeed);
		
		if (cachedPlayerName[0] != '\0') {
			ImGui::TextColored(rainbowColor, "玩家Name: %s", cachedPlayerName);
		} else {
			ImGui::TextColored(rainbowColor, "玩家Name: 读取失败");
		}
		
		ImGui::TextColored(rainbowColor, "位置: (%.1f, %.1f, %.1f)", cachedPosX, cachedPosY, cachedPosZ);
		ImGui::TextColored(rainbowColor, "视觉位置: (%.1f, %.1f, %.1f)", cachedVisualX, cachedVisualY, cachedVisualZ);
	} else {
		ImGui::TextColored(rainbowColor, "未瞄准任何对象");
	}
	
	// 结束窗口
	ImGui::End();
}