#include "OffsetResolver.h"

#include "PatternScanner.h"

#include <Windows.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace
{
    constexpr std::size_t kChunkSize = 1024 * 1024;
    constexpr std::size_t kMaxExecutableSectionSize = 64 * 1024 * 1024;

    constexpr OffsetResolver::SignatureSpec kEnhancedCatalog[] = {
        {"WorldPtr", "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 49 ?? 48 8D", 3, 7},
        {"GlobalPtr", "48 8D 3D ?? ?? ?? ?? 31 DB 48 8D 2D ?? ?? ?? ?? 4C", 3, 7},
        {"BlipPtr", "48 8D 0D ? ? ? ? 41 B8 ? ? ? ? 31 D2 E8 ? ? ? ? 8B 0D", 3, 7},
        {"PlayerMgrPtr", "75 0E 48 8B 05 ? ? ? ? 48 8B 88 F0 00 00 00", 5, 9},
        {"AimCPedPtr", "48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 4C 8D 05 ?? ?? ?? ?? BA", 3, 7}
    };
}

namespace OffsetResolver
{
    std::span<const SignatureSpec> GetCatalog(GameType gameType)
    {
        if (gameType == GameType::GTA5_Enhanced)
        {
            return kEnhancedCatalog;
        }
        return {};
    }

    ResolvedOffset ResolveOne(
        const SignatureSpec& spec,
        std::span<const std::uint8_t> bytes,
        std::uintptr_t sectionRuntimeAddress,
        std::uintptr_t moduleBase,
        std::uint32_t imageSize,
        std::uintptr_t fallback)
    {
        const auto fallbackResult = [&spec, fallback](std::string diagnostic) {
            return ResolvedOffset{
                std::string(spec.name),
                fallback,
                OffsetSource::Fallback,
                std::move(diagnostic)
            };
        };

        const PatternScanner::ScanResult match = PatternScanner::FindUnique(bytes, spec.pattern);
        if (match.status != PatternScanner::ScanStatus::Found)
        {
            return fallbackResult("Pattern scan failed for " + std::string(spec.name) + ".");
        }

        const auto target = PatternScanner::ResolveRelativeTarget(
            bytes,
            match.offset,
            spec.displacementOffset,
            spec.instructionSize,
            sectionRuntimeAddress);
        if (!target)
        {
            return fallbackResult("RIP-relative target could not be resolved for " + std::string(spec.name) + ".");
        }

        if (moduleBase == 0 || imageSize == 0 || *target < moduleBase ||
            *target - moduleBase >= imageSize)
        {
            return fallbackResult("Resolved target is outside the module image for " + std::string(spec.name) + ".");
        }

        return {std::string(spec.name), *target - moduleBase, OffsetSource::Pattern, {}};
    }

    std::optional<ExecutableSection> LoadExecutableSection(
        const MemoryReader& reader,
        std::uintptr_t moduleBase,
        std::uint32_t imageSize)
    {
        if (!reader || moduleBase == 0 || imageSize < sizeof(IMAGE_DOS_HEADER))
        {
            return std::nullopt;
        }

        IMAGE_DOS_HEADER dosHeader{};
        if (!reader(moduleBase, &dosHeader, sizeof(dosHeader)) || dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        {
            return std::nullopt;
        }

        const std::int64_t ntOffset = dosHeader.e_lfanew;
        if (ntOffset < 0 || static_cast<std::uint64_t>(ntOffset) > imageSize ||
            sizeof(IMAGE_NT_HEADERS64) > imageSize - static_cast<std::size_t>(ntOffset))
        {
            return std::nullopt;
        }

        const auto ntAddress = moduleBase + static_cast<std::uintptr_t>(ntOffset);
        IMAGE_NT_HEADERS64 ntHeaders{};
        if (!reader(ntAddress, &ntHeaders, sizeof(ntHeaders)) ||
            ntHeaders.Signature != IMAGE_NT_SIGNATURE ||
            ntHeaders.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        {
            return std::nullopt;
        }

        const std::size_t sectionHeadersOffset =
            static_cast<std::size_t>(ntOffset) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) +
            ntHeaders.FileHeader.SizeOfOptionalHeader;
        const std::size_t sectionHeadersSize =
            static_cast<std::size_t>(ntHeaders.FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
        if (sectionHeadersOffset > imageSize || sectionHeadersSize > imageSize - sectionHeadersOffset)
        {
            return std::nullopt;
        }

        std::vector<IMAGE_SECTION_HEADER> sections(ntHeaders.FileHeader.NumberOfSections);
        if (!sections.empty() && !reader(
                moduleBase + sectionHeadersOffset,
                sections.data(),
                sectionHeadersSize))
        {
            return std::nullopt;
        }

        for (const IMAGE_SECTION_HEADER& section : sections)
        {
            if ((section.Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0)
            {
                continue;
            }

            const std::size_t sectionRva = section.VirtualAddress;
            const std::size_t declaredSize = section.Misc.VirtualSize > section.SizeOfRawData
                ? section.Misc.VirtualSize
                : section.SizeOfRawData;
            if (declaredSize == 0 || sectionRva >= imageSize)
            {
                continue;
            }

            const std::size_t availableSize = imageSize - sectionRva;
            std::size_t sectionSize = declaredSize;
            if (availableSize < sectionSize)
            {
                sectionSize = availableSize;
            }
            if (kMaxExecutableSectionSize < sectionSize)
            {
                sectionSize = kMaxExecutableSectionSize;
            }
            if (sectionSize == 0)
            {
                continue;
            }

            std::vector<std::uint8_t> bytes(sectionSize);
            for (std::size_t offset = 0; offset < sectionSize; offset += kChunkSize)
            {
                const std::size_t chunkSize = (std::min)(kChunkSize, sectionSize - offset);
                if (!reader(moduleBase + sectionRva + offset, bytes.data() + offset, chunkSize))
                {
                    return std::nullopt;
                }
            }

            char sectionName[IMAGE_SIZEOF_SHORT_NAME + 1]{};
            std::memcpy(sectionName, section.Name, IMAGE_SIZEOF_SHORT_NAME);
            return ExecutableSection{
                sectionName,
                moduleBase + sectionRva,
                moduleBase,
                imageSize,
                std::move(bytes)
            };
        }

        return std::nullopt;
    }
}
