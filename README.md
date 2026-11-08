# CasioMini3D

A calculator-native 3D/software graphics renderer and benchmark suite for the Casio fx-CG50, built with Gint and fxSDK.

## Project direction

The active V3 application is **fxGLMark2**, a native port of selected glmark2-inspired graphics workloads. It runs against the CasioMini3D software GLES-style renderer and writes results to the calculator filesystem. It is not the upstream Linux/EGL executable, and its results are not directly comparable with desktop/mobile glmark2 scores.

The rendering library is being developed toward OpenGL ES 2.0 semantics. It already has an end-to-end programmable triangle path, selected GLES resource/state APIs, a constrained GLSL ES compiler/VM, texture sampling and off-screen framebuffer support. Full GLES compatibility and Khronos conformance have not been achieved.

## Current fxGLMark2 menu

The add-in starts in a benchmark selector; it no longer launches a benchmark automatically.

| Key | Action |
|---|---|
| Up / Down | Move through individual tests |
| EXE or F6 | Toggle the selected test |
| F1 | Select Quick Smoke (2 tests) |
| F2 | Select glmark2 Supported Default (5 tests) |
| F3 | Select All Implemented (6 tests) |
| F4 | View unported glmark2 scene families |
| F5 | Run the selected tests |
| MENU | Exit |

Initially the Quick Smoke battery is selected. Press F5 to start it, or change the selection first.

### Implemented workloads

| Workload | What it exercises |
|---|---|
| Clear baseline | Color/depth clear and Gint presentation |
| Build / client arrays | Procedural sphere drawn from client-memory vertex arrays |
| Build / VBO | Same sphere drawn from a vertex buffer object |
| Texture / nearest | RGB565 texture sampling with nearest filtering |
| Texture / linear | RGB565 texture sampling with bilinear filtering |
| Shading / Gouraud | Vertex lighting, normal transformation, interpolated fragment color |

The first geometry workload uses a generated 144-triangle sphere, not glmark2's upstream model files. The port aims to reproduce comparable graphics workload categories, but scene geometry and performance are not identical to upstream glmark2.

### Test batteries

- **Quick Smoke:** VBO build and nearest texture.
- **glmark2 Supported Default:** client-array build, VBO build, nearest texture, linear texture, Gouraud shading.
- **All Implemented:** all five of the supported default-subset tests plus clear baseline.

The five-test glmark2 preset is an explicitly named subset, not the full upstream default battery. Additional scene families are listed by F4 and are disabled until implemented.

The current run configuration is 2 warm-up frames, 20 measured frames and 2 repetitions per test. Results report min/max/mean/median frame time and derived FPS. These are full-frame software-rendering measurements, including the Gint dupdate() call.

Results append to `/glmark2-fx-cg50-v1.csv` in the calculator's filesystem. This logger is separate from the earlier `/benchmarks-v2.csv` fxmark hardware microbenchmark dataset, which is preserved and not overwritten.

## Graphics library status

Implemented areas include:

- Single-current-context GLES-style C API and RGB565 render surfaces.
- Bounded buffer, texture, framebuffer and renderbuffer object pools.
- Client-memory and VBO vertex attributes.
- glDrawArrays / glDrawElements with points, lines, strips, loops, triangles, triangle strips and fans.
- Six-plane homogeneous clipping, perspective-correct varying interpolation and triangle/point/line raster paths.
- Depth, stencil, scissor, culling, blending and color masks.
- RGB565 nearest/bilinear texture sampling with clamp/repeat/mirrored-repeat.
- Color/depth/stencil framebuffer attachments, texture-backed color targets, renderbuffer storage, completeness checks and glReadPixels.
- Shader/program object lifecycle, link validation, attribute/uniform locations and selected queries/setters.
- A constrained GLSL ES 1.00 compiler and bytecode VM supporting the shader constructs currently required by the first ported scenes.

Still missing for broad OpenGL ES 2.0 compatibility are full GLSL ES control flow/functions/types/built-ins, broader texture formats and mipmaps, complete state/query/error semantics, and Khronos conformance. See [docs/opengl-compatibility-roadmap.md](docs/opengl-compatibility-roadmap.md).

## Build

From the repository root, use the fxSDK wrapper so the correct SH toolchain, CMake toolchain file, and fxSDK modules are selected:

```sh
cd V3
fxsdk build-cg
```

The V3 CMake project requires Gint 2.11 or newer and LibProf 2.4 or newer, enables C++20, and generates `3D.g3a` for fx-CG50. For GiteaPC-managed LibProf installations, use the fxSDK sysroot and install the package before building.

Inspect the generated add-in with `fxgxa -d 3D.g3a` before transferring it to the calculator. A raw CMake invocation is only appropriate when the equivalent fxSDK toolchain and module configuration is supplied.

## Host regression tests

From `V3/`:

```sh
cmake -S tests/host -B /tmp/casiomini3d-host-tests
cmake --build /tmp/casiomini3d-host-tests
ctest --test-dir /tmp/casiomini3d-host-tests --output-on-failure
```

The host suite compiles production renderer code and the real app entry point against minimal Gint/fxlibc/LibProf stubs. It covers the triangle rasterizer, texture sampler, matrix/clipping code, GLES context/state, software pipeline, shader VM, glmark2 scene parser/batteries, and the first GL-backed workloads. Host tests validate CPU behavior; they do not replace the fxSDK/SH build or physical calculator testing.

## Earlier fxmark baseline

The repository retains the earlier four-workload fxmark microbenchmark source and its design notes. The previously hardware-validated `/benchmarks-v2.csv` results and the corresponding `main` branch baseline are kept separate from this fxGLMark2 feature work. The new menu does not modify or append to that old dataset.

See [docs/fxmark-design.md](docs/fxmark-design.md) for the previous benchmark's timing semantics and validation history, and [docs/fxglmark2-port.md](docs/fxglmark2-port.md) for the current scene catalog and menu.

## Hardware validation still required

The current host CI passing does not establish that the new add-in has been cross-built or run on the calculator. Before publishing fxGLMark2 results, complete:

1. An fxSDK/SH cross-build for the fx-CG50.
2. G3A inspection and installation on the calculator.
3. Menu/key handling, all six workloads, and CSV creation/append/reopen testing.
4. Memory high-water and run-time checks for each battery.
5. Visual comparison against host reference output.
6. Timing runs with calculator model, OS, clock state, build revision and configuration recorded.

The physical test target remains the fx-CG50 running OS 3.20.0202. That is the intended hardware environment, not a claim that this new benchmark port has already passed on-device testing.

## Attribution

The benchmark organization and scene-family names are inspired by [glmark2](https://github.com/glmark2/glmark2), Copyright © Linaro Limited and contributors, licensed upstream under GPL-3.0. The current V3 workload implementation is written for Gint/fxSDK and does not intentionally copy glmark2 source or redistribute its models/textures. See [docs/THIRD_PARTY_NOTICES.md](docs/THIRD_PARTY_NOTICES.md) for scope and provenance caveats.

Gint and fxSDK are separate projects; consult their upstream license notices for their own code and tools.
