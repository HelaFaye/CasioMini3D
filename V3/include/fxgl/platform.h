#pragma once

#include <stdint.h>
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bind caller-owned calculator/host render surfaces to the current GLES-style
   context. Color storage is RGB565. Depth storage is optional uint16_t;
   stencil storage is optional uint8_t. Stride is measured in pixels.
   The caller retains ownership and must keep all storage alive until release. */
typedef struct fxgl_surface
{
    uint16_t *color;
    uint16_t *depth;
    uint8_t *stencil;
    GLsizei width;
    GLsizei height;
    GLsizei stride;
} fxgl_surface_t;

/* Returns GL_TRUE on success. Invalid descriptors leave the current context
   unchanged. Binding resets API state to the GLES defaults for this surface. */
GLboolean fxglMakeCurrent(const fxgl_surface_t *surface);
void fxglReleaseCurrent(void);
GLboolean fxglHasCurrentContext(void);

#ifdef __cplusplus
}
#endif
