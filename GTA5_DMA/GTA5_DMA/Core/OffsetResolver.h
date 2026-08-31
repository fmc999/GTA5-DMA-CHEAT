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
    };

    enum class OffsetSource
    {
        Pattern,
        Fallback
    };

    struct ResolvedOffset
    {
        std::string name;
        std::uintptr_t value = 0;
        OffsetSource source = OffsetSource::Fallback;
        std::string diagnostic;
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
        std::uintptr_t fallback);

    // diagnostic: 失败原因（写入具体阶段与读到的字节，便于定位 FPGA 读取异常）
    std::optional<ExecutableSection> LoadExecutableSection(
        const MemoryReader& reader,
        std::uintptr_t moduleBase,
        std::uint32_t imageSize,
        std::string* diagnostic = nullptr);
}
