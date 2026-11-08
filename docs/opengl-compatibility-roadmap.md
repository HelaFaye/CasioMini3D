# OpenGL Compatibility and fxmark Workload Roadmap

**Status:** design target; implementation is incremental  
**Hardware target:** Casio fx-CG50, OS 3.20.0202  
**Platform:** SH-4A, Gint 2.11+, fxSDK  
**Reference:** glmark2 (OpenGL 2.0 / OpenGL ES 2.0)

## 1. Project direction

CasioMini3D should grow from a four-scene benchmark into a reusable CPU-rendered graphics library with an OpenGL-style API.

The primary compatibility target is the OpenGL ES 2.0 programming model: vertex attributes and draw calls, vertex and fragment processing, uniforms and interpolated varyings, texture objects and samplers, render state, resource queries, error reporting, and off-screen render targets where memory permits.

There is no OpenGL GPU in the fx-CG50. Commands are executed by a software pipeline on the SH-4A CPU and presented through Gint. The API should preserve familiar GLES 2.0 names and semantics where implemented, but expose implementation limits. It must not claim Khronos conformance without conformance testing.

A desktop OpenGL 1.x compatibility facade can be considered after the GLES-style core is stable. It should translate fixed-function state into the same internal pipeline rather than create a second renderer.

## 2. Proposed architecture

Application and benchmark scenes  
→ GLES-style public C API  
→ context and resource state  
→ software graphics pipeline  
→ caller-owned render surfaces  
→ Gint presentation backend

The context owns program/uniform state, vertex attribute and buffer bindings, texture objects and sampler state, and render state. The software pipeline performs vertex fetch and vertex processing, primitive assembly, clip-volume clipping, perspective divide, viewport transformation, rasterization, varying interpolation, fragment processing, texture sampling, scissor/stencil/depth tests, blending, and color writes.

The renderer core must not include Gint headers. A surface descriptor supplies pixel storage, dimensions, stride and format. The Gint backend binds the active calculator framebuffer and handles presentation.

Start with one current context, matching the single-screen calculator use case, but keep state in an explicit context structure so multiple contexts remain architecturally possible.

### Current implementation status on this feature branch

This is a partial OpenGL ES 2.0-style software renderer, not a conformant GLES implementation.

| Area | Present implementation | Major gaps |
|---|---|---|
| Context and surface | Single current context; caller-owned RGB565 default surface; framebuffer/renderbuffer objects; RGB565 texture/renderbuffer color attachment, depth16 and stencil8 renderbuffers, completeness/dimension checks, glReadPixels | Multiple contexts, broader color/depth formats, multisample and the remaining specification edge cases |
| Buffers and attributes | Bounded buffer objects, upload/sub-update, eight attribute slots, client-memory and VBO attribute fetch, constant attributes | Full GLES object/query semantics and complete type/edge-case coverage |
| Draw calls | glDrawArrays and glDrawElements; points, line pairs/strips/loops, triangles/strips/fans are routed to software primitive paths; basic point/line clipping and scissor regression tests pass | Broader primitive edge-rule, clipping, interpolation and full specification conformance remain |
| Vertex/primitive pipeline | Attribute decoding, vertex shader execution, six-plane homogeneous clipping, perspective divide, viewport transform, triangle coverage, perspective-correct varying interpolation | Full GLES clipping/precision behavior, polygon offset, provoking-vertex details and exhaustive raster rules |
| Fragment pipeline | Fragment shader execution/discard, scissor, depth, stencil, blend equations/factors, color masks, RGB565 writes; framebuffer target resolution includes colorless depth/stencil targets | Multisample behavior, alpha-capable targets, full blend/logic semantics and exact conformance |
| GLSL ES front end/VM | Bounded source, lexer/parser/typed bytecode VM; float, int, bool, vec2/3/4, mat4, sampler2D; attributes/uniforms/varyings; constructors, swizzles, arithmetic, selected vector functions, texture2D; compile diagnostics | No control flow or loops, user functions, arrays/structs, mat2/mat3, full built-in library, precision model, complete type conversion/overload rules, preprocessor |
| Shader/program objects | Create/source/compile, attach/detach, link, use, delete-pending lifetime, attribute/uniform locations, selected vector/matrix/sampler setters, active-variable and source/log queries | Full active-variable semantics, uniform arrays and all matrix/vector setters, relinking semantics, validation and object-query edge cases |
| Textures | Two units; bounded RGB565 image/sub-image upload; unpack alignment; nearest/linear sampling; clamp/repeat/mirrored-repeat | RGBA/alpha/luminance formats, mipmaps, mipmap filters, texture completeness, full pixel-store behavior and more than two units |
| Render state and queries | Common viewport/scissor/cull/depth/blend/stencil state and selected queries | Broad GLES state/query coverage, extension strings, complete limits and exact invalid-operation/enum rules |
| Calculator integration | Renderer core is separated from Gint; V3 binds caller-owned VRAM/depth surfaces and presents through dupdate() in the app runner | The new app has not yet been fxSDK/SH cross-built or run on the fx-CG50 |

The triangle path is connected end-to-end: GL-style calls → linked vertex shader → vertex fetch → clip-space primitive → rasterizer → fragment shader → depth/stencil/blend → RGB565 surface. Basic point and line tests now cover point placement, homogeneous clipping and scissor-limited line coverage. These tests are useful regression checks, not a substitute for the GLES rasterization conformance suite.

The shader compiler is intentionally a strict subset. It rejects unsupported syntax instead of pretending to implement it. A program that compiles here is not necessarily a valid program for every GLES 2.0 driver, and a GLES 2.0 program may use syntax or built-ins that this compiler does not yet accept.

Current implementation budgets are eight vertex attributes, two texture units, eight varying scalar components, bounded shader/resource pools, a 128×128 texture dimension limit, and a 64 KiB aggregate RGB565 texture budget. These are provisional software limits, not hardware capability claims.

The host suite now has eight CTest targets: legacy triangle rasterizer, benchmark registry, texture sampler, GLES context/state, matrix/clipping, software triangle pipeline, GLSL VM, and fxGLMark2 scene/runner integration. An additional warnings-as-errors object target compiles the real app entry point against Gint/fxlibc stubs. Host CI success validates parser/battery behavior, the first shader-driven scene variants, and menu compilation, but does not establish calculator compatibility or Khronos conformance. No Khronos GLES conformance suite has been run.


## 3. Feature roadmap

### 3.1 Foundation and raster correctness

- RGB565 texture views with nearest and bilinear sampling.
- Independent clamp and repeat addressing for U and V.
- Explicit stride and non-owning texture storage.
- Render-surface descriptors with dimensions, pitch and pixel format.
- Viewport and scissor clipping.
- Triangle, line and point primitive assembly.
- Homogeneous clip planes and safe perspective divide.
- Perspective-correct interpolation of texture coordinates and varyings.
- Back/front-face culling and configurable winding.
- Fixed-point inner loops where they improve SH performance without compromising range safety.

### 3.2 GLES-style resources and state

Implement progressively with standard GLES names and typed handles:

- Buffers: generation, binding, deletion, data upload and sub-data update.
- Vertex attributes: enable/disable, pointer, stride and component type.
- Draw arrays and draw elements.
- Texture objects: 2D image upload, sub-image update, filters, wrap modes and mip levels.
- Viewport, scissor, clear color/depth/stencil.
- Depth function and depth write mask.
- Blend functions/equations and color write masks.
- Cull face and front-face winding.
- Stencil functions, operations and masks.
- Error queue, state queries, vendor/renderer/version strings.

Define queryable limits for texture dimensions, resource counts, vertex attributes, varyings, uniforms, render-target dimensions and shader instruction budgets. Fixed-capacity handles or bounded allocators are preferred over unbounded fragmentation.

### 3.3 Programmable shaders

OpenGL ES 2.0 shader support is a major milestone, not a small add-on.

Implement a constrained GLSL ES 1.00 front end and interpreter/bytecode backend rather than runtime native-code generation. SH has no LLVM JIT backend, and executable code generation would add unnecessary complexity.

The initial language subset should cover:
- scalar, vec2/vec3/vec4 and mat2/mat3/mat4 values;
- attributes, uniforms and varyings;
- arithmetic, swizzles, constructors and assignment;
- if/else and bounded for/while loops;
- common built-ins such as dot, normalize, length, min, max, clamp, mix, step, smoothstep, reflect, sin, cos, pow and texture2D;
- vertex output gl_Position and fragment output gl_FragColor;
- compile/link diagnostics, type checking and bounded instruction count.

Unsupported constructs must fail compilation with a useful log. They must never silently produce a different shader.

### 3.4 Texture and fragment features

- RGB565, RGBA4444 and alpha/luminance formats where useful.
- Nearest and bilinear sampling.
- Clamp-to-edge, repeat and mirrored repeat.
- Mipmap generation and minification filters.
- Perspective-correct texture coordinates.
- Alpha blending, alpha test as an internal extension, and color masks.
- Optional palette texture formats for memory efficiency.
- Render-to-texture and framebuffer objects with completeness validation.

### 3.5 3D pipeline features

- Model/view/projection transforms and normal transforms.
- Near/far and side-plane clipping in homogeneous clip space.
- Depth buffer and comparison operations.
- Stencil buffer operations.
- Back-face culling.
- Perspective-correct interpolation.
- Polygon offset where practical.
- Material/light helpers as optional convenience APIs, not a replacement for programmable shaders.

### 3.6 Optional desktop compatibility layer

After the GLES-style path is stable, provide selected desktop OpenGL 1.x conveniences: matrix stacks and matrix mode, current color/normal/texture coordinates, immediate-mode begin/end, fixed-function material and light state, and generated vertex/fragment programs feeding the same internal pipeline.

Do not implement an independent legacy rasterizer.

## 4. glmark2-inspired workload mapping

The active app is fxGLMark2, a native fx-CG50 benchmark selector using the CasioMini3D GLES-style software renderer. It implements a strict first subset of upstream scene families; it is not the upstream Linux/EGL program and the scores are not directly comparable with desktop/mobile glmark2.

### 4.1 Selectable workloads today

| Menu label | Upstream-style description | Current workload path |
|---|---|---|
| Clear baseline | `clear` | Clear active RGB565/depth surface and present |
| Build / client arrays | `build:use-vbo=false` | Procedural 144-triangle UV sphere from client-memory arrays |
| Build / VBO | `build:use-vbo=true` | Same sphere using a vertex buffer object |
| Texture / nearest | `texture:texture-filter=nearest` | RGB565 texture, repeat addressing, nearest sampling |
| Texture / linear | `texture:texture-filter=linear` | Same mesh/texture with bilinear sampling |
| Shading / Gouraud | `shading:shading=gouraud` | Vertex normal transform, one diffuse light, interpolated color varying |

The generated sphere is a compact deterministic workload specific to this port. It does not use glmark2's upstream model assets. Texture is also generated locally; no upstream texture assets are included.

### 4.2 Test batteries and menu

- **Quick Smoke (2):** VBO build and nearest texture.
- **glmark2 Supported Default (5):** client arrays, VBO, nearest texture, linear texture and Gouraud shading. This is the currently ported subset of the upstream default workload categories, not the complete upstream default suite.
- **All Implemented (6):** all five supported default-subset tests plus the clear baseline.

The app opens the selection menu rather than running tests automatically. Up/Down moves the cursor, EXE/F6 toggles a test, F1/F2/F3 selects the named batteries, F4 lists pending scene families, F5 runs the selection, and MENU exits. The first menu selection is Quick Smoke but requires explicit F5 to start.

The initial run profile is deliberately bounded at 2 warm-up frames, 20 measured frames and 2 repetitions per scene. It measures the full frame path including dupdate() call time. Result rows are appended to `/glmark2-fx-cg50-v1.csv`; the older fxmark hardware data file `/benchmarks-v2.csv` is not modified.

### 4.3 Recognized but unported scene families

| glmark2 family | Intended fx-CG50 workload | Status |
|---|---|---|
| build | Client arrays vs buffers; later indexed draws/interleaved attributes/model selection | Client/VBO subset implemented |
| texture | Nearest, bilinear, mipmapped and address-mode comparisons | Nearest/linear on RGB565 implemented; mipmaps pending |
| shading | Gouraud, Blinn-Phong, Phong and cel-style workloads | One-light Gouraud implemented; remaining variants pending |
| buffer | Static/stream/dynamic data and partial updates | Pending |
| bump | Normal-map and height-map perturbation | Pending |
| effect2d | Convolution kernels, blur and edge detection | Pending |
| pulsar | Animated textured/untextured quad workloads with lighting | Pending |
| desktop | 2D sprite/window compositing, shadows and separable blur | Pending |
| ideas | Animated shader effects | Pending |
| jellyfish | Animated multi-object geometry | Pending |
| terrain | Generated height field, normals and layered textures | Pending |
| shadow | Depth-map/shadow comparison using render-to-texture | Pending |
| refract | Environment/refraction approximation | Pending |
| conditionals | Vertex/fragment shader branch stress | Pending |
| function | Shader function-call and arithmetic complexity | Pending |
| loop | Bounded shader loop comparisons | Pending |

The library parser recognizes these scene names but returns an explicit not-implemented diagnostic. It never silently substitutes a different workload. The app's F4 page exposes this backlog to users.

Every future workload needs a written definition: geometry, vertex/index counts, texture dimensions and format, shader variant, render state, measured phase, warm-up, repetitions and known limitations.

## 5. Test strategy

Host tests should compile production renderer code without Gint dependencies.

### Unit and property tests

- Texture address modes, filter endpoints, negative coordinates, padded stride and invalid views.
- Matrix multiplication, projection, normal matrices and transform order.
- Clip-plane intersections, vertices on planes, fully outside primitives and extreme coordinates.
- Primitive assembly for triangles, strips, fans, lines and points.
- Perspective-correct interpolation against analytic fixtures.
- Depth comparisons, depth write masks, stencil operations and blend equations.
- Buffer bounds, index validation, attribute stride/offset and invalid handles.
- Shader lexer/parser/type errors, compile/link failures, instruction budgets and uniform/varying limits.
- Resource allocation failure and cleanup.

### Deterministic renderer tests

Render small scenes into host memory and compare exact framebuffer output for integer/nearest paths, per-channel tolerances for filtered/shaded paths, depth/stencil values, coverage masks and edge rules, and stable hashes where floating-point variance is controlled.

### Fuzz and stress tests

Fuzz malformed shader input, malformed vertex/index buffers, degenerate and extreme geometry, texture dimension/stride overflow, resource exhaustion, repeated create/delete, render-state transitions and invalid enum values.

Run host regression tests with warnings-as-errors and, where supported, AddressSanitizer and UndefinedBehaviorSanitizer.

### Hardware tests

Host tests do not establish SH timing or calculator behavior. Each release candidate still requires fxSDK/SH cross-build, G3A inspection and installation, launch and scene smoke tests, memory/high-water checks, hardware timing at a recorded clock configuration, CSV append/readback and restart persistence, and visual comparison against host reference output where possible.

## 6. Hardware constraints

The fx-CG50 display is 396×224 RGB565. A full-resolution color surface is 177,408 bytes; a 16-bit depth surface costs another 177,408 bytes before textures, vertex buffers, shader state, stack and runtime allocations.

Therefore:
- do not assume a second full-resolution color buffer is always affordable;
- make depth and stencil attachments optional and allocate on demand;
- expose render-target and texture limits through API queries;
- prefer 16-bit storage formats and bounded intermediate buffers;
- permit reduced-resolution off-screen surfaces for effects;
- use caller-owned surfaces and explicit ownership rules;
- measure heap/high-water usage on OS 3.20.0202 before raising limits;
- do not rely on undocumented raw DRAM addresses or OS-version-specific memory regions.

A 64×64 texture, 128×128 texture and full-screen render target must have distinct, explicit memory budgets.

## 7. Reference implementations and licensing

Potential references evaluated:

- **TinyGL:** portable C99 software rasterizer with an OpenGL 1.1-style API and MIT license. Useful for studying software pipeline structure and legacy compatibility. Its published feature list identifies incomplete GL 1.1 coverage and limitations including no stencil buffer and no mipmapping.
- **TinyGLES:** older software OpenGL ES driver based on TinyGL 0.4. Useful for understanding a small GLES-facing software renderer, but it is not a GLSL ES 2.0 implementation.
- **Mesa llvmpipe:** useful architectural reference for CPU shader/raster execution, but it relies on LLVM runtime code generation and is not a suitable direct SH-4A port.
- **glmark2:** workload organization and benchmark semantics reference. Do not copy its GPL-3.0 source or assets into this repository without a deliberate licensing decision.

The initial implementation remains native CasioMini3D code unless a separate third-party port is explicitly adopted after API, architecture, memory, performance and license review.

## 8. Milestone order

1. **Texture foundation** — reusable sampler, explicit texture views, tests and textured-triangle integration.
2. **Render surfaces and core raster pipeline** — viewport/scissor, lines/points, clipper, depth and perspective-correct attributes.
3. **Buffers and GLES-style state API** — handles, vertex attributes, draw calls, state queries and errors.
4. **Texture objects and render targets** — formats, mipmaps, filtering, blending, framebuffer objects.
5. **GLSL ES subset VM** — lexer/parser/type checker/bytecode and program/uniform/varying APIs.
6. **Lighting and effects** — software shader examples corresponding to glmark2 shading, bump and effect2d families.
7. **Expanded benchmark suite** — build, texture, shading, bump, effects, buffer, terrain, shadow, refraction and control-flow workloads.
8. **Optional desktop GL 1.x compatibility facade** — fixed-function convenience translated through the same internal pipeline.

The validated fxmark 2.0 schema-v2 hardware dataset and commits on main remain untouched while this work proceeds on a feature branch.
