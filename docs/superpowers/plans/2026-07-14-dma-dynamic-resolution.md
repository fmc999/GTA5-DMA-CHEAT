# DMA Dynamic Resolution Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add tested pattern scanning, per-offset Enhanced runtime resolution with static fallback, and a staged Scatter memory backend without changing existing feature behavior.

**Architecture:** Pure byte-pattern and PE-section logic lives outside the VMM API so it can be unit tested with synthetic buffers. `MemoryBackend` wraps the existing MemProcFS handle and provides checked single and scatter access. `DMA::Initialize()` establishes static offsets first, applies independently validated Enhanced pattern results, and leaves Legacy on verified static fallbacks.

**Tech Stack:** C++23, Visual Studio MSBuild, Win32 PE structures, MemProcFS `VMMDLL_*`, PowerShell contract tests

---

## File Structure

- Create `GTA5_DMA/GTA5_DMA/PatternScanner.h/.cpp`: parse and scan wildcard byte patterns and resolve signed RIP-relative targets.
- Create `GTA5_DMA/GTA5_DMA/MemoryBackend.h/.cpp`: checked VMM reads/writes and RAII Scatter batches.
- Create `GTA5_DMA/GTA5_DMA/OffsetResolver.h/.cpp`: PE executable-section cache, Enhanced signature catalog, independent fallback reports.
- Modify `GTA5_DMA/GTA5_DMA/DMA.h/.cpp`: attach the backend, invoke the resolver, and migrate the core refresh path.
- Modify `GTA5_DMA/GTA5_DMA/Offsets.h`: expose a fallback snapshot helper without changing existing constants.
- Modify `GTA5_DMA/GTA5_DMA/GTA5_DMA.vcxproj` and `.filters`: register production files.
- Create `tests/DmaInfrastructureTests.cpp` and `tests/DmaInfrastructureTests.vcxproj`: standalone behavioral tests.
- Create `tests/DynamicResolutionContractTests.ps1`: source and project integration contracts.

### Task 1: Add the failing pattern scanner tests

**Files:**
- Create: `tests/DmaInfrastructureTests.cpp`
- Create: `tests/DmaInfrastructureTests.vcxproj`
- Test: `tests/DmaInfrastructureTests.cpp`

- [ ] **Step 1: Add a standalone test executable**

The test must include `PatternScanner.h` and assert exact matching, wildcard matching, malformed patterns, zero/multiple matches, positive and negative `rel32`, and bounds rejection. The initial source references the not-yet-created API:

```cpp
#include <cassert>
#include <cstdint>
#include <vector>
#include "../GTA5_DMA/GTA5_DMA/PatternScanner.h"

int main()
{
    using PatternScanner::ScanStatus;
    const std::vector<std::uint8_t> bytes{0x90, 0x48, 0x8B, 0x0D, 0x04, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90};
    auto exact = PatternScanner::FindUnique(bytes, "48 8B 0D 04 00 00 00");
    assert(exact.status == ScanStatus::Found && exact.offset == 1);
    auto wildcard = PatternScanner::FindUnique(bytes, "48 8B ?? ? ? ? ?");
    assert(wildcard.status == ScanStatus::Found && wildcard.offset == 1);
    assert(PatternScanner::FindUnique(bytes, "48 GG").status == ScanStatus::InvalidPattern);
    assert(PatternScanner::FindUnique(bytes, "CC").status == ScanStatus::NotFound);
    assert(PatternScanner::FindUnique(std::vector<std::uint8_t>{0x90, 0x90}, "90").status == ScanStatus::MultipleMatches);
    auto target = PatternScanner::ResolveRelativeTarget(bytes, 1, 3, 7, 0x1000);
    assert(target.has_value() && *target == 0x100C);
    assert(!PatternScanner::ResolveRelativeTarget(bytes, 10, 3, 7, 0x1000).has_value());
    return 0;
}
```

The `.vcxproj` is an x64 console application using C++23 and compiles `DmaInfrastructureTests.cpp` plus `../GTA5_DMA/GTA5_DMA/PatternScanner.cpp`.

- [ ] **Step 2: Verify RED**

Run:

```powershell
MSBuild.exe tests\DmaInfrastructureTests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

Expected: build failure because `PatternScanner.h/.cpp` do not exist.

### Task 2: Implement the pure pattern scanner

**Files:**
- Create: `GTA5_DMA/GTA5_DMA/PatternScanner.h`
- Create: `GTA5_DMA/GTA5_DMA/PatternScanner.cpp`
- Test: `tests/DmaInfrastructureTests.cpp`

- [ ] **Step 1: Define the public API**

```cpp
namespace PatternScanner
{
    enum class ScanStatus { Found, InvalidPattern, NotFound, MultipleMatches };
    struct ScanResult { ScanStatus status; std::size_t offset; std::string diagnostic; };
    ScanResult FindUnique(std::span<const std::uint8_t> bytes, std::string_view pattern);
    std::optional<std::uintptr_t> ResolveRelativeTarget(
        std::span<const std::uint8_t> bytes,
        std::size_t matchOffset,
        std::size_t displacementOffset,
        std::size_t instructionSize,
        std::uintptr_t bufferRuntimeAddress);
}
```

- [ ] **Step 2: Implement minimal parsing and matching**

Split on ASCII whitespace. Accept exactly two hexadecimal digits or `?`/`??`. Reject empty patterns and malformed tokens. Scan all valid starting positions, count matches, and return a unique offset only when exactly one match exists.

- [ ] **Step 3: Implement signed RIP resolution**

Bounds-check `matchOffset + displacementOffset + sizeof(int32_t)`, copy the displacement with `std::memcpy`, sign-extend it, and calculate `bufferRuntimeAddress + matchOffset + instructionSize + displacement` without reading outside the supplied span.

- [ ] **Step 4: Verify GREEN**

Build and run:

```powershell
MSBuild.exe tests\DmaInfrastructureTests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
tests\x64\Debug\DmaInfrastructureTests.exe
```

Expected: build exit code `0`, test executable exit code `0`.

### Task 3: Add and test the checked memory backend

**Files:**
- Create: `GTA5_DMA/GTA5_DMA/MemoryBackend.h`
- Create: `GTA5_DMA/GTA5_DMA/MemoryBackend.cpp`
- Modify: `tests/DmaInfrastructureTests.cpp`
- Modify: `tests/DmaInfrastructureTests.vcxproj`

- [ ] **Step 1: Add failing validation tests**

Add tests for a detached backend, zero addresses, zero sizes, oversized requests, and invalid Scatter batches. The intended API is:

```cpp
MemoryBackend memory;
std::uint64_t value = 0;
assert(!memory.Read(0x1000, &value, sizeof(value)));
assert(!memory.Write(0x1000, &value, sizeof(value)));
memory.Attach(reinterpret_cast<VMM_HANDLE>(1), 77);
assert(memory.IsAttached());
assert(!memory.Read(0, &value, sizeof(value)));
assert(!memory.Read(0x1000, nullptr, sizeof(value)));
assert(!memory.Read(0x1000, &value, 0));
memory.Reset();
assert(!memory.IsAttached());
```

Build and confirm RED because `MemoryBackend` does not exist.

- [ ] **Step 2: Implement checked single access**

`Read` calls `VMMDLL_MemReadEx` only after validation and succeeds only when the API returns true and `bytesRead == size`. `Write` calls `VMMDLL_MemWrite` only after validation and returns the API result.

- [ ] **Step 3: Implement `ScatterBatch` RAII**

`BeginScatter()` initializes a handle with `VMMDLL_FLAG_NOCACHE`. `PrepareRead` owns expected/read byte counts in stable `std::deque` storage. `PrepareWrite` copies payloads into owned byte vectors. `Execute` calls `VMMDLL_Scatter_Execute` and verifies every prepared read completed fully. The destructor always closes a non-null scatter handle.

- [ ] **Step 4: Keep unit tests linkable without hardware**

The test executable provides fake definitions for the seven `VMMDLL_Mem*`/`VMMDLL_Scatter_*` functions referenced by `MemoryBackend.cpp`. Fakes record validation calls and return deterministic success without initializing a real VMM device.

- [ ] **Step 5: Verify GREEN**

Build and run the test executable. Expected exit code: `0`.

### Task 4: Add failing resolver and CT catalog tests

**Files:**
- Create: `GTA5_DMA/GTA5_DMA/OffsetResolver.h`
- Create: `GTA5_DMA/GTA5_DMA/OffsetResolver.cpp`
- Modify: `tests/DmaInfrastructureTests.cpp`
- Modify: `tests/DmaInfrastructureTests.vcxproj`

- [ ] **Step 1: Add synthetic resolver tests**

Construct a synthetic buffer containing one RIP-relative signature and assert `ResolveOne` returns a module-relative value with `OffsetSource::Pattern`. Assert zero/multiple matches return the provided fallback with `OffsetSource::Fallback` and a non-empty diagnostic.

```cpp
SignatureSpec spec{"WorldPtr", "48 8B 0D ?? ?? ?? ??", 3, 7};
auto resolved = OffsetResolver::ResolveOne(spec, bytes, 0x140001000, 0x140000000, 0x4000000, 0x443D1E8);
assert(resolved.source == OffsetSource::Pattern);
assert(resolved.value == 0x1010);
```

Add catalog assertions for the five exact Enhanced patterns and an empty Legacy catalog. Build and verify RED because the resolver does not exist.

- [ ] **Step 2: Implement result and catalog types**

Define `SignatureSpec`, `OffsetSource`, `ResolvedOffset`, `OffsetSnapshot`, and `OffsetResolutionReport`. `GetCatalog(GameType::GTA5_Enhanced)` returns the five verified CT signatures; `GetCatalog(GameType::GTA5)` returns an empty span.

- [ ] **Step 3: Implement independent resolution**

`ResolveOne` delegates to `PatternScanner`, verifies the runtime target lies in `[moduleBase, moduleBase + imageSize)`, converts it to a module-relative offset, and otherwise returns the exact fallback with a diagnostic.

- [ ] **Step 4: Implement executable-section loading**

Read and validate `IMAGE_DOS_HEADER`, `IMAGE_NT_HEADERS64`, and section headers using a supplied `MemoryReader` callback. Select the first non-empty section with `IMAGE_SCN_MEM_EXECUTE`, cap it at 64 MiB, and read it in 1 MiB chunks. Return section runtime address, bytes, and module metadata.

- [ ] **Step 5: Verify GREEN**

Build and run `DmaInfrastructureTests.exe`. Expected exit code: `0`.

### Task 5: Add the failing integration contract

**Files:**
- Create: `tests/DynamicResolutionContractTests.ps1`

- [ ] **Step 1: Write source integration assertions**

The script reads `OffsetResolver.cpp`, `DMA.cpp`, `DMA.h`, `MemoryBackend.h`, and `GTA5_DMA.vcxproj` and fails unless:

- all five CT signatures are present;
- `OffsetSource::Pattern` and `OffsetSource::Fallback` are used;
- `DMA::Initialize()` invokes the resolver after `SetOffsetsByPackageName`;
- the four migrated DMA methods use `Memory()`/`BeginScatter`;
- all six new production files are registered in the project;
- Legacy static constants remain in `Offsets.h`.

- [ ] **Step 2: Verify RED**

Run:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\DynamicResolutionContractTests.ps1
```

Expected: failure because production integration is not registered.

### Task 6: Integrate resolver and Scatter backend into DMA

**Files:**
- Modify: `GTA5_DMA/GTA5_DMA/Offsets.h`
- Modify: `GTA5_DMA/GTA5_DMA/DMA.h`
- Modify: `GTA5_DMA/GTA5_DMA/DMA.cpp`
- Modify: `GTA5_DMA/GTA5_DMA/GTA5_DMA.vcxproj`
- Modify: `GTA5_DMA/GTA5_DMA/GTA5_DMA.vcxproj.filters`

- [ ] **Step 1: Add a fallback snapshot helper**

`Offsets.h` exposes a plain snapshot containing the current static selection after `SetOffsetsByPackageName`. It does not change the existing constants or package-name routing.

- [ ] **Step 2: Attach the backend during initialization**

Add `DMA::Memory()` returning a function-local static `MemoryBackend`. After PID/base discovery and static selection, attach it, load the executable section, resolve the selected catalog, assign each validated/fallback result to the existing runtime `Offsets::*` variables, and print one diagnostic line per field.

- [ ] **Step 3: Migrate checked global access**

`GetGlobalValue` calls `Memory().Read`; `SetGlobalValue` calls `Memory().Write`. `GetGlobalAddress` uses `Memory().Read` for the chunk pointer and validates a non-zero chunk address.

- [ ] **Step 4: Migrate the core refresh path**

`UpdateEssentials()` uses checked reads for the world and local player pointers, then one Scatter batch for the independent model, navigation, player-info, inventory, weapon-manager, and vehicle pointer fields. Required pointers are validated after execution. Weapon info, model hash, location, and vehicle navigation continue through the migrated checked helper methods.

- [ ] **Step 5: Reset backend state on close**

`DMA::Close()` resets the backend, clears handle/PID/base and cached addresses, then closes a non-null VMM handle exactly once.

- [ ] **Step 6: Register files and verify GREEN**

Run the dynamic-resolution contract and infrastructure executable. Expected: both exit code `0`.

### Task 7: Full regression and build verification

**Files:**
- Verify only

- [ ] **Step 1: Run all PowerShell contracts**

```powershell
Get-ChildItem tests\*.ps1 | ForEach-Object {
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File $_.FullName
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
```

- [ ] **Step 2: Run infrastructure tests**

```powershell
MSBuild.exe tests\DmaInfrastructureTests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
tests\x64\Debug\DmaInfrastructureTests.exe
```

- [ ] **Step 3: Build the application**

```powershell
MSBuild.exe GTA5_DMA\GTA5_DMA.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
MSBuild.exe GTA5_DMA\GTA5_DMA.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

- [ ] **Step 4: Check the diff**

```powershell
git diff --check
git status --short
```

Confirm only the planned files contain new changes and do not stage or commit overlapping user work.
