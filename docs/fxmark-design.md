# fxmark: benchmark design, current implementation, and roadmap

**Status reviewed:** 2026-11-07
**Active application:** `V3/`
**Target:** Casio fx-CG family using Gint and fxSDK
**Physical validation target:** fx-CG50, OS 3.20.0202

## 1. Purpose and scope

`fxmark` is a calculator-native graphics benchmark inspired by the workload organization of glmark2. It measures the Gint/fxSDK rendering path and the software rasterizer implemented by CasioMini3D. It is **not** a desktop OpenGL/OpenGL ES port, an OpenGL conformance suite, or a score that can be compared directly with glmark2 results.

The V3 application has a benchmark runner, a four-entry scene registry, scene selection, warm-up and measured frame loops, an on-screen result summary, and an experimental CSV append path. These pieces are implemented in source. The benchmark suite remains a prototype: timing resolution, scene semantics, configuration, storage behavior, and repeatability need further validation before publishing stable scores.

## 2. Current implementation (V3)

The active implementation is in `V3/src/`. Its execution path is:

```text
V3/src/main.cpp
  -> benchmark registry / suite control
  -> V3/src/bench.cpp
       -> clear-only
       -> solid-fill
       -> triangle-throughput
       -> textured-geometry
  -> V3/src/shapes/triangle.cpp
  -> V3/src/display.cpp / Gint VRAM
  -> dupdate() presentation
  -> result screen and experimental CSV append
```

### 2.1 Runner and controls

`main.cpp` registers four `BenchId` values: `ClearOnly`, `SolidFill`, `TriangleThroughput`, and `TexturedGeometry`. On startup it runs the suite once. The result screen allows individual tests to be rerun with `F6` (clear-only), `F5` (solid-fill), `EXE` (triangle-throughput), and `DEL` (textured-geometry); `MENU` exits.

The default configuration is 5 warm-up frames, 60 measured frames per repetition, 5 measured repetitions, and `triangle_count = 64`. `BenchConfig` also contains width and height fields. At present, `bench.cpp` validates that these fields are positive, but the scene and rasterizer use compile-time `SCREEN_WIDTH` and `SCREEN_HEIGHT` constants. Those fields do not currently provide configurable render resolution.

### 2.2 Implemented scenes and what they measure

| Scene | Current operation | What the result includes | Important limitation |
|---|---|---|---|
| `clear-only` | `dclear(C_BLACK)` followed by `dupdate()` for each frame | Clear-call time, presentation-call time, and full-frame time | Raster time is zero because the scene has no draw callback. |
| `solid-fill` | Clears the frame, submits `triangle_count` screen-space triangles through `DrawTriangleSolid()` using `C_WHITE`, then calls `dupdate()` | Separate clear, raster, presentation-call, and full-frame timing | Skips UV interpolation and texture lookup; remains a 2D screen-space test. |
| `triangle-throughput` | Clears the frame, submits `triangle_count` screen-space triangles through `DrawTriangle()`, then calls `dupdate()` | Separate clear, UV-interpolated raster, presentation-call, and full-frame timing | 2D screen-space triangles; no 3D transform pipeline. |
| `textured-geometry` | Clears the frame, submits two `DrawTriangle()` calls per configured item, then calls `dupdate()` | Separate clear, UV-interpolated raster, presentation-call, and full-frame timing | Samples the generated 40×40 UV map, not the loaded image asset. |

The three geometry workloads use deterministic coordinate formulas based on item index and frame number. With the default `triangle_count` of 64, `solid-fill` and `triangle-throughput` each submit 64 triangles per measured frame; `textured-geometry` submits 128 triangles per measured frame. These are scene counts, not triangles transformed from a 3D mesh.

`DrawTriangle()` and `DrawTriangleSolid()` share the same scan-conversion traversal. The solid specialization computes edge positions and writes the supplied color directly; it does not interpolate UV coordinates or call `get_uv_map()`. This makes `solid-fill` a baseline for the same triangle geometry without texture sampling work.

`display.cpp` initializes `uv_map` as a 40×40 generated color gradient. `triangle.cpp` performs scanline filling and linearly interpolates UV coordinates across screen-space edges and spans before calling `get_uv_map()`. Image/palette helper functions also exist, but the current `DrawTriangle()` sampling path reads `uv_map`; it does not exercise a complete loaded-image texture pipeline.

### 2.3 Rasterizer and 3D demo boundaries

`V3/src/shapes/triangle.cpp` is a scanline triangle rasterizer. It clips scanline and horizontal spans against screen bounds, but it does not implement 3D view-volume clipping, a depth buffer, or perspective-correct attribute interpolation. UV interpolation is linear in screen space.

The interactive block-world demo in `V3/src/world.cpp` is separate from the four benchmark scenes. It applies a camera transform/projection, collects visible cube quads, sorts them by average depth, and draws them. Average-depth face sorting is a painter-order approximation; it can fail for intersecting or cyclically overlapping geometry. The camera now converts degree input to radians before evaluating its fixed-point trigonometric approximation, but the projection still clamps near/small Z values rather than clipping geometry against a near plane.

`ModelRenderer/` is a separate exploratory/legacy implementation. It should not be treated as the V3 benchmark backend or as a validated general-purpose renderer without separate source review and tests.

### 2.4 Host-side rasterizer and benchmark regression tests

The host test target compiles the production `V3/src/shapes/triangle.cpp` and `V3/src/bench.cpp` sources with minimal Gint header stubs. It checks:

- solid-fill color output and the current 230-pixel coverage for a known right-triangle fixture;
- all six vertex permutations for the same triangle coverage;
- flat-top and flat-bottom triangles;
- horizontal, vertical, and collinear degenerate triangles;
- fully off-screen and partially clipped triangles, including extreme integer coordinates;
- that `DrawTriangleSolid()` performs zero UV-map reads;
- that `DrawTriangle()` continues sampling the UV map without out-of-range coordinates for the fixture;
- benchmark IDs and names resolve for all four scenes;
- the new `solid-fill` scene writes pixels without UV reads;
- the existing `triangle-throughput` and `textured-geometry` scenes continue sampling the UV map.

Run it from `V3/`:

```sh
cmake -S tests/host -B /tmp/casiomini3d-host-tests
cmake --build /tmp/casiomini3d-host-tests
ctest --test-dir /tmp/casiomini3d-host-tests --output-on-failure
```

The benchmark test uses a deterministic LibProf-compatible stub to test timing phase separation and statistics. These host tests validate CPU behavior and aggregation logic; they do not emulate the hardware TMU or replace an fxSDK/SH build or physical calculator test.

## 3. Timing and result reporting

### 3.1 LibProf timer and measurement phases

The target build now requires LibProf 2.4 or newer:

```cmake
find_package(LibProf 2.4 REQUIRED)
target_link_libraries(myaddin PRIVATE Gint::Gint LibProf::LibProf stdc++)
```

`main()` initializes the profiler with `prof_init()` and releases its timer with `prof_quit()`. Each measured repetition creates independent `prof_t` contexts with `prof_make()` and brackets each phase using `prof_enter()` / `prof_leave()`. `prof_time()` returns elapsed time in microseconds. The API exposes integer-microsecond results; the CSV records that report quantum as `timer_resolution_us = 1`. The library uses the calculator's hardware TMU timer.

A LibProf-enabled measurement performs this sequence:

1. Execute `warmup_frames` unmeasured frames using clear, scene draw (if any), and `dupdate()`.
2. Run `repeat_count` measured repetitions. Each repetition contains `measure_frames` frames; frame indices restart at zero for each repetition so geometry is reproducible.
3. For every measured frame, independently bracket `dclear(C_BLACK)`, the scene draw callback, and the `dupdate()` call.
4. Bracket the complete measured frame loop with an outer context to measure the full-frame path.
5. Divide each accumulated context duration by `measure_frames` to obtain that repetition's per-frame average.
6. Aggregate the per-repetition samples into minimum, maximum, mean, median, and spread (maximum minus minimum).

Defaults are 5 warm-up frames, 60 measured frames per repetition, and 5 repetitions. Therefore, each workload records 300 measured frames across five samples by default. `repeat_count` is validated against the compile-time maximum of 9 to keep the target memory footprint fixed and bounded.

### 3.2 Meaning of the reported phases

| Metric | Timed region | Interpretation |
|---|---|---|
| `clear` | `dclear(C_BLACK)` | CPU time spent in the framebuffer clear call. |
| `raster` | One scene draw callback | CPU time for the workload's triangle generation/rasterization; excludes clear and `dupdate()`. `clear-only` reports zero. |
| `present` | `dupdate()` | CPU time spent in the display-update call. It does not measure physical LCD scanout completion. |
| `frame_total` | Entire measured frame loop | End-to-end frame-path time, including clear, draw, update, loop and inner profiler-context overhead. |

The component metrics are independently bracketed. `frame_total` should not be expected to equal the sum of `clear`, `raster`, and `present`, because the outer context also includes loop and profiling instrumentation overhead. The displayed FPS is derived from the mean `frame_total` microseconds per frame. The on-screen summary shows raster and presentation mean/median, plus full-frame mean/spread and derived FPS. The CSV contains min/max/mean/median/spread for all four phases.

`dupdate()` can initiate or schedule display transfer work that continues asynchronously. Its measured call duration is therefore not the duration of the LCD scanout. This distinction is essential when comparing raster costs with presentation overhead. [The libprof author describes separate measurement of drawing and display-update calls and notes background transfer behavior.](https://www.planet-casio.com/Fr/forums/topic15796-1-libprof-mesure-de-temps-et-profilage-pour-gint.html)

### 3.3 Repeat statistics and interpretation

Every sample is a per-frame average over `measure_frames` frames in one repetition. The five-sample default is summarized as:

- `min`: smallest repetition average;
- `max`: largest repetition average;
- `mean`: arithmetic mean of repetition averages;
- `median`: middle repetition average (or the mean of the two middle values for an even repeat count);
- `spread`: `max - min`, not standard deviation.

The repeated runs use identical deterministic frame-index geometry. They improve visibility into run-to-run variation but do not remove environmental effects such as clock configuration, thermal state, interrupt activity, or display-driver behavior.

### 3.4 Current result structure

`BenchResult` contains the scene name, warm-up/measured frame counts, repetition count, triangles per frame, and five statistics for each of `clear`, `raster`, `present`, and `frame_total`. For `textured-geometry`, the recorded triangle count is twice `triangle_count`; for the other triangle scenes it is `triangle_count`; `clear-only` records zero.

## 4. CSV result persistence

The logger in `V3/src/main.cpp` writes schema v2 to a separate normalized Gint/Fugue storage-root path:

```cpp
static const char path[] = "/benchmarks-v2.csv";
```

The original `/benchmarks.csv` schema-v1 file is intentionally left unchanged so new rows cannot be appended under an incompatible header.

The raw volume-prefixed path spelling (for example, `\\\\fls0\\file`) is the BFile path form. The active logger uses Gint's POSIX/C99 filesystem interface instead, and the file operations remain inside `gint_world_switch(GINT_CALL(...))` so they execute in the OS world on fx-CG targets.

The schema-v2 CSV header records benchmark and timer identifiers, reported timer quantum, warm-up/measured frame counts, repeat count, triangles per frame, and per-frame mean/median/minimum/maximum/spread for clear, raster, presentation-call, and full-frame timings. The file contains one row per named workload after each suite or manual rerun.

The logger opens an existing file for update with `r+`; if the file does not exist (`ENOENT`), it creates it with `w+`. It then explicitly seeks to EOF, writes the header if the file is empty, appends one row, flushes, checks stream errors, closes the file, and returns a success or failure result. This avoids relying on append-mode semantics. The results UI now distinguishes the failing stage: `CSV OPEN`, `CSV SEEK`, `CSV SIZE`, `CSV HEADER`, `CSV WRITE`, `CSV FLUSH`, `CSV STREAM`, `CSV CLOSE`, or `CSV SWITCH`. It also displays the captured `errno` or world-switch return code.

This remains an implementation in source, not proof of persistence. Do not mark CSV logging as hardware-validated until the target has been tested for file creation, repeated append, reopen/readback, export, restart persistence, and failure behavior. If the diagnostic still reports a failure, record the stage and code; that narrows the next investigation.

Schema v2 does not yet record a unique run ID, timestamp, calculator model, OS version, clock mode, or fxSDK/Gint versions. These remain release-engineering metadata to add in a future schema revision.

## 5. Corrective changes in the 2026-11-07 source audit

The audit patch makes the following targeted corrections:

1. **CSV path and diagnostics:** use the normalized `/benchmarks.csv` path and expose the failed filesystem stage plus its error code on the results screen. The OS-world boundary is retained.
2. **CSV open modes:** open an existing log with `r+`, create with `w+` only when `r+` fails with `ENOENT`, and explicitly seek to EOF instead of relying on append mode.
3. **RTC rollover:** calculate the elapsed tick count across the RTC's midnight reset using the 24-hour tick modulus.
4. **Framebuffer bounds:** reject `DrawPixel()` coordinates outside the active screen dimensions before writing Gint VRAM.
5. **UV-map bounds:** validate writes to the 40×40 map and define out-of-range sampling as clamp-to-edge.
6. **P8 image access:** validate image/data pointers, pixel coordinates, and palette indices before dereferencing.
7. **Fixed-point conversion:** replace signed left shifts used for integer scaling/division with multiplication by the fixed-point scale.
8. **Triangle interpolation:** promote coordinate deltas and interpolation math before subtraction/multiplication to reduce signed-int overflow risk.
9. **World ownership:** delete copy construction and copy assignment because `World` owns its `ids` allocation and shallow copying would cause aliasing/double-free risks.
10. **Camera trigonometry:** convert camera degrees to radians, wrap the angle to the approximation's domain, and shift cosine by π/2 rather than 4/π.
11. **Documentation integrity:** replace unresolved merge-conflict markers in the README and this design document.

### Findings not silently redesigned

- `World::set_block()` stores an ID of 0 or 1 and writes the supplied pointer to `blocks[0]` or `blocks[1]`. This representation only supports an empty entry plus one non-null block type. Passing a second distinct non-null block pointer overwrites `blocks[1]`, changing the block observed by every cell whose ID is 1. Supporting multiple block types requires an explicit registry/count or a different per-cell representation.
- `World` dimensions are multiplied as signed `int` values before allocation; positivity and multiplication overflow are not validated. The render path also assumes `Camera::current` is non-null.
- The block-world projection clamps near/small Z values instead of clipping triangles against a near plane. Average-depth whole-quad sorting cannot correctly resolve every intersecting geometry arrangement; there is no per-pixel Z-buffer.
- `BenchConfig.width` and `.height` are validated but ignored by the active scene/rasterizer bounds.
- LibProf measurements report integer microseconds from the TMU-backed profiler. The component phases separate clear, raster, and `dupdate()` call time; the full-frame context includes profiler/loop overhead.
- `dupdate()` duration is the call time and does not guarantee that physical LCD scanout has completed.
- The separate `ModelRenderer/` mesh-loader work has not been revalidated in this patch. The previous source review identified unsafe count/read/index assumptions and face lengths copied into fixed four-element arrays; it needs its own input validation and malformed-file tests before use with untrusted meshes.

## 6. Recommended implementation milestones

### Milestone 1 — Validate and characterize the existing suite

- Build with the fxSDK wrapper and exact Gint/toolchain versions.
- Run all four scenes at least three times under identical clock/configuration.
- Retain raw results and compare median, minimum, maximum, and spread.
- Verify CSV header-once behavior, appended rows, readback/export, and restart persistence on physical hardware.

### Milestone 2 — Improve timing without changing workload semantics

- Integrate a microsecond-capable timer (such as the separately tested `libprof` API) with explicit initialization/error handling.
- Record timer source and resolution in result metadata.
- Separate render-only and end-to-end presentation modes rather than changing the meaning of current scores.

### Milestone 3 — Make scene definitions precise

- Keep `solid-fill` as the solid-color baseline and verify that it performs no UV interpolation or texture-map lookups.
- Preserve `triangle-throughput` as the UV-interpolated triangle workload; it is intentionally distinct from solid fill.
- Keep the synthetic-gradient test clearly named and add a separate embedded-asset texture test.
- Record exact primitive counts and texture format/dimensions.
- Make width/height configuration effective or remove those parameters until it is.

### Milestone 4 — Define a reusable renderer interface

For a general-purpose renderer, define caller-owned color/depth surfaces, an explicit render context, model/view/projection transforms, clipping, a documented fill rule, and bounded texture sampling. Validate mesh counts, face sizes, indices, and all file reads.

### Milestone 5 — Add general 3D visibility and workloads

Implement geometric near-plane/viewport clipping and per-pixel depth testing before advertising depth-tested 3D workloads. Add transform, mesh, depth/overdraw, and texture-filtering benchmarks only once the underlying feature exists. Label CPU equivalents of shader-oriented workloads clearly as software shading.

### Milestone 6 — Publish defensible results

Publish workload definitions, result schema, device/clock/OS settings, timer source/resolution, repeated measurements, raw CSV, and known limitations. Keep fxmark scores distinct from desktop/mobile glmark2 results. Preserve third-party notices and audit source/asset provenance before redistribution.

## 7. Source map

- `V3/src/main.cpp` — scene registry, startup suite, controls, result display, and CSV logging.
- `V3/src/bench.cpp` — four scene draw callbacks, LibProf phase timing, and repeated-run statistics.
- `V3/src/bench.hpp` — scene IDs, configuration, and result structures.
- `V3/src/shapes/triangle.cpp` — shared scan conversion with specialized solid-fill and UV-interpolated triangle paths.
- `V3/tests/host/` — host-side rasterizer and benchmark timing regression tests using minimal Gint and LibProf stubs.
- `V3/src/display.cpp` — Gint VRAM writes, generated UV map, and image/palette helpers.
- `V3/src/world.cpp` — separate block-world camera/projection and average-depth quad sorting.
- `THIRD_PARTY_NOTICES.md` — glmark2, Gint, and fxSDK reference/attribution notes.

## 8. Scope and licensing

The current third-party notice states that glmark2's benchmark organization was consulted as design inspiration and that its code was not intentionally copied into the current V3 benchmark implementation. That is not a comprehensive provenance audit of all historical files. If source code, assets, or other materials are copied or adapted, review their licenses and preserve applicable notices and terms.

## Definition of a useful first validated release

A first validated fxmark release should run all four deterministic workloads on the physical target, report exact workload and measurement semantics, produce repeatable results across multiple runs, demonstrate CSV persistence on the target OS, and clearly distinguish screen-space raster workloads from general 3D rendering. It must not claim OpenGL compatibility or direct score comparability with glmark2.
