# fxGLMark: native Casio fx-CG50 benchmark

**Application:** `apps/fxGLMark/`  
**Renderer:** fxOGL software graphics library  
**Target:** Casio fx-CG50, Gint/fxSDK, 396x224 RGB565  
**Reference:** [upstream glmark2](https://github.com/glmark2/glmark2)

## Scope

fxGLMark is a calculator-native benchmark implementation inspired by
glmark2's workload organization. It is not the upstream Linux/EGL program,
does not redistribute upstream scene assets and does not produce scores
directly comparable with desktop/mobile glmark2.

The first workloads exercise the fxOGL renderer end-to-end: shader objects,
GLSL bytecode, uniforms, client-array/VBO vertex fetch, primitive assembly,
homogeneous clipping, rasterization, perspective-correct varyings, texture
sampling, depth testing, framebuffer clears and Gint presentation.

## Main menu

| Input | Action |
|---|---|
| Up / Down | Move selection cursor |
| EXE or F6 | Toggle highlighted benchmark |
| F1 | Select Quick Smoke |
| F2 | Select supported glmark2 subset |
| F3 | Select All Implemented |
| F4 | Run renderer component microbenchmarks |
| F5 | Run selected benchmarks |
| MENU | Exit add-in |

## Selectable workloads

| Menu label | Workload | Implementation |
|---|---|---|
| Clear baseline | `clear` | Clear active color/depth target and present |
| Build / client arrays | `build:use-vbo=false` | Procedural UV sphere through client-memory arrays |
| Build / VBO | `build:use-vbo=true` | Same mesh through a vertex buffer object |
| Texture / nearest | `texture:texture-filter=nearest` | RGB565 generated texture, nearest repeat sampling |
| Texture / linear | `texture:texture-filter=linear` | Same geometry/texture with bilinear sampling |
| Shading / Gouraud | `shading:shading=gouraud` | Vertex lighting, normal transformation and interpolated fragment color |

The geometry workloads use a compact procedural low-resolution UV sphere
(6 latitude bands by 12 longitude sectors, 144 triangles). It is specific to
this calculator port and is not an upstream glmark2 model.

## Test batteries

| Preset | Workloads | Purpose |
|---|---:|---|
| Quick Smoke | 2 | VBO build and nearest-texture sanity check |
| glmark2 Supported Default | 5 | Current ported subset: client arrays, VBO, nearest, linear and Gouraud |
| All Implemented | 6 | All supported workloads plus clear baseline |

The five-workload preset is explicitly a supported subset, not the complete
upstream glmark2 default suite. Other scene families remain pending until
their required renderer features are implemented.

## Measurement configuration

Each workload currently uses two warm-up frames, 20 measured frames and two
repetitions. Results report min/max/mean/median frame time and derived FPS.
The full-frame measurement includes software rendering and the Gint
`dupdate()` call; it is not a pure rasterizer measurement.

Results append to `/glmark2-fx-cg50-v1.csv`.

## Renderer component microbenchmarks

F4 reports seven independent stages:

| Stage | Timed operation |
|---|---|
| `texture-sample-fixed` | 16,384 validated fixed-point RGB565 samples |
| `texture-nearest-clamp` | 16,384 normalized nearest samples with clamp addressing |
| `texture-nearest-repeat` | 16,384 prepared power-of-two nearest repeat samples |
| `triangle-raster` | 8 production GLES triangle draws with fixed-color fragment callback |
| `shader-vm` | 1,024 arithmetic-only GLSL fragment shader executions |
| `texture2d-vm` | 1,024 GLSL texture2D shader executions, including sampler callback |
| `framebuffer-present` | 8 calls to Gint `dupdate()` |

Each stage has an untimed warm-up and three timed repetitions. Results append
to `/glmark2-renderer-microbench-v1.csv` with schema:

`schema_version,stage,iterations,min_us_per_iteration,max_us_per_iteration,mean_us_per_iteration,operations_per_second`

The `framebuffer-present` timing measures the call duration, not physical LCD
scanout completion. Component timings are diagnostics and must not be
compared numerically with complete scene frame times.

## Validation status

fxGLMark has been built for the SH target and exercised on a physical fx-CG50
running OS 3.20.0202. The latest supplied 24-frame nearest-texture run averaged
3.117216 seconds (144 triangles/frame); its paired VBO build run averaged
3.133346 seconds. These are calculator-specific measurements rather than
desktop/mobile glmark2 scores.

Remaining release validation includes linear texture and Gouraud scene runs,
all test batteries, CSV persistence across calculator restart, memory
high-water, long-run stability and reproducible reference output.

See [`fxOGL-roadmap.md`](fxOGL-roadmap.md) for the graphics library's
compatibility status and remaining implementation work.
