# CasioMini3D

A small 3D renderer and benchmark prototype for Casio fx-CG calculators, built around the Gint / fxSDK toolchain.

## What is in this repo

- `V3/` — current benchmark and demo app
- `V3/src/` — renderer, benchmark runner, and main loop
- `docs/fxmark-design.md` — benchmark design and workload roadmap

## Build

Use the fxSDK toolchain that provides the Gint package and the fx conversion utilities.

From the repository root:

```bash
cd V3
cmake -S . -B build-cg
cmake --build build-cg
```

If your fxSDK exposes separate build helpers for the calculator family you are targeting, prefer those wrappers instead of a raw CMake invocation so the correct toolchain file and platform settings are applied.

## Run

The app currently supports:

- `F6` — clear-only benchmark
- `EXE` — triangle-throughput benchmark
- `DEL` — textured-geometry benchmark
- `MENU` — quit

The benchmark runner reports warm-up time, measured elapsed time, FPS, and average milliseconds per frame.

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

## Notes

This is not a desktop OpenGL glmark2 port. It is a target-native benchmark runner for the fx-CG50 rendering path, using the existing Gint display and rasterizer pipeline.
