#pragma once

// ============================================================================
// Esp —— 方框透视（纯 DMA 实现：不注入、不调游戏函数、不另开窗口）
//
// 与参考项目（YimMenuV2 frontend/ESP.cpp）的差别：
//   · Yim 调 GRAPHICS::GET_SCREEN_COORD_FROM_WORLD_COORD 拿屏幕坐标，并在游戏进程内用 ImGui 画；
//   · 本实现只读内存：自己定位游戏的「视投影矩阵」→ 本进程算投影 → 画在自己的窗口画布上。
// ============================================================================

#include <atomic>
#include <cstdint>
#include <string>

struct EspMatrix
{
    uintptr_t Address = 0;
    float     M[16] = {};
};

class Esp
{
public:
    // 界面开关（UI 线程写、绘制线程读）
    static inline std::atomic<bool>  bEnable{ false };
    static inline std::atomic<bool>  bBox{ true };
    static inline std::atomic<bool>  bName{ true };
    static inline std::atomic<bool>  bDistance{ true };
    static inline std::atomic<bool>  bHealth{ true };
    static inline std::atomic<bool>  bIncludeLocal{ false };
    static inline std::atomic<float> MaxDistance{ 500.0f };
    static inline std::atomic<float> BoxScale{ 1.0f };   // 方框高度系数（1.0 = 1.85 米）

    // 每帧绘制（画在窗口画布上；由 UI 每帧调用一次）
    static void RenderOverlay();

    // 矩阵定位（后台线程；需要战局里 ≥2 名玩家来验证）
    static void RequestLocate();
    static bool IsLocating();
    static bool GetMatrix(EspMatrix& out);
    static std::string GetMatrixStatus();

    // 世界 → 屏幕（游戏分辨率坐标系）
    static bool WorldToScreen(float x, float y, float z, float screenW, float screenH, float& outX, float& outY);

    // 诊断入口
    static int ProbeMatrix(int maxWindows = 24);   // --esp-probe [窗口数]
    static int ProbeCamera();                      // --esp-cam
};
