#pragma once

// 菜单可见性与渲染入口。布局由 ConsoleShell 完成，页面状态由 MenuManager 管理。
class MyMenu
{
public:
    static bool Render();

    static inline bool bMenuVisible = true;
};
