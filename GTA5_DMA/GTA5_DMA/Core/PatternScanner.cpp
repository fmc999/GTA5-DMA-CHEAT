#include "PatternScanner.h"

#include <charconv>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
    struct PatternByte
    {
        std::uint8_t value = 0;
        bool wildcard = false;
    };

    std::optional<std::vector<PatternByte>> ParsePattern(std::string_view pattern)
    {
        std::vector<PatternByte> parsed;
        std::size_t cursor = 0;

        while (cursor < pattern.size())
        {
            while (cursor < pattern.size() &&
                   (pattern[cursor] == ' ' || pattern[cursor] == '\t' ||
                    pattern[cursor] == '\r' || pattern[cursor] == '\n'))
            {
                ++cursor;
            }

            if (cursor == pattern.size())
            {
                break;
            }

            const std::size_t tokenStart = cursor;
            while (cursor < pattern.size() && pattern[cursor] != ' ' &&
                   pattern[cursor] != '\t' && pattern[cursor] != '\r' &&
                   pattern[cursor] != '\n')
            {
                ++cursor;
            }

            const std::string_view token = pattern.substr(tokenStart, cursor - tokenStart);
            if (token == "?" || token == "??" || token == "**")
            {
                parsed.push_back(PatternByte{.wildcard = true});
                continue;
            }

            if (token.size() != 2)
            {
                return std::nullopt;
            }

            unsigned int value = 0;
            const auto [end, error] = std::from_chars(
                token.data(), token.data() + token.size(), value, 16);
            if (error != std::errc{} || end != token.data() + token.size() || value > 0xFF)
            {
                return std::nullopt;
            }

            parsed.push_back(PatternByte{.value = static_cast<std::uint8_t>(value)});
        }

        if (parsed.empty())
        {
            return std::nullopt;
        }

        return parsed;
    }
}

namespace PatternScanner
{
    ScanResult FindUnique(std::span<const std::uint8_t> bytes, std::string_view pattern)
    {
        const auto parsed = ParsePattern(pattern);
        if (!parsed)
        {
            return {ScanStatus::InvalidPattern, 0, "Pattern contains an invalid token."};
        }

        if (parsed->size() > bytes.size())
        {
            return {ScanStatus::NotFound, 0, "Pattern is larger than the scanned buffer."};
        }

        std::size_t matchOffset = 0;
        std::size_t matchCount = 0;
        const std::size_t lastStart = bytes.size() - parsed->size();

        for (std::size_t start = 0; start <= lastStart; ++start)
        {
            bool matches = true;
            for (std::size_t index = 0; index < parsed->size(); ++index)
            {
                const PatternByte& expected = (*parsed)[index];
                if (!expected.wildcard && bytes[start + index] != expected.value)
                {
                    matches = false;
                    break;
                }
            }

            if (!matches)
            {
                continue;
            }

            matchOffset = start;
            ++matchCount;
            if (matchCount > 1)
            {
                return {ScanStatus::MultipleMatches, 0, "Pattern matched more than once."};
            }
        }

        if (matchCount == 0)
        {
            return {ScanStatus::NotFound, 0, "Pattern was not found."};
        }

        return {ScanStatus::Found, matchOffset, {}};
    }

    std::optional<std::uintptr_t> ResolveRelativeTarget(
        std::span<const std::uint8_t> bytes,
        std::size_t matchOffset,
        std::size_t displacementOffset,
        std::size_t instructionSize,
        std::uintptr_t bufferRuntimeAddress)
    {
        if (matchOffset > bytes.size() || displacementOffset > bytes.size() - matchOffset)
        {
            return std::nullopt;
        }

        const std::size_t displacementIndex = matchOffset + displacementOffset;
        if (displacementIndex > bytes.size() ||
            sizeof(std::int32_t) > bytes.size() - displacementIndex)
        {
            return std::nullopt;
        }

        constexpr std::uintptr_t maxAddress = std::numeric_limits<std::uintptr_t>::max();
        if (matchOffset > maxAddress - bufferRuntimeAddress)
        {
            return std::nullopt;
        }

        const std::uintptr_t matchAddress = bufferRuntimeAddress + matchOffset;
        if (instructionSize > maxAddress - matchAddress)
        {
            return std::nullopt;
        }

        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes.data() + displacementIndex, sizeof(displacement));

        const std::uintptr_t instructionEnd = matchAddress + instructionSize;
        if (displacement >= 0)
        {
            const auto distance = static_cast<std::uintptr_t>(displacement);
            if (distance > maxAddress - instructionEnd)
            {
                return std::nullopt;
            }
            return instructionEnd + distance;
        }

        const auto distance = static_cast<std::uintptr_t>(-static_cast<std::int64_t>(displacement));
        if (distance > instructionEnd)
        {
            return std::nullopt;
        }
        return instructionEnd - distance;
    }
}
