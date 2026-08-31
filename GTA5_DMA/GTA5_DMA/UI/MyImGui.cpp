#include "pch.h"

#include "MyImGui.h"
#include "AppFonts.h"
#include "AppRuntime.h"
#include "ConsoleTheme.h"
#include "InputManager.h"
#include "MyMenu.h"
#include "MenuManager.h"
#include "Teleport.h"
#include "UiToast.h"
#include "WindowState.h"

#include <d3d11.h>
#include <dwmapi.h>

HWND g_AppHwnd = nullptr;   // 窗口句柄全局（WindowState 持久化用）

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")

bool MyImGui::CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
        featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // 无硬件设备时回退到 WARP 软件渲染
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags,
            featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void MyImGui::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void MyImGui::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void MyImGui::CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI MyImGui::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // 排队等待主循环处理
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // 禁用 ALT 应用菜单
            return 0;
        break;
    case WM_GETMINMAXINFO:
    {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 980;
        mmi->ptMinTrackSize.y = 640;
        return 0;
    }
    case WM_CLOSE:
        WindowState::Save();   // 窗口销毁前持久化几何
        ::DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

bool MyImGui::Initialize()
{
    // 创建带边框的标准窗口（可拖动、可缩放），居中显示
    ImGui_ImplWin32_EnableDpiAwareness();
    wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"GTA5_DMA_Tool", nullptr };
    ::RegisterClassExW(&wc);

    WindowState::Load();

    const int windowWidth = WindowState::Width;
    const int windowHeight = WindowState::Height;
    int posX = (GetSystemMetrics(SM_CXSCREEN) - windowWidth) / 2;
    int posY = (GetSystemMetrics(SM_CYSCREEN) - windowHeight) / 2;
    if (WindowState::X != -1 && WindowState::Y != -1)
    {
        posX = WindowState::X;
        posY = WindowState::Y;
    }
    hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"GTA5 DMA 控制台", WS_OVERLAPPEDWINDOW,
        posX, posY, windowWidth, windowHeight, nullptr, nullptr, wc.hInstance, nullptr);

    g_AppHwnd = hwnd;
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 0;
    }

    // 标题栏暗色模式（Win10 1809+ 使用属性 20，更早版本回退 19）
    BOOL darkMode = TRUE;
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode))))
        DwmSetWindowAttribute(hwnd, 19, &darkMode, sizeof(darkMode));

    ::ShowWindow(hwnd, WindowState::Maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    ::UpdateWindow(hwnd);

    // ImGui 上下文
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    // 注：不启用 NavEnableKeyboard，避免占用全局按键（热键需要穿透）

    // 字体：常规 + 粗体，中文全字形
    const ImWchar* glyphRanges = io.Fonts->GetGlyphRangesChineseFull();
    const char* regularPaths[] = {
        "C:/Windows/Fonts/msyh.ttc",    // 微软雅黑
        "C:/Windows/Fonts/simhei.ttf",  // 黑体
        "C:/Windows/Fonts/simsun.ttc",  // 宋体
    };
    const char* boldPaths[] = {
        "C:/Windows/Fonts/msyhbd.ttc",  // 微软雅黑 粗体
        "C:/Windows/Fonts/simhei.ttf",
    };
    for (const char* path : regularPaths) {
        AppFonts::Regular = io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr, glyphRanges);
        if (AppFonts::Regular) break;
    }
    if (!AppFonts::Regular)
        AppFonts::Regular = io.Fonts->AddFontDefault();
    for (const char* path : boldPaths) {
        AppFonts::Bold = io.Fonts->AddFontFromFileTTF(path, 18.0f, nullptr, glyphRanges);
        if (AppFonts::Bold) break;
    }
    if (!AppFonts::Bold)
        AppFonts::Bold = AppFonts::Regular;

    // 主题 + 后端（恢复上次主题）
    ConsoleTheme::SetTheme(static_cast<ConsoleThemeId>(WindowState::ThemeIndex));
    ConsoleTheme::Apply();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    io.Fonts->Build();

    return 1;
}

bool MyImGui::Close()
{
    WindowState::Save();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return 1;
}

bool MyImGui::OnFrame()
{
    // 消息泵
    MSG msg;
    while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
        if (msg.message == WM_QUIT)
            AppRuntime::RequestStop();
    }
    if (!AppRuntime::IsRunning())
        return 0;

    // 最小化 / 锁屏时不渲染
    if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
    {
        ::Sleep(10);
        return 1;
    }
    g_SwapChainOccluded = false;

    // 窗口尺寸变化（不在 WM_SIZE 中直接处理）
    if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
    {
        CleanupRenderTarget();
        g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
        g_ResizeWidth = g_ResizeHeight = 0;
        CreateRenderTarget();
    }

    // 新帧
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ---- 全局热键（本地 + 目标主机） ----
    static bool insertWasDown = false;
    const bool insertIsDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    const bool targetInsertPressed = g_inputManager.IsKeyPressed(VK_INSERT);
    if ((insertIsDown && !insertWasDown) || targetInsertPressed) {
        MyMenu::bMenuVisible = !MyMenu::bMenuVisible;
    }
    insertWasDown = insertIsDown;

    static bool f5WasDown = false;
    const bool f5IsDown = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
    const bool targetF5Pressed = g_inputManager.IsKeyPressed(VK_F5);
    if ((f5IsDown && !f5WasDown) || targetF5Pressed) {
        Teleport::RequestWaypointTeleport();
    }
    f5WasDown = f5IsDown;

    static bool f6WasDown = false;
    const bool f6IsDown = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    const bool targetF6Pressed = g_inputManager.IsKeyPressed(VK_F6);
    if ((f6IsDown && !f6WasDown) || targetF6Pressed) {
        Teleport::RequestObjectiveTeleport();
    }
    f6WasDown = f6IsDown;

    // ---- 菜单渲染 ----
    if (MyMenu::bMenuVisible) {
        MenuManager::GetInstance().RenderCurrentPage();
    }

    // ---- 窗口几何持久化（每帧采样，开销可忽略）----
    {
        {
            WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
            if (::GetWindowPlacement(hwnd, &wp))
            {
                if (wp.showCmd == SW_SHOWMAXIMIZED || wp.showCmd == SW_SHOWMINIMIZED)
                {
                    WindowState::Maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
                }
                else
                {
                    WindowState::Maximized = false;
                    WindowState::X = wp.rcNormalPosition.left;
                    WindowState::Y = wp.rcNormalPosition.top;
                    WindowState::Width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
                    WindowState::Height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
                    WindowState::ThemeIndex = static_cast<int>(ConsoleTheme::GetTheme());
                }
            }
        }
    }

    // ---- 呈现 ----
    ImGui::Render();
    const ImVec4 bg = ConsoleTheme::Background();
    const float clearColor[4] = { bg.x, bg.y, bg.z, 1.0f };
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    const HRESULT hr = g_pSwapChain->Present(1, 0); // 垂直同步
    g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

    return 1;
}
