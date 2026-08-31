#include "OffsetResolver.h"

#include "PatternScanner.h"

#include <Windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace
{
    constexpr std::size_t kChunkSize = 1024 * 1024;
    constexpr std::size_t kMaxExecutableSectionSize = 64 * 1024 * 1024;
    constexpr int kChunkReadRetries = 3;

    constexpr OffsetResolver::SignatureSpec kEnhancedCatalog[] = {
        // 主特征码来自 GTA5_Enhanced_Offsets.CT；备用特征码来自本机验证过的
        // DMA dumper（同游戏版本 + 同设备实测解析成功，值为 0x434A958 等）。
        {"WorldPtr", "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 49 ?? 48 8D", 3, 7,
            1, {"48 8B 3D ? ? ? ? 49 8B B6"}},
        {"GlobalPtr", "48 8D 3D ?? ?? ?? ?? 31 DB 48 8D 2D ?? ?? ?? ?? 4C", 3, 7,
            1, {"48 8B 0D ? ? ? ? 0F 1F 44 00"}},
        {"BlipPtr", "48 8D 0D ? ? ? ? 41 B8 ? ? ? ? 31 D2 E8 ? ? ? ? 8B 0D", 3, 7,
            1, {"4C 8D 3D ? ? ? ? 49 8B 34 C7"}},
        {"PlayerMgrPtr", "75 0E 48 8B 05 ? ? ? ? 48 8B 88 F0 00 00 00", 5, 9},
        {"AimCPedPtr", "48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 4C 8D 05 ?? ?? ?? ?? BA", 3, 7},
        {"WaypointPtr", "48 8D 0D ?? ?? ?? ?? C6 44 08 ?? 01 C7", 3, 7},
        // 以下三条来自 GTA5_Enhanced_Offsets.CT（实体池/本地脚本/GTA+）
        {"LocalScriptsPtr", "48 8B 05 ? ? ? ? 48 89 34 F8 48 FF C7 48 39 FB 75 97", 3, 7},
        {"GTAPlusPtr", "48 8D 15 ? ? ? ? 41 B8 18 02 00 00 E8", 3, 7},
        // YimMenuV2 的 Ped 池加密指针（现代实现，跟版本维护）
        // 命中后: match + 0x18 + 3 处 RIP 相对 -> PoolEncryption*
        {"PedPoolPtr", "80 79 4B 00 0F 84 F5 00 00 00 48 89 F1", 0x1B, 0x1F}
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

        PatternScanner::ScanResult match = PatternScanner::FindUnique(bytes, spec.pattern);
        if (match.status != PatternScanner::ScanStatus::Found)
        {
            // 主特征码未命中：按序尝试备用特征码
            for (std::size_t i = 0; i < spec.alternativeCount && i < SignatureSpec::kMaxAlternatives; ++i)
            {
                match = PatternScanner::FindUnique(bytes, spec.alternativePatterns[i]);
                if (match.status == PatternScanner::ScanStatus::Found)
                    break;
            }
        }
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
        std::uint32_t imageSize,
        std::string* diagnostic)
    {
        const auto fail = [diagnostic](const char* reason) {
            if (diagnostic)
            {
                *diagnostic = reason;
            }
            return std::nullopt;
        };

        if (!reader || moduleBase == 0 || imageSize < sizeof(IMAGE_DOS_HEADER))
        {
            return fail("invalid arguments (moduleBase=0 or imageSize < DOS header size)");
        }

        IMAGE_DOS_HEADER dosHeader{};
        bool dosRead = false;
        for (int attempt = 0; attempt < kChunkReadRetries && !dosRead; ++attempt)
            dosRead = reader(moduleBase, &dosHeader, sizeof(dosHeader), "dos-header");
        if (!dosRead)
        {
            return fail("DOS header read failed (FPGA transfer error)");
        }
        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        {
            char reason[96];
            std::snprintf(reason, sizeof(reason),
                "DOS header magic mismatch: read 0x%04X (expected 0x5A4D 'MZ') — likely zero/garbage data",
                static_cast<unsigned>(dosHeader.e_magic));
            return fail(reason);
        }

        const std::int64_t ntOffset = dosHeader.e_lfanew;
        if (ntOffset < 0 || static_cast<std::uint64_t>(ntOffset) > imageSize ||
            sizeof(IMAGE_NT_HEADERS64) > imageSize - static_cast<std::size_t>(ntOffset))
        {
            char reason[96];
            std::snprintf(reason, sizeof(reason),
                "e_lfanew out of range: 0x%llX (imageSize 0x%X) — garbage DOS header",
                static_cast<unsigned long long>(ntOffset), imageSize);
            return fail(reason);
        }

        const auto ntAddress = moduleBase + static_cast<std::uintptr_t>(ntOffset);
        IMAGE_NT_HEADERS64 ntHeaders{};
        bool ntRead = false;
        for (int attempt = 0; attempt < kChunkReadRetries && !ntRead; ++attempt)
            ntRead = reader(ntAddress, &ntHeaders, sizeof(ntHeaders), "nt-headers");
        if (!ntRead)
        {
            return fail("NT headers read failed (FPGA transfer error)");
        }
        if (ntHeaders.Signature != IMAGE_NT_SIGNATURE ||
            ntHeaders.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        {
            char reason[128];
            std::snprintf(reason, sizeof(reason),
                "NT header signature mismatch: 0x%08X (expected 0x4550 'PE\0\0') — likely zero/garbage data",
                static_cast<unsigned>(ntHeaders.Signature));
            return fail(reason);
        }

        // PE 头里的 SizeOfImage 是权威值：VMMDLL 报告的 cbImageSize 偶发偏小/过期，
        // 会让 .text 的 RVA 被误判越界而跳过（表现为 no executable section found）。
        if (const std::uint32_t peImageSize = ntHeaders.OptionalHeader.SizeOfImage; peImageSize != 0)
        {
            if (peImageSize != imageSize && diagnostic)
            {
                char note[160];
                std::snprintf(note, sizeof(note),
                    "note: VMMDLL cbImageSize 0x%X != PE SizeOfImage 0x%X, using PE value",
                    imageSize, peImageSize);
                *diagnostic = note;
            }
            imageSize = peImageSize;
        }

        // 段数健全性校验（GTA5 主模块约 8~12 段；0 或离谱值说明段表是垃圾）
        if (ntHeaders.FileHeader.NumberOfSections == 0 ||
            ntHeaders.FileHeader.NumberOfSections > 96)
        {
            char reason[128];
            std::snprintf(reason, sizeof(reason),
                "NumberOfSections implausible: %u — section table is garbage",
                static_cast<unsigned>(ntHeaders.FileHeader.NumberOfSections));
            return fail(reason);
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
        bool sectionsRead = false;
        for (int attempt = 0; attempt < kChunkReadRetries && !sectionsRead; ++attempt)
            sectionsRead = reader(
                moduleBase + sectionHeadersOffset,
                sections.data(),
                sectionHeadersSize,
                "section-headers");
        if (!sectionsRead)
        {
            return fail("section headers read failed (FPGA transfer error)");
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
            // 参考 DMA 社区实践：大块读遇换出页失败时，改用小块（64KB）重读，
            // 逐块推进——换出页只在个别物理页上，缩小读粒度即可绕过。
            constexpr std::size_t kSubChunkSize = 64 * 1024;
            for (std::size_t offset = 0; offset < sectionSize; offset += kChunkSize)
            {
                const std::size_t chunkSize = (std::min)(kChunkSize, sectionSize - offset);
                bool chunkRead = false;
                for (int attempt = 0; attempt < kChunkReadRetries && !chunkRead; ++attempt)
                {
                    chunkRead = reader(moduleBase + sectionRva + offset, bytes.data() + offset, chunkSize, "code-section-chunk");
                }
                if (chunkRead)
                    continue;

                // 整块失败：降级为 64KB 小块逐段读
                bool subChunkFailed = false;
                for (std::size_t sub = 0; sub < chunkSize; sub += kSubChunkSize)
                {
                    const std::size_t subSize = (std::min)(kSubChunkSize, chunkSize - sub);
                    bool subRead = false;
                    for (int attempt = 0; attempt < kChunkReadRetries && !subRead; ++attempt)
                    {
                        subRead = reader(moduleBase + sectionRva + offset + sub, bytes.data() + offset + sub, subSize, "code-subchunk");
                    }
                    if (!subRead)
                    {
                        // 最后手段：单页（4KB）逐页读
                        for (std::size_t page = 0; page < subSize; page += 4096)
                        {
                            const std::size_t pageSize = (std::min)(std::size_t{4096}, subSize - page);
                            bool pageRead = false;
                            for (int attempt = 0; attempt < kChunkReadRetries && !pageRead; ++attempt)
                            {
                                pageRead = reader(moduleBase + sectionRva + offset + sub + page, bytes.data() + offset + sub + page, pageSize, "code-page");
                            }
                            if (!pageRead)
                            {
                                char reason[200];
                                std::snprintf(reason, sizeof(reason),
                                    "page read failed at RVA 0x%zX + 0x%zX (%zu bytes) after %d retries — page not resident",
                                    sectionRva, offset + sub + page, pageSize, kChunkReadRetries);
                                return fail(reason);
                            }
                        }
                    }
                }
            }

            if (diagnostic)
            {
                diagnostic->clear();
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

        {
            char reason[192];
            std::snprintf(reason, sizeof(reason),
                "no executable section among %u sections (imageSize 0x%X) — section table garbage or all RVAs out of range",
                static_cast<unsigned>(ntHeaders.FileHeader.NumberOfSections), imageSize);
            return fail(reason);
        }
    }
}
