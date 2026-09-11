#pragma once

#include "imgui.h"

// 全局字体句柄：由 MyImGui::Initialize 装载，UI 层各处引用。
// 三级排版：Small 分节/说明、Regular 正文、Bold 强调、Title 大标题、Logo 字标。
namespace AppFonts
{
    inline ImFont* Small   = nullptr;   // 13-14px，分节标题 / 状态栏
    inline ImFont* Regular = nullptr;   // 正文
    inline ImFont* Bold    = nullptr;   // 强调 / 数值
    inline ImFont* Title   = nullptr;   // 页标题 / 指标数值
    inline ImFont* Logo    = nullptr;   // 侧栏字标（Zen Dots，仅拉丁）
}
