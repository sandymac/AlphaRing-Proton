# AlphaRing on SteamOS / Proton (Steam Machine, Steam Deck)

This fork exists because the upstream AlphaRing DLL crashes at startup under Proton on a very
common prefix setup, showing only a bare **"Fatal error!"** dialog. This page explains what
fails, why, what this fork changes, and how to install and debug it. Everything below was
worked out on a Valve Steam Machine on 2026-09-06.

Tested configuration:

| Item | Value |
|---|---|
| Device | Valve Steam Machine, SteamOS 3.8.16 (build 20260716.1) |
| Proton | GE-Proton11-6 (forced under Properties → Compatibility) |
| Halo MCC | Steam build 1.3528.0.0 (buildid 19905945) |
| Prefix | `compatdata/976730`, with `vcrun2019` installed by winetricks (msvcp140 14.29.30157) |
| Result | 2-player campaign split-screen co-op working |

## TL;DR install

1. Download `WTSAPI32.dll` from this repo's Releases (verify with the `.sha256` file).
2. Copy it to the game's binaries folder. **The path is case-sensitive on Linux:**
   ```
   ~/.local/share/Steam/steamapps/common/Halo The Master Chief Collection/MCC/Binaries/Win64/WTSAPI32.dll
   ```
   A hand-made lowercase `mcc/binaries/win64/` folder is silently ignored by the game.
3. In Steam: MCC → Properties → Launch Options:
   ```
   WINEDLLOVERRIDES="WTSAPI32=n,b" %command%
   ```
4. Properties → Compatibility → force a Proton version. GE-Proton11-6 is what was tested.
5. Launch and pick **"Play Halo: MCC Anti-Cheat Disabled"**.
6. At the main menu press **F4** (or **Start + Left-Stick click** on a pad) to open the AlphaRing
   menu. Set the player count, bind a controller to each player, close the menu, start a mission.

If the menu appears, you are done. If not, see Debugging below.

## What was crashing, and why

**Symptom:** the game reaches the title screen, a small dialog reading only "Fatal error!" floats
over it, and the game exits when you click OK. Nothing useful is logged.

**Mechanism (from `PROTON_LOG=1` plus the build's PDB):**

- Wine loads the native `WTSAPI32.dll` correctly. About 0.24 s later AlphaRing's init thread dies
  with `EXCEPTION_ACCESS_VIOLATION` reading address 0 inside `MSVCP140.dll`. The backtrace is
  `std::_Mutex_base::lock` ← `AlphaRing::Log::Init`. Unreal's process-wide unhandled-exception
  handler shows the "Fatal error!" box and terminates the game.
- Binaries built with Visual Studio 2022 17.10 or newer (MSVC toolset 14.40+) construct
  `std::mutex` as a zero-initialised `constexpr` object and never call `_Mtx_init_in_situ`.
  Locking such a mutex requires a `msvcp140.dll` of version 14.40 or newer. Older runtimes read a
  pointer out of the zeroed storage and crash. Microsoft documents this break.
- A Proton prefix prepared with `winetricks vcrun2019` (the advice in most Linux AlphaRing guides)
  carries `msvcp140.dll` **14.29**, so the very first `LOG_INFO` in the mod locks a mutex and dies.
  Wine's builtin `msvcp140` has the same limitation.
- On Windows the system runtime is current, which is why the same DLL works there.

You can see the problem in the binaries: the upstream release imports `_Mtx_lock` but not
`_Mtx_init_in_situ`. This fork's build imports both.

## What this fork changes versus upstream

1. **`CMakeLists.txt`:** `add_compile_definitions(_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR)`.
   This makes every `std::mutex` in code compiled here call `_Mtx_init_in_situ`, so the DLL works
   with any `msvcp140`, including the 14.29 redist and Wine's builtin. This is the actual fix.
2. **`src/log/Log.cpp`, `src/log/Log.h`:** file logging ported from kirklandsig/AlphaRing.
   The mod now writes `alpharing.log` next to the DLL, flushed on every line, and no longer aborts
   the game if a console window cannot be allocated. The `LOG_*` macros are null-guarded so
   logging can never itself crash the game.
3. This document and a banner in `README.md`.

No gameplay, offset, or UI code differs from upstream megabitt01/AlphaRing.

## Alternative fix without this fork

Install a Visual C++ redistributable of version 14.40 or newer into the prefix. Then the
unmodified upstream DLL works. Caveat: on GE-Proton 11.x, `protontricks`/`winetricks` currently
fail to install VC runtimes (GloriousEggroll/proton-ge-custom issue #705). Running the official
`VC_redist.x64.exe` directly through Proton's `wine` in that prefix does work.

## Debugging

**Is the DLL loading at all?** Add `PROTON_LOG=1` in front of the launch options. Proton writes
`~/steam-976730.log`. Look for:

```
grep -n 'WTSAPI32' ~/steam-976730.log
```

You want a line ending in `WTSAPI32.dll" ... : native`. If you only see `builtin`, the override
is not being applied. A registry override survives any environment scrubbing:

```
protontricks -c "wine reg.exe add 'HKCU\Software\Wine\DllOverrides' /v wtsapi32 /d native /f" 976730
```

**Is anti-cheat actually off?** The first lines of the Proton log show the command line. It must
contain `-no-eac`. In this test the launch picker was not inverted, unlike some 2023 reports.

**Did the mod initialise?** `MCC/Binaries/Win64/alpharing.log` should start with
`=== AlphaRing Started ===` and include `Game Version[Steam]: 1.3528.0.0`. A zero-byte log means
it crashed before the first line (the mutex bug, or something equally early).

**Where did it crash?** Search the Proton log for `Unhandled exception` and the
`backtrace:` lines above it. Offsets like `WTSAPI32.dll + 0x6D03A` resolve against the `.pdb`
published with each release using any Windows symbolizer (DbgHelp `SymFromAddr`,
`llvm-symbolizer --obj=WTSAPI32.dll --relative-address`, or WinDbg `ln`).

**Files the mod writes**, relative to the game's working directory (the game root):

| File | Purpose |
|---|---|
| `MCC/Binaries/Win64/alpharing.log` | Log (this fork) |
| `alpha_ring_menu.cfg` | Menu hotkeys and controller profiles |
| `MCC/Binaries/Win64/alpha_ring_menu.bin` | Saved menu state |
| `../../../alpha_ring/` | Created by the mod; resolves to the Steam root, e.g. `~/.local/share/Steam/alpha_ring/` |

Seeing `alpha_ring/` appear in your Steam root is proof the DLL loaded and ran at least a little.

**Remove `PROTON_LOG=1` (or set `PROTON_LOG=0`) when done.** The log grows to tens of MB per session.

## Controllers

- Steam assigns controller order by first input. Press a button on each pad, in player order, at
  the game's main menu before opening the AlphaRing menu. Reorder any time via the Steam button →
  Controller Settings; the order does not persist across launches.
- Xbox-style pads: consider Properties → Controller → Disable Steam Input so the game sees raw
  XInput devices.
- Other pads (8BitDo, DualSense, Switch Pro): keep Steam Input enabled and turn on
  Steam → Settings → Controller → "Xbox Configuration Support" so each pad is presented as XInput.
- Four players is the hard ceiling: the engine's split-screen tables and XInput both stop at 4.

## Building from source

Upstream's README omits two required flags. The project needs SDL2 and SDL2_mixer through vcpkg
manifest mode and links their static targets, so configure with the toolchain file and the
static-md triplet:

```powershell
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=<path-to-vcpkg>/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static-md
msbuild build\WTSAPI32.vcxproj -p:Configuration=Release -p:Platform=x64 -m
```

Output: `build\Release\WTSAPI32.dll` (and `.pdb`). Visual Studio 2022 Community with the C++
desktop workload and CMake 3.27+ is sufficient. The `LNK4098: defaultlib 'LIBCMT' conflicts`
warning is expected.

## Credits and references

- Upstream: [megabitt01/AlphaRing](https://github.com/megabitt01/AlphaRing) (formerly thejackbitt),
  itself descended from [WinterSquire/AlphaRing](https://github.com/WinterSquire/AlphaRing).
- File logging and Proton hardening: [kirklandsig/AlphaRing](https://github.com/kirklandsig/AlphaRing).
- Microsoft, on the `std::mutex` constexpr constructor change in VS 2022 17.10 and the
  `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR` escape hatch.
- GE-Proton issues [#705](https://github.com/GloriousEggroll/proton-ge-custom/issues/705)
  (winetricks vcrun on GE-Proton 11) and
  [#635](https://github.com/GloriousEggroll/proton-ge-custom/issues/635) (`WINEDLLOVERRIDES` scrubbing).
