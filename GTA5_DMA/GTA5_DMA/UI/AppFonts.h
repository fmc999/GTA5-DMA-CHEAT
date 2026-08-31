#pragma once

#include "imgui.h"

// 全局字体句柄：由 MyImGui::Initialize 装载，UI 层各处引用。
namespace AppFonts
{
    inline ImFont* Regular = nullptr;
    inline ImFont* Bold    = nullptr;
}
