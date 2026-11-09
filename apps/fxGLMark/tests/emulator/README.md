# fxOGL fx-CG50 emulated GLES regression suite

This test builds a Gint add-in against the current fxOGL sources and runs it
in the headless mode of [Heath123/casio-emu](https://github.com/Heath123/casio-emu).
It exercises the public GLES2 draw path and validates the transferred LCD
framebuffer independently in the emulator host process.

## Rendering coverage

The suite retains the ten `gl_PointSize` cases: requested sizes 1, 3, 16, 32,
0, 0.25, and -2 pixels; float and integer `GL_ALIASED_POINT_SIZE_RANGE`
queries; GLSL `out`/`inout` copy-out; viewport, scissor, and LCD-edge clipping;
and rejection of a point centered outside homogeneous clip space.

It adds public GLES2 regressions for:

- **Depth testing:** a farther fragment is rejected and a nearer fragment
  replaces the first draw at the same pixel.
- **Alpha blending:** half-opacity red is blended over a blue destination using
  `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA`; expected RGB565 result `0x8010`.
- **Color write masks:** a green-only write preserves destination blue,
  producing RGB565 `0x07ff`.
- **Stencil testing:** `GL_ALWAYS` with `GL_REPLACE` writes a reference value;
  a subsequent `GL_EQUAL` draw overwrites the marked pixel and rejects a
  neighboring pixel whose stencil value remains zero.
- **Triangle primitives:** one-pixel `GL_TRIANGLES` draws exercise both
  client-memory vertex arrays with `glDrawArrays` and indexed VBO/EBO data
  with `glDrawElements`, including coverage, varying interpolation, and
  fragment-stage texturing.
- **Texture sampling:** a `sampler2D` uniform and GLSL `texture2D` sample all
  four texels of a 2x2 RGB565 image using nearest filtering.
- **Texture addressing:** out-of-range positive and negative S coordinates
  verify both `GL_REPEAT` and `GL_CLAMP_TO_EDGE`.
- **GLSL local arrays:** a second linked program writes a `float[3]` through
  a loop index and reads it back through dynamic and constant indices,
  alongside a constant-index `vec4[2]` element swizzle write. The fragment
  is yellow (RGB565 `0xffe0`) only if every access addresses its own slot;
  aliased or off-by-one indexing clamps to black.

## Independent framebuffer oracle

To keep execution bounded on the emulated SH-4, the add-in checks the 615
expected non-black pixels: 600 red point pixels, four depth/blend/color-mask/
stencil pixels, two triangle pixels, eight texture-sampling pixels, and one
local-array pixel. It
also asserts that the stencil-rejected neighbor remains black. The independent
host-side emulator oracle scans the entire 396x224 transferred RGB565
framebuffer against the exact expected pattern, rejects any unexpected
non-black pixel, and checks the green pass marker. CI uploads
`emulated-screen.ppm` as a visual artifact.

## Toolchain and limitations

The GitHub Actions workflow builds the fxSDK/Gint toolchain using the pinned
recipe from [hexbinoct/casio-cg50](https://github.com/hexbinoct/casio-cg50),
builds the add-in, builds the headless emulator, and runs the test. The
freestanding compiler preflight checks the C++ standard-library and math-header
paths used by the add-in.

**Scope:** this is a focused software GLES regression suite, not a performance
benchmark or full Khronos GLES conformance test. It currently validates point
rasterization, depth testing, blending, channel write masks, and basic
texture sampling/addressing, stencil compare/replace behavior, direct plus
indexed triangle draws, program switching, and GLSL local-array indexing. It does not attempt to cover the complete GLES2 API. `casio-emu` is work in progress, does not emulate
the calculator OS or syscalls, and has known hardware-model inaccuracies.
Passing emulation is not physical fx-CG50 validation.
