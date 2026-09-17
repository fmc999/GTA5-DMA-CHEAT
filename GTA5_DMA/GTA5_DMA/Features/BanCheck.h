#pragma once

// ============================================================================
// BanCheck —— BattlEye 封禁查询（只读查询，不修改任何东西）
//
// 原理（参考开源项目 calamity-inc/AmIBattlEyeBanned 的做法）：
//   用 BattlEye 官方服务端库 BEServer_x64.dll 起一个「虚拟玩家」，把目标的
//   Rockstar ID（RID）以 base64 形式注册成该玩家的 GUID，再让 BE 主服务器校验。
//   若该 RID 已被 BE 封禁，BE 会回调 kick_player(peerid, reason)，reason 即封禁理由；
//   超时未收到回调 = 未封禁。
//
// 依赖（放在 exe 同目录）：
//   BEServer_x64.dll   ← BEServer_x64.cfg 同目录，且**工作目录**需为 exe 目录
//   BEServer_x64.cfg   ← GameID paradise / MasterPort 61455
// ============================================================================

#include <cstdint>
#include <string>

namespace BanCheck
{
    enum class State
    {
        Idle,       // 未查询
        Checking,   // 查询中
        Banned,     // 已封禁（理由见 Reason()）
        Clean,      // 未封禁
        Failed      // 失败（DLL 缺失 / 初始化失败）
    };

    // 是否可用（BEServer_x64.dll 能加载）
    bool Available();

    // 开始异步查询（会先结束上一次）。返回 false = 无法查询（看 LastError()）
    bool Start(long long rid);

    // 每帧调用：驱动 BE 网络循环并捕获 kick 理由，超时后判为「未封禁」
    void Tick();

    // ---- 自动查询（界面每帧调用即可；内部去重、串行排队、24 小时缓存）----
    void AutoQuery(long long rid);
    int  PendingCount();      // 还在排队的数量
    int  ActiveCount();       // 正在查询中的数量（并行槽位）
    int  ParallelPeak();      // 并行度峰值
    long long LastBannedRid();
    int  WaitFor(long long rid, int timeoutMs);   // CLI：等某个 RID 出结果（2=封禁 3=未封禁 0=超时）
    int  CachedCount();       // 已缓存的结果数
    uint64_t QueriesDone();   // 本次运行完成的查询数

    // 同步查询（CLI 自检用）。blocking_timeout_ms：等待上限
    inline void StartSync(long long rid) { Start(rid); }
    State Current();
    const std::string& Reason();
    long long CurrentRid();
    const std::string& LastError();

    // 结束当前查询（释放 BE 状态）
    void Stop();

    // 查询过的历史（RID → 结果），用于界面缓存
    struct Cached
    {
        long long   rid = 0;
        State       state = State::Idle;
        std::string reason;
        uint64_t    at_unix = 0;
    };
    const Cached* FindCached(long long rid);
}
