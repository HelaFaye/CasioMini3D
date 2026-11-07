# fxmark: benchmark design, current implementation, and roadmap

**Status reviewed:** 2026-11-07  
**Active application:** `V3/`  
**Target:** Casio fx-CG family using Gint and fxSDK  
**Physical validation target:** fx-CG50, OS 3.20.0202

## 1. Purpose and scope

`fxmark` is a calculator-native graphics benchmark inspired by the workload organization of glmark2. It measures the Gint/fxSDK rendering path and the software rasterizer implemented by CasioMini3D. It is **not** a desktop OpenGL/OpenGL ES port, an OpenGL conformance suite, or a score that can be compared directly with glmark2 results.

The V3 application has a benchmark runner, a three-entry scene registry, scene selection, warm-up and measured frame loops, an on-screen result summary, and an experimental CSV append path. These pieces are implemented in source. The benchmark suite remains a prototype: timing resolution, scene semantics, configuration, storage behavior, and repeatability need further validation before publishing stable scores.

## 2. Current implementation (V3)

The active implementation is in `V3/src/`. Its execution path is:

```text
V3/src/main.cpp
  -> benchmark registry / suite control
  -> V3/src/bench.cpp
       -> clear-only
       -> triangle-throughput
       -> textured-geometry
  -> V3/src/shapes/triangle.cpp
  -> V3/src/display.cpp / Gint VRAM
  -> dupdate() presentation
  -> result screen and experimental CSV append
```

### 2.1 Runner and controls

`main.cpp` registers three `BenchId` values: `ClearOnly`, `TriangleThroughput`, and `TexturedGeometry`. On startup it runs the suite once. The result screen allows individual tests to be rerun with `F6` (clear-only), `EXE` (triangle-throughput), and `DEL` (textured-geometry); `MENU` exits.

The default configuration is 5 warm-up frames, 60 measured frames, and `triangle_count = 64`. `BenchConfig` also contains width and height fields. At present, `bench.cpp` validates that these fields are positive, but the scene and rasterizer use compile-time `SCREEN_WIDTH` and `SCREEN_HEIGHT` constants. Those fields do not currently provide configurable render resolution.

### 2.2 Implemented scenes and what they measure

| Scene | Current operation | What the result includes | Important limitation |
|---|---|---|---|
| `clear-only` | `dclear(C_BLACK)` followed by `dupdate()` for each frame | Framebuffer clear plus display presentation | Not a clear-only CPU measurement. |
| `triangle-throughput` | Clears the frame, submits `triangle_count` screen-space triangles through `DrawTriangle()`, then calls `dupdate()` | Scanline rasterization, UV lookup/pixel writes, clear, and presentation | 2D screen-space triangles; no 3D transform pipeline. |
| `textured-geometry` | Clears the frame, submits two `DrawTriangle()` calls per configured item, then calls `dupdate()` | Triangle rasterization, UV interpolation and lookup, clear, and presentation | Samples the generated 40×40 UV map, not the loaded image asset. |

The two geometry workloads use deterministic coordinate formulas based on item index and frame number. With the default `triangle_count` of 64, the triangle workload submits 64 triangles per measured frame; the textured-geometry workload submits 128 triangles per measured frame. These are scene counts, not triangles transformed from a 3D mesh.

`display.cpp` initializes `uv_map` as a 40×40 generated color gradient. `triangle.cpp` performs scanline filling and linearly interpolates UV coordinates across screen-space edges and spans before calling `get_uv_map()`. Image/palette helper functions also exist, but the current `DrawTriangle()` sampling path reads `uv_map`; it does not exercise a complete loaded-image texture pipeline.

### 2.3 Rasterizer and 3D demo boundaries

`V3/src/shapes/triangle.cpp` is a scanline triangle rasterizer. It clips scanline and horizontal spans against screen bounds, but it does not implement 3D view-volume clipping, a depth buffer, or perspective-correct attribute interpolation. UV interpolation is linear in screen space.

The interactive block-world demo in `V3/src/world.cpp` is separate from the three benchmark scenes. It applies a camera transform/projection, collects visible cube quads, sorts them by average depth, and draws them. Average-depth face sorting is a painter-order approximation; it can fail for intersecting or cyclically overlapping geometry. The camera now converts degree input to radians before evaluating its fixed-point trigonometric approximation, but the projection still clamps near/small Z values rather than clipping geometry against a near plane.

`ModelRenderer/` is a separate exploratory/legacy implementation. It should not be treated as the V3 benchmark backend or as a validated general-purpose renderer without separate source review and tests.

## 3. Timing and result reporting

### 3.1 Current timer

`run_benchmark()` in `V3/src/bench.cpp`:

1. Executes `warmup_frames` through the selected scene.
2. Reads `rtc_ticks()`.
3. Executes `measure_frames` through the same scene.
4. Reads `rtc_ticks()` again and calculates elapsed time at 128 ticks per second.
5. Derives elapsed milliseconds, FPS, and average milliseconds per frame.

The runner accounts for `rtc_ticks()` resetting at midnight by calculating elapsed ticks with a 24-hour modulus. The RTC is still 128 Hz, so one tick is 7.8125 ms. A zero-tick interval is treated as a failed measurement.

The timed scene functions call `dupdate()` inside every measured frame. Therefore, all three benchmark results include display presentation. This is an end-to-end frame workload, not an isolated software-render time. In particular, `clear-only` includes both clearing and presentation.

### 3.2 Higher-resolution timing

A separate Gint bring-up application has been physically tested with `libprof` 2.4.0 on the maintainer's fx-CG50. That validates the library integration in that separate application; it does **not** mean `libprof` is already linked or used by CasioMini3D.

A future V3 timing change should initialize a microsecond-capable timer, handle timer-acquisition failure explicitly, bracket only the region whose timing is being reported, and record the timer source/resolution. Keep RTC as a coarse cross-check where useful. Prefer separate render and presentation measurements rather than silently changing the meaning of the existing workload names.

### 3.3 Current result structure

`BenchResult` currently contains the scene name, warm-up frame count, measured frame count, elapsed milliseconds (integer), FPS, and average milliseconds per frame. The on-screen result page displays FPS and average milliseconds per frame.

## 4. CSV result persistence

The logger in `V3/src/main.cpp` now uses the normalized Gint/Fugue storage-root path:

```cpp
static const char path[] = "/benchmarks.csv";
```

The raw volume-prefixed path spelling (for example, `\\\\fls0\\file`) is the BFile path form. The active logger uses Gint's POSIX/C99 filesystem interface instead, and the file operations remain inside `gint_world_switch(GINT_CALL(...))` so they execute in the OS world on fx-CG targets.

The CSV header is:

```csv
name,warmup_frames,measure_frames,elapsed_ms,fps,avg_ms_per_frame
```

The logger opens an existing file for update with `r+`; if the file does not exist (`ENOENT`), it creates it with `w+`. It then explicitly seeks to EOF, writes the header if the file is empty, appends one row, flushes, checks stream errors, closes the file, and returns a success or failure result. This avoids relying on append-mode semantics. The results UI now distinguishes the failing stage: `CSV OPEN`, `CSV SEEK`, `CSV SIZE`, `CSV HEADER`, `CSV WRITE`, `CSV FLUSH`, `CSV STREAM`, `CSV CLOSE`, or `CSV SWITCH`. It also displays the captured `errno` or world-switch return code.

This remains an implementation in source, not proof of persistence. Do not mark CSV logging as hardware-validated until the target has been tested for file creation, repeated append, reopen/readback, export, restart persistence, and failure behavior. If the diagnostic still reports a failure, record the stage and code; that narrows the next investigation.

The current schema does not record a unique run ID, timestamp, calculator model, OS version, clock mode, fxSDK/Gint versions, timer source, resolution, or workload-specific primitive count. Add those only with a versioned schema or documented migration strategy; do not silently change the existing CSV columns.

## 5. Corrective changes in the 2026-11-07 source audit

The audit patch makes the following targeted corrections:

1. **CSV path and diagnostics:** use the normalized `/benchmarks.csv` path and expose the failed filesystem stage plus its error code on the results screen. The OS-world boundary is retained.
2. **CSV open modes:** open an existing log with `r+`, create with `w+` only when `r+` fails with `ENOENT`, and explicitly seek to EOF instead of relying on append mode.
2. **RTC rollover:** calculate the elapsed tick count across the RTC's midnight reset using the 24-hour tick modulus.
3. **Framebuffer bounds:** reject `DrawPixel()` coordinates outside the active screen dimensions before writing Gint VRAM.
4. **UV-map bounds:** validate writes to the 40×40 map and define out-of-range sampling as clamp-to-edge.
5. **P8 image access:** validate image/data pointers, pixel coordinates, and palette indices before dereferencing.
6. **Fixed-point conversion:** replace signed left shifts used for integer scaling/division with multiplication by the fixed-point scale.
7. **Triangle interpolation:** promote coordinate deltas and interpolation math before subtraction/multiplication to reduce signed-int overflow risk.
8. **World ownership:** delete copy construction and copy assignment because `World` owns its `ids` allocation and shallow copying would cause aliasing/double-free risks.
9. **Camera trigonometry:** convert camera degrees to radians, wrap the angle to the approximation's domain, and shift cosine by π/2 rather than 4/π.
10. **Documentation integrity:** replace unresolved merge-conflict markers in the README and this design document.

### Findings not silently redesigned

- `World::set_block()` stores an ID of 0 or 1 and writes the supplied pointer to `blocks[0]` or `blocks[1]`. This representation only supports an empty entry plus one non-null block type. Passing a second distinct non-null block pointer overwrites `blocks[1]`, changing the block observed by every cell whose ID is 1. Supporting multiple block types requires an explicit registry/count or a different per-cell representation.
- `World` dimensions are multiplied as signed `int` values before allocation; positivity and multiplication overflow are not validated. The render path also assumes `Camera::current` is non-null.
- The block-world projection clamps near/small Z values instead of clipping triangles against a near plane. Average-depth whole-quad sorting cannot correctly resolve every intersecting geometry arrangement; there is no per-pixel Z-buffer.
- `BenchConfig.width` and `.height` are validated but ignored by the active scene/rasterizer bounds.
- RTC timing remains coarse, and each measured frame includes `dclear()` and `dupdate()`.
- The separate `ModelRenderer/` mesh-loader work has not been revalidated in this patch. The previous source review identified unsafe count/read/index assumptions and face lengths copied into fixed four-element arrays; it needs its own input validation and malformed-file tests before use with untrusted meshes.

## 6. Recommended implementation milestones

### Milestone 1 — Validate and characterize the existing suite

- Build with the fxSDK wrapper and exact Gint/toolchain versions.
- Run all three scenes at least three times under identical clock/configuration.
- Retain raw results and compare median, minimum, maximum, and spread.
- Verify CSV header-once behavior, appended rows, readback/export, and restart persistence on physical hardware.

### Milestone 2 — Improve timing without changing workload semantics

- Integrate a microsecond-capable timer (such as the separately tested `libprof` API) with explicit initialization/error handling.
- Record timer source and resolution in result metadata.
- Separate render-only and end-to-end presentation modes rather than changing the meaning of current scores.

### Milestone 3 — Make scene definitions precise

- Add a solid-color triangle path if `triangle-throughput` is intended to be untextured.
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
- `V3/src/bench.cpp` — three scene implementations and RTC-based timing.
- `V3/src/bench.hpp` — scene IDs, configuration, and result structures.
- `V3/src/shapes/triangle.cpp` — scanline triangle rasterization and screen-space UV interpolation.
- `V3/src/display.cpp` — Gint VRAM writes, generated UV map, and image/palette helpers.
- `V3/src/world.cpp` — separate block-world camera/projection and average-depth quad sorting.
- `THIRD_PARTY_NOTICES.md` — glmark2, Gint, and fxSDK reference/attribution notes.

## 8. Scope and licensing

The current third-party notice states that glmark2's benchmark organization was consulted as design inspiration and that its code was not intentionally copied into the current V3 benchmark implementation. That is not a comprehensive provenance audit of all historical files. If source code, assets, or other materials are copied or adapted, review their licenses and preserve applicable notices and terms.

## Definition of a useful first validated release

A first validated fxmark release should run all three deterministic workloads on the physical target, report exact workload and measurement semantics, produce repeatable results across multiple runs, demonstrate CSV persistence on the target OS, and clearly distinguish screen-space raster workloads from general 3D rendering. It must not claim OpenGL compatibility or direct score comparability with glmark2.
