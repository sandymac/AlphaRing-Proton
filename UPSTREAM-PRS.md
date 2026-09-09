# Fixes to submit upstream (megabitt01/AlphaRing)

Ledger of changes in this fork that stand on their own as pull requests. Each entry lists the
files, the evidence, and whether it has been verified in play. Keep one fix per commit on this
fork so they cherry-pick cleanly onto upstream `master-chief`.

| # | Fix | Files | Status | Fork commit |
|---|---|---|---|---|
| 1 | Proton startup crash: build with `_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR` | `CMakeLists.txt` (1 line) | **Verified** on Steam Machine, GE-Proton11-6 | `edd5d8d` (bundled with #2; split before submitting) |
| 2 | File logging (`alpharing.log`), non-fatal console allocation, null-guarded `LOG_*` | `src/log/Log.cpp`, `src/log/Log.h` | **Verified** | `edd5d8d` |
| 3 | Do not `AllocConsole` under Wine/Proton | `src/log/Log.cpp` | Verified (no window, game unaffected) | `606b28c` (bundled with #5) |
| 4 | Halo CE Anniversary split-screen projection: post a settings reload ~3 s after a level starts | `src/mcc/mcc.cpp`, `src/mcc/mcc.h`, `src/render/imgui/ImGui.cpp` | Built, **awaiting in-game test** | pending |
| 5 | Diagnostic logging of `module_load/unload`, `game_setup`, `game_restart`, `set_state` | `src/mcc/CGameManager.cpp`, `src/mcc/module/Module.cpp` | Working; optional for upstream (could go behind a debug level) | `606b28c` |
| 6 | Linux/SteamOS install and troubleshooting guide | `STEAMOS-PROTON.md` | Docs only | `edd5d8d`, `606b28c`, follow-ups |

## Details and evidence

### 1. Constexpr `std::mutex` vs old `msvcp140` (Proton "Fatal error!")

Binaries built with MSVC 14.40+ construct `std::mutex` as constexpr and skip `_Mtx_init_in_situ`.
Any `msvcp140.dll` older than 14.40 (the `vcrun2019` redist most Linux guides install, and Wine's
builtin) dereferences null on the first lock. Upstream's 1.3.7 release imports `_Mtx_lock` without
`_Mtx_init_in_situ`; with the define it imports both. Backtrace from `PROTON_LOG=1` resolved with
the PDB: `std::_Mutex_base::lock` ← `AlphaRing::Log::Init` → `MSVCP140.dll+0x13080`, access
violation reading 0. Suggested PR text: one-line CMake change, zero runtime cost on Windows.

### 2. File logging

Port of kirklandsig/AlphaRing's `Log.cpp`/`Log.h`: `alpharing.log` next to the DLL, flushed on every
line, console sink kept when available, `LOG_*` macros no-op if the logger failed to initialise.
Without this, upstream logs only to a console window, which is invisible in Steam Game Mode and
lost on crash. Credit kirklandsig in the PR.

### 3. No console window under Wine

`GetProcAddress(ntdll, "wine_get_version")` detects Wine. Under gamescope the console is a second
window that can take focus, and closing it terminates the game. The file log makes it redundant.

### 4. Halo CE Anniversary split-screen aspect ratio

Symptom: with Anniversary graphics in 2-player split-screen each half-height view renders vertically
squished; classic graphics are correct. Closing the F4 menu (which calls `engine->load_setting()`)
was observed to fix it instantly, so the Anniversary renderer rebuilds its projection from the current
viewports on a settings reload. The fix watches `MCC::IsInGame()` every presented frame and, 180
frames after it rises, posts `load_setting()` once, only when `current_game == Halo1` and
split-screen is enabled with 2+ players. If the in-game test confirms it, submit as a standalone PR.
Upstream already does a delayed `load_setting()` for Halo 2 (`Sleep(10000)` thread in
`apply_menu_state_from_bin`); mention that the same idea generalises.

### 5. Diagnostic hook logging

Cheap `LOG_INFO` lines in the transition hooks. Useful for the still-open CE level-end freeze
(upstream WinterSquire issues #19, #40, #62, #85, #135). Offer upstream as optional; they may prefer
`LOG_DEBUG`.

## Not yet fixes, being investigated

- **Halo CE level-end freeze** (white/black screen after mission completion is saved). Known upstream
  since v1.0.69. Manual workaround: toggle the legacy F1 menu, which makes `get_key_state` report
  "connected, nothing pressed" for every slot. If the mechanism is confirmed, an automatic version
  (emulate that state during the transition window) would be PR #7.
- **ServiceTag `%ls` overread** in `CUserProfile::ImGuiContext` is already an open upstream PR from
  kirklandsig (megabitt01/AlphaRing #6). Do not duplicate; support that one.
