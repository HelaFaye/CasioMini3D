# fxGLMark

**fxGLMark** is a native benchmark application for the Casio fx-CG50. It runs
selected glmark2-inspired workload categories on top of the fxOGL software
renderer. It is not the upstream Linux/EGL glmark2 executable, and its scores
are not directly comparable with desktop or mobile glmark2 results.

## Build

From this directory:

```sh
fxsdk build-cg
```

The output add-in is `fxGLMark.g3a`.

## Main menu

| Key | Action |
|---|---|
| Up / Down | Move through individual tests |
| EXE or F6 | Toggle the selected test |
| F1 | Select Quick Smoke |
| F2 | Select the currently supported glmark2 subset |
| F3 | Select all implemented tests |
| F4 | Run renderer component microbenchmarks |
| F5 | Run the selected tests |
| MENU | Exit |

Selectable workloads currently cover clear, client-array geometry, VBO
geometry, nearest and linear texture sampling, and Gouraud shading. The
supported-default preset is explicitly a subset of glmark2's full default
suite.

## Output files

- `/glmark2-fx-cg50-v1.csv`: complete benchmark runs.
- `/glmark2-renderer-microbench-v1.csv`: seven component timing stages.

The full scene measurements include software rendering and the Gint
`dupdate()` call. The presentation microbenchmark measures the duration of
`dupdate()`, not physical LCD scanout completion.

See [`docs/fxGLMark.md`](../../docs/fxGLMark.md) for the scene catalog,
measurement semantics and pending workloads.
