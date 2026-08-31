#pragma once

#include <cstdint>
#include <string>

// ---------------------------------------------------------------------------
// 窗口状态持久化：窗口位置/大小 + 功能开关状态，存到 ini（%LOCALAPPDATA%）。
// 程序退出时保存，启动时恢复。开关持久化由各功能模块的 Save/Load 钩子接入。
// ---------------------------------------------------------------------------

class WindowState
{
public:
    // 启动时载入（文件不存在则用默认值）
    static void Load();

    // 退出时保存
    static void Save();

    // ---- 窗口几何 ----
    static inline int  X = -1, Y = -1;          // -1 = 居中
    static inline int  Width = 1280, Height = 820;
    static inline bool Maximized = false;

    // ---- 布局选项 ----
    static inline bool SidebarCollapsed = false; // 折叠侧边栏（扩大工作区）

    // ---- 常用功能状态（快速控制区）----
    static inline bool QuickGodMode = false;
    static inline bool QuickNoWanted = false;
    static inline bool QuickInvisible = false;
    static inline bool QuickNoCollision = false;

    // ---- 主题 ----
    static inline int ThemeIndex = 0;

private:
    static std::string IniPath();
};
