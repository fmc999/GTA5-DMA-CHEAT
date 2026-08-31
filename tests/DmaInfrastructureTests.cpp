#include <cassert>

#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include "../GTA5_DMA/GTA5_DMA/Core/MemoryBackend.h"
#include "../GTA5_DMA/GTA5_DMA/Features/ArmorManager.h"
#include "../GTA5_DMA/GTA5_DMA/Core/OffsetResolver.h"
#include "../GTA5_DMA/GTA5_DMA/Core/PatternScanner.h"

namespace
{
    struct FakeScatterRead
    {
        PBYTE buffer = nullptr;
        PDWORD bytesRead = nullptr;
        DWORD size = 0;
    };

    int g_memoryReadCalls = 0;
    int g_memoryWriteCalls = 0;
    int g_scatterCloseCalls = 0;
    bool g_memoryReadSucceeds = true;
    bool g_memoryReadIsComplete = true;
    bool g_scatterInitializeSucceeds = true;
    bool g_scatterPrepareSucceeds = true;
    bool g_scatterExecuteSucceeds = true;
    bool g_scatterReadsComplete = true;
    std::vector<FakeScatterRead> g_scatterReads;
    PBYTE g_scatterWriteBuffer = nullptr;
    DWORD g_scatterWriteSize = 0;
    std::vector<std::uint8_t> g_executedWriteBytes;

    constexpr std::uintptr_t kFakeScatterHandleValue = 0x1234;

    void ResetMemoryFakes()
    {
        g_memoryReadCalls = 0;
        g_memoryWriteCalls = 0;
        g_scatterCloseCalls = 0;
        g_memoryReadSucceeds = true;
        g_memoryReadIsComplete = true;
        g_scatterInitializeSucceeds = true;
        g_scatterPrepareSucceeds = true;
        g_scatterExecuteSucceeds = true;
        g_scatterReadsComplete = true;
        g_scatterReads.clear();
        g_scatterWriteBuffer = nullptr;
        g_scatterWriteSize = 0;
        g_executedWriteBytes.clear();
    }
}

extern "C" BOOL VMMDLL_MemReadEx(
    VMM_HANDLE,
    DWORD,
    ULONG64,
    PBYTE buffer,
    DWORD size,
    PDWORD bytesRead,
    ULONG64)
{
    ++g_memoryReadCalls;
    if (buffer != nullptr && size != 0)
    {
        std::memset(buffer, 0x5A, size);
    }
    if (bytesRead != nullptr)
    {
        *bytesRead = g_memoryReadIsComplete ? size : size - 1;
    }
    return g_memoryReadSucceeds ? TRUE : FALSE;
}

extern "C" BOOL VMMDLL_MemWrite(VMM_HANDLE, DWORD, ULONG64, PBYTE, DWORD)
{
    ++g_memoryWriteCalls;
    return TRUE;
}

extern "C" VMMDLL_SCATTER_HANDLE VMMDLL_Scatter_Initialize(VMM_HANDLE, DWORD, DWORD)
{
    if (!g_scatterInitializeSucceeds)
    {
        return nullptr;
    }
    return reinterpret_cast<VMMDLL_SCATTER_HANDLE>(kFakeScatterHandleValue);
}

extern "C" BOOL VMMDLL_Scatter_PrepareEx(
    VMMDLL_SCATTER_HANDLE,
    QWORD,
    DWORD size,
    PBYTE buffer,
    PDWORD bytesRead)
{
    if (!g_scatterPrepareSucceeds)
    {
        return FALSE;
    }
    g_scatterReads.push_back({buffer, bytesRead, size});
    return TRUE;
}

extern "C" BOOL VMMDLL_Scatter_PrepareWrite(
    VMMDLL_SCATTER_HANDLE,
    QWORD,
    PBYTE buffer,
    DWORD size)
{
    if (!g_scatterPrepareSucceeds)
    {
        return FALSE;
    }
    g_scatterWriteBuffer = buffer;
    g_scatterWriteSize = size;
    return TRUE;
}

extern "C" BOOL VMMDLL_Scatter_Execute(VMMDLL_SCATTER_HANDLE)
{
    if (!g_scatterExecuteSucceeds)
    {
        return FALSE;
    }

    for (const FakeScatterRead& read : g_scatterReads)
    {
        std::memset(read.buffer, 0xA5, read.size);
        *read.bytesRead = g_scatterReadsComplete ? read.size : read.size - 1;
    }

    if (g_scatterWriteBuffer != nullptr)
    {
        g_executedWriteBytes.assign(
            g_scatterWriteBuffer,
            g_scatterWriteBuffer + g_scatterWriteSize);
    }
    return TRUE;
}

extern "C" VOID VMMDLL_Scatter_CloseHandle(VMMDLL_SCATTER_HANDLE)
{
    ++g_scatterCloseCalls;
    g_scatterReads.clear();
    g_scatterWriteBuffer = nullptr;
    g_scatterWriteSize = 0;
}

int main()
{
    using PatternScanner::ScanStatus;

    const std::vector<std::uint8_t> bytes{
        0x90,
        0x48, 0x8B, 0x0D, 0x04, 0x00, 0x00, 0x00,
        0x90, 0x90, 0x90, 0x90
    };

    const auto exact = PatternScanner::FindUnique(bytes, "48 8B 0D 04 00 00 00");
    assert(exact.status == ScanStatus::Found);
    assert(exact.offset == 1);

    const auto wildcard = PatternScanner::FindUnique(bytes, "48 8B ?? ? ? ? ?");
    assert(wildcard.status == ScanStatus::Found);
    assert(wildcard.offset == 1);

    assert(PatternScanner::FindUnique(bytes, "").status == ScanStatus::InvalidPattern);
    assert(PatternScanner::FindUnique(bytes, "48 GG").status == ScanStatus::InvalidPattern);
    // ** 通配符（用户 CT 特征码使用 ** 形式）
    {
        const std::vector<std::uint8_t> wild{0xAA, 0x11, 0x22, 0x33, 0x44};
        auto r1 = PatternScanner::FindUnique(wild, "AA ** ** ** 44");
        assert(r1.status == ScanStatus::Found && r1.offset == 0);
    }
    assert(PatternScanner::FindUnique(bytes, "4").status == ScanStatus::InvalidPattern);
    assert(PatternScanner::FindUnique(bytes, "CC").status == ScanStatus::NotFound);
    assert(PatternScanner::FindUnique(std::vector<std::uint8_t>{0x90, 0x90}, "90").status ==
           ScanStatus::MultipleMatches);

    const auto positiveTarget = PatternScanner::ResolveRelativeTarget(bytes, 1, 3, 7, 0x1000);
    assert(positiveTarget.has_value());
    assert(*positiveTarget == 0x100C);

    const std::vector<std::uint8_t> negativeDisplacement{
        0x48, 0x8B, 0x0D, 0xF5, 0xFF, 0xFF, 0xFF
    };
    const auto negativeTarget =
        PatternScanner::ResolveRelativeTarget(negativeDisplacement, 0, 3, 7, 0x2000);
    assert(negativeTarget.has_value());
    assert(*negativeTarget == 0x1FFC);

    assert(!PatternScanner::ResolveRelativeTarget(bytes, 10, 3, 7, 0x1000).has_value());
    assert(!PatternScanner::ResolveRelativeTarget(bytes, 1, 8, 7, 0x1000).has_value());
    assert(!PatternScanner::ResolveRelativeTarget(
                negativeDisplacement,
                0,
                3,
                7,
                (std::numeric_limits<std::uintptr_t>::max)() - 1)
                .has_value());

    assert(ArmorManager::ShouldAutoRefresh(69.99f));
    assert(!ArmorManager::ShouldAutoRefresh(70.0f));
    assert(!ArmorManager::ShouldAutoRefresh(70.01f));
    assert(!ArmorManager::ShouldAutoRefresh(std::numeric_limits<float>::quiet_NaN()));

    ResetMemoryFakes();
    MemoryBackend detachedMemory;
    std::uint64_t value = 0;
    assert(!detachedMemory.Read(0x1000, &value, sizeof(value)));
    assert(!detachedMemory.Write(0x1000, &value, sizeof(value)));
    assert(g_memoryReadCalls == 0);
    assert(g_memoryWriteCalls == 0);
    assert(!detachedMemory.BeginScatter().IsValid());

    detachedMemory.Attach(reinterpret_cast<VMM_HANDLE>(1), 77);
    assert(detachedMemory.IsAttached());
    assert(!detachedMemory.Read(0, &value, sizeof(value)));
    assert(!detachedMemory.Read(0x1000, nullptr, sizeof(value)));
    assert(!detachedMemory.Read(0x1000, &value, 0));
    assert(!detachedMemory.Read(
        0x1000,
        &value,
        static_cast<std::size_t>((std::numeric_limits<DWORD>::max)()) + 1));
    assert(g_memoryReadCalls == 0);

    assert(detachedMemory.Read(0x1000, &value, sizeof(value)));
    assert(g_memoryReadCalls == 1);
    g_memoryReadIsComplete = false;
    assert(!detachedMemory.Read(0x1000, &value, sizeof(value)));
    g_memoryReadIsComplete = true;

    assert(detachedMemory.Write(0x1000, &value, sizeof(value)));
    assert(g_memoryWriteCalls == 1);
    assert(!detachedMemory.Write(0, &value, sizeof(value)));
    assert(!detachedMemory.Write(0x1000, nullptr, sizeof(value)));
    assert(!detachedMemory.Write(0x1000, &value, 0));

    {
        auto scatter = detachedMemory.BeginScatter();
        assert(scatter.IsValid());
        std::uint32_t firstRead = 0;
        std::uint64_t secondRead = 0;
        assert(!scatter.PrepareRead(0, &firstRead, sizeof(firstRead)));
        assert(!scatter.PrepareRead(0x2000, nullptr, sizeof(firstRead)));
        assert(scatter.PrepareRead(0x2000, &firstRead, sizeof(firstRead)));
        assert(scatter.PrepareRead(0x3000, &secondRead, sizeof(secondRead)));

        std::vector<std::uint8_t> writePayload{1, 2, 3, 4};
        assert(scatter.PrepareWrite(0x4000, writePayload.data(), writePayload.size()));
        writePayload.assign(writePayload.size(), 0xFF);

        assert(scatter.Execute());
        assert(firstRead == 0xA5A5A5A5);
        assert(secondRead == 0xA5A5A5A5A5A5A5A5);
        assert((g_executedWriteBytes == std::vector<std::uint8_t>{1, 2, 3, 4}));
    }
    assert(g_scatterCloseCalls == 1);

    {
        auto scatter = detachedMemory.BeginScatter();
        std::uint32_t incompleteRead = 0;
        assert(scatter.PrepareRead(0x5000, &incompleteRead, sizeof(incompleteRead)));
        g_scatterReadsComplete = false;
        assert(!scatter.Execute());
    }
    assert(g_scatterCloseCalls == 2);

    detachedMemory.Reset();
    assert(!detachedMemory.IsAttached());

    const std::vector<std::uint8_t> resolverBytes{
        0x48, 0x8B, 0x0D, 0x09, 0x00, 0x00, 0x00,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90
    };
    const OffsetResolver::SignatureSpec worldSpec{
        "WorldPtr",
        "48 8B 0D ?? ?? ?? ??",
        3,
        7
    };
    const auto resolved = OffsetResolver::ResolveOne(
        worldSpec,
        resolverBytes,
        0x140001000,
        0x140000000,
        0x4000000,
        0x443D1E8);
    assert(resolved.source == OffsetResolver::OffsetSource::Pattern);
    assert(resolved.value == 0x1010);
    assert(resolved.diagnostic.empty());

    const auto fallbackForMissing = OffsetResolver::ResolveOne(
        worldSpec,
        std::span<const std::uint8_t>{},
        0x140001000,
        0x140000000,
        0x4000000,
        0x443D1E8);
    assert(fallbackForMissing.source == OffsetResolver::OffsetSource::Fallback);
    assert(fallbackForMissing.value == 0x443D1E8);
    assert(!fallbackForMissing.diagnostic.empty());

    auto duplicateBytes = resolverBytes;
    duplicateBytes.insert(duplicateBytes.end(), resolverBytes.begin(), resolverBytes.begin() + 7);
    const auto fallbackForMultiple = OffsetResolver::ResolveOne(
        worldSpec,
        duplicateBytes,
        0x140001000,
        0x140000000,
        0x4000000,
        0x443D1E8);
    assert(fallbackForMultiple.source == OffsetResolver::OffsetSource::Fallback);
    assert(fallbackForMultiple.value == 0x443D1E8);
    assert(!fallbackForMultiple.diagnostic.empty());

    const OffsetResolver::SignatureSpec outsideSpec{
        "Outside",
        "48 8B 0D ?? ?? ?? ??",
        3,
        7
    };
    const std::vector<std::uint8_t> outsideBytes{
        0x48, 0x8B, 0x0D, 0x00, 0xE0, 0xFF, 0xFF
    };
    const auto fallbackForOutsideTarget = OffsetResolver::ResolveOne(
        outsideSpec,
        outsideBytes,
        0x140001000,
        0x140000000,
        0x4000000,
        0x443D1E8);
    assert(fallbackForOutsideTarget.source == OffsetResolver::OffsetSource::Fallback);
    assert(fallbackForOutsideTarget.value == 0x443D1E8);

    const auto enhancedCatalog = OffsetResolver::GetCatalog(GameType::GTA5_Enhanced);
    assert(enhancedCatalog.size() == 10);
    assert(enhancedCatalog[0].name == "WorldPtr");
    assert(enhancedCatalog[0].pattern == "48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 49 ?? 48 8D");
    assert(enhancedCatalog[1].name == "GlobalPtr");
    assert(enhancedCatalog[1].pattern == "48 8D 3D ?? ?? ?? ?? 31 DB 48 8D 2D ?? ?? ?? ?? 4C");
    assert(enhancedCatalog[2].name == "BlipPtr");
    assert(enhancedCatalog[2].pattern == "48 8D 0D ? ? ? ? 41 B8 ? ? ? ? 31 D2 E8 ? ? ? ? 8B 0D");
    assert(enhancedCatalog[3].name == "PlayerMgrPtr");
    assert(enhancedCatalog[3].pattern == "75 0E 48 8B 05 ? ? ? ? 48 8B 88 F0 00 00 00");
    assert(enhancedCatalog[4].name == "AimCPedPtr");
    assert(enhancedCatalog[4].pattern == "48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 4C 8D 05 ?? ?? ?? ?? BA");
    assert(enhancedCatalog[5].name == "WaypointPtr");
    assert(enhancedCatalog[5].pattern == "48 8D 0D ?? ?? ?? ?? C6 44 08 ?? 01 C7");
    assert(enhancedCatalog[6].name == "LocalScriptsPtr");
    assert(enhancedCatalog[6].pattern == "48 8B 05 ? ? ? ? 48 89 34 F8 48 FF C7 48 39 FB 75 97");
    assert(enhancedCatalog[7].name == "GTAPlusPtr");
    assert(enhancedCatalog[7].pattern == "48 8D 15 ? ? ? ? 41 B8 18 02 00 00 E8");
    assert(enhancedCatalog[8].name == "PedPoolPtr");
    assert(enhancedCatalog[8].pattern == "80 79 4B 00 0F 84 F5 00 00 00 48 89 F1");
    assert(enhancedCatalog[9].name == "VehiclePoolPtr");
    assert(enhancedCatalog[9].pattern == "48 8B 05 ? ? ? ? ?? ?? ?? 48 83 78 18 0D");
    assert(OffsetResolver::GetCatalog(GameType::GTA5).empty());

    return 0;
}
