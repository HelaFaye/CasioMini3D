# Repository split

Plan for dividing this repository into one repository per project. Status:
**planned**; component histories are extracted locally with
`git subtree split` and are ready to push once the repositories exist.

## Target repositories

| Repository | Comes from | Contents |
|---|---|---|
| **fxOGL** (this repository, renamed from CasioMini3D) | `lib/fxOGL`, the library tests now under `apps/fxGLMark/tests/host` and `tests/emulator`, `archive/`, the library docs | The software OpenGL ES library for the fx-CG50, its host and emulator tests, the historical renderers |
| **fxGLMark** | `apps/fxGLMark` (80 commits of history) | The benchmark add-in, its scenes, golden frames and device cost model; uses fxOGL as a git submodule |
| **fxglxgears** | new | glxgears on fxOGL through a GL 1.x compatibility layer (see `docs/fxOGL-roadmap.md`) |
| **fxSDL** | new | An SDL-compatible layer on gint (video, input, timers; audio through the ESP32), so SDL programs port with less change |
| **fxLauncher** | `lib/cglink` (1 commit), plus the launcher add-in and ESP32 firmware when written | The launcher, the calculator <-> ESP32 link library and the ESP32-S3 firmware, versioned together because they share one protocol |
| GB-Link compatibility | later | Link-cable emulation for ports such as Pokémon Emerald, talking to GB-Link hosts through the ESP32 |

Project Picori's port stays in HelaFaye/tmc on the `fx-cg50` branch and uses
fxLauncher's link library.

## Which tests go where

Most of `apps/fxGLMark/tests` tests the library, not the benchmark:

| Test | Goes to |
|---|---|
| `tests/host`: GLSL, GLES state, texture, pipeline, math, triangle and benchmark-kernel suites, the installed-package consumer | fxOGL |
| `tests/emulator`: the GLES regression add-in run in casio-emu | fxOGL |
| `tests/host/test_glmark2.cpp`, `tests/profile` (golden frames, device cost model) | fxGLMark |
| `lib/cglink/tests` | fxLauncher |

## How the pieces connect after the split

- fxGLMark and fxglxgears add fxOGL as a git submodule at `external/fxOGL`
  and build it with `add_subdirectory`. fxOGL already installs a CMake package
  (`fxOGLConfig.cmake`), which stays the option for builds outside fxSDK.
- tmc's `fx-cg50` branch adds fxLauncher as a submodule for the link library.
- Each repository gets its own CI: fxOGL keeps the host and emulator
  workflows; fxGLMark builds against the pinned fxOGL submodule.

## Steps

What needs your GitHub account (this session cannot create or rename
repositories):

1. Rename HelaFaye/CasioMini3D to **fxOGL** in its GitHub settings. GitHub
   redirects the old URL, so existing clones keep working.
2. Create empty repositories **fxGLMark**, **fxglxgears**, **fxSDL** and
   **fxLauncher** (no README, so the first push carries the history).
3. Add them to the session.

Then, in the session:

1. Push the extracted histories: `split/fxGLMark` to fxGLMark and
   `split/cglink` to fxLauncher (under `lib/cglink`).
2. Move the library tests into fxOGL and restructure it with the library at
   the root, then remove `apps/fxGLMark` and `lib/cglink` from fxOGL.
3. Wire fxGLMark to the fxOGL submodule and check that its host tests, golden
   frames and the fx-CG50 build pass on their own.
4. Start fxglxgears, fxSDL and the fxLauncher launcher and firmware in their
   repositories.

Re-run `git subtree split` just before pushing, so the extracted histories
include every commit up to then.
