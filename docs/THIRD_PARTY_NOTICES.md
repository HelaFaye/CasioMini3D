# Third-party notices and references

## glmark2

- **Project:** glmark2, an OpenGL 2.0 and OpenGL ES 2.0 benchmark
- **Repository:** https://github.com/glmark2/glmark2
- **Copyright:** Copyright © 2010–2011 Linaro Limited (as stated in source-file notices); individual source files also name their authors.
- **License:** GNU General Public License, version 3.0 (GPL-3.0), as reported by the repository metadata. Consult upstream and its COPYING file for authoritative terms.
- **Use here:** Scene/benchmark separation, scene-family names and high-level workload organization informed fxGLMark's design.
- **Code use:** fxGLMark is independently written for the Gint/fxSDK target. It does not intentionally copy glmark2 source or redistribute upstream scene assets. This is not a complete provenance audit of all historical repository content.

No glmark2 assets or test data are intentionally redistributed by this project.

## Gint and fxSDK

fxGLMark uses Gint and the fxSDK for calculator hardware access, add-in
runtime, build configuration and asset conversion. They are separate projects.
fxOGL's software renderer is layered above Gint and does not replace Gint's
low-level hardware drivers.

Consult the upstream Gint/fxSDK repositories and their license notices for
their current source, contribution and redistribution requirements.

## Attribution scope

This file records specific upstream projects consulted as references. It does
not replace notices required for third-party source or assets if any are
later copied or adapted. Preserve the original notices when moving archived
source, assets or historical project snapshots.
