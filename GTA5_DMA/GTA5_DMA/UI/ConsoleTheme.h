#pragma once

#include "imgui.h"

// 控制台主题 ID。主题同时决定整体配色与共享控件的绘制风格。
enum class ConsoleThemeId
{
    Midnight,   // 午夜蓝（默认）
    Graphite,   // 石墨灰
    Ocean,      // 海洋青
    Crimson,    // 绯红
};

// 控制台视觉系统：主题管理 + 全部共享控件（行开关、区块标题、导航项、状态胶囊）。
class ConsoleTheme
{
public:
    /* 主题管理 */
    static void Apply();                          // 应用当前主题（样式 + 配色）
    static void SetTheme(ConsoleThemeId id);      // 切换主题并立即应用
    static ConsoleThemeId GetTheme();
    static const char* ThemeName(ConsoleThemeId id);
    static void RenderThemeSelector();            // 设置页使用的主题下拉框

    /* 主题色访问器（供布局代码取色） */
    static ImVec4 Accent();
    static ImVec4 Success();
    static ImVec4 Warning();
    static ImVec4 Danger();
    static ImVec4 Background();       // 主背景
    static ImVec4 BackgroundDeep();   // 页眉 / 状态栏深色背景

    /* 共享控件 */
    static void SectionHeader(const char* title, const char* description = nullptr);
    static bool ToggleRow(const char* id, const char* label, const char* description, bool* value);
    static bool NavItem(const char* label, bool selected);
    static bool NavItem(const char* label, bool selected, bool slim);   // slim: 图标模式
    static void StatPill(const char* label, const char* value, bool good);
    static void StatusDot(bool ok, const char* label);
};
