# Auto Refresh Armor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an independent threshold-based armor refresh toggle below the existing health refresh control while preserving continuous armor lock behavior.

**Architecture:** Keep `ArmorManager` as the single owner of the existing armor pointer chain. Add configurable static state and a pure threshold predicate, then make `OnDMAFrame()` read once, honor lock priority, and write the target only after a successful read below the strict threshold. Route the existing armor chain through `DMA::Memory()` so the new behavior uses the checked DMA backend already integrated into the project.

**Tech Stack:** C++23, ImGui/`ConsoleTheme::ToggleRow`, PowerShell source contracts, the existing standalone x64 infrastructure test executable.

---

### Task 1: Add failing threshold and source-contract tests

**Files:**
- Modify: `tests/DmaInfrastructureTests.cpp`
- Create: `tests/ArmorRefreshContractTests.ps1`

- [ ] **Step 1: Add pure threshold assertions before implementation**

Include `../GTA5_DMA/GTA5_DMA/ArmorManager.h` in `DmaInfrastructureTests.cpp` and add these assertions near the other pure value tests:

```cpp
assert(ArmorManager::ShouldAutoRefresh(69.99f));
assert(!ArmorManager::ShouldAutoRefresh(70.0f));
assert(!ArmorManager::ShouldAutoRefresh(70.01f));
assert(!ArmorManager::ShouldAutoRefresh(std::numeric_limits<float>::quiet_NaN()));
```

The test must not initialize a VMM handle or call the memory chain.

- [ ] **Step 2: Add the failing PowerShell source contract**

Create `tests/ArmorRefreshContractTests.ps1` with this behavior:

```powershell
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$armorHeader = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'GTA5_DMA/GTA5_DMA/ArmorManager.h')
$armorSource = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'GTA5_DMA/GTA5_DMA/ArmorManager.cpp')
$menu = Get-Content -Raw -Encoding UTF8 (Join-Path $root 'GTA5_DMA/GTA5_DMA/MenuManager.cpp')

function Require([bool]$condition, [string]$message) {
    if (-not $condition) { throw "Armor refresh contract failed: $message" }
}

Require $armorHeader.Contains('bAutoRefreshArmor') 'independent armor toggle is missing'
Require $armorHeader.Contains('ArmorRefreshThreshold') 'armor threshold state is missing'
Require $armorHeader.Contains('ArmorRefreshValue') 'armor target state is missing'
Require $armorHeader.Contains('ShouldAutoRefresh') 'pure threshold predicate is missing'
Require $armorSource.Contains('bLockArmor') 'existing lock behavior is missing'
Require $armorSource.Contains('bAutoRefreshArmor') 'auto-refresh branch is missing'
Require $armorSource.Contains('ArmorRefreshValue') 'auto-refresh does not use the configured target'
Require $armorSource.Contains('ShouldAutoRefresh') 'strict threshold predicate is not used'
Require $armorSource.Contains('DMA::Memory().Read') 'armor reads bypass the checked backend'
Require $armorSource.Contains('DMA::Memory().Write') 'armor writes bypass the checked backend'

$healthIndex = $menu.IndexOf('refresh_health')
$armorIndex = $menu.IndexOf('refresh_armor')
Require ($healthIndex -ge 0 -and $armorIndex -gt $healthIndex) 'armor toggle is not below health toggle'
Require $menu.Contains('refresh_armor') 'armor toggle id is missing'
Require $menu.Contains('ArmorManager::bAutoRefreshArmor') 'armor toggle is not bound to ArmorManager'

Write-Host 'Armor refresh contract passed.'
```

- [ ] **Step 3: Verify RED**

Run:

```powershell
MSBuild.exe tests\DmaInfrastructureTests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\ArmorRefreshContractTests.ps1
```

Expected: the C++ build fails because `ArmorManager::ShouldAutoRefresh` and the new state do not exist; if the build is temporarily bypassed, the contract fails on the missing independent toggle.

### Task 2: Implement the threshold state and frame behavior

**Files:**
- Modify: `GTA5_DMA/GTA5_DMA/ArmorManager.h`
- Modify: `GTA5_DMA/GTA5_DMA/ArmorManager.cpp`

- [ ] **Step 1: Add independent state and predicate declarations**

Add these public inline members to `ArmorManager` without removing `bLockArmor`. Include `<cmath>` in the header for the predicate:

```cpp
#include <cmath>

static inline bool bAutoRefreshArmor = false;
static inline float ArmorRefreshThreshold = 70.0f;
static inline float ArmorRefreshValue = 200.0f;
static bool ShouldAutoRefresh(float armor) noexcept
{
    return std::isfinite(armor) && armor < ArmorRefreshThreshold;
}
```

- [ ] **Step 2: Make `OnDMAFrame()` preserve lock priority and threshold semantics**

Replace the current frame branch with this behavior:

```cpp
bool ArmorManager::OnDMAFrame()
{
    if (bLockArmor)
    {
        if (SetArmor(ArmorRefreshValue))
        {
            currentArmor = ArmorRefreshValue;
        }
        return true;
    }

    if (!UpdateArmor())
    {
        return false;
    }

    if (bAutoRefreshArmor && ShouldAutoRefresh(currentArmor) &&
        SetArmor(ArmorRefreshValue))
    {
        currentArmor = ArmorRefreshValue;
    }

    return true;
}
```

This keeps the cache unchanged after a failed read or failed write and does not write at exactly `70.0f`.

- [ ] **Step 3: Route the existing chain through the checked backend**

In `ReadArmorFromMemory`, replace each direct `VMMDLL_MemReadEx` call with `DMA::Memory().Read(address, &value, sizeof(value))`. In `WriteArmorToMemory`, replace the direct write with:

```cpp
return DMA::Memory().Write(finalAddr, &value, sizeof(value));
```

Keep the existing offsets and pointer chain unchanged: `BaseAddress + Offsets::WorldPtr`, dereference `+0x8`, then armor at `+0x150C`.

- [ ] **Step 4: Run the focused tests GREEN**

Run:

```powershell
MSBuild.exe tests\DmaInfrastructureTests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
tests\x64\Debug\DmaInfrastructureTests.exe
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\ArmorRefreshContractTests.ps1
```

Expected: build exit code `0`, test executable exit code `0`, and `Armor refresh contract passed.`

### Task 3: Add the UI control below health refresh

**Files:**
- Modify: `GTA5_DMA/GTA5_DMA/MenuManager.cpp`

- [ ] **Step 1: Add the independent toggle immediately after the health refresh detail**

Insert this block after the existing `RefreshHealth::bEnable` detail and before the following section divider:

```cpp
    char armorRefreshDescription[96] = {};
    std::snprintf(
        armorRefreshDescription,
        sizeof(armorRefreshDescription),
        "当前防弹衣 %.0f，低于 %.0f 自动恢复至 %.0f",
        ArmorManager::currentArmor,
        ArmorManager::ArmorRefreshThreshold,
        ArmorManager::ArmorRefreshValue);
    ConsoleTheme::ToggleRow(
        "refresh_armor",
        "自动刷新防弹衣",
        armorRefreshDescription,
        &ArmorManager::bAutoRefreshArmor);
```

Do not move or remove the existing `lock_armor` row.

- [ ] **Step 2: Re-run the source contract**

Run `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests\ArmorRefreshContractTests.ps1` and expect `Armor refresh contract passed.`

### Task 4: Full regression and build verification

**Files:**
- Verify only

- [ ] **Step 1: Run every PowerShell contract**

```powershell
Get-ChildItem tests\*.ps1 | ForEach-Object {
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File $_.FullName
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
```

Expected: every existing contract plus `Armor refresh contract passed.`.

- [ ] **Step 2: Build and run infrastructure tests**

```powershell
MSBuild.exe tests\DmaInfrastructureTests.vcxproj /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
tests\x64\Debug\DmaInfrastructureTests.exe
```

Expected: both commands exit with code `0`.

- [ ] **Step 3: Build the application in both configurations**

```powershell
MSBuild.exe GTA5_DMA\GTA5_DMA.sln /t:Build /p:Configuration=Debug /p:Platform=x64 /m /v:quiet
MSBuild.exe GTA5_DMA\GTA5_DMA.sln /t:Build /p:Configuration=Release /p:Platform=x64 /m /v:quiet
```

Expected: both builds exit with code `0`; existing MemProcFS C4200/C4201 warnings may remain.

- [ ] **Step 4: Check whitespace and preserve unrelated work**

```powershell
git diff --check
git status --short
```

Do not stage or commit implementation files because the worktree already contains user changes in overlapping modules.
