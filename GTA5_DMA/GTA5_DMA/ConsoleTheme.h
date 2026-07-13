#pragma once

class ConsoleTheme
{
public:
    static void Apply();
    static void SectionHeader(const char* title, const char* description = nullptr);
    static bool ToggleRow(const char* id, const char* label, const char* description, bool* value);
};
