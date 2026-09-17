#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "Offsets.h"

namespace OffsetResolver
{
    // 候选校验器（可选）：把算出来的候选地址交给调用方做只读体检（是否可读、结构是否合理）。
    // 返回 false 表示该候选不可信；ResolveOne 会换下一个候选 / 下一条特征码 / 回退静态值。
    // 不传校验器时保持旧的「唯一命中」语义（多命中直接放弃）。
    using CandidateValidator = std::function<bool(std::uintptr_t candidateAddress, std::string* reason)>;

    struct SignatureSpec
    {
        std::string_view name;
        std::string_view pattern;
        std::size_t displacementOffset = 0;
        std::size_t instructionSize = 0;
        // 备用特征码（游戏更新后主特征码失配时按序尝试；同样失败才回退静态值）
        static constexpr std::size_t kMaxAlternatives = 4;
        std::size_t alternativeCount = 0;
        std::string_view alternativePatterns[kMaxAlternatives]{};
        // 每条备用特征码各自的 rel32 位移 / 指令长度（0 = 沿用主特征码的值）。
        // 必须能单独指定：主/备用是两条不同指令（例：主 48 8D 15 → disp=10/insn=14；
        // 备用 48 8B 0D → disp=3/insn=7）。沿用主模式的值会算出完全错误的地址，
        // 进而把垃圾地址当成真身采用（实机事故：GlobalPtr 解析出 0x4737178 而不是 0x3ED15A8）。
        std::size_t alternativeDisplacementOffset[kMaxAlternatives]{};
        std::size_t alternativeInstructionSize[kMaxAlternatives]{};
    };

    enum class OffsetSource
    {
        Pattern,
        Fallback
    };

    // 外部追加的特征码候选（由应用在启动时接上 RuntimeTables；离线单测不接 → 只用内置候选，
    // 这样测试工程不必链接 RuntimeTables/DMA 一整套依赖）
    struct ExternalPattern
    {
        std::string pattern;
        std::size_t displacementOffset = 0;
        std::size_t instructionSize = 0;
        bool        hasLayout = false;
    };
    using ExternalPatternProvider = std::uint32_t (*)(const char* offsetName, ExternalPattern* out, std::uint32_t maxCount);
    void SetExternalPatternProvider(ExternalPatternProvider provider);

    struct ResolvedOffset
    {
        std::string name;
        std::uintptr_t value = 0;
        OffsetSource source = OffsetSource::Fallback;
        std::string diagnostic;
        std::vector<std::uintptr_t> candidates;   // 本次尝试过的候选（诊断/测试用）
        bool validated = false;                   // 是否通过了调用方的候选校验器
    };

    struct OffsetResolutionReport
    {
        std::vector<ResolvedOffset> entries;
    };

    struct ExecutableSection
    {
        std::string name;
        std::uintptr_t runtimeAddress = 0;
        std::uintptr_t moduleBase = 0;
        std::uint32_t imageSize = 0;
        std::vector<std::uint8_t> bytes;
    };

    // 读取回调：返回 false 表示读取失败（内部已重试），附带失败阶段到诊断输出
    using MemoryReader = std::function<bool(std::uintptr_t, void*, std::size_t, const char* /*stage*/)>;

    std::span<const SignatureSpec> GetCatalog(GameType gameType);

    ResolvedOffset ResolveOne(
        const SignatureSpec& spec,
        std::span<const std::uint8_t> bytes,
        std::uintptr_t sectionRuntimeAddress,
        std::uintptr_t moduleBase,
        std::uint32_t imageSize,
        std::uintptr_t fallback,
        const CandidateValidator& validator = {});

    // diagnostic: 失败原因（写入具体阶段与读到的字节，便于定位 FPGA 读取异常）
    std::optional<ExecutableSection> LoadExecutableSection(
        const MemoryReader& reader,
        std::uintptr_t moduleBase,
        std::uint32_t imageSize,
        std::string* diagnostic = nullptr);
}
