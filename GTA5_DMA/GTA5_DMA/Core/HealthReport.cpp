// ============================================================================
// HealthReport.cpp —— 长寿体检实现
//
// 每一项都做**功能性实测**，并附上"坏了怎么办"。核心思想：
//   游戏更新（改偏移/改脚本索引/上新 DLC）后，用户只要跑一次 --health，
//   就能知道：哪一项坏了 → 改哪个文本文件 → 加哪一行 → 不用等我重编译。
// ============================================================================

#include "pch.h"

#include "HealthReport.h"

#include "BanCheck.h"
#include "DMA.h"
#include "DynamicOffsets.h"
#include "PhoneSilencer.h"
#include "PlayerList.h"
#include "ScriptGlobals.h"
#include "ScriptThreads.h"
#include "Tunables.h"
#include "VehicleList.h"
#include "VehicleNameOverrides.h"
#include "VehicleNames.h"

#include <fstream>
#include <windows.h>

namespace HealthReport
{
    namespace
    {
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

        bool FileExists(const std::string& p)
        {
            const DWORD a = GetFileAttributesA(p.c_str());
            return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
        }

        const char* kPatternsFile = "GTA5_DMA_patterns.txt";
        const char* kTunablesFile = "GTA5_DMA_tunables.txt";
        const char* kExtraNamesFile = "vehicle_names_extra.txt";
        const char* kBeDll = "BEServer_x64.dll";
    }  // namespace

    std::vector<Item> Run()
    {
        std::vector<Item> items;

        // 1) DMA 是否连上
        {
            Item it{};
            it.name = "DMA 连接";
            if (DMA::IsReady())
            {
                it.ok = true;
                char b[96];
                std::snprintf(b, sizeof(b), "已连接（PID %u，基址 0x%llX）", DMA::PID,
                              static_cast<unsigned long long>(DMA::BaseAddress));
                it.detail = b;
                it.fix = "-";
            }
            else
            {
                it.detail = "未连接";
                it.fix = "确认游戏在跑、FPGA/FTD3XX 驱动正常；常驻实例会独占设备（先关掉别的实例）";
            }
            items.push_back(it);
        }

        if (!DMA::IsReady())
            return items;

        // 2) 世界/本地玩家
        {
            Item it{};
            it.name = "世界与本地玩家";
            char b[128];
            std::snprintf(b, sizeof(b), "LocalPlayerAddress=0x%llX",
                          static_cast<unsigned long long>(DMA::LocalPlayerAddress));
            it.detail = b;
            if (DMA::LocalPlayerAddress)
            {
                it.ok = true;
                it.fix = "-";
            }
            else
            {
                it.fix = "多半不在线上战局；若在战局里仍为空 → 外部文件追加：WorldPtr = <新特征码>（见 " +
                         std::string(kPatternsFile) + "）";
            }
            items.push_back(it);
        }

        // 3) 脚本全局（抢劫/经济/GTA+ 等全依赖它）
        {
            Item it{};
            it.name = "脚本全局";
            const int total = ScriptGlobals::kSlotCount;
            const int got = ScriptGlobals::GetResolvedCount();
            char b[160];
            std::snprintf(b, sizeof(b), "已解析 %d/%d 条；拒绝写入 %d 次；自动重基线 %d 次", got, total,
                          ScriptGlobals::GetBlockedWriteCount(), ScriptGlobals::GetRebaselineCount());
            it.detail = b;
            if (got == total)
            {
                it.ok = true;
                it.fix = "-";
            }
            else if (got > 0)
            {
                it.warn = true;
                it.fix = "部分全局索引在本版本上对不上 → 外部文件里改 GlobalPtr 特征码，或进线上战局后重试";
            }
            else
            {
                it.fix = "全局块读不到 → 追加 GlobalPtr = <新特征码>；同时确认已在线上战局";
            }
            items.push_back(it);
        }

        // 4) tunables（防挂机踢出等）
        {
            Item it{};
            it.name = "Tunables（防踢等）";
            const int got = Tunables::GetResolvedCount();
            char b[160];
            std::snprintf(b, sizeof(b), "已解析 %d 条；被拒绝写入 %d 次", got, Tunables::GetBlockedWriteCount());
            it.detail = b;
            if (got > 0)
            {
                it.ok = true;
                it.fix = "-";
            }
            else
            {
                it.fix = "用 " + std::string(kTunablesFile) + "（格式：名字哈希 = 索引）补新表；"
                         "索引会随更新漂移，但名字哈希稳定，所以只需改这个文本文件";
            }
            items.push_back(it);
        }

        // 5) 玩家池
        {
            Item it{};
            it.name = "玩家池";
            const int n = PlayerList::GetPlayerCount();
            char b[96];
            std::snprintf(b, sizeof(b), "读到 %d 名玩家", n);
            it.detail = b;
            it.ok = n > 0;
            it.fix = n > 0 ? "-" : "不在线上战局时属正常；若在战局里仍为 0 → 追加 PedPoolPtr = <新特征码>";
            items.push_back(it);
        }

        // 6) 载具池 + 模型链（第27轮定死的两级链，最容易随版本失效）
        {
            Item it{};
            it.name = "载具池与模型链";
            VehicleList::RefreshVehicles();
            const auto snap = VehicleList::GetSnapshot();
            int named = 0;
            for (const auto& v : snap)
            {
                if (LookupVehicleNameEx(v.ModelHash))
                    ++named;
            }
            char b[192];
            std::snprintf(b, sizeof(b), "池内 %zu 辆；其中型号命中名表 %d 辆", snap.size(), named);
            it.detail = b;
            if (!snap.empty() && named > 0)
            {
                it.ok = true;
                it.fix = "-";
            }
            else if (!snap.empty())
            {
                it.warn = true;
                it.fix = "池能读但型号全不认 → 模型链偏移可能变了（当前用 载具+0x20 → +0x18）；"
                         "先把新偏移告诉我，或临时用 " + std::string(kExtraNamesFile) + " 补名";
            }
            else
            {
                it.fix = "载具池为空 → 追加 VehiclePoolPtr = <新特征码>";
            }
            items.push_back(it);
        }

        // 7) 脚本线程表（脚本局部写 / 叫车等功能的底座）
        {
            Item it{};
            it.name = "脚本线程表";
            ScriptThreads::Resolve();
            int idx = -1;
            char name[64] = {};
            const uint32_t n = ScriptThreads::Count();
            for (uint32_t i = 0; i < n; ++i)
            {
                ScriptThreads::ThreadInfo info{};
                if (!ScriptThreads::Get(i, info))
                    continue;
                if (info.hash == ScriptThreads::GetFreemodeHash())   // "freemode"（用现成 API，不写死）
                {
                    idx = static_cast<int>(i);
                    std::snprintf(name, sizeof(name), "%s", info.name);
                    break;
                }
            }
            char b[160];
            std::snprintf(b, sizeof(b), "共 %u 个线程；freemode %s", n, idx >= 0 ? "已找到" : "未找到");
            it.detail = b;
            if (idx >= 0)
            {
                it.ok = true;
                it.fix = "-";
            }
            else
            {
                it.warn = true;
                it.fix = "不在线上战局属正常；若在战局里也无 → 追加 LocalScriptsPtr = <新特征码>";
            }
            items.push_back(it);
        }

        // 8) 静音来电（纯脚本全局）
        {
            Item it{};
            it.name = "静音来电";
            const PhoneSilencer::Snapshot s = PhoneSilencer::Read();
            it.detail = s.ok ? "4 个全局全部可解析" : "全局地址解析不到";
            if (s.ok)
            {
                it.ok = true;
                it.fix = "-";
            }
            else
            {
                it.warn = true;
                it.fix = "不在线上战局属正常；若在战局里仍不可解析 → 电话相关全局索引变了（改 PhoneSilencer.cpp 的 4 个索引后重编译）";
            }
            items.push_back(it);
        }

        // 9) BE 封禁查询（依赖 BE 自己的库，与游戏版本无关）
        {
            Item it{};
            it.name = "BE 封禁查询";
            const std::string dll = ExeDir() + kBeDll;
            const bool hasDll = FileExists(dll);
            it.detail = hasDll ? "BEServer_x64.dll 存在" : "缺 BEServer_x64.dll";
            if (hasDll && BanCheck::Available())
            {
                it.ok = true;
                it.fix = "-";
            }
            else
            {
                it.fix = "把 BEServer_x64.dll / BEServer_x64.cfg 放到 exe 同目录（发布包里已含）";
            }
            items.push_back(it);
        }

        // 10) 字段偏移/索引外部覆盖表（结构体字段这类没法扫特征码，只能靠这个文件）
        {
            Item it{};
            it.name = "偏移覆盖表";
            DynamicOffsets::Load();
            char b[256];
            std::snprintf(b, sizeof(b), "%d 个键，其中外部文件改了 %d 个（%s）",
                          DynamicOffsets::LoadedKeyCount(), DynamicOffsets::OverriddenKeyCount(),
                          DynamicOffsets::FilePath());
            it.detail = b;
            it.ok = true;
            it.fix = "游戏更新后：直接改这个文件里的字段偏移/脚本索引，重启即生效（无需重编译）";
            items.push_back(it);
        }

        // 10) 外部覆盖文件（更新时的第一手工具）
        {
            Item it{};
            it.name = "外部覆盖文件";
            const std::string p = ExeDir() + kPatternsFile;
            const std::string t = ExeDir() + kTunablesFile;
            const std::string x = ExeDir() + kExtraNamesFile;
            char b[256];
            std::snprintf(b, sizeof(b), "%s=%s  %s=%s  %s=%s", kPatternsFile, FileExists(p) ? "有" : "无",
                          kTunablesFile, FileExists(t) ? "有" : "无", kExtraNamesFile, FileExists(x) ? "有" : "无");
            it.detail = b;
            it.warn = true;   // 有它们才好修；没有也不算坏
            it.fix = "游戏更新后优先改这三个文件（无需重编译）：特征码 / tunable 索引 / 新车名";
            items.push_back(it);
        }

        return items;
    }

    void Print(const std::vector<Item>& items)
    {
        std::printf("[health] 体检报告（✓正常 / ⚠可用但脆弱 / ✗需要处理）\n");
        int bad = 0, warn = 0;
        for (const auto& it : items)
        {
            const char* mark = it.ok ? "✓" : (it.warn ? "⚠" : "✗");
            if (!it.ok && !it.warn)
                ++bad;
            if (it.warn)
                ++warn;
            std::printf("[health] %s %-16s %s\n", mark, it.name.c_str(), it.detail.c_str());
            if (!it.ok && it.fix != "-")
                std::printf("[health]      └ 修法：%s\n", it.fix.c_str());
        }
        std::printf("[health] 汇总：%zu 项，✗%d 项，⚠%d 项\n", items.size(), bad, warn);
    }

    void AppendToDiagnostics()
    {
        const std::string path = ExeDir() + "GTA5_DMA_health.txt";
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f)
            return;
        const auto items = Run();
        f << "GTA5 DMA 体检报告\n";
        for (const auto& it : items)
        {
            f << (it.ok ? "[✓] " : (it.warn ? "[⚠] " : "[✗] ")) << it.name << " ： " << it.detail << "\n";
            if (!it.ok && it.fix != "-")
                f << "     修法：" << it.fix << "\n";
        }
    }
}
