# fxOGL

**fxOGL** is a C++ software graphics library for Casio fx-CG calculators.
**fxGLMark** is its native calculator benchmark application.

The project targets the SH-4A CPU and Gint/fxSDK environment. The renderer
implements a growing subset of the OpenGL ES 2.0 programming model in
software. It does not rely on a graphics GPU and does not currently claim
full GLES 2.0 compatibility or Khronos conformance.

## Projects

| Component | Location | Purpose |
|---|---|---|
| **fxOGL** | [`lib/fxOGL/`](lib/fxOGL/) | Reusable software graphics library, GLES-style API, shader VM, rasterizer and RGB565 texture sampler |
| **fxGLMark** | [`apps/fxGLMark/`](apps/fxGLMark/) | Native fx-CG50 benchmark selector, glmark2-inspired scenes, test batteries and on-device CSV logging |
| **Historical projects** | [`archive/`](archive/) | Earlier renderer generations and model-renderer experiments |
| **Documentation** | [`docs/`](docs/) | Architecture, compatibility roadmap, benchmark design and third-party notices |

## Build fxGLMark for fx-CG50

Install a working fxSDK/Gint toolchain, then run:

```sh
cd apps/fxGLMark
fxsdk build-cg
```

The generated add-in is `fxGLMark.g3a`.

## Run host regression tests

From the repository root:

```sh
cmake -S apps/fxGLMark/tests/host -B /tmp/fxogl-host-tests
cmake --build /tmp/fxogl-host-tests --parallel 2
ctest --test-dir /tmp/fxogl-host-tests --output-on-failure
```

These tests compile the renderer and app integration against host-side Gint
stubs. They validate CPU-side behavior but do not replace an SH cross-build or
physical calculator validation.

## Current graphics capabilities

fxOGL currently provides a programmable vertex/fragment pipeline, a bounded
GLSL ES 1.00 compiler/VM, client arrays and VBOs, primitive clipping and
rasterization, RGB565 nearest/bilinear texture sampling, common depth/stencil/
blend/scissor state, and off-screen framebuffer support.

The implementation remains a strict subset. Major gaps include broader GLSL
control flow and functions, additional texture formats and mipmap behavior,
complete GLES state/query semantics, and Khronos conformance testing. The
current compatibility boundary is documented in
[`docs/fxOGL-roadmap.md`](docs/fxOGL-roadmap.md).

## Hardware measurements

fxGLMark has been measured on a physical fx-CG50. The latest supplied run
(commit `67ac1cd`, 24 frames of 144 triangles) averaged 1.914 seconds per
frame for the nearest-texture scene, 2.292 seconds for the VBO build scene,
2.293 seconds for Gouraud shading and 2.323 seconds for linear texture
filtering, 1.39x to 1.99x faster than the preceding build. These are calculator-specific software-rendering measurements, not
glmark2 scores comparable with desktop/mobile GPUs. Per-scene and
microbenchmark figures are recorded in
[`docs/fxOGL-roadmap.md`](docs/fxOGL-roadmap.md#performance-work).

The device has also produced all seven F4 renderer microbenchmark stages.
Further release validation remains: every battery and workload, CSV
persistence across restart, memory high-water, long-run stability and
reproducible visual output.

## Community contributions

fxOGL is intended to remain a reusable layer above Gint rather than duplicate
Gint's low-level hardware drivers. Potential upstream contributions should be
small and independently useful: fxSDK integration fixes, reproducible
toolchain/build reports, or Gint driver/primitive bug fixes demonstrated by a
minimal case. The software GLES renderer itself belongs here.

See [`docs/repository-layout.md`](docs/repository-layout.md) for the full tree,
[`docs/source-provenance.md`](docs/source-provenance.md) for inherited-code
and licensing boundaries, and [`docs/THIRD_PARTY_NOTICES.md`](docs/THIRD_PARTY_NOTICES.md)
for project references and attribution.
