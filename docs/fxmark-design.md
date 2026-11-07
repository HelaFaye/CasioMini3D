# fxmark: benchmark design and implementation status

## Current state

This document describes the benchmark direction for the CasioMini3D project, grounded in the code that exists today.

The project is not a desktop OpenGL or glmark2 port. It is a target-native benchmark prototype built for Casio fx-CG hardware using the Gint / fxSDK toolchain.

The active V3 app currently implements a small benchmark set:

- `clear-only`
- `triangle-throughput`
- `textured-geometry`

These are the workloads currently represented in code. The project is intentionally conservative about benchmark scope until the rendering path and filesystem behavior are validated on real hardware.

## Goal

Build a repeatable graphics benchmark for Casio fx-CG-class calculators using the graphics facilities available through Gint and, where useful, the software renderer in CasioMini3D.

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
```

### 1. Benchmark runner

Owns test selection, warm-up frames, measurement frames, and result presentation. Keep benchmark logic separate from interactive camera controls and demo setup.

### 2. Benchmark scenes

Each scene should use deterministic geometry and fixed workload parameters, and state what it measures and which rendering features it requires.

### 3. Rendering backend

The current implementation routes work through the existing software rasterizer and Gint display path. It is not a general-purpose 3D engine abstraction.

### 4. Timing and results

The runner is intended to report benchmark name, warm-up and measurement frame counts, elapsed time, FPS, and average milliseconds per frame. Treat results as experimental until repeated target-device runs establish their accuracy and repeatability.

## Workload rules

For results to be useful and reproducible:

- keep scene data deterministic
- keep screen size, geometry count, and texture settings fixed per benchmark
- separate warm-up from measured iterations
- avoid keyboard polling and UI updates inside the timed section where possible
- record calculator model, clock mode, and software configuration alongside results
- publish only workloads that are stable on real hardware

## Current implementation status

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

## Suggested next steps

### Milestone 1: validate the current suite

- run each benchmark several times on the target calculator
- record clock mode and exact frame counts
- confirm repeatability before broadening the suite
- verify CSV logging independently from benchmark rendering

### Milestone 2: keep the benchmark layer small and stable

- keep benchmark scenes fixed and deterministic
- avoid mixing demo controls into timed benchmark sections
- keep metrics and failure states explicit

### Milestone 3: add only supported workloads

Add workloads only when the rendering path can define them clearly and repeatably. Describe the exact feature measured and how the workload differs from any desktop OpenGL analogue.

## Attribution and licensing

The architecture and scene/benchmark concepts were informed by the public glmark2 project, and its source was consulted as a design reference. See [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md) for the project URL, license identification, consulted files, and scope of this attribution. No glmark2 source code is intentionally copied into the current benchmark implementation; that is not a complete provenance audit of all historical repository content.

## Definition of a useful release

A useful release can run deterministic tests on-device, report repeatable per-test results, document workload settings, and distinguish implemented behavior from target-validated behavior.
