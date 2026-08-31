#pragma once

#include <d3d11.h>

// DirectX 11 + Win32 平台层：创建宿主窗口、D3D 设备与 ImGui 上下文，驱动渲染主循环。
class MyImGui
{
public:
    static bool Initialize();
    static bool Close();
    static bool OnFrame();

private:
    static bool CreateDeviceD3D(HWND hWnd);
    static void CleanupDeviceD3D();
    static void CreateRenderTarget();
    static void CleanupRenderTarget();
    static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

    static inline ID3D11Device* g_pd3dDevice = nullptr;
    static inline ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
    static inline IDXGISwapChain* g_pSwapChain = nullptr;
    static inline ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
    static inline HWND hwnd = 0;
    static inline WNDCLASSEX wc = { 0 };
    static inline bool g_SwapChainOccluded = false;
    static inline UINT g_ResizeWidth = 0, g_ResizeHeight = 0;
};
