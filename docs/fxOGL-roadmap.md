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
| Vertex/primitive pipeline | Attribute decoding, vertex shader execution, six-plane homogeneous clipping, perspective divide, viewport transform, triangle coverage, perspective-correct varying interpolation, and square points sized by vertex-shader `gl_PointSize` (reported range 1–16 pixels) | Full GLES precision/clipping behavior, polygon offset, point-sprite coordinates and exhaustive raster rules |
| Fragment pipeline | Fragment execution/discard, scissor, depth, stencil, blend equations/factors, color masks and RGB565 writes | Multisample behavior, alpha-capable targets and full blend/logic semantics |
| GLSL ES front end/VM | Bounded lexer/parser/typed bytecode VM; scalar, vector and matrix values; attributes/uniforms/varyings; constructors, swizzles, arithmetic, selected built-ins; if/else, for/while, break/continue; user-defined functions with prototypes, forward definitions, exact-type overload resolution, up to four in/out/inout parameters, mutable input copies, typed returns, writable vector-swizzle assignments to locals, vertex varyings, gl_Position and gl_FragColor (including scalar/reordered selectors), stage-output copy-out, and bounded nested calls, constant-index matrix-column reads and writable local matrix-column assignments (mat2/mat3/mat4); one-dimensional local arrays with constant-index element l-values (whole elements, swizzles, matrix columns, out/inout arguments) and bounds-checked int-expression indices for whole-element reads and writes; non-sampler uniform arrays with constant and bounds-checked dynamic reads; texture2D | Other built-in outputs, sampler/attribute/varying arrays, array parameters and returns, structs, do-while/switch, full built-in library, precision model, preprocessor and complete overload/type rules |
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

The latest user-supplied physical fx-CG50 run used the GLSL array build
(commit `11d1320`; local and uniform arrays, no array use in the measured
scenes). Each scene is 24 frames of 144 triangles, except the clear scene.

| fxGLMark scene | Mean frame time |
|---|---|
| `build:use-vbo=true` (two runs) | 3.205132 s, 3.204416 s |
| `build:use-vbo=false` | 3.204427 s |
| `texture:texture-filter=nearest` (two runs) | 3.167329 s, 3.169339 s |
| `texture:texture-filter=linear` | 4.621749 s |
| `shading:shading=gouraud` | 3.203591 s |
| `clear` (no triangles) | 38.253 ms |

| F4 microbenchmark stage | Mean per iteration |
|---|---|
| `texture-sample-fixed` | 8.214 µs |
| `texture-nearest-clamp` | 17.953 µs |
| `texture-nearest-repeat` | 10.833 µs |
| `triangle-raster` | 263.791 ms |
| `shader-vm` | 123.791 µs |
| `texture2d-vm` | 34.781 µs |
| `framebuffer-present` | 10.997 ms |

Relative to the preceding overloaded-shader run (3.206439 s VBO build,
3.180418 s nearest texture), the VBO scene moved by -0.04%/-0.06% and the
nearest-texture scene by -0.41%/-0.35%, within the observed run-to-run
variation of up to about 0.4%. The shader VM microbenchmark rose 1.03% from
122.535 µs with a 0.09% within-run spread, so it is treated as a real but
scene-invisible cost of the array opcodes. As an experiment, the array
opcodes were then appended to the end of the opcode enum and interpreter
switch, restoring the original opcode values and dispatch source for every
pre-array opcode; this build awaits remeasurement. The linear, Gouraud, client-array, clear and remaining microbenchmark figures
are first recorded baselines. These device measurements are
configuration-specific and not directly comparable to desktop/mobile glmark2.

Earlier hardware baselines: overloaded-shader run, 3.206439 s VBO build and
3.180418 s nearest texture; prototype/forward-definition run, 3.198292 s and
3.167104 s.

## Priorities

1. Add array function parameters and sampler arrays; extend remaining GLSL l-value forms; expand built-in function and overload coverage.
2. Add additional texture formats and close remaining texture-completeness and pixel-store edge cases.
3. Increase coverage of GLES state, queries, errors and resource lifetime.
4. Add differential tests for every new shader and pipeline optimization.
5. Measure memory high-water and long-run behavior on physical hardware.
6. Build deterministic reference images and a repeatable hardware test log.
7. Run a formal conformance suite before making any compatibility claim.

Host CI validates parser, sampler, context, pipeline and app integration
behavior. It does not replace the fxSDK/SH build or physical calculator
testing.
