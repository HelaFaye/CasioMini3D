# fxmark: benchmark design, current implementation, and roadmap

**Audit date:** 2026-11-06
**Active target:** Casio fx-CG50 / Gint / fxSDK
**Active implementation:** `V3/`

<<<<<<< HEAD
This document describes the benchmark direction for the CasioMini3D project, grounded in the code that exists today.
=======
## 1. Status summary
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

The V3 benchmark runner and its three initial workloads are implemented in source. They are still a prototype and are not yet a fully validated graphics benchmark suite.

<<<<<<< HEAD
The active V3 app currently implements a small benchmark set:
=======
| Capability | Current status | Evidence / limitation |
|---|---|---|
| Benchmark registry | Implemented | `V3/src/bench.cpp` registers three named scenes. |
| Warm-up and fixed-frame measurement | Implemented | `run_benchmark()` runs `warmup_frames`, then `measure_frames`. |
| On-device selection/results UI | Implemented | `V3/src/main.cpp`; F6, EXE, DEL rerun tests; MENU exits. |
| Clear-only workload | Implemented | `dclear()` and `dupdate()` per frame. |
| Triangle workload | Implemented | Screen-space `DrawTriangle()` calls; not a 3D mesh/transform test. |
| Textured-geometry workload | Implemented | Two screen-space triangles per configured item; samples the generated UV map. |
| CSV appender | Implemented in source; runtime validation pending | OS-world callback appends to `\\fls0\\benchmarks.csv`; verify on hardware. |
| Microsecond timing | Not integrated | V3 currently uses 128 Hz `rtc_ticks()`, not `libprof`. |
| Isolated render vs presentation timing | Not implemented | `dupdate()` remains inside each timed scene frame. |
| General renderer facade | Not implemented | Scenes call the existing drawing functions directly. |
| General 3D depth buffer/clipping | Not implemented in V3 benchmark path | The separate exploratory `ModelRenderer/` path has its own limitations (see below). |
| Repeatability on physical hardware | Pending | Run each workload at least three times under fixed conditions. |
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

This is a source-level audit of the public `main` branch. “Implemented in source” does not mean the behavior has been validated on every physical calculator or OS revision.

<<<<<<< HEAD
These are the workloads currently represented in code. The project is intentionally conservative about benchmark scope until the rendering path and filesystem behavior are validated on real hardware.
=======
## 2. Current source map
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

- `V3/src/main.cpp` — benchmark registry selection, default configuration, result UI, startup suite, and CSV appender.
- `V3/src/bench.hpp` — `BenchId`, `BenchConfig`, and `BenchResult` data structures.
- `V3/src/bench.cpp` — scene registry, scene loops, warm-up/measurement runner, RTC timing, and metric calculation.
- `V3/src/shapes/shapes.hpp` — triangle/rectangle/parallelogram drawing interfaces and `VertexLocal`.
- `V3/src/shapes/triangle.cpp` — scanline triangle rasterizer and UV interpolation.
- `V3/src/display.hpp` / `display.cpp` — Gint display integration, direct pixel writes, and UV lookup map.
- `V3/src/world.cpp` — separate block-world/cube demo with camera projection and face sorting.

## 3. What the three benchmark scenes actually measure

<<<<<<< HEAD
The goal is to measure the target-native rendering path in a way that is stable enough to compare runs on the same device, clock configuration, and workload setup. Results are not directly comparable to glmark2 scores from desktop/mobile OpenGL or OpenGL ES implementations.

## Benchmark model

The benchmark structure follows the broad scene/runner separation used by glmark2:

- a benchmark runner selects a workload
- each workload is represented as a render scene
- the timed run is separated from UI and interaction code
- results are reported as timing and throughput metrics

This project uses those architectural ideas as inspiration. It does not claim compatibility with glmark2, and its workload metrics are not equivalent to glmark2's.

## Benchmark architecture

```text
benchmark runner
    -> benchmark scene (fixed workload)
        -> render primitive(s) / rasterizer path
            -> Gint display framebuffer
    -> timing and result reporting
=======
The default configuration in `main.cpp` is:

```cpp
cfg.warmup_frames = 5;
cfg.measure_frames = 60;
cfg.triangle_count = 64;
cfg.width = SCREEN_WIDTH;
cfg.height = SCREEN_HEIGHT;
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)
```

### 3.1 `clear-only`

<<<<<<< HEAD
Owns test selection, warm-up frames, measurement frames, and result presentation. Keep benchmark logic separate from interactive camera controls and demo setup.
=======
Each frame calls `dclear(C_BLACK)` followed by `dupdate()`. The scene ignores the `BenchConfig` argument. This is a framebuffer-clear plus presentation baseline, not a measurement of clear cost in isolation.
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

### 3.2 `triangle-throughput`

<<<<<<< HEAD
Each scene should use deterministic geometry and fixed workload parameters, and state what it measures and which rendering features it requires.
=======
Each frame clears the display, loops `cfg.triangle_count` times, generates screen-space coordinates from the loop index and frame number, calls `DrawTriangle()`, and then calls `dupdate()`.
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

The generated triangle positions are deterministic for a fixed configuration and frame sequence. However, this is a 2D screen-space primitive workload. It does not exercise 3D model/view/projection transforms, mesh submission, clipping, or depth testing.

<<<<<<< HEAD
The current implementation routes work through the existing software rasterizer and Gint display path. It is not a general-purpose 3D engine abstraction.
=======
The current `DrawTriangle()` implementation interpolates UV coordinates and samples `get_uv_map()`. Consequently, this is not a pure untextured triangle-fill test: each covered pixel also performs a lookup in the UV map.
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

### 3.3 `textured-geometry`

<<<<<<< HEAD
The runner is intended to report benchmark name, warm-up and measurement frame counts, elapsed time, FPS, and average milliseconds per frame. Treat results as experimental until repeated target-device runs establish their accuracy and repeatability.
=======
Each configured item constructs a rectangle as two triangles and submits both to `DrawTriangle()`. With the default `triangle_count = 64`, the scene submits 128 triangles per measured frame.

`main()` calls `init_uv_map()`. The current implementation initializes a 40x40 red/green gradient in a software array. `DrawTriangle()` samples this generated map through `get_uv_map()`. Although the build includes a fxconv texture asset and `display.cpp` has image-pixel helper functions, this benchmark path does not currently sample the compiled image asset.

The scene is therefore a deterministic two-triangle textured-quad/UV-lookup workload using a synthetic gradient, not yet a representative asset-texturing or perspective-correct 3D texture test.
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

## 4. Measurement semantics and current defects

`run_benchmark()` currently:

<<<<<<< HEAD
- keep scene data deterministic
- keep screen size, geometry count, and texture settings fixed per benchmark
- separate warm-up from measured iterations
- avoid keyboard polling and UI updates inside the timed section where possible
- record calculator model, clock mode, and software configuration alongside results
- publish only workloads that are stable on real hardware
=======
1. Rejects a null output pointer, zero measured frames, non-positive configured width/height, and zero triangle count for the two geometry tests.
2. Executes the warm-up frames.
3. Reads `rtc_ticks()` before the measured scene loop.
4. Executes exactly `measure_frames` scene frames.
5. Subtracts the start tick value from the end tick value.
6. Converts elapsed ticks using 128 ticks/second, rounds `elapsed_ms` to an integer, and derives FPS and average milliseconds per frame.
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

The active timer is **Gint RTC**, not `libprof`. The RTC tick resolution is 1/128 second (7.8125 ms per tick), which is coarse for short graphics measurements. The simple unsigned subtraction also does not explicitly handle the RTC counter's midnight reset. A zero-tick interval is treated as failure.

<<<<<<< HEAD
The current V3 app contains the following benchmark identifiers and scenes:

- `clear-only` — frame-clear baseline
- `triangle-throughput` — deterministic triangle drawing workload
- `textured-geometry` — geometry workload intended to exercise the texture path

The code being present does not establish that the workload or result is validated. In particular, verify the textured path and timer behavior on target hardware before treating measurements as reliable.

The following are not established as implemented or validated benchmark workloads:

- generic shader-style workloads
- advanced post-processing or blending tests
- fill-rate or overdraw-heavy scenes
- an open-ended glmark2-style suite
=======
The UI and keyboard selection logic are outside the measured scene loop. However, each scene calls `dupdate()` during every measured frame. The elapsed time therefore includes rendering work and display presentation. It must not be described as renderer-only or rasterizer-only time.

`BenchConfig.width` and `height` are checked for positive values, but the active scenes use the compile-time `SCREEN_WIDTH` and `SCREEN_HEIGHT` constants. The configured dimensions therefore do not currently control the rasterizer's bounds or workload area.

## 5. CSV result persistence

`main.cpp` implements an experimental appender:
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

- File path in the C source: `\\\\fls0\\benchmarks.csv` (runtime path: `\\fls0\benchmarks.csv`).
- The write callback is invoked through `gint_world_switch(GINT_CALL(...))`, keeping Fugue filesystem calls in the OS world rather than calling them directly from the Gint world.
- The file is opened with `fopen(path, "a+")`.
- The header is written only when the file is empty.
- One row is appended for each completed benchmark result.
- The callback checks seek/size operations, `fprintf`, `fflush`, `ferror`, and `fclose`.
- The results screen reports `CSV OK` or `CSV FAIL` for each scene.

Current CSV columns are:

<<<<<<< HEAD
- run each benchmark several times on the target calculator
- record clock mode and exact frame counts
- confirm repeatability before broadening the suite
- verify CSV logging independently from benchmark rendering
=======
```text
name,warmup_frames,measure_frames,elapsed_ms,fps,avg_ms_per_frame
```
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

This is an implementation in source, but persistence remains experimental until tested on the actual calculator. In particular, verify that the file is created in the intended filesystem, rows and header are correct, the file can be read/exported, and data remains available after a calculator restart. `CSV OK` is a useful runtime indicator, but it is not a substitute for inspecting the resulting file.

<<<<<<< HEAD
- keep benchmark scenes fixed and deterministic
- avoid mixing demo controls into timed benchmark sections
- keep metrics and failure states explicit

### Milestone 3: add only supported workloads

Add workloads only when the rendering path can define them clearly and repeatably. Describe the exact feature measured and how the workload differs from any desktop OpenGL analogue.

## Attribution and licensing

The architecture and scene/benchmark concepts were informed by the public glmark2 project, and its source was consulted as a design reference. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) for the project URL, license identification, consulted files, and scope of this attribution. No glmark2 source code is intentionally copied into the current benchmark implementation; that is not a complete provenance audit of all historical repository content.
=======
The current path writes a single CSV file at the Fugue `fls0` root. It does not create a dedicated directory, unique session file, or per-run identifier. It does not currently record a timestamp, calculator OS, clock setting, build revision, renderer revision, actual triangle count, or separate render/presentation times.

## 6. Renderer status and relationship to ModelRenderer

V3 is the active benchmark/demo app. Its three benchmark workloads are screen-space raster tests and do not establish a general-purpose 3D rendering API.

The separate `ModelRenderer/` directory is exploratory mesh-oriented work. Its current approach includes vertex transforms, camera projection, polygon faces, backface/visibility checks, and sorting whole faces by average depth before drawing. Whole-face painter sorting is an approximation; it cannot correctly resolve all intersecting geometry. The projection currently clamps very small/near camera-space Z values rather than clipping triangles against a near plane. This path does not yet provide a general per-pixel Z-buffer pipeline.

Keep the benchmark runner and its test registry separate from the interactive cube/world demo. Do not describe the current V3 triangle tests as 3D mesh benchmarks.

## 7. Recommended next implementation steps
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)

### Milestone 1 — Make the existing measurements trustworthy

<<<<<<< HEAD
A useful release can run deterministic tests on-device, report repeatable per-test results, document workload settings, and distinguish implemented behavior from target-validated behavior.
=======
1. Add a timer abstraction. Prefer a microsecond-capable timer such as `libprof` when the dependency is available and initializes successfully; retain RTC only as a clearly labelled coarse fallback/cross-check. Do not assume every Gint installation has libprof installed.
2. Check timer resource ownership and failure behavior. A benchmark must fail or clearly downgrade its timing mode rather than silently report misleading precision.
3. Handle RTC rollover if RTC remains in use.
4. Make workload configuration effective: either apply width/height to viewport and raster clipping or remove those fields until implemented.
5. Report actual submitted triangle counts and workload parameters.
6. Run at least three repeats per scene with unchanged clock/configuration; retain raw results and summarize median/min/max and spread.

### Milestone 2 — Separate renderer timing from presentation

Refactor the scene contract so rendering and presentation can be controlled separately. Report at least two distinct modes:

- **render-only:** CPU rasterization/drawing work without a display update inside every timed iteration (present a final frame after the timer stops);
- **end-to-end:** the current style of per-frame render plus `dupdate()`.

A dedicated presentation test can measure repeated display updates of a stable frame. Keep clear-only labelled as clear-plus-presentation unless the test is intentionally split.

### Milestone 3 — Define representative workload semantics

1. Make `triangle-throughput` an explicit geometry/fill workload. If it should measure untextured fill, add a solid-color path rather than relying on the current UV-map sampling path.
2. Keep the current synthetic-gradient workload but name/document it accurately; add a separate test that samples a known embedded texture asset.
3. Record exact primitive counts and texture format/dimensions.
4. Add transform/object-scaling tests only when they exercise a real 3D transform/mesh path.
5. Add a per-pixel depth test and clipping workload only after the renderer implements those features.

### Milestone 4 — Add renderer correctness before broad benchmark expansion

For a general 3D software renderer, define and test:

- explicit render context and caller-owned color/depth surfaces;
- model/view/projection transforms;
- geometric near-plane and viewport clipping;
- triangle winding and edge/fill rules;
- per-pixel depth testing/writes;
- perspective-correct interpolation for UVs and other attributes;
- bounded texture sampling and defined wrap/clamp behavior;
- safe mesh parsing with validated counts, face lengths, indices, and read results.

A painter-sorted quad demo remains useful, but it is not a substitute for a general visibility-correct triangle pipeline.

### Milestone 5 — Make CSV data suitable for comparison

Version the CSV schema before treating it as stable. Add fields for run/session ID, timestamp or monotonic run index, benchmark and renderer versions, calculator model, OS version, clock setting, timer backend, screen dimensions, workload parameters, measured frames, actual primitives/pixels, render time, presentation time, and status. Preserve raw timing values rather than only rounded milliseconds.

Keep file operations outside the timed render loop and continue using the correct Gint/OS-world boundary for Fugue operations. Validate append/readback/export/restart behavior on physical hardware.

## 8. Hardware validation protocol

For every published result, record:

- calculator model and OS version;
- Gint/fxSDK/compiler versions and build revision;
- clock configuration;
- benchmark ID and exact workload parameters;
- warm-up and measured frame counts;
- timer backend and resolution;
- whether display presentation is included;
- at least three repeated runs under identical conditions;
- raw CSV file and any failed/unsupported tests.

The current maintainer's physical test target is an fx-CG50 with OS 3.20.0202. This is the intended hardware baseline for upcoming validation; it does not assert that the CasioMini3D benchmark suite or CSV logger has already passed on that device/OS combination.

Do not compare fxmark numbers directly with desktop/mobile glmark2 scores. The calculator workloads use a different software rendering pipeline, display path, API, and timing environment.

## 9. Third-party and upstream contribution policy

The high-level scene/benchmark organization is inspired by glmark2, as described in `THIRD_PARTY_NOTICES.md`. Keep the implementation original and retain attribution. Before distribution, perform the documented source/asset provenance audit, including legacy directories.

When a component becomes independently useful, evaluate whether it belongs in CasioMini3D, Gint, fxSDK, or a broader SDK such as ParticuleSDK. Keep target-specific benchmark and demo code in this repository; propose general Gint/fxSDK improvements upstream only when they solve a broader platform/toolchain problem and the relevant maintainers' contribution requirements are satisfied.

## 10. Completion criteria

A benchmark is ready for published results only when its workload semantics are explicit, its configuration is actually applied, its timer behavior and precision are documented, its scene timing scope is clear, its filesystem output is validated if logging is enabled, and repeated physical-device runs are stable. A 3D benchmark label requires the tested feature to exist in the renderer; do not infer 3D pipeline coverage from screen-space triangle drawing.
>>>>>>> 8b3728a (docs: reconcile fxmark status and ignore build artifacts)
