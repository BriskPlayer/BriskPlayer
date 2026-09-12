# BriskPlayer — Session Changes Summary

This document summarizes a full audit-and-fix pass across the C/C++ core and the Rust codec crate (`rust/codecs`), plus the build tooling issues encountered along the way. Findings originated from a systematic code review; each item below was either fixed, or explicitly deferred with a documented reason.

---

## 1. Build & Tooling Fixes

### vcpkg / CMake 4.3 compatibility (`triplets/`)
A previously-missing overlay triplet directory (already referenced by `vcpkg-configuration.json` and `CMakePresets.json`, but not present in the repo) was created:

- **`triplets/msvc-toolchain-fixes.cmake`** — chainloaded toolchain fragment fixing two issues that only surface without a Visual Studio Developer environment:
  1. **RC1107 build failure**: CMake 4.3 defaults `CMAKE_NINJA_CMCLDEPS_RC=1` for MSVC, routing the resource compiler through a `cmcldeps` wrapper that mishandles vcpkg's `/c65001` codepage flag. Disabled via `CMAKE_NINJA_CMCLDEPS_RC=0`.
  2. **Compiler-not-found in `vcpkg_configure_make`'s helper step**: that helper pre-sets `CMAKE_MAKE_PROGRAM` to vcpkg's own `ninja.exe`, which suppresses CMake's "auto-detect Visual Studio for Ninja" logic. Fixed by locating `cl.exe` via `vswhere` and pre-seeding `CMAKE_C_COMPILER`/`CMAKE_CXX_COMPILER`.
- **`triplets/x64-windows.cmake`**, **`x64-windows-static.cmake`**, **`x86-windows-static.cmake`** — overlays of vcpkg's built-in triplets that chainload the fix above.

### MinGW builds no longer need MSVC at all
- **`CMakePresets.json`**: added `VCPKG_HOST_TRIPLET: "x64-mingw-static"` to the shared `_mingw-base` preset. Previously, vcpkg's default host-triplet detection silently built host-only tools (`gettext[tools]`, needed for `msgfmt.exe`) with MSVC even on MinGW builds. Host tools now build with the same clang64/MinGW toolchain as everything else.

### Corrosion / Rust-target compatibility
- **`CMakeLists.txt`**: bumped the FetchContent-pinned Corrosion version from `v0.5.1` → `v0.6.1`. The pinned version predates support for the `x86_64-pc-windows-gnullvm` Rust target (the LLVM/clang64-based Windows ABI), causing `Unknown windows environment - Can't determine implib name`.
- **`CMakeLists.txt`**: added a `CMAKE_BUILD_TYPE` default (`Release`) for single-config generators only (Ninja/Makefiles; Visual Studio's multi-config path is untouched). Previously, a bare `cmake -B build -G Ninja` (bypassing the presets) would leave `CMAKE_BUILD_TYPE` unset, causing Corrosion to silently build the Rust crate with cargo's `dev` profile instead of the `[profile.release]` hardening (`panic = "abort"`, LTO) `rust/Cargo.toml` specifically opts into.

### MinGW/clang64 C++ compile errors
- **`stdafx.h`**: removed manual `strchr`/`strrchr`/`strstr` redeclarations (originally added for an old MinGW distribution). Modern MinGW-w64's own `<string.h>` already declares these with a const-correct C++ overload pair; the redeclaration collided with it (`functions that differ only in their return type cannot be overloaded`), breaking every `.c` file compiled as C++ under clang64.

---

## 2. C/C++ Fixes

### Memory safety
- **`CompositeFile.c`** (`CP_BuildDirectory`) — **use-after-free**: a ZIP-directory-entry node was linked into `pContext->m_pFirstSubFile` *before* its name buffer was allocated. On allocation failure, the node was freed but never unlinked, leaving a dangling pointer reachable from the list. Fixed by linking the node in only after it's fully initialized.
- **`CPI_Player.c`** (`CPI_Player__Destroy`) — **use-after-free**: if the player-engine thread didn't exit within the 5s shutdown timeout, the code freed `pPlayEngine` anyway while the OS thread could still be running and dereferencing it. Now leaks the allocation instead of freeing it on that (rare) path — a small leak beats a use-after-free or heap corruption.
- **`CPI_TagLib.c`** (`CPTL_CreateBitmapFromImageData`) — **COM reference leak**: `CoInitialize` was called on every album-art render with no matching `CoUninitialize` and no HRESULT check. Fixed: the `HRESULT` is captured and `CoUninitialize()` is called only when it actually succeeded (`S_OK`/`S_FALSE`), after all WIC interfaces are released.

### Buffer/string safety
- **`main.c`** (`cmdline_parse_options`) — unbounded `strcpy` from `argv[i]` into a fixed `MAX_PATH` buffer, plus a missing bounds check that could read past the end of `argv` if `-skin` were the last argument. Fixed with `strcpy_s` and an `i >= argc` guard. Two related same-buffer-size `strcpy` calls converted to `strcpy_s` for consistency.
- **`options.c`** — one `strcpy` into a `MAX_PATH` buffer converted to `strcpy_s` for consistency with the rest of the codebase's safe-string convention.
- **`CPI_Gettext.c`** — all 65 `strcpy(lang->name/region, "literal")` calls converted to the codebase's `CP_STRCPY` safe-string macro (confirmed zero-risk beforehand — all literals fit their buffers — but inconsistent with the established convention).

### Registry / file associations
- **`CPI_Player_FileAssoc.c`** (`CPFA_AssociateWithEXE`) — file-association registry writes targeted `HKEY_CLASSES_ROOT` with `KEY_ALL_ACCESS`, which fails with `ERROR_ACCESS_DENIED` for a standard (non-elevated) user, silently and unchecked. Fixed: writes now go to `HKEY_CURRENT_USER\Software\Classes` (writable without elevation, and correctly merged into the same effective view Explorer reads), with the result checked and logged on failure, plus an `SHChangeNotify(SHCNE_ASSOCCHANGED, ...)` call so Explorer picks up the change immediately.

### Non-ASCII path support
- **`CP_Config.c`** — fully converted from the ANSI Win32 profile APIs (`GetModuleFileNameA`, `GetPrivateProfileStringA`, `WritePrivateProfileStringA`, ...) to their wide (`W`) equivalents internally, with a truncation check on the exe-path lookup. An install path containing characters outside the process's ANSI code page (e.g. a non-Latin Windows username) previously could not be represented at all in the old ANSI path — now handled correctly. The public `char*`-based API (`CPConfig_GetString`/`SetString`/etc.) is unchanged, so no caller elsewhere needed updating; `CPConfig_GetFilePath()` now returns a UTF-8 mirror of the real path for accurate display/logging.

### Multi-monitor DPI
- **`CPI_Interface.c`** (`exp_InterfaceWindowProc`) — added a `WM_DPICHANGED` handler to the shared window procedure used by the playlist window (and other `IF_Create`-based popups, e.g. the equalizer). These are real top-level `WS_POPUP` windows that receive their own `WM_DPICHANGED` when dragged to a monitor with a different DPI than the main window, but nothing handled it — the window never rescaled itself. Fixed by updating the DPI state and resizing to the OS-suggested rect, which naturally triggers the existing `WM_WINDOWPOSCHANGED`-driven relayout these windows already use for ordinary resizing.
  - Known residual limitation (unchanged, pre-existing): `CPI_DpiScale.c`'s DPI state is a single process-wide value, not per-window. Each window now correctly rescales *when it moves*, but two windows simultaneously sitting on different-DPI monitors without either having just moved still share one scale factor.

### Reviewed, left as-is (with reasoning)
- **`CP_SafeGlobals.h`**'s `SAFE_*_CALL` macros have no locking around the global player/playlist handles. Investigated the actual handle lifecycle: they're set once at startup and (in release builds) never nulled until process exit; `CPI_Player__Destroy` properly joins its thread before freeing state. The race is real in the sense that the macros have zero synchronization, but not demonstrated to be reachable given current usage — left alone rather than adding a speculative, invasive locking layer.

---

## 3. Rust Codec Crate (`rust/codecs`) Fixes

### Confirmed crash fix
- **`wav.rs`** — a WAV file with a corrupt/zeroed `nSamplesPerSec` in its `fmt` chunk was accepted by `wav_open_file` (only `format_tag` was checked), making `bytes_per_second` zero; `wav_seek` then divided by it unconditionally. With `panic = "abort"` in the release profile, this **crashed the whole process** on a malformed file. Fixed with a check at open time (matching the pattern every sibling codec — flac/mp3/ogg — already used) plus a defensive re-check in `wav_seek` itself.

### Unsafe/soundness fixes
- **`tags.rs`** (`CPTL_ReadAlbumArt`/`CPTL_FreeAlbumArt`) — used `Vec::shrink_to_fit()` then reconstructed the buffer via `Vec::from_raw_parts(ptr, len, len)`, which is only sound if capacity exactly equals length. `shrink_to_fit` is documented as merely a *hint* — the allocator may leave excess capacity. Replaced with `into_boxed_slice()`, which *is* documented to reallocate to an exact-size buffer when needed; `CPTL_FreeAlbumArt` now reconstructs via `Box::from_raw` to match.
- **`playlist.rs`** — systemic rewrite. The `pl(h)` helper returned `&'static mut Playlist`, and this reference was routinely held across calls to *other* functions that called `pl(h)` again on the same handle (e.g. `CPL_Empty` → `CPL_UnlinkItem`, `CPL_RemoveItem` → `CPL_Stack_Remove` → `CPL_Stack_Renumber`, and many more beyond the few originally flagged) — two simultaneously-live exclusive references to the same object, a Stacked-Borrows/Tree-Borrows aliasing violation (Miri-flagged UB, though not observed to miscompile under current LLVM). Fixed by changing `pl(h)` to return a raw `*mut Playlist` (`Copy`, never creates an exclusive borrow) and converting every field/method access from `p.field` to `(*p).field` throughout all ~24 `#[no_mangle]` functions. Also removed now-unnecessary `p2`/`p3` re-derivation workarounds that existed specifically to dodge this issue in `CPL_PlayItem`.
  - A follow-on compile error from this change (rustc's `dangerous_implicit_autorefs` lint, which specifically targets `[]`-indexing through a raw pointer dereference since `Index`/`IndexMut` need `&self`/`&mut self`) was fixed by making the reference explicit at all 19 affected sites: `(*p).stack[i]` → `(&(*p).stack)[i]`, per rustc's own suggested fix.
- **`stream_internet.rs`** — `StreamContext`/`FillerContext`'s `unsafe impl Send` had no `SAFETY` comment, unlike every other one in the crate (and unlike those others, this one involves genuine concurrent access: the filler thread writes into a circle buffer the main thread reads from, relying on the C-side `CPCB_*` functions being internally synchronized). Added comments documenting the real justification.
  - `inet_uninitialise`'s "no double-free guard" was investigated rather than patched: its only call site in the entire codebase is `InStream::drop`, and Rust's ownership model guarantees `Drop::drop` runs at most once per value — a runtime guard would have had nothing real to protect against (and couldn't have anyway, since a hypothetical second call would already be reading through freed memory to check any guard).

### ABI-drift detection
- **`ffi.rs`** — added compile-time `size_of`/`offset_of!` assertions for `CPs_FileInfo`, `CPs_CoDecModule`, and `CpsInStream`, expressed in terms of `size_of::<usize>()`/`size_of::<u32>()` rather than hardcoded byte counts so they hold on both the x86 and x64 triplets this project builds.
- **`globals.h`**, **`CPI_Player_CoDec.h`**, **`CPI_Stream.h`** — matching `static_assert`s added on the C side (tightening a pre-existing loose `sizeof(CPs_FileInfo) <= 32` check to an exact match, plus adding the two structs that had no check at all). Either side changing field count, type, or order without updating the other now fails to compile immediately instead of silently desyncing the shared vtable ABI at runtime.

### Error diagnosability
- **`tags.rs`** — all 23 sites that discarded a `lofty::Result` error (`Err(_) => return FALSE` / `Err(_) => FALSE`) now log the actual `LoftyError` via `OutputDebugStringA` (the same debugger-Output-window channel the C side's `CP_TRACE`/`_CrtDbgReport` macros use) before returning `FALSE`. Purely additive — the `BOOL` return contract across the FFI boundary is unchanged, but the failure reason ("file not found" vs "unsupported format" vs "corrupt tag") is no longer silently discarded.

### Test infrastructure (previously nonexistent)
- **`Cargo.toml`** — added `"rlib"` to `crate-type` (alongside the existing `"staticlib"` the C build consumes) so `cargo test` can build a test harness at all; added `windows-sys`'s `Win32_System_Diagnostics_Debug` feature for the new logging.
- **`wav.rs`** — added a focused regression-test module: a pure `fmt_chunk_is_valid` predicate (extracted from `wav_open_file`) directly tests the zero-samplerate fix, plus a test exercising the actual RIFF-parsing helpers (`skip_to_chunk`, `read_u16_le`, etc.) against a hand-built in-memory WAV byte stream — no C linkage required, since those helpers are generic over `Read`/`Read + Seek`.
- **`test_stubs.rs`** (new, `#[cfg(test)]`-only) — `cargo test` builds one test-harness binary containing the *whole* crate, and several other modules (`playlist.rs`, `stream_internet.rs`, `ffi.rs`) call `extern "C"` functions that only exist in the C build (`CPFA_*`, `CPLI_*`, `CP_opt_*`, `CPL_cb_*`, `CP_CreateInStream`, `CPCB_*` — 46 distinct symbols). Added minimal link-only stand-ins (inert bodies: null pointers, `FALSE`, no-ops) so the test binary can link; none of the actual tests exercise these paths, they exist purely to satisfy the linker. Verified `cargo test` now runs (4/4 tests passing) and that the real `cargo build --release` path (what Corrosion actually uses) is unaffected by the new test-only module.

---

## 4. Follow-Up Audit Round

A second pass, specifically re-examining everything not yet individually reviewed: the large remaining C surface (skin loading, playlist core/UI, window management, output backends) and the three Rust codecs not yet re-checked after the `wav.rs` fix (`flac.rs`, `mp3.rs`, `ogg.rs`), plus a fresh skeptical re-read of the `wav.rs` fix and `playlist.rs` rewrite themselves.

### Rust: flac.rs and ogg.rs clean; two real gaps found and fixed in wav.rs and mp3.rs
- **`flac.rs`** and **`ogg.rs`** — confirmed as panic-safe as the fixed `wav.rs`: every division on a file-derived value (sample rate, frame size, duration) is guarded consistently, verified in part by reading the actual vendored `flac-codec`/`lewton` source rather than assuming their guarantees.
- **`wav.rs`** — the zero-samplerate fix (see section 3) had a residual gap: `n_samples_per_sec` was checked `!= 0` but not bounded, so a value near `u32::MAX` could make `bytes_per_second = n_samples_per_sec as i32 * up-to-4` overflow `i32` and wrap to **negative**. `wav_seek`'s guard used `== 0` (doesn't catch negative) while its two sibling guards correctly used `> 0` — an inconsistency that let a malformed-but-nonzero sample rate slip through and silently corrupt seek/position bookkeeping. Fixed by capping the accepted sample rate in `fmt_chunk_is_valid` to whatever can't overflow the multiplication, and hardening the inconsistent guard to `<= 0` for defense-in-depth. Added a regression test.
- **`mp3.rs`** — added a defensive `consumed.min(slice_len)` clamp before advancing the input-buffer read cursor: `mp3.rs` trusted nanomp3's returned "bytes consumed" without re-validating it against the actual slice length given to `decode()`. Traced nanomp3's ~6300-line c2rust-translated decode loop and confirmed the invariant holds today, but nothing in `mp3.rs` itself would have caught a violation (a future nanomp3 version, or a bug in that translation) before it turned the next call's slice-start into an out-of-range index — a panic/process-abort, the same failure mode the `wav.rs` bug had. Also confirmed (but left as a known limitation, not fixed): MP2/MP1 file associations are registered, but nanomp3's decoder unconditionally rejects non-Layer-III frames, so `.mp2`/`.mp1` files can never actually open — dead functionality rather than a crash.
- **`playlist.rs`** rewrite — spot-checked again independently; no issues found.

### C/C++: five confirmed bugs found and fixed, ranging from latent-but-real to actively reachable
- **`CPI_CircleBuffer.c`/`.h`** — the C23-threading `CircleBufferRead` path had a genuine missing closing brace (traced precisely by hand: the "no data yet, wait" block's brace was never closed, so the actual read logic and the mutex unlock ended up nested inside it, leaving the enclosing `while` loop unclosed by the time the shared post-`#endif` return code is reached). Confirmed via a compiler probe that `HAVE_C23_THREADING` evaluates to `0` under this project's actual clang64/MinGW toolchain today (this MinGW-w64 distribution doesn't even ship `<threads.h>`), so the bug is currently latent — but a *second*, independent bug compounded it: `CPI_CircleBuffer.h` had its own duplicate feature-detection with a logically-backwards condition (`defined(__STDC_NO_THREADS__) && __STDC_NO_THREADS__ == 0`, which can never be true under a standards-conforming compiler), permanently disabling the C23 path regardless of real compiler support. Fixed both: closed the brace, and replaced the broken duplicate detection with a `#include "threading_compat.h"` (which already has the correct logic).
- **`CPI_Playlist.c`** (`CPL_DestroyPlaylist`) — same use-after-free shape as the earlier `CPI_Player.c` fix: on a worker-thread shutdown timeout, the code logged a warning but fell through to `CPPL_FreePlaylist(hPlaylist)` regardless, freeing the Rust-owned playlist while the worker thread could still be running and dereferencing it. Fixed by returning early (leaking) on timeout, matching the established pattern.
- **`CPI_PlaylistItem.c`** (`CPLI_CalculateLength_MP3`) — unsigned underflow: `dwBufferSize` (from `ReadFile`) is a `DWORD`, and for a file under 4 bytes, `dwBufferSize - 4` wrapped to a huge value, scanning the frame-sync-detection loop far past the actual 0x8000-byte stack buffer. Reachable via a truncated/corrupt `.mp3` in the playlist with length calculation enabled. Fixed with a `dwBufferSize >= 4` guard. Also reordered an adjacent `GetFileSize`-before-`INVALID_HANDLE_VALUE`-check nit.
- **`CPI_PlaylistItem.c`** (`CPLI_SetTrackNum_AsText`) — `strncpy` into an exact-size 16-byte buffer does not null-terminate when the source is that long or longer; confirmed reachable from the UI, since (unlike the Title/Artist in-place editors) the TrackNum column editor has no length cap. Fixed by switching to the codebase's `cp_strncpy_s`, which always null-terminates.
- **`CPI_PlaylistItem.c`** (`CPLI_ReadTag_TagLib`) — a `!pItem || !pItem->m_pcPath` guard called `CPLII_RemoveTagInfo(pItem)` (which itself dereferences `pItem` with no null check) and then wrote `pItem->m_enTagType` in the very branch meant to handle `pItem == NULL` — the guard would have crashed on exactly the condition it was checking for. Currently unreached (no caller passes NULL today) but fixed by separating the two cases.
- **`CPI_PlaylistItem.c`** (`CPLI_GrowFile`) — `BYTE* pbReadBlock[0x10000]` was a stray-`*` typo: an array of 65536 *pointers* (512KB/256KB of stack) instead of a 64KB byte buffer, unlike the correctly-typed sibling `CPLI_ShrinkFile`. Not an overflow (the array was oversized, not undersized) but a real stack-bloat defect; fixed to `BYTE pbReadBlock[0x10000]`.
- **`WinModern.c`** (`FileDialog_OpenFile`) — the multiselect file-open path reused the original 64KB `GetOpenFileNameW` buffer size for its pipe-joined reconstruction, but that reconstruction duplicates the full directory path once *per selected file* with no bounds check — a few hundred files selected from one directory (easily reached selecting a whole album folder) overflows it. Currently dead code (never called from anywhere in the codebase), but fixed by computing the exact required buffer size before allocating, since it's a public API that could be wired up at any time.
- **`skin.c`** (`main_skin_check_ini_value`) — the `associate[]` lookup table has exactly `ReducedSize` (29) entries, but the loop bound used `Lastone` (`ReducedSize + 1` = 30), reading one element past the array's end on every skin `.ini` line parsed. **Correction during fixing**: the first fix attempt used `sizeof(associate)/sizeof(associate[0])`, which looked right but was actually wrong — `associate` is a *pointer parameter* in this function (`Associate* associate`), not the real array, so `sizeof(associate)` silently computed `sizeof(Associate*)` instead of the array's byte size, which would have made the loop run essentially zero times and broken all skin parsing. Caught via a compiler warning (`-Wsizeof-pointer-div`) during verification and corrected to use the `ReducedSize` enum constant directly.

### Not fixed (explicitly out of scope or low-value)
- `nanomp3`'s MP2/MP1 non-support (see above) — a quality/feature gap, not a crash.
- A handful of lower-severity/currently-unreachable items the audit also found (unchecked allocations in `CPI_PlaylistItem.c`/`CPI_Playlist.c` reachable only under real OOM; `CPSK_Skin.c`'s unchecked `CF_GetSubFile` result, not reachable while custom `.CPSkin` loading stays disabled; a one-byte-before-buffer read in `CPL_AddDirectory_Recurse` unreachable from current call sites) — left as-is given the pattern established earlier in this session of prioritizing confirmed, reachable issues.

---

## Verification (updated)

- **Rust**: `cargo build`, `cargo build --release`, and `cargo test` (5/5 passing, including the new overflow regression test) all succeed cleanly after this round's `wav.rs`/`mp3.rs` changes.
- **C/C++**: all five files touched this round (`skin.c`, `CPI_PlaylistItem.c`, `CPI_Playlist.c`, `WinModern.c`, `CPI_CircleBuffer.c`/`.h`) were syntax-checked with the project's actual compile flags (extracted from the real Ninja build, not guessed) and compile with **zero errors and zero warnings**. This caught a real mistake before it shipped: an initial fix in `skin.c` used `sizeof(associate)/sizeof(associate[0])`, which silently computed the wrong thing because `associate` is a pointer parameter in that function, not the array itself — `-Wsizeof-pointer-div` flagged it, and it was corrected to use the `ReducedSize` enum constant instead.
- The `CPI_CircleBuffer.c` brace fix could not be exercised by an actual compile in this environment (this MinGW-w64/clang64 distribution doesn't provide `<threads.h>` at all), so it rests on a careful manual brace-depth trace, cross-checked twice, against the exact structural requirement of the shared post-`#endif` code.
- Not yet done: a full C/CMake rebuild + manual runtime smoke test of this round's changes (only per-file syntax checks were run, not a full link/build). Worth doing before considering this round fully closed out.

