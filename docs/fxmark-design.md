# fxmark: benchmark design and implementation status

## Current state

This document describes the benchmark direction for the CasioMini3D project, but it is intentionally grounded in the code that exists today.

The project is not a desktop OpenGL or glmark2 port. It is a target-native benchmark prototype built for Casio fx-CG hardware using the Gint / fxSDK toolchain.

The active V3 app currently implements a small, deterministic benchmark set:

- `clear-only`
- `triangle-throughput`
- `textured-geometry`

These are the implemented workloads at the moment. This project is intentionally conservative about benchmark scope until the rendering path and filesystem behavior are validated on real hardware.

## Goal

Build a repeatable graphics benchmark for Casio fx-CG-class calculators using the graphics facilities available through Gint and, where useful, the software renderer in CasioMini3D.

The goal is to measure the target-native rendering path in a way that is stable enough to compare runs within the same device, clock configuration, and workload setup.

The benchmark output is not directly comparable to glmark2 scores from a desktop/mobile OpenGL or OpenGL ES implementation.

## Benchmark model

The benchmark structure follows the same broad idea as glmark2:

- a benchmark runner selects a fixed workload
- each workload is a deterministic render scene
- the timed section is kept separate from UI and interaction code
- results are reported as measured throughput and timing values

In practice, this repo keeps the model smaller and more calculator-native than a full glmark2 reimplementation.

## Benchmark architecture

```text
benchmark runner
    -> benchmark scene (fixed workload)
        -> render primitive(s) / rasterizer path
            -> Gint display framebuffer
    -> timing and result reporting
```

### 1. Benchmark runner

Owns test selection, warm-up frames, measurement frames, and final reporting. It should keep the benchmark logic separate from interactive camera controls and demo setup.

### 2. Benchmark scenes

Each scene creates a deterministic workload with fixed geometry, fixed camera state, and fixed frame counts. A scene should state what it measures and which rendering features it requires.

### 3. Rendering backend

The current implementation routes work through the existing software rasterizer and Gint display path. This is enough for a small benchmark suite, but it is not a general-purpose 3D engine abstraction yet.

### 4. Timing and results

The runtime reports:

- benchmark name
- warm-up frame count
- measurement frame count
- elapsed time
- FPS
- average milliseconds per frame

This is a stable result format for simple on-device validation and comparison.

## Workload rules

For results to be useful and reproducible:

- keep scene data deterministic
- keep screen size, geometry count, and texture settings fixed per benchmark
- separate warm-up from measured iterations
- avoid keyboard polling and UI updates inside the timed section where possible
- record calculator model, clock mode, and software configuration alongside results
- only publish workloads that are stable on real hardware

## Current implementation status

The following are implemented in the current V3 benchmark app:

- `clear-only` — baseline clear cost
- `triangle-throughput` — fixed triangle count and deterministic placement
- `textured-geometry` — repeated textured geometry path

The following are not yet part of the stable benchmark suite and should only be added after validation:

- generic shader-style workloads
- deep 3D object scenes
- advanced post-processing or blending tests
- fill-rate or overdraw-heavy scenes
- open-ended glmark2-style suites

## Suggested next steps

### Milestone 1: validate the current suite

- run each benchmark several times on the target calculator
- record clock mode and exact frame counts
- confirm repeatability before broadening the suite

### Milestone 2: keep the benchmark layer small and stable

- keep benchmark scenes fixed and deterministic
- do not mix camera-demo logic into the timed benchmark path
- keep metrics clear and comparable

### Milestone 3: add only what is genuinely supported

Add new workloads only when the underlying rendering path can define them clearly and repeatably. This includes describing the exact feature being measured and any differences from a desktop OpenGL analogue.

## Scope and licensing note

This repository intentionally does not claim to be a verbatim copy of glmark2. The project draws inspiration from glmark2's general benchmark structure, but the implementation remains target-native and calculator-specific.

If code or assets are adapted from upstream projects, those materials should be reviewed carefully for licensing and attribution before reuse.

## Definition of a useful release

A useful release is one that can run a small deterministic test suite on-device, report repeatable results, document workload settings, and clearly distinguish between supported and unsupported rendering features.
