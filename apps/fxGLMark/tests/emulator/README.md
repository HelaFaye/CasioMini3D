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

## Independent framebuffer oracle

The add-in compares the complete RGB565 framebuffer against an exact expected
pattern: 600 red point pixels plus three separately colored state-test pixels.
The headless emulator independently checks every transferred pixel, including
that all unlisted pixels remain black, and checks the green pass marker.
CI uploads `emulated-screen.ppm` as a visual artifact.

## Toolchain and limitations

The GitHub Actions workflow builds the fxSDK/Gint toolchain using the pinned
recipe from [hexbinoct/casio-cg50](https://github.com/hexbinoct/casio-cg50),
builds the add-in, builds the headless emulator, and runs the test. The
freestanding compiler preflight checks the C++ standard-library and math-header
paths used by the add-in.

**Scope:** this is a focused software GLES regression suite, not a performance
benchmark or full Khronos GLES conformance test. It currently validates point
rasterization, depth testing, blending, and channel write masks. It does not
cover the complete GLES2 API. `casio-emu` is work in progress, does not emulate
the calculator OS or syscalls, and has known hardware-model inaccuracies.
Passing emulation is not physical fx-CG50 validation.
