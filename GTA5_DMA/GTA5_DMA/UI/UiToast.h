#pragma once

#include <chrono>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Toast 通知系统：功能操作（击杀/传送/设置切换等）的即时视觉反馈。
// UI 线程调用 UiToast::Show()，ConsoleShell 每帧渲染浮动通知。
// ---------------------------------------------------------------------------

enum class ToastKind
{
    Info,     // 主题色
    Success,  // 绿
    Warning,  // 黄
    Danger    // 红
};

class UiToast
{
public:
    static constexpr size_t kMaxToasts = 5;
    static constexpr float kDuration = 3.2f;   // 秒

    struct Item
    {
        std::string text;
        ToastKind kind = ToastKind::Info;
        std::chrono::steady_clock::time_point born;
    };

    // 显示一条通知（线程安全，UI 线程调用）
    static void Show(const std::string& text, ToastKind kind = ToastKind::Info);

    // 渲染所有未过期通知（右上角堆叠，自上而下），并清理过期项
    static void Render();

private:
    static std::vector<Item>& Queue();
};
