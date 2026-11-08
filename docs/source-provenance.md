# Source provenance and licensing status

**Status: review required before external redistribution or upstream code contributions.**

## Original repository

This repository is a fork of
[`leofarhi/CasioMini3D`](https://github.com/leofarhi/CasioMini3D). On
2026-11-08, GitHub reported no recognized license for the parent repository,
and the parent tree contained no root `LICENSE` or `COPYING` file. Public
visibility and the fork relationship do not themselves grant permission to
relicense inherited code.

This is a path/blob comparison, not legal advice or a complete copyright
similarity audit.

## Inherited material retained

A tree comparison against the parent repository found:

- 321 unchanged inherited files still present in this fork.
- 313 of those remain in `archive/`.
- 8 unchanged inherited files remain in active `apps/fxGLMark/`.
- 10 inherited paths have been modified in this fork.
- 76 parent paths are no longer represented, primarily generated build and
  editor-specific material removed during cleanup.

The 8 unchanged active-app files include the original display header,
shape files, types header, and app assets. The modified inherited app files
include the original CMake project, display/world code, fixed-point helpers,
triangle shape code, and app entry point.

Moving inherited material into `archive/` does not change its provenance or
licensing status.

## New fxOGL implementation

The `lib/fxOGL/` paths were introduced in this fork after it diverged from the
parent tree. The library's GLES-style context, shader compiler/VM, math and
raster pipeline, RGB565 texture sampler, headers, and CMake target were added
as the fxOGL implementation.

That tree comparison is useful evidence of path-level provenance, but does
not replace a line-by-line source audit against third-party implementations.
Before publishing fxOGL as a separately licensed library:

1. Review its source for copied or adapted third-party code.
2. Preserve notices for non-original code and assets.
3. Choose a license only for code whose ownership and redistribution rights
   are clear.
4. Keep inherited application and archive files under their existing
   provenance until written permission or a compatible license is identified.

No repository-wide license is asserted by this reorganization.

## External contributions

Do not submit inherited files, or derivative changes to them, to Gint, fxSDK,
fxcraft, FastGraph, or other community repositories unless the relevant
license or the original author permits that redistribution.

The newly introduced fxOGL library may be a candidate for independent reuse
after its own provenance and licensing review.
