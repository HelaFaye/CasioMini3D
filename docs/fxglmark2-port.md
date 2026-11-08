# fxGLMark2: native fx-CG50 benchmark port

**Application:** `V3/`  
**Target:** Casio fx-CG50, Gint/fxSDK, 396×224 RGB565  
**Renderer:** CasioMini3D's GLES-style software pipeline  
**Upstream reference:** [glmark2](https://github.com/glmark2/glmark2)

## Scope and naming

fxGLMark2 is a calculator-native implementation of glmark2-inspired workloads. It does not run the upstream Linux/EGL program, does not copy upstream scene assets, and does not produce scores directly comparable to desktop/mobile glmark2.

The first ported workloads exercise our software renderer end-to-end: shader objects, GLSL bytecode, uniforms, client-array/VBO vertex fetch, triangle assembly, homogeneous clipping, rasterization, perspective-correct varyings, texture sampling, depth testing, framebuffer clears, and Gint display presentation.

The menu uses familiar glmark2 scene names and option syntax where supported. Unsupported scene names and option values are rejected with diagnostics rather than silently mapped to another workload.

## Main menu

The application starts in the benchmark selector. It does not automatically run a suite.

| Input | Action |
|---|---|
| Up / Down | Move the selection cursor |
| EXE or F6 | Toggle the highlighted benchmark |
| F1 | Select Quick Smoke battery |
| F2 | Select glmark2 Supported Default battery |
| F3 | Select All Implemented battery |
| F4 | Run renderer component microbenchmarks |
| F5 | Run the selected benchmarks |
| MENU | Exit the add-in |

The initial selection is Quick Smoke, but no benchmark starts until F5 is pressed.

## Currently selectable workloads

| Menu label | glmark2-style description | Implementation |
|---|---|---|
| Clear baseline | `clear` | Clear the active color/depth target and present |
| Build / client arrays | `build:use-vbo=false` | Procedural sphere mesh through client-memory vertex arrays |
| Build / VBO | `build:use-vbo=true` | Same mesh through a vertex buffer object |
| Texture / nearest | `texture:texture-filter=nearest` | RGB565 checker/gradient texture using nearest sampling and repeat addressing |
| Texture / linear | `texture:texture-filter=linear` | Same geometry and texture with bilinear sampling |
| Shading / Gouraud | `shading:shading=gouraud` | Vertex lighting with normal transformation and a fragment color varying |

The geometry workloads use a procedurally generated low-resolution UV sphere (6 latitude bands × 12 longitude sectors, 144 triangles). This compact deterministic mesh is specific to the fx-CG50 port. It is not one of glmark2's upstream model assets, and its triangle count and shading output are not identical to upstream reference scenes.

## Standard test batteries

| Preset | Workloads | Purpose |
|---|---:|---|
| Quick Smoke | 2 | Fast VBO + nearest-texture sanity pass |
| glmark2 Supported Default | 5 | The currently ported subset: client-array build, VBO build, nearest texture, linear texture, Gouraud shading |
| All Implemented | 6 | All five supported default-subset workloads plus the clear baseline |

**The five-test preset is not the complete upstream glmark2 default battery.** Upstream defines additional texture, shading, bump, post-processing, buffer, shader-control-flow and other workloads. Those remain pending until their required rendering features are implemented.

## Scene catalog and pending work

The library catalog recognizes these upstream scene families:

- Implemented: `clear`, `build`, `texture`, `shading`.
- Pending: `buffer`, `bump`, `effect2d`, `pulsar`, `desktop`, `ideas`, `jellyfish`, `terrain`, `shadow`, `refract`, `conditionals`, `function`, and `loop`.

The parser returns a not-implemented diagnostic for pending families; there are no no-op or substitute results. The catalog remains documented here rather than occupying a menu page.

## Measurement configuration

The first on-device runner uses a bounded configuration for each workload:

- 2 warm-up frames;
- 20 measured frames per repetition;
- 2 measured repetitions;
- per-repetition mean frame time, then min/max/mean/median across repetitions;
- FPS derived from mean frame time.

The short first-pass configuration is intentional: software rasterization on the calculator must be profiled before using long default runs. These timings include the full frame path and the dupdate() call, not just triangle rasterization or physical LCD scanout.

Results are appended in the Gint OS world to `/glmark2-fx-cg50-v1.csv`.

## Renderer component microbenchmarks

F4 runs a second, component-level suite against the same production renderer code. It reports:

| Stage | Iterations per measured repeat | Timed work |
|---|---:|---|
| `texture-sample` | 16,384 | Validated nearest-filter RGB565 texture samples from a generated 32×32 texture |
| `triangle-raster` | 8 | Production GLES triangle draws into the active 396×224 framebuffer using a fixed-color fragment callback |
| `shader-vm` | 1,024 | Executions of a previously compiled GLSL ES fragment shader |
| `framebuffer-present` | 8 | Gint `dupdate()` calls |

Each stage runs one untimed warm-up operation before three measured repetitions. The result screen and CSV contain min/max/mean microseconds per operation and derived operations per second. Shader compilation and benchmark setup are excluded from the shader execution timing. The raster test does not execute the GLSL VM or texture sampler, so it isolates triangle rasterization more directly than a complete scene.

The component results append separately to `/glmark2-renderer-microbench-v1.csv` with schema:

`schema_version,stage,iterations,min_us_per_iteration,max_us_per_iteration,mean_us_per_iteration,operations_per_second`

The `framebuffer-present` figure measures the time spent inside `dupdate()`; it does not measure completion of the LCD scanout. These microbenchmarks are component diagnostics, not glmark2 scene scores, and should not be compared numerically with the per-frame CSV or earlier `/benchmarks-v2.csv` results. The target build and physical calculator timings are still required to establish SH-4A performance.

Schema v1 columns are:

`schema_version,benchmark,frames,triangles_per_frame,min_frame_us,max_frame_us,mean_frame_us,median_frame_us,fps`

The app reports CSV write errors separately from benchmark execution errors. Older `/benchmarks-v2.csv` fxmark hardware microbenchmark data is not modified by this logger.

## Next implementation priorities

1. Complete the GLSL ES 1.00 subset required by upstream shaders: control flow, loops, functions, additional built-ins, integer/bool/vector/matrix uniform APIs, and better diagnostics.
2. Add mipmap levels/filters, RGBA/alpha/luminance formats, and texture completeness.
3. Implement buffer update variants for the buffer workloads.
4. Expand off-screen framebuffer rendering, render-to-texture, and depth targets for effect2d, shadow, and refract.
5. Add advanced lighting, bump mapping, post-processing and procedural scene generation.
6. Profile on hardware and compare each workload against a host reference image.

No claim of full OpenGL ES 2.0 compatibility or upstream glmark2 parity should be made until the language/API gaps, target build, and device tests have been addressed.
