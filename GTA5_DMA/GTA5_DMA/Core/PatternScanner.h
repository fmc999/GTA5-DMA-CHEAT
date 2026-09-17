#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PatternScanner
{
    enum class ScanStatus
    {
        Found,
        InvalidPattern,
        NotFound,
        MultipleMatches
    };

    struct ScanResult
    {
        ScanStatus status = ScanStatus::NotFound;
        std::size_t offset = 0;
        std::string diagnostic;
    };

    ScanResult FindUnique(std::span<const std::uint8_t> bytes, std::string_view pattern);

    struct FindAllResult
    {
        ScanStatus status = ScanStatus::NotFound;
        std::vector<std::size_t> offsets;      // 全部命中起点（地址升序）
        std::string diagnostic;
    };

    // 全量匹配：返回**所有**命中起点。FindUnique 在命中多于一次时直接放弃（MultipleMatches），
    // 而解析「同一签名可能出现多处」的偏移（如 GlobalPtr）需要看到全部候选，
    // 再交给调用方的 CandidateValidator 逐个体检后挑选。
    FindAllResult FindAll(std::span<const std::uint8_t> bytes, std::string_view pattern);

    std::optional<std::uintptr_t> ResolveRelativeTarget(
        std::span<const std::uint8_t> bytes,
        std::size_t matchOffset,
        std::size_t displacementOffset,
        std::size_t instructionSize,
        std::uintptr_t bufferRuntimeAddress);
}
