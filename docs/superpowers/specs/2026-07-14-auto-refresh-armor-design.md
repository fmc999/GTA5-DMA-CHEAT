# Auto Refresh Armor Design

## Goal

Add an independent armor auto-refresh toggle directly below the existing health auto-refresh control. When enabled, the feature reads the current armor value and writes `200.0f` only when the value is strictly below `70.0f`.

The existing continuous armor-lock feature remains available and unchanged.

## Behavior

- `ArmorManager::bAutoRefreshArmor` controls threshold-based refresh.
- `ArmorManager::ArmorRefreshThreshold` defaults to `70.0f`.
- `ArmorManager::ArmorRefreshValue` defaults to `200.0f`.
- `ArmorManager::bLockArmor` retains priority and continues writing `200.0f` every DMA frame when enabled.
- When armor lock is disabled, auto-refresh reads armor once per DMA frame and writes the target only for values `< 70.0f`.
- Values exactly equal to `70.0f` and values above it are not written.
- A failed read prevents any write and leaves the cached value unchanged for that frame.
- A failed write does not claim the armor reached the target.

## Data Flow

`DMAThreadEntry()` continues invoking `ArmorManager::OnDMAFrame()` once per frame. `ArmorManager` remains the single owner of the existing world-pointer chain and exposes the cached `currentArmor` value to the UI. The new branch reuses `UpdateArmor()` and `SetArmor()` rather than adding a second memory path.

The UI adds one `ConsoleTheme::ToggleRow` below the health refresh row. Its description reports the cached armor and target value, matching the existing health control style. The existing armor-lock row remains in its current section.

## Testing

- Add pure threshold tests for values below, equal to, and above `70.0f`.
- Add source contracts for the independent toggle, strict comparison, target value `200.0f`, lock priority, and UI placement below the health toggle.
- Run all existing PowerShell contracts, infrastructure tests, and Debug/Release builds.

## Scope

This change only adds the threshold-based armor refresh behavior and its UI control. It does not add Native invocation, DLL injection, shellcode, or anti-cheat bypass logic.
