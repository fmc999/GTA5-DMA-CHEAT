// ============================================================================
// BanCheck.cpp —— BattlEye 封禁查询（并行引擎 + 持久缓存）
//
// 原理：用 BattlEye 官方服务端库 BEServer_x64.dll 起一个「虚拟服务端」，
//       把每个待查 RID（base64）注册成一个虚拟玩家的 GUID，再让 BE 主服务器校验：
//         被封禁 → BE 回调 kick_player(peerId, reason)，reason 即封禁理由
//         超时无回调 → 未封禁
//
// 并行：一次 Init 建立会话，同一会话里同时挂 kMaxParallel 个虚拟玩家（每个 RID 一个
//       peerId），谁先出结果谁先让位给队列里的下一个。封禁答复实测 1.8~3.3 秒，
//       所以 26 个玩家大约 3~4 轮就跑完（原来串行要 2.8 分钟）。
//
// 缓存：结果落盘 be_bans_cache.json（exe 同目录），24 小时内复用，不重复问 BE。
// ============================================================================

#include "pch.h"

#include "BanCheck.h"

#include <windows.h>

#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <deque>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
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

        constexpr int      kMaxParallel = 10;      // 同时挂几个虚拟玩家（实测 BE 支持并发，批内同时出结果）
        constexpr int      kTimeoutMs = 5000;      // 单个 RID 等待上限（封禁答复实测 ≤3.3s，留 1.5 倍余量）
        constexpr uint64_t kFreshSec = 24 * 3600;  // 缓存有效期
        constexpr uint64_t kBasePeer = 1337;

        // 一个正在查询的槽位
        struct Slot
        {
            long long rid = 0;
            uint64_t  peer = 0;
            uint64_t  startedAt = 0;
        };

        HMODULE         g_dll = nullptr;
        init_t          g_init = nullptr;
        BattlEyeApi     g_api{};
        bool            g_session = false;      // BE 会话是否已 Init

        std::vector<Slot>     g_slots;          // 并行中的槽位
        std::deque<long long> g_queue;          // 待查队列（去重）
        std::unordered_map<uint64_t, long long> g_peerToRid;

        std::vector<Cached>   g_cache;
        bool            g_cacheLoaded = false;
        std::string     g_cachePath;
        std::string     g_error;
        std::string     g_lastReason;           // 最近一次封禁理由（界面显示用）
        long long       g_lastBannedRid = 0;
        uint64_t        g_queriesDone = 0;
        uint64_t        g_parallelPeak = 0;
        uint64_t        g_nextPeer = kBasePeer;

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

        // 只有明确是封禁的理由才算「已封禁」，其它 kick 原因按原样记录下来
        bool LooksLikeBan(const std::string& reason)
        {
            std::string low;
            low.reserve(reason.size());
            for (char c : reason)
                low.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(c))));
            return low.find("ban") != std::string::npos;
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
        void OnKick(uint64_t peerId, const char* reason)
        {
            const std::string r = reason ? reason : "";
            for (size_t i = 0; i < g_slots.size(); ++i)
            {
                if (g_slots[i].peer == peerId)
                {
                    const long long rid = g_slots[i].rid;
                    const State st = LooksLikeBan(r) ? State::Banned : State::Clean;
                    g_lastReason = r;
                    if (st == State::Banned)
                        g_lastBannedRid = rid;
                    AppendCache(rid, st, r);
                    ++g_queriesDone;
                    g_peerToRid.erase(peerId);
                    g_slots.erase(g_slots.begin() + i);
                    return;
                }
            }
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

        bool EnsureSession()
        {
            if (g_session && g_api.run)
                return true;
            if (!EnsureLoaded())
                return false;

            BattlEyeUserData ud{};
            ud.game_name = "paradise";
            ud.log_func = &OnLog;
            ud.kick_player = &OnKick;
            ud.send_message = &OnMessage;
            ud.unk = nullptr;
            g_api = BattlEyeApi{};
            g_init(1, &ud, &g_api);
            g_session = (g_api.run != nullptr);
            return g_session;
        }

        // 把某个 RID 挂上 BE 会话（占用一个新 peerId）
        bool Attach(long long rid, uint64_t peer)
        {
            const std::string guid = Base64(std::to_string(rid));
            if (g_api.add_player) g_api.add_player(peer, static_cast<uint32_t>(-1), 0, "Deez", 0);
            if (g_api.assign_guid) g_api.assign_guid(peer, guid.data(), static_cast<uint32_t>(guid.size()));
            if (g_api.assign_guid_verified) g_api.assign_guid_verified(peer, guid.data(), static_cast<uint32_t>(guid.size()));
            if (g_api.set_player_state) g_api.set_player_state(peer, 1);
            return true;
        }

        void StartSlot(long long rid)
        {
            const uint64_t peer = g_nextPeer++;
            Attach(rid, peer);
            Slot s{};
            s.rid = rid;
            s.peer = peer;
            s.startedAt = NowMs();
            g_slots.push_back(s);
            g_peerToRid[peer] = rid;
            if (g_slots.size() > g_parallelPeak)
                g_parallelPeak = g_slots.size();
        }
    }  // namespace

    bool Available()
    {
        return EnsureLoaded();
    }

    // 兼容 CLI：立刻建会话并挂上这一个 RID
    bool Start(long long rid)
    {
        LoadCache();
        if (!EnsureSession())
            return false;
        StartSlot(rid);
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
                return;
        }
        for (const auto& s : g_slots)
        {
            if (s.rid == rid)
                return;
        }
        for (long long q : g_queue)
        {
            if (q == rid)
                return;
        }
        g_queue.push_back(rid);
    }

    int PendingCount() { return static_cast<int>(g_queue.size()); }
    int ActiveCount() { return static_cast<int>(g_slots.size()); }
    int ParallelPeak() { return static_cast<int>(g_parallelPeak); }
    int CachedCount() { LoadCache(); return static_cast<int>(g_cache.size()); }
    uint64_t QueriesDone() { return g_queriesDone; }

    void Tick()
    {
        LoadCache();
        if (!g_session && g_queue.empty() && g_slots.empty())
            return;

        if (!EnsureSession())
            return;

        // 1) 收割超时的槽位（未封禁）
        const uint64_t now = NowMs();
        for (size_t i = 0; i < g_slots.size();)
        {
            if (now - g_slots[i].startedAt > static_cast<uint64_t>(kTimeoutMs))
            {
                const long long rid = g_slots[i].rid;
                const uint64_t peer = g_slots[i].peer;
                AppendCache(rid, State::Clean, std::string());
                ++g_queriesDone;
                g_peerToRid.erase(peer);
                g_slots.erase(g_slots.begin() + i);
                continue;
            }
            ++i;
        }

        // 2) 补满并行槽位
        while (!g_queue.empty() && static_cast<int>(g_slots.size()) < kMaxParallel)
        {
            const long long next = g_queue.front();
            g_queue.pop_front();
            StartSlot(next);
        }

        // 3) 驱动 BE 网络循环
        if (g_api.run)
            g_api.run();
    }

    State Current()
    {
        if (!g_slots.empty())
            return State::Checking;
        return State::Idle;
    }
    const std::string& Reason() { return g_lastReason; }
    long long CurrentRid() { return g_lastBannedRid; }
    long long LastBannedRid() { return g_lastBannedRid; }
    const std::string& LastError() { return g_error; }

    void Stop()
    {
        if (g_api.shutdown)
            g_api.shutdown();
        g_api = BattlEyeApi{};
        g_session = false;
        g_slots.clear();
        g_peerToRid.clear();
    }

    // CLI 用：等某个 RID 出结果（内部自己 Tick）
    int WaitFor(long long rid, int timeoutMs)
    {
        const uint64_t t0 = NowMs();
        while (NowMs() - t0 < static_cast<uint64_t>(timeoutMs))
        {
            Tick();
            const Cached* c = FindCached(rid);
            if (c && c->state != State::Idle)
                return c->state == State::Banned ? 2 : 3;
            Sleep(50);
        }
        return 0;
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
