// ============================================================================
// BanCheck.cpp —— BattlEye 封禁查询（自动队列 + 持久缓存）
//
// 结构与调用序列完全对齐 BEServer_x64.dll 的服务端 API（api_level = 1）：
//   Init(1, &user_data, &api)
//   api.add_player(peerid, ip = -1, port = 0, name, false)
//   api.assign_guid(peerid, base64(rid))          // GUID = base64(RID 字符串)
//   api.assign_guid_verified(peerid, base64(rid))
//   api.set_player_state(peerid, 1)               // 1 = 请求校验
//   loop { api.run(); }  ← 被封禁时 BE 回调 kick_player(peerid, reason)
//
// 自动模式：界面每帧把战局里的 RID 交给 AutoQuery()，本模块串行查询
// （同一时刻只查一个，两个查询之间留间隔），结果落盘 be_bans_cache.json，
// 24 小时内的结果直接复用，不重复问 BE。
// ============================================================================

#include "pch.h"

#include "BanCheck.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace BanCheck
{
    namespace
    {
        // ------------------------------------------------------------------
        // BEServer_x64.dll 的回调表（布局照抄自 BattlEye 服务端 API）
        // ------------------------------------------------------------------
        struct BattlEyeUserData
        {
            using log_func_t = void(*)(const char* msg, int level);
            using kick_player_t = void(*)(uint64_t id, const char* reason);
            using send_message_t = void(*)(uint64_t id, const void* msg, uint32_t size);

            const char*     game_name;
            log_func_t      log_func;
            kick_player_t   kick_player;
            send_message_t  send_message;
            void*           unk;
        };

        struct BattlEyeApi
        {
            using shutdown_t = void(*)();
            using run_t = bool(*)();
            using run_command_t = void(*)(const char* command);
            using add_player_t = void(*)(uint64_t id, uint32_t ip_addr, uint16_t port, const char* name, char unused);
            using set_player_state_t = void(*)(uint64_t id, int reason);
            using assign_guid_t = void(*)(uint64_t id, const void* guid, uint32_t size);
            using receive_message_t = void(*)(uint64_t id, const void* data, uint32_t size);

            shutdown_t          shutdown;
            run_t               run;
            run_command_t       run_command;
            add_player_t        add_player;
            set_player_state_t  set_player_state;
            assign_guid_t       assign_guid;
            assign_guid_t       assign_guid_verified;
            receive_message_t   receive_message;
        };

        using init_t = bool(*)(int api_level, BattlEyeUserData* data, BattlEyeApi* api);

        constexpr uint64_t kPeerId = 1337;
        constexpr int      kTimeoutMs = 6000;      // 单个查询等待上限（实测封禁答复 1.8~3.3 秒，留 2 倍余量）
        constexpr int      kGapMs = 600;           // 两个查询之间的间隔（别把 BE 问爆）
        constexpr uint64_t kFreshSec = 24 * 3600;  // 缓存有效期：24 小时

        HMODULE         g_dll = nullptr;
        init_t          g_init = nullptr;
        BattlEyeApi     g_api{};
        std::string     g_reason;
        std::string     g_error;
        std::string     g_pendingReason;
        long long       g_rid = 0;
        State           g_state = State::Idle;
        uint64_t        g_startedAt = 0;
        uint64_t        g_lastFinish = 0;
        std::vector<Cached>   g_cache;
        std::deque<long long> g_queue;   // 待查询 RID（去重）
        bool            g_cacheLoaded = false;
        std::string     g_cachePath;
        uint64_t        g_queriesDone = 0;

        uint64_t NowMs()
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
        }

        uint64_t UnixNow()
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(duration_cast<seconds>(system_clock::now().time_since_epoch()).count());
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

        const std::string& CachePath()
        {
            if (g_cachePath.empty())
                g_cachePath = ExeDir() + "be_bans_cache.json";
            return g_cachePath;
        }

        std::string Base64(const std::string& in)
        {
            static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string out;
            out.reserve(((in.size() + 2) / 3) * 4);
            size_t i = 0;
            while (i + 2 < in.size())
            {
                const uint32_t v = (uint8_t)in[i] << 16 | (uint8_t)in[i + 1] << 8 | (uint8_t)in[i + 2];
                out.push_back(tbl[(v >> 18) & 63]); out.push_back(tbl[(v >> 12) & 63]);
                out.push_back(tbl[(v >> 6) & 63]);  out.push_back(tbl[v & 63]);
                i += 3;
            }
            if (i < in.size())
            {
                uint32_t v = (uint8_t)in[i] << 16;
                const bool two = (i + 1) < in.size();
                if (two) v |= (uint8_t)in[i + 1] << 8;
                out.push_back(tbl[(v >> 18) & 63]);
                out.push_back(tbl[(v >> 12) & 63]);
                out.push_back(two ? tbl[(v >> 6) & 63] : '=');
                out.push_back('=');
            }
            return out;
        }

        // ---------------------------------------------------------------- 缓存文件
        void LoadCache()
        {
            if (g_cacheLoaded)
                return;
            g_cacheLoaded = true;

            std::ifstream f(CachePath(), std::ios::binary);
            if (!f)
                return;
            std::stringstream ss;
            ss << f.rdbuf();
            const std::string text = ss.str();

            // 极简解析：每条形如 {"rid":123,"state":2,"reason":"...","at":1700000000}
            size_t pos = 0;
            while ((pos = text.find("\"rid\":", pos)) != std::string::npos)
            {
                Cached c{};
                pos += 6;
                c.rid = std::atoll(text.c_str() + pos);

                const size_t sp = text.find("\"state\":", pos);
                if (sp != std::string::npos)
                {
                    const int v = std::atoi(text.c_str() + sp + 8);
                    c.state = (v == 2) ? State::Banned : (v == 3 ? State::Clean : State::Idle);
                }
                const size_t rp = text.find("\"reason\":\"", pos);
                if (rp != std::string::npos)
                {
                    const size_t b = rp + 10;
                    const size_t e = text.find('"', b);
                    if (e != std::string::npos)
                        c.reason = text.substr(b, e - b);
                }
                const size_t ap = text.find("\"at\":", pos);
                if (ap != std::string::npos)
                    c.at_unix = static_cast<uint64_t>(std::atoll(text.c_str() + ap + 5));

                g_cache.push_back(c);
                pos = (ap != std::string::npos) ? ap + 5 : pos + 1;
            }
        }

        void SaveCache()
        {
            std::string out = "{\n  \"saved_at\": " + std::to_string(UnixNow()) + ",\n  \"entries\": [\n";
            for (size_t i = 0; i < g_cache.size(); ++i)
            {
                const Cached& c = g_cache[i];
                const int st = (c.state == State::Banned) ? 2 : (c.state == State::Clean ? 3 : 0);
                out += "    {\"rid\":" + std::to_string(c.rid) +
                       ",\"state\":" + std::to_string(st) +
                       ",\"reason\":\"" + c.reason + "\"" +
                       ",\"at\":" + std::to_string(c.at_unix) + "}";
                if (i + 1 < g_cache.size())
                    out += ",";
                out += "\n";
            }
            out += "  ]\n}\n";

            std::ofstream f(CachePath(), std::ios::binary | std::ios::trunc);
            if (f)
                f << out;
        }

        void AppendCache(long long rid, State st, const std::string& reason)
        {
            for (auto& c : g_cache)
            {
                if (c.rid == rid)
                {
                    c.state = st;
                    c.reason = reason;
                    c.at_unix = UnixNow();
                    SaveCache();
                    return;
                }
            }
            Cached c{};
            c.rid = rid;
            c.state = st;
            c.reason = reason;
            c.at_unix = UnixNow();
            g_cache.push_back(c);
            SaveCache();
        }

        bool CacheFresh(const Cached& c)
        {
            return c.state != State::Idle && (UnixNow() - c.at_unix) < kFreshSec;
        }

        // ---------------------------------------------------------------- 回调
        void OnKick(uint64_t /*id*/, const char* reason)
        {
            g_pendingReason = reason ? reason : "";
        }
        void OnLog(const char*, int) {}
        void OnMessage(uint64_t, const void*, uint32_t) {}

        bool EnsureLoaded()
        {
            if (g_init)
                return true;

            HMODULE dll = LoadLibraryA("BEServer_x64.dll");
            if (!dll)
            {
                const std::string alt = ExeDir() + "BEServer_x64.dll";
                dll = LoadLibraryA(alt.c_str());
            }
            if (!dll)
            {
                g_error = "无法加载 BEServer_x64.dll（需与 exe 同目录）";
                return false;
            }
            g_init = reinterpret_cast<init_t>(GetProcAddress(dll, "Init"));
            if (!g_init)
            {
                g_error = "BEServer_x64.dll 里找不到 Init 导出";
                FreeLibrary(dll);
                return false;
            }
            g_dll = dll;
            g_error.clear();
            return true;
        }

        void InitApi()
        {
            BattlEyeUserData ud{};
            ud.game_name = "paradise";
            ud.log_func = &OnLog;
            ud.kick_player = &OnKick;
            ud.send_message = &OnMessage;
            ud.unk = nullptr;
            g_api = BattlEyeApi{};
            g_init(1, &ud, &g_api);
        }

        void FinishQuery(State result, const std::string& reason)
        {
            g_reason = reason;
            g_state = result;
            AppendCache(g_rid, result, reason);
            g_lastFinish = NowMs();
            ++g_queriesDone;
            Stop();
        }
    }  // namespace

    bool Available()
    {
        return EnsureLoaded();
    }

    bool Start(long long rid)
    {
        Stop();
        if (!EnsureLoaded())
        {
            g_state = State::Failed;
            return false;
        }
        g_reason.clear();
        g_pendingReason.clear();
        g_rid = rid;
        InitApi();

        const std::string guid = Base64(std::to_string(rid));

        if (g_api.add_player) g_api.add_player(kPeerId, static_cast<uint32_t>(-1), 0, "Deez", 0);
        if (g_api.assign_guid) g_api.assign_guid(kPeerId, guid.data(), static_cast<uint32_t>(guid.size()));
        if (g_api.assign_guid_verified) g_api.assign_guid_verified(kPeerId, guid.data(), static_cast<uint32_t>(guid.size()));
        if (g_api.set_player_state) g_api.set_player_state(kPeerId, 1);

        g_startedAt = NowMs();
        g_state = State::Checking;
        return true;
    }

    void AutoQuery(long long rid)
    {
        if (rid <= 0)
            return;
        LoadCache();
        for (const auto& c : g_cache)
        {
            if (c.rid == rid && CacheFresh(c))
                return;   // 已有新鲜结果
        }
        if (g_state == State::Checking && g_rid == rid)
            return;       // 正在查这个
        for (long long q : g_queue)
        {
            if (q == rid)
                return;   // 已在队列里
        }
        g_queue.push_back(rid);
    }

    int PendingCount()
    {
        return static_cast<int>(g_queue.size());
    }

    int CachedCount()
    {
        LoadCache();
        return static_cast<int>(g_cache.size());
    }

    uint64_t QueriesDone() { return g_queriesDone; }

    void Tick()
    {
        LoadCache();

        if (g_state == State::Checking)
        {
            if (g_api.run)
                g_api.run();

            if (!g_pendingReason.empty())
            {
                FinishQuery(State::Banned, g_pendingReason);
                return;
            }
            if (NowMs() - g_startedAt > static_cast<uint64_t>(kTimeoutMs))
                FinishQuery(State::Clean, std::string());
            return;
        }

        // 空闲：从队列取下一个（两个查询之间留间隔）
        if (!g_queue.empty() && (g_lastFinish == 0 || NowMs() - g_lastFinish >= static_cast<uint64_t>(kGapMs)))
        {
            const long long next = g_queue.front();
            g_queue.pop_front();
            if (!Available())
            {
                g_state = State::Failed;
                return;
            }
            Start(next);
        }
    }

    State Current() { return g_state; }
    const std::string& Reason() { return g_reason; }
    long long CurrentRid() { return g_rid; }
    const std::string& LastError() { return g_error; }

    void Stop()
    {
        if (g_api.shutdown)
            g_api.shutdown();
        g_api = BattlEyeApi{};
        if (g_state == State::Checking)
            g_state = State::Idle;
    }

    const Cached* FindCached(long long rid)
    {
        LoadCache();
        for (const auto& c : g_cache)
        {
            if (c.rid == rid)
                return &c;
        }
        return nullptr;
    }
}
