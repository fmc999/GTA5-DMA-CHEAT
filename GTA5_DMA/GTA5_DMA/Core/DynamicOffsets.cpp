// ============================================================================
// DynamicOffsets.cpp —— 外部文件驱动的字段偏移 / 脚本索引
//
// 文件格式（GTA5_DMA_offsets.txt，exe 同目录）：
//     # 注释
//     PedVehiclePtr = 0xD10          ← 十六进制可带 0x，也可不带
//     PhoneCallState = 23040         ← 十进制也行
// 解析规则：按 '=' 切分，键名大小写不敏感，值支持 0x 前缀；解析失败的行忽略并计数。
// 文件不存在时自动生成一份**带注释的当前实测值**，方便用户直接改。
// ============================================================================

#include "pch.h"

#include "DynamicOffsets.h"

#include <windows.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <string>
#include <unordered_map>

namespace DynamicOffsets
{
    namespace
    {
        // 内置默认值 = 当前实测通过的一套（游戏更新后主要由外部文件覆盖）
        struct Default
        {
            const char* key;
            uint64_t    value;
            const char* note;
        };

        const Default kDefaults[] = {
            // ---- 实体结构字段（第26~27轮实测定死）----
            { "PedVehiclePtr",        0xD10, "Ped → 所在载具指针（人不在车上时为 0）" },
            { "VehicleModelInfo",     0x20,  "载具(fwEntity) → m_ModelInfo 指针" },
            { "ModelInfoHash",        0x18,  "CBaseModelInfo → 模型哈希(u32)" },
            { "VehicleHealth",        0x280, "CVehicle → 车身血量(float)" },
            { "VehicleEngineHealth",  0x910, "CVehicle → 引擎血量(float)" },
            { "VehicleGodBits",       0x0,   "CVehicle → 无敌位（0 = 未启用该功能）" },
            { "PedGodFlags",          0x0,   "PED → 无敌标志字（0 = 用 offsetof 里的编译期值）" },
            { "PedHealth",            0x0,   "PED → 血量（0 = 用 offsetof 里的编译期值）" },
            // ---- 脚本全局索引（纯脚本类功能用）----
            { "PhoneCallState",       23040, "静音来电：电话状态（写 6 = 静音）" },
            { "PhoneCallInProgress",  23046, "静音来电：通话进行中" },
            { "PhoneCallIncoming",    23050, "静音来电：有来电" },
            { "PhoneCaller",          8818,  "静音来电：来电角色 ID" },
        };

        std::unordered_map<std::string, uint64_t> g_values;   // 键（小写）→ 值
        std::string g_path;
        bool        g_loaded = false;
        int         g_overrides = 0;

        std::string Lower(const std::string& s)
        {
            std::string o = s;
            for (char& c : o)
                c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            return o;
        }

        std::string Trim(const std::string& s)
        {
            size_t a = 0, b = s.size();
            while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n'))
                ++a;
            while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
                --b;
            return s.substr(a, b - a);
        }

        bool ParseU64(const std::string& s, uint64_t* out)
        {
            const std::string t = Trim(s);
            if (t.empty())
                return false;
            char* end = nullptr;
            const uint64_t v = std::strtoull(t.c_str(), &end, 0);   // base 0：自动识别 0x
            if (end == t.c_str())
                return false;
            *out = v;
            return true;
        }

        std::string ExeDir()
        {
            char exePath[MAX_PATH]{};
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            std::string dir(exePath);
            const size_t slash = dir.find_last_of("\\/");
            if (slash != std::string::npos)
                dir = dir.substr(0, slash + 1);
            return dir;
        }

        void SeedDefaults()
        {
            for (const Default& d : kDefaults)
                g_values[Lower(d.key)] = d.value;
        }

        // 文件不存在 → 生成一份带注释的模板（值 = 当前默认值）
        void WriteTemplate()
        {
            std::ofstream f(g_path, std::ios::binary | std::ios::trunc);
            if (!f)
                return;
            f << "# ============================================================================\n";
            f << "#  GTA5 DMA —— 偏移/索引外部覆盖表（改完保存 → 重启工具即生效，无需重编译）\n";
            f << "#\n";
            f << "#  游戏更新导致功能失效时，优先改这里：把新值填进去即可。\n";
            f << "#  值支持 0x 前缀（十六进制）或十进制。左边键名不要改。\n";
            f << "#  留空 / 删掉某行 = 用内置默认值。\n";
            f << "# ============================================================================\n\n";
            for (const Default& d : kDefaults)
            {
                char line[256];
                std::snprintf(line, sizeof(line), "%-22s = 0x%llX        # %s\n", d.key,
                              static_cast<unsigned long long>(d.value), d.note);
                f << line;
            }
            f << "\n# 提示：跑一次 `GTA5_DMA.exe --health` 可以看到每一项是否正常，以及该改哪里。\n";
        }
    }  // namespace

    bool Load()
    {
        if (g_loaded)
            return true;
        g_loaded = true;

        g_path = ExeDir() + "GTA5_DMA_offsets.txt";
        SeedDefaults();

        std::ifstream f(g_path, std::ios::binary);
        if (!f)
        {
            WriteTemplate();   // 首次运行：铺一份带注释的模板，方便以后改
            return true;
        }

        std::string line;
        while (std::getline(f, line))
        {
            const std::string t = Trim(line);
            if (t.empty() || t[0] == '#' || t[0] == ';')
                continue;
            const size_t eq = t.find('=');
            if (eq == std::string::npos)
                continue;
            std::string key = Trim(t.substr(0, eq));
            std::string val = t.substr(eq + 1);
            // 去掉行尾注释
            const size_t hash = val.find('#');
            if (hash != std::string::npos)
                val = val.substr(0, hash);

            uint64_t v = 0;
            if (!ParseU64(val, &v))
                continue;

            const std::string lk = Lower(key);
            if (g_values.count(lk) && g_values[lk] != v)
                ++g_overrides;
            g_values[lk] = v;
        }
        return true;
    }

    uint64_t Get(const char* key, uint64_t def)
    {
        Load();
        const auto it = g_values.find(Lower(key ? key : ""));
        if (it == g_values.end())
            return def;
        return it->second;
    }

    uintptr_t GetPtr(const char* key, uintptr_t def)
    {
        return static_cast<uintptr_t>(Get(key, def));
    }

    int32_t GetI32(const char* key, int32_t def)
    {
        return static_cast<int32_t>(Get(key, static_cast<uint64_t>(def)));
    }

    const char* FilePath()
    {
        Load();
        return g_path.c_str();
    }

    int LoadedKeyCount()
    {
        Load();
        return static_cast<int>(g_values.size());
    }

    int OverriddenKeyCount()
    {
        Load();
        return g_overrides;
    }
}
