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
- A bounded GLSL ES 1.00 compiler and bytecode VM supporting the subset required
  by the currently implemented scenes.

This is an incremental compatibility implementation, not full OpenGL ES 2.0
support and not a Khronos-conformant implementation. See
[`docs/fxOGL-roadmap.md`](../../docs/fxOGL-roadmap.md).

## Integration

The library is built as a separate CMake target. In an fxSDK project:

```cmake
add_subdirectory(path/to/fxOGL lib-fxOGL)
target_link_libraries(your_addin PRIVATE fxOGL::fxOGL)
```

Public API headers are under `include/`. Some current application and
benchmark code still includes implementation headers from `src/`; those are
internal interfaces and are not yet a stable public API.

The library core is kept separate from the Gint application/presentation layer.
The calling application supplies its render surface and any platform binding.

## Validation

The library is covered by host regression tests in
`apps/fxGLMark/tests/host` and is built for the fx-CG50 through the
`apps/fxGLMark` fxSDK project. Host tests do not replace SH cross-builds or
physical calculator testing.
