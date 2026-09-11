#include "pch.h"

#include "UiToast.h"
#include "AppFonts.h"
#include "ConsoleTheme.h"

#include <imgui.h>

#include <algorithm>
#include <mutex>

namespace
{
ImU32 WithAlphaV(float r, float g, float b, float alpha)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, alpha));
}
}

std::vector<UiToast::Item>& UiToast::Queue()
{
    static std::vector<Item> queue;
    return queue;
}

void UiToast::Show(const std::string& text, ToastKind kind)
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    auto& queue = Queue();
    if (queue.size() >= kMaxToasts)
        queue.erase(queue.begin());
    queue.push_back(Item{ text, kind, std::chrono::steady_clock::now() });
}

void UiToast::Render()
{
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    auto& queue = Queue();

    const auto now = std::chrono::steady_clock::now();

    // 右上角固定区域（页面下方），从上往下堆叠，Portfolio #8 玻璃卡片样式
    const ImVec2 viewport = ImGui::GetMainViewport()->WorkSize;
    const float width = 320.0f;
    const float height = 46.0f;
    const float gap = 8.0f;
    float y = 26.0f;
    const float rightX = viewport.x - 26.0f;

    for (auto it = queue.begin(); it != queue.end();)
    {
        const float elapsed = std::chrono::duration<float>(now - it->born).count();
        if (elapsed >= kDuration)
        {
            it = queue.erase(it);
            continue;
        }

        // 淡入淡出 + 轻微右侧滑入
        float alpha = 1.0f;
        float slide = 0.0f;
        if (elapsed < 0.18f)
        {
            alpha = elapsed / 0.18f;
            slide = (1.0f - alpha) * 24.0f;
        }
        else if (elapsed > kDuration - 0.45f)
        {
            alpha = (kDuration - elapsed) / 0.45f;
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        ImVec4 kindColor = ConsoleTheme::Accent();
        UiIcon icon = UiIcon::Dot;
        switch (it->kind)
        {
        case ToastKind::Success: kindColor = ConsoleTheme::Success(); icon = UiIcon::Check; break;
        case ToastKind::Warning: kindColor = ConsoleTheme::Warning(); icon = UiIcon::Zap;   break;
        case ToastKind::Danger:  kindColor = ConsoleTheme::Danger();  icon = UiIcon::Close; break;
        default: break;
        }

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        const ImVec2 min(rightX - width + slide, y);
        const ImVec2 max(rightX + slide, y + height);

        // 玻璃卡片 + 左侧状态条
        drawList->AddRectFilled(min, max, WithAlphaV(0.05f, 0.05f, 0.06f, 0.86f * alpha), 10.0f);
        drawList->AddRect(min, max, ConsoleTheme::U32(ConsoleTheme::Ink(0.10f * alpha), 1.0f), 10.0f, ImDrawFlags_RoundCornersAll, 1.0f);
        drawList->AddRectFilled(ImVec2(min.x, min.y + 8.0f), ImVec2(min.x + 3.0f, max.y - 8.0f),
                                ConsoleTheme::U32(kindColor, alpha), 1.5f);

        // 图标底板
        const ImVec2 tileMin(min.x + 16.0f, min.y + (height - 26.0f) * 0.5f);
        const ImVec2 tileMax(tileMin.x + 26.0f, tileMin.y + 26.0f);
        drawList->AddRectFilled(tileMin, tileMax, ConsoleTheme::U32(kindColor, 0.18f * alpha), 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.0f);
        ConsoleTheme::Icon(icon, ImVec2((tileMin.x + tileMax.x) * 0.5f, (tileMin.y + tileMax.y) * 0.5f), 14.0f,
                           ConsoleTheme::U32(kindColor, alpha), 2.0f);
        ImGui::PopStyleVar();

        // 文本（超宽截断）
        std::string display = it->text;
        while (!display.empty() && ImGui::CalcTextSize(display.c_str()).x > width - 74.0f)
            display.pop_back();
        if (display.size() != it->text.size())
            display += "…";

        ImFont* font = AppFonts::Bold ? AppFonts::Bold : AppFonts::Regular;
        const float size = font ? font->FontSize : ImGui::GetFontSize();
        ConsoleTheme::Text(drawList, font, ImVec2(tileMax.x + 12.0f, min.y + (height - size) * 0.5f),
                           WithAlphaV(0.94f, 0.95f, 0.97f, alpha), display.c_str());

        // 底部进度条（剩余时间）
        const float remaining = 1.0f - elapsed / kDuration;
        if (remaining > 0.0f)
        {
            drawList->AddRectFilled(ImVec2(min.x + 12.0f, max.y - 3.0f),
                                    ImVec2(min.x + 12.0f + (width - 24.0f) * remaining, max.y - 2.0f),
                                    ConsoleTheme::U32(kindColor, 0.55f * alpha), 1.0f);
        }

        y += height + gap;
        ++it;
    }
}
