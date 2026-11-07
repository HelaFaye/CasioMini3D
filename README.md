# CasioMini3D

A small 3D renderer and benchmark prototype for Casio fx-CG calculators, built around the Gint / fxSDK toolchain.

## What is in this repo

- `V3/` — current benchmark and demo app
- `V3/src/` — renderer, benchmark runner, and main loop
- `docs/fxmark-design.md` — benchmark design and workload roadmap

## Benchmark architecture

This project follows the same high-level separation used by glmark2:

- a benchmark runner orchestrates workload selection, warm-up, timing, and reporting
- each benchmark is a fixed render scene with deterministic inputs
- the rendering logic stays separate from the control/UI loop

The current V3 implementation contains a minimal benchmark suite inspired by glmark2's scene-based layout:

- `clear-only` — baseline frame-clear workload
- `triangle-throughput` — fixed triangle count, fixed transform workload
- `textured-geometry` — repeated textured or geometry-heavy draw workload

This is intentionally a target-native benchmark suite, not a desktop OpenGL glmark2 port.

## Build

Use the fxSDK toolchain that provides the Gint package and the fx conversion utilities.

From the repository root:

```bash
cd V3
cmake -S . -B build-cg
cmake --build build-cg
```

If your fxSDK exposes separate build helpers for the calculator family you are targeting, prefer those wrappers instead of a raw CMake invocation so the correct toolchain file and platform setting are used.

## Run

The app currently supports:

- `F6` — clear-only benchmark
- `EXE` — triangle-throughput benchmark
- `DEL` — textured-geometry benchmark
- `MENU` — quit

The benchmark runner reports warm-up time, measured elapsed time, FPS, and average milliseconds per frame.

## glmark2-inspired benchmark model

The reference implementation in glmark2 splits a benchmark into two concerns:

1. `Benchmark` — configuration and orchestration of a scene
2. `Scene` — render logic, options, validation, and per-scene timing

This repo follows the same general idea, with a smaller, calculator-native set of workloads and a simpler result report. The benchmark runner is intentionally lightweight and should remain easy to port or extend without dragging the interactive demo code into the timed section.

## Validation checklist for real hardware

Before widening the benchmark suite, validate each benchmark on the target device in the same clock configuration.

1. Build the app with the fxSDK toolchain for the target calculator.
2. Run each benchmark mode in turn:
   - clear-only
   - triangle-throughput
   - textured-geometry
3. Record the exact calculator model, clock setting, and warm-up/measurement frame counts.
4. Repeat each benchmark at least 3 times to check stability.
5. Keep the result if it is repeatable and not obviously dominated by UI or timing noise.
6. Remove or mark any benchmark as unsupported if it is inconsistent between runs.
7. Publish only the workloads that are reproducible on real hardware.

## Current status

This repository is a working benchmark prototype and a teaching/reference implementation for a target-native renderer, not a complete glmark2 reimplementation.

Current state:

- benchmark runner exists
- deterministic benchmark scenes exist for the first three workloads
- scene selection and result display are implemented in the V3 app
- additional glmark2-style workloads can be added once we validate the target rendering path and timer source

## Notes

This is not a desktop OpenGL glmark2 port. It is a target-native benchmark runner for the fx-CG50 rendering path, using the existing Gint display and rasterizer pipeline.

