# fxmark: benchmark design and implementation plan

## Status

This document describes a proposed direction for a calculator graphics benchmark inspired by glmark2. It is a design/roadmap, not a claim that the benchmark runner, compatibility API, or full scene suite already exists.

## Goal

Build a repeatable graphics benchmark for Casio fx-CG50-class calculators using the graphics facilities available through Gint and, where useful, the software renderer in CasioMini3D. Reuse glmark2's workload ideas and recognizable test categories where they make sense, but implement the workloads for this target rather than trying to run OpenGL calls unchanged.

fxmark results measure this calculator's rendering path. They are not directly comparable to glmark2 scores from an OpenGL/OpenGL ES implementation.

## Current starting point

The V3 prototype already provides useful foundations:

- Gint display access and direct pixel writes in `src/display.cpp`.
- Triangle, rectangle, and parallelogram rasterization under `src/shapes/`.
- Fixed-point math, a camera projection, cube faces, and a block-world demo in `src/world.cpp`.
- A demo loop and an on-screen FPS estimate in `src/main.cpp`.

These components are currently organized around the cube demo. The FPS display is informational, not a controlled benchmark: the scene is randomized and there is no benchmark runner, fixed test configuration, or structured result output.

## Proposed architecture

```text
benchmark runner
    -> benchmark scene (fixed workload)
        -> small renderer interface
            -> software transform / rasterization
                -> Gint display / framebuffer
    -> timing and result reporting
```

### 1. Benchmark runner

Owns test selection, configuration, warm-up, timed repetitions, and result presentation. It should keep benchmark logic separate from interactive camera controls and random demo setup.

### 2. Benchmark scenes

Each scene creates a deterministic workload: fixed geometry, texture, camera, and object/primitive count. A scene should state what it measures and which rendering features it requires.

### 3. Renderer interface

Introduce a small native API that scenes can call without depending on `World` or `Cube` internals. Begin with operations the implementation can support, such as clearing, submitting triangles or quads, choosing a solid color or texture, and applying transforms. This is a lightweight compatibility facade, not an OpenGL implementation; unsupported OpenGL concepts should not be exposed as if they were implemented.

### 4. Rendering backend

Initially route supported operations through the existing software rasterizer and Gint display path. Extend the pipeline incrementally as real tests need features. For example, a depth buffer would be needed for correct general triangle visibility; sorting whole quads by average depth is only a painter's-order approximation and can fail for intersecting geometry.

### 5. Timing and results

Use a documented timer source and resolution. Report the calculator model, clock setting, test name, workload settings, warm-up/repetition policy, elapsed time, frame count, and derived rate. Keep raw counters where possible so later versions can improve reporting without changing what the test did.

## Workload plan

Start with a small, reliable vertical slice:

1. **Clear-only baseline** — repeatedly clear the frame, measuring the display/framebuffer path with minimal scene work.
2. **Triangle throughput** — draw a fixed number of deterministic triangles at a fixed screen size.
3. **Textured geometry** — draw a fixed textured workload once texture loading, UV behavior, and bounds are well-defined.
4. **Transform/object scaling** — increase a controlled number of transformed objects or primitives to observe how workload size affects rendering time.

Then add further glmark2-inspired workloads only when the target implementation can define them accurately. For example, an OpenGL shader workload could have a clearly named CPU shading analogue, but it would measure CPU software shading—not GLSL or GPU shader performance. Hardware buffer-operation tests should be marked unsupported unless there is a meaningful corresponding operation on the calculator.

## Measurement rules

For results to be useful and reproducible:

- Use fixed scene data; avoid random geometry unless the random seed is fixed and recorded.
- Keep resolution, geometry, texture, and settings fixed per test.
- Separate warm-up from measured iterations.
- Avoid keyboard polling, text updates, and other UI work inside the timed section where practical.
- State clock settings and calculator model with each result; device clock changes can affect performance.
- Report unsupported tests explicitly. Do not silently substitute a different workload under the same test name.
- Do not claim scores are comparable to desktop/mobile glmark2 results; this is a different API and rendering implementation.

## Suggested contribution milestones

### Milestone 1: repeatable runner

Add a benchmark mode, a test registry, fixed test parameters, timed repetitions, and an on-device summary. Implement clear-only and triangle-throughput tests first. Document timer behavior and clock-setting requirements.

### Milestone 2: extract a renderer facade

Define the smallest native rendering interface needed by those tests and route them through the current rasterizer. Keep the existing interactive demo working.

### Milestone 3: expand rendering capability

Add texture sampling and other capabilities as tests require them. Add depth handling before claiming general visibility-correct 3D rendering. Validate clipping and screen bounds.

### Milestone 4: port workload concepts

Implement the remaining feasible glmark2-inspired workloads as target-native scenes, with a description of each workload and any differences from its OpenGL counterpart. Mark unsupported features clearly.

### Milestone 5: validate and publish results

Run tests on actual fx-CG50 hardware, repeat runs, record model/clock/settings, and publish sample output and known limitations.

## Scope and licensing note

A workload-inspired reimplementation is distinct from copying glmark2 implementation code. If code, assets, or other materials are copied or adapted from glmark2, review the upstream license and preserve required notices and terms. Identify the source of any reused assets and ensure their licenses permit redistribution.

## Definition of a useful first release

A first release is useful when it can run at least two deterministic tests on-device, show repeatable per-test results, document the exact workloads and settings, and clearly distinguish supported tests from unsupported OpenGL-specific features.
