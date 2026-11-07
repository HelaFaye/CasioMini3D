# Third-party notices and references

## glmark2

- **Project:** glmark2, an OpenGL 2.0 and OpenGL ES 2.0 benchmark
- **Repository:** <https://github.com/glmark2/glmark2>
- **Copyright:** Copyright © 2010–2011 Linaro Limited (as stated in source-file notices); individual source files also name their authors.
- **License:** GNU General Public License, version 3.0 (GPL-3.0), as reported by the repository metadata. Consult the upstream repository and its `COPYING` file for the authoritative license text and full copyright details.
- **Use here:** The scene/benchmark separation and high-level benchmark organization were used as design inspiration. During development, source references included `src/benchmark.cpp`, `src/benchmark.h`, `src/scene.h`, `src/scene.cpp`, and `src/main-loop.h`.
- **Code use:** The current CasioMini3D benchmark implementation was written for the Gint/fxSDK target and does not intentionally copy glmark2 source code. The files above were consulted to understand organization and lifecycle concepts; the concepts are described here in our own words. This notice is not a comprehensive source-provenance audit of every file or historical revision in this repository.

No glmark2 assets or test data are intentionally redistributed by this project.

## Gint and fxSDK

The active V3 application depends on Gint and the fxSDK build/conversion toolchain. They are separate projects; this repository does not claim ownership of their code. Consult the respective upstream projects and their license files for applicable copyright and license terms.

## Attribution scope

This document records third-party projects specifically consulted as references during the benchmark design work. It does not replace copyright or license notices that must accompany any third-party code or assets if such material is later copied or adapted. Before distribution, audit the full repository—including legacy directories and assets—for provenance and preserve any applicable notices.
