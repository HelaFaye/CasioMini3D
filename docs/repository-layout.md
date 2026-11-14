# Repository layout

The repository is split into the maintained graphics library, its benchmark
application, historical experiments and project documentation.

```text
.
├── lib/
│   └── fxOGL/
│       ├── include/       # Public GLES/fxOGL API headers
│       ├── src/gfx/       # RGB565 texture storage and sampling
│       └── src/gles/      # Context, GLSL VM, program, math and pipeline
├── apps/
│   └── fxGLMark/
│       ├── assets/        # fxSDK-converted application assets
│       ├── src/           # Benchmark UI, scenes and Gint integration
│       ├── tests/emulator/ # fx-CG50 emulator GLES regression add-in
│       ├── tests/host/    # Host regression suite and platform stubs
│       └── tests/profile/ # Golden frames and host model of device cost
├── archive/
│   ├── renderer-v1/
│   ├── renderer-v2/
│   ├── renderer-v3-history/
│   └── model-renderer/
├── docs/
└── .github/workflows/
```

## Active application

Build the fx-CG50 add-in:

```sh
cd apps/fxGLMark
fxsdk build-cg
```

Run host regression tests from the repository root:

```sh
cmake -S apps/fxGLMark/tests/host -B /tmp/fxogl-host-tests
cmake --build /tmp/fxogl-host-tests --parallel 2
ctest --test-dir /tmp/fxogl-host-tests --output-on-failure
```

## Library/app boundary

The fxOGL CMake target contains the software graphics core and does not
directly depend on Gint. fxGLMark owns the add-in entry point, benchmark
selection, workload scenes, calculator storage and Gint presentation.

Some implementation headers under `lib/fxOGL/src/` are currently included by
fxGLMark and its host tests. Those are transitional internal interfaces; the
stable public surface is the headers under `lib/fxOGL/include/`.
