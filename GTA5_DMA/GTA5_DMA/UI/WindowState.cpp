#include "pch.h"

#include "WindowState.h"
#include "MyImGui.h"

#include <shlobj.h>

#include <cstdio>
#include <fstream>

std::string WindowState::IniPath()
{
    char path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path)))
        return std::string(path) + "\\GTA5_DMA\\window.ini";
    return "window.ini";
}

void WindowState::Load()
{
    std::ifstream in(IniPath());
    if (!in.is_open())
        return;

    std::string key;
    long long value = 0;
    while (in >> key >> value)
    {
        if (key == "x") X = static_cast<int>(value);
        else if (key == "y") Y = static_cast<int>(value);
        else if (key == "w") Width = static_cast<int>(value);
        else if (key == "h") Height = static_cast<int>(value);
        else if (key == "max") Maximized = (value != 0);
        else if (key == "sidebar_collapsed") SidebarCollapsed = (value != 0);
        else if (key == "quick_god") QuickGodMode = (value != 0);
        else if (key == "quick_wanted") QuickNoWanted = (value != 0);
        else if (key == "quick_invis") QuickInvisible = (value != 0);
        else if (key == "quick_noclip") QuickNoCollision = (value != 0);
        else if (key == "theme") ThemeIndex = static_cast<int>(value);
    }

    // 有效性钳制
    if (Width < 960) Width = 960;
    if (Height < 640) Height = 640;
    if (X != -1 && (X < -200 || X > 16000)) { X = -1; Y = -1; }
    if (Y != -1 && (Y < -200 || Y > 16000)) { X = -1; Y = -1; }
}

void WindowState::Save()
{
    // 关闭前同步刷新一次窗口几何（避免节流采样错过最后一次移动）
    extern HWND g_AppHwnd;
    if (g_AppHwnd)
    {
        WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
        if (::GetWindowPlacement(g_AppHwnd, &wp))
        {
            if (wp.showCmd != SW_SHOWMAXIMIZED && wp.showCmd != SW_SHOWMINIMIZED)
            {
                WindowState::X = wp.rcNormalPosition.left;
                WindowState::Y = wp.rcNormalPosition.top;
                WindowState::Width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
                WindowState::Height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
            }
            WindowState::Maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
        }
    }

    std::string dir = IniPath();
    const size_t slash = dir.find_last_of('\\');
    if (slash != std::string::npos)
        CreateDirectoryA(dir.substr(0, slash).c_str(), nullptr);

    std::ofstream out(dir);
    if (!out.is_open())
        return;

    out << "x " << X << "\n";
    out << "y " << Y << "\n";
    out << "w " << Width << "\n";
    out << "h " << Height << "\n";
    out << "max " << (Maximized ? 1 : 0) << "\n";
    out << "sidebar_collapsed " << (SidebarCollapsed ? 1 : 0) << "\n";
    out << "quick_god " << (QuickGodMode ? 1 : 0) << "\n";
    out << "quick_wanted " << (QuickNoWanted ? 1 : 0) << "\n";
    out << "quick_invis " << (QuickInvisible ? 1 : 0) << "\n";
    out << "quick_noclip " << (QuickNoCollision ? 1 : 0) << "\n";
    out << "theme " << ThemeIndex << "\n";
}
