# fxGLMark host profiling and golden frames

Tools for changing fxOGL safely and quickly on a development machine before
measuring on an fx-CG50:

| File | Purpose |
|---|---|
| `scenes.cpp` | Renders fxGLMark scenes at 396x224 and hashes the color and depth buffers |
| `golden_frames.txt` | Reference hashes: 6 scenes x 3 frames |
| `softfloat_shim.c` | Cost-weighted soft-float routines for the device cost model |
| `profile.sh` | Builds the soft-float model, checks frames, runs callgrind per scene |
| `callgrind_calls.py` | Counts calls per function (for example soft-float calls) in a callgrind file |

## Golden frames (runs in CI)

The host test suite builds `fxglmark_scenes` and runs the
`fxglmark_golden_frames` CTest, which renders every fxGLMark scene and
compares the hashes with `golden_frames.txt`. Any change to rendered pixels or
stored depth fails it, so a performance change that is meant to be exact can
be proven exact.

When a rendering change is intended, regenerate the file and commit it with
the change:

```sh
cmake -S apps/fxGLMark/tests/host -B /tmp/fxogl-host-tests
cmake --build /tmp/fxogl-host-tests --parallel 2
/tmp/fxogl-host-tests/fxglmark_scenes --write apps/fxGLMark/tests/profile/golden_frames.txt
```

`fxglmark_scenes <scene> [frames]` prints the per-frame hashes of a single
scene, for example `fxglmark_scenes texture:texture-filter=linear 3`.

The hashes depend only on IEEE single-precision results. They are built with
`-ffp-contract=off`, and the soft-float build below produces the same hashes,
so x86-64 hardware floats and the fx-CG50's software floats agree.

## Device cost model

The fx-CG50's SH-4A has no FPU: every float add, multiply, divide, compare
and conversion is a libgcc call, and those calls dominate frame time. A normal
x86 profile, where floats are single instructions, ranks the hot spots
wrongly.

`profile.sh` builds fxOGL and the scenes as 32-bit code with
`-msoft-float -mno-80387`, so every float operation becomes a call. The calls
land in `softfloat_shim.c`, which computes the exact IEEE result and also burns
a fixed number of instructions per operation (add 45, multiply 40, divide 90,
compare 15, conversion 20, double operations 110-250). callgrind's instruction
count then approximates device cost. Before profiling, the script checks the
soft-float frames against `golden_frames.txt`.

```sh
sudo apt-get install gcc-multilib g++-multilib valgrind
apps/fxGLMark/tests/profile/profile.sh /tmp/fxogl-profile-before
# ... change fxOGL ...
apps/fxGLMark/tests/profile/profile.sh /tmp/fxogl-profile-after /tmp/fxogl-profile-before/costs.txt
callgrind_annotate --inclusive=yes /tmp/fxogl-profile-after/callgrind.build_use-vbo_true
python3 apps/fxGLMark/tests/profile/callgrind_calls.py \
    /tmp/fxogl-profile-after/callgrind.build_use-vbo_true '__[a-z]+sf[0-9]$'
```

### Calibration against the fx-CG50

| Check | Model | Device |
|---|---|---|
| Linear / build frame cost, before the soft-float optimizations | 1.453 | 1.445 |
| Linear / build frame cost, commit `ea02e8c` | 0.958 | 1.015 |
| Nearest / build frame cost, commit `ea02e8c` | 0.804 | 0.833 |
| Clear / build frame cost, commit `ea02e8c` | 0.018 | 0.017 |
| Device cycles per modelled instruction (117.96 MHz) | | 1.9-2.1 |
| Predicted saving of the soft-float optimizations (build / nearest / linear) | 37% / 50% / 58% | 28% / 40% / 50% |

The model ranks hot spots correctly and predicts relative scene costs within
about 6%. It overstates the savings from removing float operations by about a
quarter, so its software-float weights are somewhat higher than the SH-4's
libgcc routines actually cost. Use it to find hot spots and to compare
changes; the fx-CG50 remains the arbiter of performance claims. On the device,
tight microbenchmark loops also move by up to about 10% from code placement
alone, so judge changes by whole scenes (see `docs/fxOGL-roadmap.md`).
