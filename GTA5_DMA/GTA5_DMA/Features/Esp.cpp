#include "pch.h"

#include "Esp.h"

#include "DMA.h"
#include "PlayerList.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>

// ============================================================================
// 方框透视（纯 DMA 实现：不注入、不调游戏函数、不另开窗口）
//
// 参考项目（YimMenuV2 frontend/ESP.cpp）的做法：
//   · 调 GRAPHICS::GET_SCREEN_COORD_FROM_WORLD_COORD 拿屏幕坐标（调游戏函数）；
//   · 在游戏进程内用 ImGui 画（注入）。
// 我们的等价做法（受纯 DMA 约束）：
//   · 只读内存：自己定位游戏的「视投影矩阵」，在本进程里做世界→屏幕投影；
//   · 画在自己的控制台窗口画布上（辅机 + 融合器合成画面时，本窗口就是叠加图层）。
// ============================================================================

namespace
{
    std::mutex  g_EspMutex;
    EspMatrix   g_Matrix;
    bool        g_MatrixValid = false;
    std::string g_MatrixStatus = "未定位（点右侧按钮定位）";
    std::atomic<bool> g_Locating{ false };

    bool ReadRegion(uintptr_t address, void* buffer, size_t size)
    {
        uint8_t* dst = static_cast<uint8_t*>(buffer);
        if (DMA::Memory().Read(address, dst, size))
            return true;
        bool any = false;
        for (size_t off = 0; off < size; off += 0x1000)
        {
            const size_t page = (size - off < 0x1000) ? (size - off) : 0x1000;
            if (DMA::Memory().Read(address + off, dst + off, page))
                any = true;
        }
        return any;
    }

    // 世界点 → 屏幕像素（行向量约定：v' = v * M）
    bool ProjectWorld(const float* m, const float* v, float screenW, float screenH, float& outX, float& outY)
    {
        const float cx = m[0] * v[0] + m[1] * v[1] + m[2] * v[2] + m[3];
        const float cy = m[4] * v[0] + m[5] * v[1] + m[6] * v[2] + m[7];
        const float cw = m[12] * v[0] + m[13] * v[1] + m[14] * v[2] + m[15];
        if (!std::isfinite(cw) || cw <= 0.01f)
            return false;
        const float ndcX = cx / cw;
        const float ndcY = cy / cw;
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
            return false;
        outX = (ndcX + 1.0f) * 0.5f * screenW;
        outY = (1.0f - ndcY) * 0.5f * screenH;
        return true;
    }

    // 代数判据：m33 == 0，且前向轴单位长（行/列主序各试一次）
    bool LooksLikeViewProjection(const float* m, bool& rowMajor)
    {
        for (int i = 0; i < 16; ++i)
            if (!std::isfinite(m[i]) || std::fabs(m[i]) > 1.0e7f)
                return false;
        if (std::fabs(m[15]) > 1e-6f)
            return false;
        if (std::fabs(m[0]) < 1e-3f || std::fabs(m[5]) < 1e-3f)
            return false;

        const float rowLen = std::sqrt(m[12] * m[12] + m[13] * m[13] + m[14] * m[14]);
        const float colLen = std::sqrt(m[3] * m[3] + m[7] * m[7] + m[11] * m[11]);
        const bool rowOk = std::fabs(rowLen - 1.0f) < 0.02f;
        const bool colOk = std::fabs(colLen - 1.0f) < 0.02f;
        if (!rowOk && !colOk)
            return false;
        rowMajor = rowOk;
        return true;
    }

    struct ScoreResult
    {
        int   onScreen = 0;
        float spread = 0.0f;
    };

    ScoreResult ScoreMatrix(const float* m, const std::vector<Vec3>& points, float screenW, float screenH)
    {
        ScoreResult r{};
        float minY = 1e9f, maxY = -1e9f;
        for (const Vec3& p : points)
        {
            float x = 0, y = 0;
            if (!ProjectWorld(m, &p.x, screenW, screenH, x, y))
                continue;
            if (x < -screenW * 0.25f || x > screenW * 1.25f || y < -screenH * 0.25f || y > screenH * 1.25f)
                continue;
            ++r.onScreen;
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
        if (r.onScreen > 0)
            r.spread = maxY - minY;
        return r;
    }

    // 分块搜索视投影矩阵：引擎侧搜「第 4 行 = (0,0,1,0)」→ 本地代数 + 几何打分
    bool SearchMatrix(const std::vector<Vec3>& points, float screenW, float screenH,
                      EspMatrix& out, std::string& status, int maxWindows)
    {
        if (!DMA::vmh || !DMA::PID)
        {
            status = "DMA 未就绪";
            return false;
        }
        if (points.size() < 2)
        {
            status = "需要战局里 ≥2 名玩家才能确认矩阵";
            return false;
        }

        VMMDLL_MEM_SEARCH_CONTEXT_SEARCHENTRY entry{};
        entry.cbAlign = 16;
        entry.cb = 16;
        const float tail[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
        std::memcpy(entry.pb, tail, sizeof(tail));

        float bestScore = -1.0f;
        EspMatrix best{};
        bool found = false;

        const uint64_t windowSize = 1ull << 30;   // 1GB 一窗，避免单次搜索过大不稳
        for (int w = 0; w < maxWindows; ++w)
        {
            VMMDLL_MEM_SEARCH_CONTEXT ctx{};
            ctx.dwVersion = VMMDLL_MEM_SEARCH_VERSION;
            ctx.cMaxResult = 2048;
            ctx.cSearch = 1;
            ctx.pSearch = &entry;
            ctx.vaMin = static_cast<uint64_t>(w) * windowSize;
            ctx.vaMax = ctx.vaMin + windowSize;

            QWORD* hits = nullptr;
            DWORD hitCount = 0;
            const BOOL ok = VMMDLL_MemSearch(DMA::vmh, DMA::PID, &ctx, &hits, &hitCount);
            if (!ok || !hits || !hitCount)
            {
                if (hits)
                    VMMDLL_MemFree(hits);
                if (ctx.cbReadTotal == 0 && w >= 2)
                    break;      // 后面都是未分配区
                continue;
            }

            for (DWORD i = 0; i < hitCount; ++i)
            {
                const uintptr_t rowAddr = static_cast<uintptr_t>(hits[i]);
                if (rowAddr < 64)
                    continue;
                float m[16] = {};
                if (!ReadRegion(rowAddr - 48, m, sizeof(m)))
                    continue;
                bool rowMajor = true;
                if (!LooksLikeViewProjection(m, rowMajor))
                    continue;
                const float aspect = std::fabs(m[5]) / std::fabs(m[0]);
                if (aspect < 1.05f || aspect > 2.9f)
                    continue;

                const ScoreResult sc = ScoreMatrix(m, points, screenW, screenH);
                const float score = static_cast<float>(sc.onScreen) * 10.0f + sc.spread * 0.01f;
                if (score > bestScore)
                {
                    bestScore = score;
                    std::memcpy(best.M, m, sizeof(m));
                    best.Address = rowAddr - 48;
                    found = true;
                }
            }
            VMMDLL_MemFree(hits);

            if (found && bestScore >= static_cast<float>(points.size()) * 10.0f)
                break;
        }

        if (!found)
        {
            status = "未找到视投影矩阵";
            return false;
        }
        out = best;
        char buf[160];
        std::snprintf(buf, sizeof(buf), "已定位 0x%llX（%d/%d 玩家在屏内）",
                      static_cast<unsigned long long>(best.Address),
                      static_cast<int>(bestScore / 10.0f), static_cast<int>(points.size()));
        status = buf;
        return true;
    }
}

bool Esp::GetMatrix(EspMatrix& out)
{
    std::lock_guard<std::mutex> lock(g_EspMutex);
    if (!g_MatrixValid)
        return false;
    out = g_Matrix;
    return true;
}

std::string Esp::GetMatrixStatus()
{
    std::lock_guard<std::mutex> lock(g_EspMutex);
    return g_MatrixStatus;
}

bool Esp::IsLocating()
{
    return g_Locating.load();
}

void Esp::RequestLocate()
{
    if (g_Locating.exchange(true))
        return;
    std::thread([]()
    {
        {
            std::lock_guard<std::mutex> lock(g_EspMutex);
            g_MatrixStatus = "定位中…（战局里玩家越多越准）";
        }
        for (int i = 0; i < 6; ++i)
        {
            PlayerList::OnDMAFrame();
            Sleep(80);
        }
        const auto players = PlayerList::GetSnapshot();
        std::vector<Vec3> points;
        for (const auto& p : players)
            if (p.Position.x != 0.0f || p.Position.y != 0.0f)
                points.push_back(p.Position);

        const float screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
        const float screenH = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
        EspMatrix m{};
        std::string status;
        const bool ok = SearchMatrix(points, screenW, screenH, m, status, 8);
        {
            std::lock_guard<std::mutex> lock(g_EspMutex);
            g_MatrixValid = ok;
            if (ok)
                g_Matrix = m;
            g_MatrixStatus = status;
        }
        g_Locating.store(false);
    }).detach();
}

bool Esp::WorldToScreen(float x, float y, float z, float screenW, float screenH, float& outX, float& outY)
{
    std::lock_guard<std::mutex> lock(g_EspMutex);
    if (!g_MatrixValid)
        return false;
    const float v[3] = { x, y, z };
    return ProjectWorld(g_Matrix.M, v, screenW, screenH, outX, outY);
}

// ---------------------------------------------------------------- 每帧绘制
// 画在**本控制台窗口的画布**上（辅机 + 融合器把游戏画面与本窗口合成时，这里就是叠加层）：
//   · 投影用游戏分辨率（与系统分辨率一致），再等比缩放/居中到窗口客户区；
//   · 用背景层绘制，不会被面板内容挡住。
void Esp::RenderOverlay()
{
    if (!bEnable.load())
        return;

    EspMatrix mat;
    if (!GetMatrix(mat))
        return;

    ImGuiIO& io = ImGui::GetIO();
    const float canvasW = io.DisplaySize.x;
    const float canvasH = io.DisplaySize.y;
    if (canvasW <= 1.0f || canvasH <= 1.0f)
        return;

    // 投影坐标系 = 游戏屏幕分辨率；映射到窗口画布（等比 + 居中）
    const float screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    const float screenH = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
    const float scale = std::min(canvasW / screenW, canvasH / screenH);
    const float offX = (canvasW - screenW * scale) * 0.5f;
    const float offY = (canvasH - screenH * scale) * 0.5f;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    const auto players = PlayerList::GetSnapshot();
    const float maxDist = MaxDistance.load();
    const bool wantName = bName.load();
    const bool wantDist = bDistance.load();
    const bool wantHealth = bHealth.load();
    const bool wantBox = bBox.load();
    const float height = 1.85f * BoxScale.load();

    for (const auto& p : players)
    {
        if (p.IsLocal && !bIncludeLocal.load())
            continue;
        if (maxDist > 0.0f && p.Distance > maxDist)
            continue;

        // CNavigation.Position 约在胸口：脚 = -1.0m，头 = 脚 + 高度
        const float feetZ = p.Position.z - 1.0f;
        const float headZ = feetZ + height;

        float fx = 0, fy = 0, hx = 0, hy = 0;
        if (!ProjectWorld(mat.M, &p.Position.x, screenW, screenH, fx, fy))
            continue;
        const float vHead[3] = { p.Position.x, p.Position.y, headZ };
        const float vFeet[3] = { p.Position.x, p.Position.y, feetZ };
        if (!ProjectWorld(mat.M, vHead, screenW, screenH, hx, hy))
            continue;
        if (!ProjectWorld(mat.M, vFeet, screenW, screenH, fx, fy))
            continue;

        const float boxH = std::fabs(fy - hy);
        if (boxH < 4.0f || boxH > canvasH * 1.5f)
            continue;   // 太小/太离谱（贴脸或投影异常）不画
        const float boxW = boxH * 0.42f;
        const float cx = (fx + hx) * 0.5f;

        // 映射到窗口画布
        auto map = [&](float sx, float sy) {
            return ImVec2(offX + sx * scale, offY + sy * scale);
        };
        const float halfW = boxW * 0.5f;
        const ImVec2 tl = map(cx - halfW, hy);
        const ImVec2 br = map(cx + halfW, fy);

        const bool dead = p.Health <= 0.5f;
        const ImU32 color = dead ? IM_COL32(150, 150, 150, 220) : IM_COL32(80, 230, 120, 235);

        if (wantBox)
        {
            dl->AddRect(tl, br, color, 0.0f, 0, 1.6f);
            // 四角加重，视觉更像「方框透视」
            const float cl = (br.x - tl.x) * 0.25f;
            const float ch = (br.y - tl.y) * 0.20f;
            dl->AddLine(ImVec2(tl.x, tl.y), ImVec2(tl.x + cl, tl.y), color, 2.4f);
            dl->AddLine(ImVec2(tl.x, tl.y), ImVec2(tl.x, tl.y + ch), color, 2.4f);
            dl->AddLine(ImVec2(br.x - cl, tl.y), ImVec2(br.x, tl.y), color, 2.4f);
            dl->AddLine(ImVec2(br.x, tl.y), ImVec2(br.x, tl.y + ch), color, 2.4f);
            dl->AddLine(ImVec2(tl.x, br.y - ch), ImVec2(tl.x, br.y), color, 2.4f);
            dl->AddLine(ImVec2(tl.x, br.y), ImVec2(tl.x + cl, br.y), color, 2.4f);
            dl->AddLine(ImVec2(br.x - cl, br.y), ImVec2(br.x, br.y), color, 2.4f);
            dl->AddLine(ImVec2(br.x, br.y - ch), ImVec2(br.x, br.y), color, 2.4f);
        }

        if (wantHealth && p.MaxHealth > 0.0f)
        {
            const float hp = std::clamp(p.Health / p.MaxHealth, 0.0f, 1.0f);
            const ImVec2 hbTL(tl.x - 5.0f, tl.y);
            const ImVec2 hbBR(tl.x - 2.0f, br.y);
            dl->AddRectFilled(hbTL, hbBR, IM_COL32(20, 20, 20, 180));
            const ImVec2 fillTL(hbTL.x, br.y - (br.y - tl.y) * hp);
            dl->AddRectFilled(fillTL, hbBR, hp > 0.5f ? IM_COL32(80, 220, 120, 230) : IM_COL32(230, 170, 60, 230));
        }

        if (wantName || wantDist)
        {
            char text[96] = {};
            if (wantName && wantDist)
                std::snprintf(text, sizeof(text), "%s  %.0fm", p.Name, p.Distance);
            else if (wantName)
                std::snprintf(text, sizeof(text), "%s", p.Name);
            else
                std::snprintf(text, sizeof(text), "%.0fm", p.Distance);
            const ImVec2 ts = ImGui::CalcTextSize(text);
            const ImVec2 tp(tl.x + ((br.x - tl.x) - ts.x) * 0.5f, tl.y - ts.y - 3.0f);
            dl->AddText(ImVec2(tp.x + 1.0f, tp.y + 1.0f), IM_COL32(0, 0, 0, 200), text);
            dl->AddText(tp, IM_COL32(240, 240, 240, 240), text);
        }
    }
}

// ---------------------------------------------------------------- 诊断入口
int Esp::ProbeCamera()
{
    for (int i = 0; i < 10; ++i)
    {
        PlayerList::OnDMAFrame();
        Sleep(80);
    }
    const auto players = PlayerList::GetSnapshot();
    std::println("[cam] 玩家 {} 个", players.size());
    int plausible = 0;
    for (const auto& p : players)
    {
        if (!p.NavigationAddress)
            continue;
        float camZ = 0, camX = 0, camY = 0;
        DMA::Memory().Read(p.NavigationAddress + 0x88, &camZ, sizeof(camZ));
        DMA::Memory().Read(p.NavigationAddress + 0x90, &camX, sizeof(camX));
        DMA::Memory().Read(p.NavigationAddress + 0x94, &camY, sizeof(camY));
        const float dx = camX - p.Position.x;
        const float dy = camY - p.Position.y;
        const float dz = camZ - p.Position.z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        std::println("[cam] {:<18} 本地={} 坐标({:.1f},{:.1f},{:.1f}) 相机字段({:.1f},{:.1f},{:.1f}) 差 {:.2f} m",
                     p.Name, p.IsLocal ? 1 : 0, p.Position.x, p.Position.y, p.Position.z, camX, camY, camZ, dist);
        if (dist > 0.5f && dist < 12.0f)
            ++plausible;
    }
    std::println("[cam] 可信玩家数 = {}", plausible);
    return plausible > 0 ? 0 : 1;
}

int Esp::ProbeMatrix(int maxWindows)
{
    for (int i = 0; i < 10; ++i)
    {
        PlayerList::OnDMAFrame();
        Sleep(80);
    }
    const auto players = PlayerList::GetSnapshot();
    std::vector<Vec3> points;
    for (const auto& p : players)
        if (p.Position.x != 0.0f || p.Position.y != 0.0f)
            points.push_back(p.Position);

    const float screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    const float screenH = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));
    std::println("[esp] 战局玩家 {} 个（有坐标 {}），屏幕 {}x{}", players.size(), points.size(),
                 static_cast<int>(screenW), static_cast<int>(screenH));

    EspMatrix m{};
    std::string status;
    const bool ok = SearchMatrix(points, screenW, screenH, m, status, maxWindows > 0 ? maxWindows : 24);
    std::println("[esp] 结果：{} —— {}", ok ? "成功" : "失败", status);
    if (ok)
    {
        std::println("        m = [{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}] "
                     "[{:.4f} {:.4f} {:.4f} {:.4f}] [{:.4f} {:.4f} {:.4f} {:.4f}]",
                     m.M[0], m.M[1], m.M[2], m.M[3], m.M[4], m.M[5], m.M[6], m.M[7],
                     m.M[8], m.M[9], m.M[10], m.M[11], m.M[12], m.M[13], m.M[14], m.M[15]);
        for (const Vec3& p : points)
        {
            float x = 0, y = 0;
            if (ProjectWorld(m.M, &p.x, screenW, screenH, x, y))
                std::println("        玩家 ({:.1f},{:.1f},{:.1f}) → 屏幕 ({:.0f},{:.0f})", p.x, p.y, p.z, x, y);
        }
    }
    return ok ? 0 : 1;
}
