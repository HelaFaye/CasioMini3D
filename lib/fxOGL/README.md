# fxOGL

**fxOGL** is a calculator-oriented C++ software graphics library targeting the
Casio fx-CG family and the SH-4A CPU. It provides a constrained OpenGL ES 2.0-
style API backed by a software pipeline; it does not use a hardware OpenGL GPU.

## Current implementation

The library currently contains:

- A C-facing GLES-style API with one current context and bounded resource pools.
- Client-array and VBO attribute fetching plus triangle, line and point paths.
- Homogeneous clipping, perspective-correct varying interpolation and rasterization.
- RGB565 texture storage and nearest/bilinear sampling with clamp, repeat and
  mirrored-repeat address modes.
- Depth, stencil, scissor, blending, color masks and framebuffer objects.
- A bounded GLSL ES 1.00 compiler and bytecode VM with conditional/loop control
  flow, loop jumps, and user-defined functions with prototypes, forward
  definitions, exact-type overload resolution, typed returns, and bounded
  `in`/`out`/`inout` copy semantics. Input parameters are mutable local
  copies; outputs copy back to writable locals, vertex varyings, `gl_Position`,
  `gl_PointSize`, and `gl_FragColor`, including non-overlapping vector
  swizzles.
- Square point rasterization driven by vertex-shader `gl_PointSize`, clamped
  to the reported `GL_ALIASED_POINT_SIZE_RANGE` of 1–16 pixels.

This is an incremental compatibility implementation, not full OpenGL ES 2.0
support and not a Khronos-conformant implementation. See
[`docs/fxOGL-roadmap.md`](../../docs/fxOGL-roadmap.md).

## Integration from a source checkout

In an fxSDK project, fxOGL can be included directly:

```cmake
add_subdirectory(path/to/fxOGL lib-fxOGL)
target_link_libraries(your_addin PRIVATE fxOGL::fxOGL)
```

## Install and consume as a CMake package

fxOGL exports the `fxOGL::fxOGL` target and installs its public headers and
package metadata. A host build can stage an installation with:

```sh
cmake -S lib/fxOGL -B build/fxOGL -DCMAKE_INSTALL_PREFIX="$PWD/stage"
cmake --build build/fxOGL
cmake --install build/fxOGL
```

A separate CMake project can then consume it:

```cmake
find_package(fxOGL 0.1 CONFIG REQUIRED)
target_link_libraries(your_application PRIVATE fxOGL::fxOGL)
```

Set `CMAKE_PREFIX_PATH` to the staging prefix when configuring that project.
The package exposes only headers under `include/`; internal headers under
`src/` are not part of the installed interface and may change without notice.

## Platform boundary

The library core is kept separate from the Gint application/presentation layer.
The calling application supplies caller-owned render surfaces through
`<fxgl/platform.h>` and performs its own platform presentation. fxOGL itself
does not call Gint display functions.

## Validation

Host regression tests in `apps/fxGLMark/tests/host` build the standalone
library target and verify that an installed package can be consumed by an
independent CMake project. Host tests do not replace an fxSDK/SH cross-build
or physical calculator testing.
