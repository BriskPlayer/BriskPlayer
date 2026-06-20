/*
 * brisk-codecs — unified Rust codec library for BriskPlayer.
 *
 * Each codec lives in its own module.  The exported #[no_mangle] init
 * functions are picked up directly from the static library by the C linker;
 * no explicit re-export is needed.
 *
 * Adding a new codec:
 *   1. Create src/<name>.rs and implement CP_InitialiseCodec_<NAME>.
 *   2. Add `mod <name>;` below.
 *   3. In CMakeLists.txt, remove the corresponding C file and, if the codec
 *      needed a vcpkg library, conditionally skip linking it.
 */

#![allow(non_snake_case, non_camel_case_types)]

mod ffi;
mod flac;
mod mp3;
mod tags;
mod wav;
