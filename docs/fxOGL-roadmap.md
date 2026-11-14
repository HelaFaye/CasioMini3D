# fxOGL compatibility roadmap

**Status:** incremental software implementation; not Khronos-conformant  
**Hardware target:** Casio fx-CG50, OS 3.20.0202  
**Platform:** SH-4A, Gint 2.11+, fxSDK  
**API target:** OpenGL ES 2.0 programming model

## Direction

fxOGL is a reusable CPU-rendered graphics library for Casio calculators. The
primary API target is the OpenGL ES 2.0 programming model: vertex attributes
and draw calls, vertex and fragment processing, uniforms and interpolated
varyings, texture objects and samplers, render state, resource queries and
off-screen render targets where memory permits.

There is no OpenGL GPU in the fx-CG50. Commands execute in software on the
SH-4A CPU and the calculator application supplies the platform framebuffer
and presentation integration.

The API preserves familiar GLES names and semantics where implemented, but
must expose implementation limits. It must not claim Khronos conformance
without running the applicable conformance suite.

## Architecture

```text
fxGLMark or another add-in
  -> fxOGL public GLES-style API
  -> context and resource state
  -> software graphics pipeline
  -> caller-owned RGB565 render surface
  -> platform presentation backend (Gint in fxGLMark)
```

The renderer core is separated from Gint. The current library uses a bounded
single-current-context implementation; state is grouped so multiple contexts
can be considered later without binding the core to calculator display
globals.

## Current implementation

| Area | Present implementation | Major gaps |
|---|---|---|
| Context and surface | Single current context; caller-owned RGB565 surface; framebuffer/renderbuffer objects; RGB565 texture/renderbuffer attachments; depth16 and stencil8 renderbuffers; completeness/dimension checks; glReadPixels | Multiple contexts, broader color/depth formats, multisample and remaining specification edge cases |
| Buffers and attributes | Bounded buffer objects, upload/sub-update, eight attribute slots, client-memory and VBO fetch, constant attributes | Complete GLES object/query semantics and all type/edge-case coverage |
| Draw calls | glDrawArrays and glDrawElements; points, lines, strips, loops, triangles, triangle strips and fans | Exhaustive primitive edge rules and conformance |
| API surface | All 142 GLES 2.0 entry points. glFlush/glFinish/glReleaseShaderCompiler are no-ops (rendering is synchronous); glShaderBinary and compressed texture uploads report GL_INVALID_ENUM (no formats are advertised); glHint, glLineWidth (lines rasterize within the aliased range [1, 1]) and glSampleCoverage (no sample buffers) store queryable state; glGetShaderPrecisionFormat reports IEEE single floats and integers exact to 2^24; glPolygonOffset offsets filled triangles by factor x max depth slope + units / 65535; glCopyTexImage2D/glCopyTexSubImage2D copy RGB565 color into GL_RGB textures | Wide lines, multisampling, compressed and binary formats |
| Vertex/primitive pipeline | Attribute decoding, vertex shader execution, six-plane homogeneous clipping, perspective divide, viewport transform, triangle coverage, perspective-correct varying interpolation, and square points sized by vertex-shader `gl_PointSize` (reported range 1–16 pixels) | Full GLES precision/clipping behavior, polygon offset, point-sprite coordinates and exhaustive raster rules |
| Fragment pipeline | Fragment execution/discard, scissor, depth, stencil, blend equations/factors, color masks and RGB565 writes | Multisample behavior, alpha-capable targets and full blend/logic semantics |
| GLSL ES front end/VM | Bounded lexer/parser/typed bytecode VM; scalar, vector and matrix values; attributes/uniforms/varyings; constructors, swizzles, arithmetic, selected built-ins; if/else, the `?:` conditional operator (only the selected operand is evaluated), for/while, break/continue; user-defined functions with prototypes, forward definitions, exact-type overload resolution, up to four in/out/inout parameters, mutable input copies, typed returns, writable vector-swizzle assignments to locals, vertex varyings, gl_Position and gl_FragColor (including scalar/reordered selectors), stage-output copy-out, and bounded nested calls, constant-index matrix-column reads and writable local matrix-column assignments (mat2/mat3/mat4); one-dimensional local arrays with constant-index element l-values (whole elements, swizzles, matrix columns, out/inout arguments) and bounds-checked int-expression indices for whole-element reads and writes; uniform arrays with constant and bounds-checked dynamic reads, including sampler arrays (constant indices only, as GLSL ES 1.00 requires); varying arrays (constant indices, matched by name, type and size at link); array function parameters passed by value with in/out/inout copy semantics (elements count toward the four-parameter limit); texture2D | Other built-in outputs, structs, do-while/switch, full built-in library, precision model, preprocessor and complete overload/type rules |
| Shader/program objects | Create/source/compile, attach/detach, link, use, delete-pending lifetime, attribute/uniform locations, selected vector/matrix/sampler setters, uniform arrays (`name[i]` locations, `glUniform*v`/`glUniformMatrix*fv` counts with excess values ignored, `name[0]` active-uniform reporting with array size, cross-stage size matching), active-variable and source/log queries | Full active-variable semantics, sampler arrays, full setter/query coverage and relinking edge cases |
| Textures | Two units; bounded RGB565 image/sub-image upload; unpack alignment; explicit mip-level upload and glGenerateMipmap; nearest/linear sampling and GLES minification filters; clamp/repeat/mirrored-repeat | RGBA/alpha/luminance formats, texture-completeness edge cases and full pixel-store behavior |
| Render state and queries | Common viewport/scissor/cull/depth/blend/stencil state and selected queries | Full GLES state/query coverage, extensions, limits and exact invalid-operation rules |
| Calculator integration | fxOGL core has no Gint framebuffer dependency; fxGLMark binds calculator VRAM/depth surfaces and presents through dupdate(); SH cross-build and physical fx-CG50 runs completed | Broader workload, memory, stability and output validation; no claim of Khronos conformance |

The implementation budgets are currently eight vertex attributes, two texture
units, eight varying scalar components, eight local slots per function and
sixteen uniform slots per program (each array element uses one slot), bounded shader/resource pools, a
128x128 texture dimension limit and a 64 KiB aggregate RGB565 texture budget.
These are software limits, not hardware capability claims.

## Performance work

Recent measured optimizations include:

- Incremental edge-plane interpolation and scissor preclipping in the
  triangle rasterizer.
- RGB565 opaque scanline writes.
- Cached texture bindings and prepared power-of-two repeat samplers.
- Fragment runtime reuse and reduced shader value copies.
- GLSL Texture2D VM stack-slot reuse.
- A draw-time fast path for the exact
  `gl_FragColor = texture2D(u_texture, v_uv);` shader form, while retaining
  the normal sampler callback.
- Removal of software floating-point work from per-pixel and per-texel
  paths (the SH7305 has no FPU, so each float operation is a library call):
  bit-pattern clamp, coverage and finite tests; compile-time RGB565
  quantization and expansion tables; stopping each scanline after its
  covered run; early depth testing when no stencil test is active; integer
  16.16 texture coordinate conversion in place of double arithmetic; and
  shift/mask texel addressing in place of 64-bit division. Every
  replacement reproduces the original float result exactly (checked over
  all 2^32 inputs) and fxGLMark frames are bit-identical.

The host tools behind this work live in `apps/fxGLMark/tests/profile/`: a
golden-frame test (run by host CI) that proves a change leaves every
fxGLMark frame bit-identical, and a soft-float cost model of the device
(`profile.sh`) with its calibration against these measurements.

Measured on a physical fx-CG50 with the optimized build (commit `67ac1cd`),
against the preceding `52798af` run (24 frames of 144 triangles):

| fxGLMark scene | `52798af` | `67ac1cd` | Change | Host model |
|---|---|---|---|---|
| `build:use-vbo=true` (two runs each) | 3.194779 s | 2.292015 s | -28.3% (1.39x) | -36.7% |
| `build:use-vbo=false` | 3.188029 s | 2.284665 s | -28.3% (1.40x) | |
| `texture:texture-filter=nearest` (two runs each) | 3.163459 s | 1.913667 s | -39.5% (1.65x) | -49.9% |
| `texture:texture-filter=linear` | 4.616741 s | 2.322774 s | -49.7% (1.99x) | -58.3% |
| `shading:shading=gouraud` | 3.194158 s | 2.292590 s | -28.2% (1.39x) | -36.7% |
| `clear` (no triangles) | 38.242 ms | 34.373 ms | -10.1% | 0% |

| F4 microbenchmark stage | `52798af` | `67ac1cd` | Change |
|---|---|---|---|
| `texture-sample-fixed` | 8.215 µs | 8.681 µs | +5.7% |
| `texture-nearest-clamp` | 18.011 µs | 17.911 µs | -0.6% |
| `texture-nearest-repeat` | 10.992 µs | 5.630 µs | -48.8% |
| `triangle-raster` | 264.724 ms | 126.269 ms | -52.3% |
| `shader-vm` | 124.059 µs | 124.352 µs | +0.2% |
| `texture2d-vm` | 34.812 µs | 29.526 µs | -15.2% |
| `framebuffer-present` | 10.997 ms | 10.997 ms | +0.0% |

The host profiling model (float operations routed through cost-weighted
soft-float shims) ranked the hot paths correctly but overstated the savings
by about a quarter, so its software-float costs are weighted somewhat higher
than the SH-4 libgcc routines actually cost. Two results are attributed to
code placement rather than to logic: `clear`, whose code did not change,
became 10% faster, and `texture-sample-fixed` (the fixed-point
`sample_texture_validated` path, unused by fxGLMark scenes, whose only
change replaced a 64-bit division and remainder by a shift and mask) became
5.7% slower. Tight loops on this device therefore move by up to about 10%
with layout alone; only larger or consistent changes are treated as
performance results.

A follow-up run on commit `ea02e8c` (GLSL `?:`, array and sampler-array
parameters, the remaining GLES 2.0 entry points and polygon offset; no
per-pixel path change when polygon offset is disabled) confirmed no
regression against `67ac1cd`:

| fxGLMark scene | `67ac1cd` | `ea02e8c` | Change |
|---|---|---|---|
| `build:use-vbo=true` (two runs each) | 2.292015 s | 2.272323 s | -0.9% |
| `build:use-vbo=false` | 2.284665 s | 2.259410 s | -1.1% |
| `texture:texture-filter=nearest` (two runs each) | 1.913667 s | 1.893010 s | -1.1% |
| `texture:texture-filter=linear` | 2.322774 s | 2.306384 s | -0.7% |
| `shading:shading=gouraud` | 2.292590 s | 2.272456 s | -0.9% |
| `clear` (no triangles) | 34.373 ms | 38.305 ms | +11.4% |

Microbenchmarks moved by at most 1.9% (`texture-nearest-clamp` 17.911 to
18.254 µs; `shader-vm` 124.352 to 124.915 µs; `triangle-raster` 126.269 to
125.817 ms). `clear`, whose code has not changed, has now measured 38.242,
34.373 and 38.305 ms across three builds, confirming that its earlier -10%
was code placement rather than a speed-up.

The two preceding physical fx-CG50 runs measured two builds of the
GLSL array work: commit `11d1320` (array opcodes inserted mid-enum) and commit
`52798af` (the same opcodes appended to the end of the enum and interpreter
switch). No measured scene uses arrays. Each scene is 24 frames of 144
triangles, except the clear scene; `52798af` microbenchmarks are the mean of
three runs.

| fxGLMark scene | `11d1320` | `52798af` | Change |
|---|---|---|---|
| `build:use-vbo=true` (two runs each) | 3.205132, 3.204416 s | 3.194901, 3.194657 s | -0.31% |
| `build:use-vbo=false` | 3.204427 s | 3.188029 s | -0.51% |
| `texture:texture-filter=nearest` (two runs each) | 3.167329, 3.169339 s | 3.164077, 3.162841 s | -0.15% |
| `texture:texture-filter=linear` | 4.621749 s | 4.616741 s | -0.11% |
| `shading:shading=gouraud` | 3.203591 s | 3.194158 s | -0.29% |
| `clear` (no triangles) | 38.253 ms | 38.242 ms | -0.03% |

| F4 microbenchmark stage | `11d1320` | `52798af` | Change |
|---|---|---|---|
| `texture-sample-fixed` | 8.214 µs | 8.215 µs | +0.01% |
| `texture-nearest-clamp` | 17.953 µs | 18.011 µs | +0.32% |
| `texture-nearest-repeat` | 10.833 µs | 10.992 µs | +1.47% |
| `triangle-raster` | 263.791 ms | 264.724 ms | +0.35% |
| `shader-vm` | 123.791 µs | 124.059 µs | +0.22% |
| `texture2d-vm` | 34.781 µs | 34.812 µs | +0.09% |
| `framebuffer-present` | 10.997 ms | 10.997 ms | +0.00% |

Relative to the preceding overloaded-shader run (3.206439 s VBO build,
3.180418 s nearest texture, 122.535 µs shader VM), every scene is equal or
faster, while the shader VM microbenchmark is 1.03% (`11d1320`) and 1.24%
(`52798af`) slower. Appending the opcodes did not recover the shader VM cost.
Within-build spread is at most 0.15%, but builds that differ only in GLSL
interpreter source moved `texture-nearest-repeat`, whose code is unchanged,
by 1.47%. Microbenchmark differences of about 1.5% between builds are
therefore dominated by code placement in the binary, and the shader VM change
is not attributed to the array opcodes. Cross-build microbenchmark comparisons
below that level should not be treated as performance claims. Scene results
remain the primary hardware measure. These device measurements are
configuration-specific and not directly comparable to desktop/mobile glmark2.

Earlier hardware baselines: overloaded-shader run, 3.206439 s VBO build and
3.180418 s nearest texture; prototype/forward-definition run, 3.198292 s and
3.167104 s.

## Priorities

1. Add the built-in fragment inputs (gl_FragCoord, gl_FrontFacing, gl_PointCoord) and the remaining texture lookups (texture2D bias, texture2DProj, texture2DLod). Array return types are not part of GLSL ES 1.00 (they arrived in GLSL ES 3.00).
2. Add additional texture formats and close remaining texture-completeness and pixel-store edge cases.
3. Increase coverage of GLES state, queries, errors and resource lifetime.
4. Add differential tests for every new shader and pipeline optimization.
5. Measure memory high-water and long-run behavior on physical hardware.
6. Build deterministic reference images and a repeatable hardware test log.
7. Run a formal conformance suite before making any compatibility claim.

Host CI validates parser, sampler, context, pipeline and app integration
behavior. It does not replace the fxSDK/SH build or physical calculator
testing.
