#include "pch.h"

#include "VehicleNameOverrides.h"

#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

// ============================================================================
// 运行时补充载具名（免重编译）
//   文件：<exe 同目录>/vehicle_names_extra.txt
//   格式：模型名 = 显示名[（分类）]   （# 开头注释；空行忽略）
//   匹配：模型名 → joaat → 哈希（与游戏一致；已用 velenogt=0xC5ADF0C2 实测核对）
// ============================================================================
namespace
{
    std::once_flag g_loadOnce;
    std::unordered_map<uint32_t, std::string> g_display;   // hash → 显示名
    std::unordered_map<uint32_t, std::string> g_model;     // hash → 模型名
    std::unordered_map<uint32_t, std::string> g_kind;      // hash → 分类
    std::vector<VehicleNameEntry> g_entries;               // 稳定存储（返回指针用）
    int g_loaded = 0;

    uint32_t Joaat(const std::string& s)
    {
        uint32_t h = 0;
        for (unsigned char c : s)
        {
            const uint8_t lc = static_cast<uint8_t>((c >= 'A' && c <= 'Z') ? (c + 32) : c);
            h += lc;
            h += h << 10;
            h ^= h >> 6;
        }
        h += h << 3;
        h ^= h >> 11;
        h += h << 15;
        return h;
    }

    std::string Trim(const std::string& s)
    {
        const std::size_t a = s.find_first_not_of(" \t\r\n");
        if (a == std::string::npos)
            return "";
        const std::size_t b = s.find_last_not_of(" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    std::string ExeDir()
    {
        char buf[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, buf, MAX_PATH);
        std::string p(buf);
        const std::size_t slash = p.find_last_of("\\/");
        return (slash == std::string::npos) ? std::string(".") : p.substr(0, slash + 1);
    }

    void LoadOnce()
    {
        std::call_once(g_loadOnce, []() {
            const std::string path = ExeDir() + "vehicle_names_extra.txt";
            std::ifstream in(path);
            if (!in)
            {
                std::println("[VehicleNames] 无补充名表（可选）：{}", path);
                return;
            }

            std::string line;
            while (std::getline(in, line))
            {
                const std::string t = Trim(line);
                if (t.empty() || t[0] == '#')
                    continue;

                const std::size_t eq = t.find('=');
                if (eq == std::string::npos)
                    continue;

                const std::string model = Trim(t.substr(0, eq));
                std::string disp = Trim(t.substr(eq + 1));
                if (model.empty() || disp.empty())
                    continue;

                std::string kind = "补充";
                const std::size_t lp = disp.find('(');
                if (lp != std::string::npos && disp.back() == ')')
                {
                    kind = disp.substr(lp + 1, disp.size() - lp - 2);
                    disp = Trim(disp.substr(0, lp));
                }

                const uint32_t h = Joaat(model);
                g_model[h] = model;
                g_display[h] = disp;
                g_kind[h] = kind;
            }

            g_entries.reserve(g_display.size());
            for (const auto& kv : g_display)
                g_entries.push_back(VehicleNameEntry{ kv.first, g_model[kv.first].c_str(),
                                                      kv.second.c_str(), g_kind[kv.first].c_str() });
            g_loaded = static_cast<int>(g_entries.size());
            std::println("[VehicleNames] 补充名表已加载 {} 条：{}", g_loaded, path);
        });
    }
}

const VehicleNameEntry* LookupVehicleNameEx(uint32_t hash)
{
    LoadOnce();

    for (const VehicleNameEntry& e : g_entries)
    {
        if (e.hash == hash)
            return &e;
    }
    return LookupVehicleName(hash);
}

int GetExtraVehicleNameCount()
{
    LoadOnce();
    return g_loaded;
}
