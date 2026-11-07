# CasioMini3D

A small 3D renderer and benchmark prototype for Casio fx-CG calculators built around the Gint / fxSDK toolchain.

## Project status

This repository is currently in a prototype stage.

The active code is in `V3/`, and it includes:

- a software rasterizer for triangles and simple geometry
- a basic 3D world demo
- a benchmark runner for a small, deterministic benchmark suite
- a minimal UI for selecting tests and reporting results

The current benchmark set is intentionally small and conservative:

- `clear-only`
- `triangle-throughput`
- `textured-geometry`

These are the workloads currently represented in code and documentation. This is not a full desktop OpenGL or glmark2 port.

## Repository layout

- `V3/` — active benchmark/demo application
- `V3/src/` — renderer, benchmarks, and app entry point
- `docs/fxmark-design.md` — benchmark design notes and implementation roadmap
- `ModelRenderer/` — exploratory or legacy rendering work
- `v1/`, `V2/` — earlier prototype generations

## Build

Use the fxSDK toolchain that provides the Gint package and the fx conversion utilities.

From the repository root:

```bash
cd V3
cmake -S . -B build-cg
cmake --build build-cg
```

If your fxSDK exposes calculator-family-specific build helpers, prefer those wrappers so the correct toolchain file and target platform settings are used.

## Run

The app currently supports:

- `F6` — clear-only benchmark
- `EXE` — triangle-throughput benchmark
- `DEL` — textured-geometry benchmark
- `MENU` — quit

The benchmark runner reports warm-up frames, measured elapsed time, FPS, and average milliseconds per frame.

## Benchmark model

The benchmark structure follows the same basic idea as glmark2:

- a benchmark runner selects a fixed workload
- each workload is a deterministic render scene
- the timed section is separated from UI and camera controls
- results are reported as runtime metrics instead of a portable desktop score

This project keeps the scope intentionally smaller and more native to the calculator pipeline than a full glmark2 port.

## Validation checklist for hardware testing

Before expanding the benchmark suite, validate each workload on the target device under the same clock configuration.

1. Build the app with the fxSDK toolchain for the target calculator.
2. Run each benchmark mode in turn:
   - clear-only
   - triangle-throughput
   - textured-geometry
3. Record the exact calculator model, clock mode, and warm-up/measurement frame counts.
4. Repeat each benchmark at least three times to check repeatability.
5. Keep the result only if it is stable and not dominated by UI or timing noise.
6. Mark any benchmark as unsupported if it behaves inconsistently across runs.
7. Publish only workloads that are reproducible on real hardware.

## Current limitations

- This is not a desktop OpenGL port.
- Rendering and timing are tied to the calculator's native display path and Gint environment.
- CSV logging is experimental and depends on the target filesystem / OS-world behavior being writable and available at runtime.
- The benchmark suite is intentionally small until more hardware validation is done.

## Notes

This project is best understood as a target-native graphics benchmark prototype for the fx-CG family, using the Gint display path and the existing software rasterizer pipeline.
