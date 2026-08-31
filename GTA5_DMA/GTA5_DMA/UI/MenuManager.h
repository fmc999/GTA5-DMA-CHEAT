#pragma once

#include <vector>

// 页面类型。时间控制 / 任务分红已停用，枚举值保留以便恢复导航。
enum class MenuPage {
    MAIN,           // 主菜单（默认重定向到人物控制）
    PLAYER,         // 玩家功能
    WEAPON,         // 武器功能
    TELEPORT,       // 传送功能
    VEHICLE,        // 载具功能
    TIME,           // 已停用：时间控制
    HEIST_DIVIDEND, // 已停用：抢劫分红
    SETTINGS        // 设置
};

// 菜单状态管理器：只负责当前页面与历史，渲染布局由 ConsoleShell 负责，
// 各页面内容由 RenderXxxPageContent() 提供。
class MenuManager {
public:
    static MenuManager& GetInstance();

    MenuPage GetCurrentPage() const { return currentPage; }
    void SetCurrentPage(MenuPage page) { currentPage = page; }
    void SwitchToPage(MenuPage page);
    void GoBack();
    const std::vector<MenuPage>& GetPageHistory() const { return pageHistory; }

    // 渲染当前页面（入口，内部转给 ConsoleShell）
    void RenderCurrentPage();

    // 活动页面内容（渲染在 ConsoleShell 的工作区中）
    void RenderPlayerPageContent();
    void RenderWeaponPageContent();
    void RenderTeleportPageContent();
    void RenderVehiclePageContent();
    void RenderSettingsPageContent();

private:
    MenuManager() : currentPage(MenuPage::MAIN) {}

    MenuPage currentPage;
    std::vector<MenuPage> pageHistory;
};
