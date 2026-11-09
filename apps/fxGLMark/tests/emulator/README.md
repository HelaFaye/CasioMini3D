# fxOGL fx-CG50 emulated point-rasterization test

This test builds a Gint add-in against the current fxOGL sources and runs it
in the headless mode of [Heath123/casio-emu](https://github.com/Heath123/casio-emu).
It exercises the public GLES2 draw path and validates the transferred LCD
framebuffer independently in the emulator host process.

The vertex shader writes `gl_PointSize` through user-function `out` and
`inout` parameters. The test draws points with requested sizes 1, 3, 16, 32,
0, 0.25, and -2 pixels. It verifies the advertised `GL_ALIASED_POINT_SIZE_RANGE`
through both integer and float queries, including minimum-size and maximum-size
clamping.

Additional draws verify that expanded point coverage is clipped by a reduced
viewport, a 5x5 scissor box, and the LCD surface edge. A point whose center lies
outside homogeneous clip space must produce no fragments. The add-in compares
the complete RGB565 framebuffer against an expected pixel mask, and the
headless emulator independently checks that same pattern after LCD transfer.
The emulator writes the resulting `emulated-screen.ppm` file as a CI artifact.

The GitHub Actions workflow builds the fxSDK/Gint toolchain using the Docker
recipe from [hexbinoct/casio-cg50](https://github.com/hexbinoct/casio-cg50),
builds the add-in, builds the headless emulator, and runs the test. It follows
the documented GiteaPC order: install fxSDK and the SH-4 compiler, install
OpenLibm and fxlibc, invoke the SH-4 compiler package again to build
`libstdc++-v3` with the target C library available, then install Gint. The
preflight also compiles C++ standard-library and C math-header probes in
freestanding mode.

**Scope:** this is a focused software point-rasterization regression suite,
not a performance benchmark, OS compatibility test, or full GLES conformance
test. `casio-emu` is work in progress; it does not emulate the calculator OS
or syscalls and has known hardware-model inaccuracies. Passing emulation is
not a replacement for physical fx-CG50 validation.
