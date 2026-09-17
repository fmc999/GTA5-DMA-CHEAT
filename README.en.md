<div align="center">

# GTA5 DMA Console

**An external GTA5 (Enhanced) memory console built on FPGA + DMA**

No injection · No game-file modification · No hooks · **After a game update, just edit a text file — no rebuild**

[![Platform](https://img.shields.io/badge/platform-Windows_x64-blue)](https://github.com/fmc999/GTA5-DMA-CHEAT)
[![Language](https://img.shields.io/badge/language-C%2B%2B23-00599C)](https://isocpp.org/)
[![UI](https://img.shields.io/badge/UI-Dear%20ImGui%20%2B%20DX11-e05361)](https://github.com/ocornut/imgui)
[![DMA](https://img.shields.io/badge/DMA-MemProcFS%20%2F%20FPGA-8A2BE2)](https://github.com/ufrisk/MemProcFS)
[![Build](https://github.com/fmc999/GTA5-DMA-CHEAT/actions/workflows/msbuild.yml/badge.svg)](https://github.com/fmc999/GTA5-DMA-CHEAT/actions/workflows/msbuild.yml)
[![Release](https://img.shields.io/badge/release-latest-2ea44f)](https://github.com/fmc999/GTA5-DMA-CHEAT/releases/latest)
[![License: Custom](https://img.shields.io/badge/license-Custom%20Non--Commercial-orange)](LICENSE)

[中文](README.md) · **English**

**Free release · Do not resell · For technical research and DMA read/write learning only**

</div>

---

## What this is

A **secondary machine** with an FPGA DMA card reads and writes the target machine's physical memory directly over PCIe —
the game never sees the tool:

| Principle | Detail |
|---|---|
| **No injection** | Nothing is loaded into the game process |
| **No file changes** | The game directory and anti-cheat files are untouched |
| **No hooks** | No byte of game code is modified |
| **Version-independent** | All key addresses are resolved at startup by **runtime signature scanning**, with plain-text external fallbacks |

The trade-off: **anything that draws to the screen is impossible by design** (no injection means no draw entry point and no way to call
internal game functions). See [Known limitations](#known-limitations).

---

## Quick start

1. Download `GTA5-DMA-vX.Y-win-x64.zip` from [Releases](https://github.com/fmc999/GTA5-DMA-CHEAT/releases/latest)
2. Extract anywhere — **don't take just the exe**; the bundled DLLs must sit next to it
3. Start the game on the target machine, then run `GTA5_DMA.exe` on the secondary machine
4. On first use, run the self-check:

```powershell
GTA5_DMA.exe --health
```

> `--health` **functionally tests** every key capability (DMA link, player pool, vehicle pool & model chain, script globals,
> tunables, BE lookup, …) and tells you **exactly which file to edit** for each failure. It also writes `GTA5_DMA_health.txt`.

---

## Screenshots

![Console](docs/screenshot-console.png)

Floating glass panel: header status pills (DMA / process / hotkeys / FPS), left navigation, grouped workspace cards, and a status bar
(live PID / base address / model hash). The panel is draggable and resizable; geometry and appearance are persisted to
`%LOCALAPPDATA%\GTA5_DMA\window.ini`. **Everything outside the panel is intentionally pure black** (for compositor overlay — not a render bug).

![Vehicle list](docs/screenshot-vehicle-list.png)

---

## Features

### Session players

| Capability | Detail |
|---|---|
| Player list | **9 columns**: index / name / RID / health / armor / distance / **vehicle** / **BE** / status; numeric columns right-aligned |
| Encrypted pool scan | Decrypts per YimMenuV2's `PoolEncryption` (rotl64) and walks `fwBasePool` instead of hardcoding indices |
| **Automatic BE ban lookup** | Every session player is looked up against BattlEye and marked in the **BE** column (`封` / `OK` / `queued`); results cached to `be_bans_cache.json` for 24 h |
| How the lookup works | Uses the **official BattlEye server library** (`BEServer_x64.dll`) — fully local, no browser, game-version independent; 10 concurrent lookups, ~70 s for 20 players, ban reasons byte-identical to third-party sites |
| Actions | Teleport to player / pull player to me / kill; teleports are offset 2 m to avoid clipping, and the panel shows the landing point plus read-back verification |
| Misc | Name search filter; join/leave notifications (off by default) |

### Session vehicles

| Capability | Detail |
|---|---|
| Vehicle list | Live vehicle-pool scan with a 50 m radius filter; model name / health / distance |
| Vehicle names | **921 official models** built in, plus `vehicle_names_extra.txt` for new DLC cars (one line each, **no rebuild**); full CJK glyph coverage embedded |
| Actions | One-click teleport to the vehicle (player placed beside it, 2 m offset) |

### Player

God mode · vehicle god mode (persistent state protection) · never wanted · auto health regen · auto armor refill · invisibility ·
no collision · speed control (incl. beast mode) · health / armor locking

### Vehicle editor

| Capability | Detail |
|---|---|
| Health | Vehicle / engine / body / tank read and write |
| One-click repair | Writes all four health values (1000 each) with **per-field read-back verification**; repairs the current vehicle, or every session vehicle within 50 m; auto-repair below 80 % (1.5 s throttle) |
| Handling & extras | Live handling data (accel / brake / traction / suspension) · extras · parachute · jet · jump restore · missile lock range · seatbelt |

### Economy & automation

| Capability | Detail |
|---|---|
| Business safes | One-click claim for 7 businesses (nightclub / arcade / agency / vehicle warehouse / bail office / garment factory / car wash), with an optional periodic claim (30 s default) |
| Heist cuts | Four-player cut writes for Apartment / Doomsday / Casino heists; **two-step confirmation** plus a write log — it only writes when you press the button, never in the background |
| Money values | Cayo Perico primary-target values and skydive rewards written as "original × multiplier (1–10×)", reverted when switched off |
| Phone silencer | Reads the call / in-call / phone-state script globals and writes 6 (silenced) when the conditions hold |
| GTA+ unlock | Writes `GTA_PLUS_ENABLED` / entitlement bits plus the engine-side flag; reverts on toggle-off or exit |
| Tunables (progress & unlocks) | RP multiplier (`XP_MULTIPLIER`) · free/instant character appearance · idle-kick immunity (`IDLEKICK_*` / `ConstrainedKick_*`); live tunable values are shown beside the page for easy verification |
| Read-only inspector | Script-thread enumeration (name / hash / stack base) used by the script-locals channel |

### Weapons / Teleport / UI

- **Weapons**: current weapon stats (damage / fire rate / range / recoil / accuracy) · infinite ammo · no reload · impact force · bullet speed
- **Teleport**: custom coordinates plus **55 presets** in 8 groups (general / casino / Cayo Perico); presets within 8 m merge automatically; `F5` map waypoint · `F6` objective
- **UI**: floating glass panel · wallpaper and CJK glyphs embedded in the exe · light/dark mode and accent colors · toast feedback · hotkeys on both machines

### Write safety (applies to every write feature)

- **Legality-range checks**: a value outside the expected range marks the entry "unresolved" and it is **never written**
- **Read-back verification**: every write is read back and compared; failures are reported explicitly
- **Pulse actions auto-revert** after 1.5 s
- **One-command kill switch**: `--all-off` (or Settings → Safety → "Shut everything off") disables every persistent-write toggle
  **and zeroes the corresponding bits in game memory** (with read-back verification)

---

## Long-lived design: no rebuild after a game update ⭐

This is the project's core design goal — **set it up once, use it for a long time**.

### Four kinds of data, four strategies

| Kind | Examples | After a game update |
|---|---|---|
| **Addressing** | `GlobalPtr` / `LocalScriptsPtr` / `GTAPlusPtr` … | **Re-resolved automatically**: four alternative signatures per offset, scanned and validated in order |
| **Runtime data** | tunable indices, script-global chunking, thread list & stacks | **Recomputed every launch**: names' `joaat` hashes are update-stable, so indices are looked up in `tunables.bin`, with value-sequence self-discovery as fallback |
| **Struct fields & script indices** | `PedVehiclePtr` / `VehicleModelInfo` / `ModelInfoHash` / `VehicleHealth` / phone indices … | **Just edit a text file**: see `GTA5_DMA_offsets.txt` |
| **Feature logic** | new features, new pages | only this needs a rebuild |

### Four external channels (all next to the exe — edit text, not code)

| File | Purpose |
|---|---|
| `GTA5_DMA_offsets.txt` | **Struct field offsets / script indices**; auto-generated on first run as a **commented file of current measured values** — just change the numbers |
| `GTA5_DMA_patterns.txt` | Extra signatures (`GlobalPtr = 48 8B 0D ? ? ? ? \| disp=3 insn=7`) used when every built-in candidate fails |
| `GTA5_DMA_tunables.txt` | Manual `name = index` overrides; or `bin = <path>` to point at another `tunables.bin` |
| `GTA5_DMA_diag.txt` | Diagnostic report written on every launch: what resolved, what didn't, and why |

### One command for health

```
GTA5_DMA.exe --health
```

11 **functional** checks (not "does the code contain a value"): DMA link / world & local player / script globals / tunables / player pool /
vehicle pool & model chain / script threads / phone silencer / BE lookup / offset override table / external override files —
each failure names the file to fix. Results also land in `GTA5_DMA_health.txt`.

Companion document: **[《更新自救说明.txt》](更新自救说明.txt)** (a one-page self-recovery guide after a game update, in Chinese).

### Value-sequence self-discovery (fallback)

A set of known values (e.g. the six Cayo Perico primary-target values `400000/560000/616000/910000/1100000/1900000`) appears
**contiguously and uniquely** inside the tunable block — so even if the whole index table moves, the correct location can be recovered.
Verified: with the index deliberately wrong, the sanity check refused it and the uniqueness search found the right slot.

---

## Command line

| Command | Purpose |
|---|---|
| `--health` | Longevity self-check (11 functional tests + per-item fix hints), also writes `GTA5_DMA_health.txt` |
| `--all-off` / `--god-off` | **Turn off every persistent-write toggle** and zero the corresponding bits in game memory (read-back verified) |
| `--gate-drill` | Write-gate drill: injects a real drift to prove automatic re-resolution + re-baselining works |
| `--ban-check <RID>` | Look up a single RID's BattlEye ban status |
| `--diag [path]` | Export diagnostics (offset resolution / tunable locations / global action slots / script threads) |
| `--vehicles-probe` | Vehicle pool and model-name resolution self-check |
| `--phone-probe [--write-test]` | Phone-silencer script-global resolution (plus an optional write round-trip) |

> While the tool is **running**, the FPGA/FTDI device is exclusively held, so other processes cannot initialize it —
> which is exactly why diagnostics are written by the running instance itself.

### Hotkeys

| Key | Action |
|---|---|
| `Insert` | Show / hide the console |
| `F5` | Teleport to map waypoint |
| `F6` | Teleport to objective (Enhanced) |
| `End` | Exit |

---

## Requirements

**Hardware**: an FPGA DMA card (75T series tested) and two machines (secondary machine runs the tool, target machine runs the game).

**Runtime (secondary machine)**: Windows x64; bundled `leechcore.dll` / `vmm.dll` / `FTD3XX.dll` / `FTD3XXWU.dll` —
**must sit next to the exe**; BE ban lookup additionally needs `BEServer_x64.dll` + `BEServer_x64.cfg` (both bundled).

**Development (only if you build it yourself)**: Visual Studio 2022 or newer with the C++23 workload, Windows SDK 10.0+.
Dear ImGui sources and MemProcFS headers/import libs are bundled in the repo.

---

## Build & test

```powershell
# Build
MSBuild.exe GTA5_DMA\GTA5_DMA.sln /t:Build /p:Configuration=Release /p:Platform=x64

# Contract tests (UI labels & layout, offset-resolution determinism, feature lifecycles, tunables / script globals / threads / diagnostics)
Get-ChildItem tests/*.ps1 | ForEach-Object { powershell -File $_.FullName }   # currently 19/19 passing

# Infrastructure tests (PatternScanner / MemoryBackend / OffsetResolver pure-logic assertions)
tests\x64\Release\DmaInfrastructureTests.exe
```

Pushing to `main` triggers the [MSBuild workflow](.github/workflows/msbuild.yml), which runs both suites.

<details>
<summary><b>Project layout & architecture</b></summary>

```text
GTA5_DMA/
├── GTA5_DMA/
│   ├── Core/          # DMA lifecycle, memory backend (VMMDLL + scatter), signature scanning & offset
│   │                  # resolution, runtime tables, diagnostics, Reclass.h structs, input manager
│   ├── Features/      # Feature modules (each implements OnDMAFrame()): player/vehicle lists, god mode,
│   │                  # repair, tunables, safe script-global writer, economy, phone silencer, BE lookup,
│   │                  # teleport, weapons, vehicle editor, …
│   ├── UI/            # DX11 + Win32 platform layer, glass shell, theme, wallpaper, fonts & glyph ranges,
│   │                  # toasts, page content
│   ├── assets/  tools/  Attic/   # wallpaper & fonts, generator scripts, retired feature sources
├── ImGui/             # Dear ImGui sources
├── MemProcFS/         # VMMDLL headers & import libs
tests/                 # contract tests (PowerShell) + infrastructure tests (C++)
docs/                  # design docs & screenshots
```

```text
main.cpp
 ├─ UI thread  ─── MyImGui::OnFrame ─── ConsoleShell::Render ─── feature pages
 └─ DMA thread ── DMA::DMAThreadEntry
                  ├─ ResolveRuntimeOffsets()  # signature-resolve all key pointers at startup
                  ├─ UpdateEssentials()       # scatter-refresh pointer chains
                  └─ Feature::OnDMAFrame()    # per-feature polling read/write
```

UI and DMA threads are fully decoupled and communicate through atomics (lock-free). All memory access goes through `MemoryBackend`,
with scatter batches on hot paths to keep PCIe traffic low.

</details>

---

## FAQ

**The game updated — does it still work?**
Run `--health` first. Most likely it **just works** (addresses are scanned live); if something is ✗, edit the file it names — no need to wait
for a new release. Full procedure in [《更新自救说明.txt》](更新自救说明.txt) (Chinese).

**Why can't I export diagnostics myself?**
The DMA device is exclusively held while the tool runs, so no other process can initialize it — hence the running instance writes
`GTA5_DMA_diag.txt` itself.

**Some vehicle names show as question marks?**
Vehicle names are runtime-dynamic text, so their glyphs must be baked into the font atlas. All CJK characters from the 921 official names
plus the extra-name table are already included; for a brand-new DLC car, add the name to `vehicle_names_extra.txt` and regenerate the
glyph ranges (`tools/gen_glyph_ranges.py`).

**How do I know a toggle isn't secretly writing memory?**
Every write reports its read-back; one-shot writes (e.g. heist cuts) require two-step confirmation and keep a write log, and never run in
the background. Any persistent-write toggle can be killed with `--all-off` or Settings → Safety → "Shut everything off".

**A feature does nothing?**
Run `--health`; make sure the game is in an **online session** (pool-based features read nothing while single-player or loading); then check
`GTA5_DMA_diag.txt`.

---

## Known limitations

**Hard boundaries of pure DMA** (not "unimplemented" — impossible):

- **On-screen rendering**: ESP boxes, wallhacks — without injection there is no draw entry point
- **Calling game natives**: spawning a vehicle out of thin air, calling the mechanic, requesting a personal vehicle — all go through internal
  natives like `CREATE_VEHICLE`
- **Hook-type behaviour**: anything that rewrites game code or replaces functions

**Other**

- The model-info table (`CBaseModelInfo`) is **an individual allocation inside a hash-keyed container**, not a contiguous array — so
  "apply any model to my car" can only reuse models already present in the session
- Network time / game clock live on the heap, not in the script-global area or the module's data section — so anything depending on them
  ("radar stealth / time control") stays **refused rather than guessed**
- Pool-based features (players / vehicles) read nothing in single-player or while loading
- Teleport-to-vehicle uses coordinates sampled at read time, so a moving target can land the player behind the car
- The area outside the panel is deliberately pure black (compositor overlay)
- Retired features (time control / heist dividend / session chaser) keep their sources under `Attic/` and are not compiled in
- **No active BattlEye bypass**; online behaviour is not guaranteed in any way

---

## Disclaimer

This project is for **technical research, software development and DMA read/write learning only**. Test in **single-player or private
sessions**. Users are responsible for complying with local laws, platform rules and terms of service. Game updates may change memory
layouts, and wrong offsets may cause misbehaviour or crash the target process. The author is not liable for any account, hardware, data,
direct or indirect loss. **Free release — do not resell.**

## Acknowledgements

- [MemProcFS](https://github.com/ufrisk/MemProcFS) / [LeechCore](https://github.com/ufrisk/LeechCore) — DMA memory access foundation
- [Dear ImGui](https://github.com/ocornut/imgui) — immediate-mode GUI
- [ReClass.NET](https://github.com/ReClassNET/ReClass.NET) — memory structure reversing
- [YimMenuV2](https://github.com/YimMenu/YimMenuV2) — reference for encrypted pool scanning and structures (ideas only, no code used)
- [AmIBattlEyeBanned](https://github.com/calamity-inc/AmIBattlEyeBanned) — the idea of querying bans through the official BattlEye server library

## License

Copyright © 2026 fmc999. Free release; **commercial use and unauthorised modification/redistribution are prohibited.**
See [LICENSE](LICENSE) / [LICENSE-zh-CN](LICENSE-zh-CN).
