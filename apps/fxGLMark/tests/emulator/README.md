# fxOGL fx-CG50 emulated smoke test

This test builds a small Gint add-in against the current fxOGL sources, then
runs that add-in in the headless mode of [Heath123/casio-emu](https://github.com/Heath123/casio-emu).

The add-in compiles vertex and fragment shaders, writes `gl_PointSize` through
user-function `out` and `inout` parameters, draws one `GL_POINTS` primitive,
checks the resulting 3x3 RGB565 pixel square, and queries
`GL_ALIASED_POINT_SIZE_RANGE`. The headless emulator checks the transferred
LCD framebuffer and saves `emulated-screen.ppm`.

The GitHub Actions workflow builds the fxSDK/Gint toolchain using the Docker
recipe from [hexbinoct/casio-cg50](https://github.com/hexbinoct/casio-cg50),
builds the add-in, builds the headless emulator, and runs the test.

**Scope:** this is a startup, shader, rasterization, and display-transfer smoke
test. `casio-emu` is a work-in-progress emulator; it does not emulate the
calculator OS or syscalls and has known hardware-model inaccuracies. This
test is not a performance benchmark, an OS compatibility test, or a
replacement for fx-CG50 hardware validation.
