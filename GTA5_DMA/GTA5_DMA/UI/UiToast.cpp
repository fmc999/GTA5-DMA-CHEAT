#include "pch.h"

#include "UiToast.h"
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

    // 右上角固定区域（页眉下方），从上往下堆叠
    const ImVec2 viewport = ImGui::GetMainViewport()->WorkSize;
    const float width = 300.0f;
    const float height = 40.0f;
    const float gap = 8.0f;
    float y = 64.0f;
    const float rightX = viewport.x - 16.0f;

    for (auto it = queue.begin(); it != queue.end();)
    {
        const float elapsed = std::chrono::duration<float>(now - it->born).count();
        if (elapsed >= kDuration)
        {
            it = queue.erase(it);
            continue;
        }

        // 淡入淡出
        float alpha = 1.0f;
        if (elapsed < 0.15f)
            alpha = elapsed / 0.15f;
        else if (elapsed > kDuration - 0.4f)
            alpha = (kDuration - elapsed) / 0.4f;
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        ImVec4 kindColor = ConsoleTheme::Accent();
        switch (it->kind)
        {
        case ToastKind::Success: kindColor = ConsoleTheme::Success(); break;
        case ToastKind::Warning: kindColor = ConsoleTheme::Warning(); break;
        case ToastKind::Danger:  kindColor = ConsoleTheme::Danger();  break;
        default: break;
        }

        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        const ImVec2 min(rightX - width, y);
        const ImVec2 max(rightX, y + height);
        const ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.05f, 0.06f, 0.08f, 0.94f * alpha));
        drawList->AddRectFilled(min, max, bgColor, 8.0f);
        drawList->AddRectFilled(ImVec2(min.x, min.y + 6.0f), ImVec2(min.x + 3.0f, max.y - 6.0f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(kindColor.x, kindColor.y, kindColor.z, alpha)), 1.5f);

        const ImVec2 textSize = ImGui::CalcTextSize(it->text.c_str());
        // 文本超宽截断（显示省略）
        std::string display = it->text;
        while (!display.empty() && ImGui::CalcTextSize(display.c_str()).x > width - 34.0f)
            display.pop_back();
        if (display.size() != it->text.size())
            display += "…";
        drawList->AddText(ImVec2(min.x + 16.0f, min.y + (height - textSize.y) * 0.5f),
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.92f, 0.94f, 0.96f, alpha)), display.c_str());

        y += height + gap;
        ++it;
    }
}
