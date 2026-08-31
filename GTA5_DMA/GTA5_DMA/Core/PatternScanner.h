#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

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

    std::optional<std::uintptr_t> ResolveRelativeTarget(
        std::span<const std::uint8_t> bytes,
        std::size_t matchOffset,
        std::size_t displacementOffset,
        std::size_t instructionSize,
        std::uintptr_t bufferRuntimeAddress);
}
