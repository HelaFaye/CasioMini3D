# Repository layout

The repository is split into the maintained graphics library, its benchmark
application, historical experiments and project documentation.

```text
.
├── lib/
│   └── fxOGL/
│       ├── include/        # Public GLES/fxOGL API headers
│       ├── src/gfx/        # 16-bit texture storage and sampling
│       ├── src/gles/       # Context, GLSL VM, program, math and pipeline
│       ├── tests/host/     # Host regression suites and Gint stubs
│       └── tests/emulator/ # fx-CG50 emulator GLES regression add-in
├── archive/                # Earlier renderer generations
├── docs/
└── .github/workflows/
```

The benchmark application and other projects moved to their own
repositories; see `repository-split.md`.

## Tests

```sh
cmake -S lib/fxOGL/tests/host -B /tmp/fxogl-host-tests
cmake --build /tmp/fxogl-host-tests --parallel 2
ctest --test-dir /tmp/fxogl-host-tests --output-on-failure
```
