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

    using MemoryReader = std::function<bool(std::uintptr_t, void*, std::size_t)>;

    std::span<const SignatureSpec> GetCatalog(GameType gameType);

    ResolvedOffset ResolveOne(
        const SignatureSpec& spec,
        std::span<const std::uint8_t> bytes,
        std::uintptr_t sectionRuntimeAddress,
        std::uintptr_t moduleBase,
        std::uint32_t imageSize,
        std::uintptr_t fallback);

    std::optional<ExecutableSection> LoadExecutableSection(
        const MemoryReader& reader,
        std::uintptr_t moduleBase,
        std::uint32_t imageSize);
}
