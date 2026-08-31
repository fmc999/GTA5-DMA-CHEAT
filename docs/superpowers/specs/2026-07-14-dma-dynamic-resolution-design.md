# DMA Dynamic Resolution and Scatter Backend Design

## Goal

Add a staged memory backend, executable-section pattern scanner, and per-offset runtime resolver to the GTA5 DMA controller without changing existing feature entry points or write semantics.

## Scope

The first implementation supports process selection for both `GTA5_Enhanced.exe` and `GTA5.exe`.

- Enhanced uses verified signatures from `CT_Scripts/GTA5_Enhanced_Offsets.CT` and `CT_Scripts/GTA5_Enhanced_Offsets_62D29736.CT`.
- Legacy uses the same resolver interface but keeps the existing static offsets until a verified Legacy signature catalog exists. No guessed Legacy signatures are added.
- `TimeBasePtr` remains static because the available CT files do not provide a stable signature for it.
- Existing feature modules remain source-compatible and are not bulk-migrated in this phase.
- Dynamic resolution is read-only. It never writes to the target process.

## CT Source Provenance

`CT_Scripts/5.13GTA5传承版和增强版通用.CT` contains static Cheat Engine entries such as module-relative addresses and pointer chains. It does not contain `aobscanmodule` definitions, so it is used for cross-checking field offsets and pointer chains, not as an AOB source.

The AOB catalog is extracted from the two Enhanced offset scanner CT files. The initial verified signatures are:

| Runtime offset | Signature | Displacement | Instruction size |
| --- | --- | ---: | ---: |
| `WorldPtr` | `48 8B 0D ?? ?? ?? ?? 48 85 C9 74 ?? 48 8B 49 ?? 48 8D` | `3` | `7` |
| `GlobalPtr` | `48 8D 3D ?? ?? ?? ?? 31 DB 48 8D 2D ?? ?? ?? ?? 4C` | `3` | `7` |
| `BlipPtr` | `48 8D 0D ? ? ? ? 41 B8 ? ? ? ? 31 D2 E8 ? ? ? ? 8B 0D` | `3` | `7` |
| `PlayerMgrPtr` | `75 0E 48 8B 05 ? ? ? ? 48 8B 88 F0 00 00 00` | `5` | `9` |
| `AimCPedPtr` | `48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 8B 0D ?? ?? ?? ?? 4C 8D 05 ?? ?? ?? ?? BA` | `3` | `7` |

The scanner treats both `?` and `??` as wildcards. The resolver calculates `match + instructionSize + sign_extend(rel32)` and converts the target address to a module-relative offset.

## Components

### `MemoryBackend`

`MemoryBackend.h/.cpp` owns the VMM handle and PID association for the new code path. It exposes:

```cpp
bool Read(uintptr_t address, void* buffer, std::size_t size);
bool Write(uintptr_t address, const void* buffer, std::size_t size);
ScatterBatch BeginScatter();
```

`ScatterBatch` is an RAII wrapper around `VMMDLL_SCATTER_HANDLE`. It supports read and write preparation and returns `false` for preparation or execution failures. Pending writes copy their payload into owned storage so caller-local variables cannot expire before execution. A batch is bound to the PID and handle used to create it and is not reused after detach or process replacement.

The existing `DMA::vmh` and `DMA::PID` remain available to untouched feature code. The new backend is introduced incrementally rather than replacing every raw VMM call.

### `PatternScanner`

`PatternScanner.h/.cpp` is a pure, VMM-independent component. It provides:

- token parsing for hexadecimal bytes and wildcards;
- bounded matching over a byte span;
- unique-match enforcement;
- signed `rel32` extraction with bounds checking;
- target-address calculation from a match.

It rejects malformed patterns, out-of-range displacement reads, zero matches, and multiple matches. It does not silently choose the first result.

### `OffsetResolver`

`OffsetResolver.h/.cpp` owns `SignatureSpec`, the per-game catalog, module-cache metadata, and resolution diagnostics.

```cpp
enum class OffsetSource { Fallback, Pattern };

struct ResolvedOffset {
    uintptr_t value;
    OffsetSource source;
    std::string diagnostic;
};
```

Each core pointer is resolved independently. A failed `BlipPtr` scan leaves the static `BlipPtr` fallback in place and does not invalidate a successfully resolved `WorldPtr`.

## Module Cache

The resolver reads the target module's PE headers, identifies bounded executable sections, and copies the selected executable section into a local cache. The cache is keyed by `PID`, module base, and image size. It is discarded when the process or base address changes.

The cache is only used for pattern matching. The final address is always computed against the current runtime module base. The cache loader validates `MZ`, `PE` signatures, section bounds, executable flags, and a maximum section size before allocating memory.

## Initialization Flow

`DMA::Initialize()` performs the following sequence:

1. Initialize VMM and select `GTA5_Enhanced.exe` or `GTA5.exe`.
2. Obtain PID and module base.
3. Call `Offsets::SetOffsetsByPackageName()` to establish static fallbacks.
4. Attach `MemoryBackend` and build the module executable-section cache.
5. Run `OffsetResolver` for the selected game type.
6. Apply only validated pattern results to `Offsets` and log `pattern` or `fallback` for every field.
7. Continue into the existing DMA loop.

Failure to initialize the scanner is non-fatal when static fallbacks are available. Failure to initialize VMM, PID, or module base remains fatal as it is today.

## Migration Boundary

The first pass migrates only:

- `DMA::UpdateEssentials()`;
- `DMA::UpdatePlayerCurrentLocation()`;
- `DMA::UpdateVehicleInformation()`;
- `DMA::GetGlobalValue()` and `DMA::SetGlobalValue()`.

The core refresh path may use two or more scatter batches because pointer chains are dependent: resolve the next pointer only after the previous batch completes, then batch independent component reads. Existing Feature files retain their public interfaces and raw VMM calls until a later, separately tested migration.

## Error Handling and Safety

- All reads and writes require a valid VMM handle, PID, non-zero address, and non-zero size.
- A failed read never overwrites a previously valid cached value with an uninitialized buffer.
- A failed write returns `false` to the caller.
- Dynamic offset resolution never performs writes.
- Resolver diagnostics identify the signature, match count, bounds failure, or fallback reason.
- No Native invocation, DLL injection, shellcode execution, or anti-cheat bypass is included.

## Testing

### Pure unit tests

Create `tests/PatternScannerTests.cpp` and cover:

1. exact byte matching;
2. `?` and `??` wildcard matching;
3. malformed tokens;
4. zero and multiple matches;
5. signed `rel32` target calculation;
6. displacement and target bounds failures.

### Contract tests

Create a PowerShell contract test that checks:

- all five Enhanced signatures are present with their displacement and instruction-size metadata;
- `OffsetSource` diagnostics and static fallback application exist;
- new source files are registered in `GTA5_DMA.vcxproj`;
- Legacy static fallback remains available.

### Build and regression verification

Run the existing PowerShell contract tests, compile the standalone scanner test, run `git diff --check`, and build both:

```powershell
MSBuild.exe GTA5_DMA\GTA5_DMA.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
MSBuild.exe GTA5_DMA\GTA5_DMA.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m /v:minimal
```

Without a live DMA/GTA environment, verification can prove scanner behavior, fallback behavior, compilation, and contracts, but not a successful in-game pattern match.

## Non-Goals

- No Native invoker implementation.
- No DLL delivery or target-process injection.
- No new gameplay features.
- No bulk rewrite of existing Feature modules.
- No unverified Legacy signatures.
