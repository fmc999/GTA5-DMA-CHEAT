#pragma once

// ============================================================================
// HealthReport —— 项目「长寿体检」：游戏更新后一眼看出哪坏了、该怎么修
//
// 设计目标（用户要求）：**不需要每个游戏版本重编译**。做法是把"能不能用"变成可自测的项：
//   1. 每项都做**功能性实测**（不是看代码里有没有值），拿到真实结果才判 ✓/⚠/✗
//   2. 每项都带**修法**：坏了要改哪个文件、加哪一行（多数情况是往 exe 同目录的文本文件里加）
//   3. 结果同时写进 GTA5_DMA_diag.txt，用户不在电脑前也能看到
//
// 用法：GTA5_DMA.exe --health   （也可由界面「体检」按钮调用）
// ============================================================================

#include <string>
#include <vector>

namespace HealthReport
{
    struct Item
    {
        std::string name;      // 检查项
        std::string detail;    // 实测到的具体值/现象
        std::string fix;       // 坏了怎么办（可直接照做）
        bool        ok = false;
        bool        warn = false;   // ⚠ 可用但脆弱（例如用了写死的兜底地址）
    };

    // 跑一遍全部检查（需要 DMA 已初始化；不依赖游戏一定在战局里）
    std::vector<Item> Run();

    // 打印到 stdout（--health 用）
    void Print(const std::vector<Item>& items);

    // 追加到诊断文件（启动时也调一次，便于事后排查）
    void AppendToDiagnostics();
}
