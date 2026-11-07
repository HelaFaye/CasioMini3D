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

## Notes

This is not a desktop OpenGL glmark2 port. It is a target-native benchmark runner for the fx-CG50 rendering path, using the existing Gint display and rasterizer pipeline.
