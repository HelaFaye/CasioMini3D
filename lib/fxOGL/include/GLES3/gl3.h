#pragma once

/* The OpenGL ES 3.0 entry points fxOGL implements, on top of the complete
   GLES 2.0 API. This is not a full gl3.h: GLSL ES 3.00 and most of the
   GLES 3.0 API are not implemented, and glGetString(GL_VERSION) still
   reports OpenGL ES 2.0. See docs/fxOGL-roadmap.md. */

#include <GLES2/gl2.h>

#define GL_VERTEX_ARRAY_BINDING 0x85B5

#ifdef __cplusplus
extern "C" {
#endif

/* Vertex array objects. Each holds the vertex attribute pointer state (the
   enabled flags, sizes, types, strides, pointers and buffer bindings) and
   the element array buffer binding. Name 0 is the default vertex array.
   Generic attribute values set with glVertexAttrib* are context state. */
void glGenVertexArrays(GLsizei n, GLuint *arrays);
void glDeleteVertexArrays(GLsizei n, const GLuint *arrays);
void glBindVertexArray(GLuint array);
GLboolean glIsVertexArray(GLuint array);

#ifdef __cplusplus
}
#endif
