#include "pch.h"
#include "ConsoleTheme.h"

void ConsoleTheme::Apply()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(0.0f, 0.0f);
    style.FramePadding = ImVec2(10.0f, 7.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 11.0f;
    style.WindowRounding = 6.0f;
    style.ChildRounding = 5.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 4.0f;
    style.CellPadding = ImVec2(10.0f, 8.0f);
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.92f, 0.95f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.49f, 0.55f, 0.63f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.047f, 0.063f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.052f, 0.068f, 0.089f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.065f, 0.083f, 0.108f, 0.99f);
    colors[ImGuiCol_Border] = ImVec4(0.16f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.086f, 0.110f, 0.145f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.17f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.14f, 0.27f, 0.39f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.045f, 0.059f, 0.077f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.055f, 0.073f, 0.095f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.31f, 0.64f, 0.93f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.57f, 0.83f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.38f, 0.70f, 0.98f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.10f, 0.14f, 0.19f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.13f, 0.23f, 0.32f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.15f, 0.34f, 0.49f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.09f, 0.17f, 0.24f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.12f, 0.25f, 0.36f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.15f, 0.35f, 0.51f, 1.00f);
    colors[ImGuiCol_Tab] = ImVec4(0.07f, 0.10f, 0.14f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.12f, 0.25f, 0.36f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.11f, 0.28f, 0.41f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.06f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.09f, 0.18f, 0.26f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.15f, 0.19f, 0.24f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.03f, 0.04f, 0.055f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.17f, 0.22f, 0.28f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.23f, 0.31f, 0.39f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.29f, 0.48f, 0.64f, 1.00f);
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.07f, 0.095f, 0.125f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.065f, 0.082f, 0.105f, 0.55f);
    colors[ImGuiCol_TableBorderStrong] = colors[ImGuiCol_Border];
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.12f, 0.15f, 0.19f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.31f, 0.64f, 0.93f, 1.00f);
}

void ConsoleTheme::SectionHeader(const char* title, const char* description)
{
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float cursorX = ImGui::GetCursorPosX();
    drawList->AddRectFilled(start, ImVec2(start.x + 3.0f, start.y + 18.0f), IM_COL32(79, 162, 236, 255), 2.0f);
    ImGui::SetCursorPosX(cursorX + 11.0f);
    ImGui::TextUnformatted(title);
    if (description && description[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", description);
    }
    ImGui::SetCursorPosX(cursorX);
    ImGui::Separator();
    ImGui::Spacing();
}

bool ConsoleTheme::ToggleRow(const char* id, const char* label, const char* description, bool* value)
{
    ImGui::PushID(id);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = description && description[0] != '\0' ? 50.0f : 38.0f;
    ImGui::InvisibleButton("##toggle_row", ImVec2(width, height));

    bool changed = false;
    if (ImGui::IsItemClicked()) {
        *value = !*value;
        changed = true;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec2 end(start.x + width, start.y + height);
    if (ImGui::IsItemHovered()) {
        drawList->AddRectFilled(start, end, IM_COL32(24, 34, 45, 220), 4.0f);
    }
    drawList->AddLine(ImVec2(start.x, end.y), end, IM_COL32(35, 44, 56, 180));

    const ImU32 labelColor = ImGui::GetColorU32(ImGuiCol_Text);
    const ImU32 descriptionColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    const float labelY = description && description[0] != '\0' ? start.y + 7.0f : start.y + 10.0f;
    drawList->AddText(ImVec2(start.x + 10.0f, labelY), labelColor, label);
    if (description && description[0] != '\0') {
        drawList->AddText(ImVec2(start.x + 10.0f, start.y + 27.0f), descriptionColor, description);
    }

    constexpr float trackWidth = 36.0f;
    constexpr float trackHeight = 20.0f;
    const ImVec2 trackMin(end.x - trackWidth - 10.0f, start.y + (height - trackHeight) * 0.5f);
    const ImVec2 trackMax(trackMin.x + trackWidth, trackMin.y + trackHeight);
    drawList->AddRectFilled(trackMin, trackMax,
        *value ? IM_COL32(61, 151, 226, 255) : IM_COL32(57, 68, 82, 255), trackHeight * 0.5f);
    const float knobX = *value ? trackMax.x - 10.0f : trackMin.x + 10.0f;
    drawList->AddCircleFilled(ImVec2(knobX, trackMin.y + 10.0f), 7.0f, IM_COL32(235, 241, 247, 255));

    ImGui::PopID();
    return changed;
}
