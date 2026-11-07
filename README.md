# CasioMini3D

A small 3D renderer and benchmark prototype for Casio fx-CG calculators, built around the Gint / fxSDK toolchain.

## Project status

This repository is currently in a prototype stage.

The active code is in `V3/`, and it includes:

- a software scanline rasterizer for triangles and related 2D primitives;
- a basic 3D block-world demo;
- a benchmark runner for four deterministic, target-native workloads;
- a minimal UI for selecting tests and reporting results;
- experimental CSV result appending through the Gint OS-world/Fugue filesystem boundary.

The current benchmark set is intentionally small:

- `clear-only` — repeated framebuffer clear and display update;
- `solid-fill` — screen-space triangles filled with one color, without UV interpolation or texture lookup;
- `triangle-throughput` — screen-space triangles with interpolated UV coordinates and generated-map lookups;
- `textured-geometry` — repeated two-triangle quads using the generated UV lookup map.

These are not a full desktop OpenGL or glmark2 port. In particular, the benchmark suite does not yet measure a general 3D mesh pipeline, per-pixel depth testing, or perspective-correct texture mapping.

## Repository layout

- `V3/` — active benchmark/demo application
- `V3/src/` — rasterizer, benchmark runner, and app entry point
- `docs/fxmark-design.md` — current implementation audit and benchmark roadmap
- `docs/THIRD_PARTY_NOTICES.md` — third-party references and attribution notes
- `ModelRenderer/` — exploratory or legacy mesh-oriented rendering work
- `v1/`, `V2/` — earlier prototype generations

## Build

Use the fxSDK wrapper for the target so the correct toolchain file, platform settings, and fxSDK CMake modules are selected.

For an fx-CG50 build, from the repository root:

```sh
cd V3
fxsdk build-cg
```

The V3 CMake project requires Gint 2.11 or newer and LibProf 2.4 or newer, enables C++20, and generates `3D.g3a` for the CG target. Install LibProf in the fxSDK sysroot (for a GiteaPC-managed setup, `giteapc install Lephenixnoir/libprof`) before configuring the target build. Inspect the generated add-in with `fxgxa -d 3D.g3a` before transferring it to a calculator.

A raw CMake invocation is only appropriate when the equivalent fxSDK toolchain and module configuration is supplied. Do not assume `cmake -S . -B build-cg` by itself selects the SH cross-compiler.

## Run

The app currently supports:

- `F6` — rerun clear-only benchmark;
- `F5` — rerun solid-fill benchmark;
- `EXE` — rerun triangle-throughput benchmark;
- `DEL` — rerun textured-geometry benchmark;
- `MENU` — quit.

On startup, the app runs the configured suite. The current default is 5 warm-up frames, 60 measured frames per repetition, 5 measured repetitions, and `triangle_count = 64`. The result view reports raster and `dupdate()` call mean/median microseconds per frame, plus full-frame mean/spread and derived FPS.

## Benchmark model

The benchmark runner selects a named workload, runs warm-up frames, then measures 5 repetitions of 60 frames by default. Each repetition produces per-frame averages; the result set reports minimum, maximum, mean, median, and spread (maximum minus minimum) across those repetitions. The current scene implementations are deterministic with respect to their frame/index formulas; the main loop's random seed is not used to generate the benchmark geometry.

The solid-fill and textured triangle paths share the same scan conversion. The solid path uses a compile-time specialization that skips UV interpolation and map reads. Host regression tests cover winding, flat edges, degenerate triangles, clipping, large coordinates, and the fact that solid fill performs no texture lookup.

From `V3/`, run the host rasterizer tests with:

```sh
cmake -S tests/host -B /tmp/casiomini3d-host-tests
cmake --build /tmp/casiomini3d-host-tests
ctest --test-dir /tmp/casiomini3d-host-tests --output-on-failure
```

These tests compile the production triangle rasterizer and benchmark runner with minimal Gint and LibProf stubs. They validate CPU-side behavior and the phase/statistics calculations, not the hardware TMU, target display driver, filesystem, or SH cross-build.

The exact workload semantics and measurement limitations are documented in [docs/fxmark-design.md](docs/fxmark-design.md). Results measure this calculator's software drawing and display path. They are not directly comparable to desktop/mobile glmark2 scores.

## CSV logging

The active V3 entry point contains a versioned CSV appender. It switches to Gint's OS world before using the Fugue filesystem, and appends rows to `/benchmarks-v2.csv` (the normalized root of the Fugue storage volume). Schema v2 records benchmark/timer identifiers, reported timer unit, warm-up/measurement/repetition settings, triangles per frame, effective display dimensions, and per-frame clear/raster/presentation/full-frame min/max/mean/median/spread. The earlier `/benchmarks.csv` v1 file is left untouched. The logger checks file operations, flush, stream error state, and close status. On failure, the results screen now shows the failing stage and either the captured `errno` value or the world-switch return code.

This is source-level implementation, not yet a claim of validated persistence. The repository still needs a physical-device test that confirms the file is created, contains the expected rows, can be read back/exported, and remains usable after a restart. See the validation checklist in [docs/fxmark-design.md](docs/fxmark-design.md).

## Hardware validation

Before widening the benchmark suite:

1. Build with the fxSDK wrapper for the exact calculator target.
2. Record calculator model, OS version, clock configuration, Gint/fxSDK versions, benchmark configuration, and build revision.
3. Retain the five measured repetitions for every benchmark and compare the recorded min/max/mean/median/spread under unchanged settings.
4. Confirm output stability and inspect the CSV file on the calculator or after export.
5. Do not publish results dominated by display presentation or timer quantization as renderer-only performance.
6. Publish only workloads that are repeatable on real hardware, with their exact measurement semantics.

The current maintainer test target is an fx-CG50 running OS 3.20.0202. That identifies the intended physical test environment; it does not imply that every workload or the CSV logger has already passed validation on that OS.

## Current limitations

- This is not a desktop OpenGL or OpenGL ES port.
- The V3 benchmark's triangle workloads are screen-space 2D raster tests, not general 3D mesh benchmarks.
- Timing now uses LibProf 2.4-or-newer's TMU-backed profiler and reports integer microseconds. The CSV marks the public report quantum as 1 µs; physical target builds still need validation.
- Raster time brackets the scene's triangle drawing only; clear time brackets `dclear()`; presentation time brackets the `dupdate()` call; full-frame time brackets the entire measured frame loop. Full-frame time also includes loop and profiler-context overhead, so it is not expected to equal the sum of the three component measurements.
- The measured `dupdate()` value is call execution time, not a measurement of physical LCD scanout completion. The display transfer can proceed asynchronously.
- `BenchConfig.width` and `.height` are validated but the active scenes use `SCREEN_WIDTH` and `SCREEN_HEIGHT` constants.
- The UV lookup map is initialized as a generated 40x40 gradient. The active `DrawTriangle()` path samples that map; the benchmark does not currently exercise the fxconv image asset as a loaded texture.
- CSV logging is experimental and still requires physical storage validation.
- The exploratory block-world camera now converts degrees to radians correctly, but the demo still uses average-depth face sorting and near-Z clamping rather than full geometric clipping and per-pixel depth testing.
- The separate `ModelRenderer/` code is not yet a general-purpose, visibility-correct 3D pipeline. See the design audit for its current depth/clipping limitations.

This project is best understood as a target-native graphics benchmark and software-rendering prototype for the fx-CG family, using Gint and fxSDK.

## Attribution

The benchmark organization is inspired by glmark2, which is credited as a design/source reference. No glmark2 source code is intentionally copied into the current V3 implementation. See [docs/THIRD_PARTY_NOTICES.md](docs/THIRD_PARTY_NOTICES.md) for attribution and scope; this is not a provenance audit of every historical file in the repository.
