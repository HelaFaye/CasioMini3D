#pragma once

/* The OpenGL ES 3.0 entry points fxOGL implements, on top of the complete
   GLES 2.0 API. This is not a full gl3.h: GLSL ES 3.00 and most of the
   GLES 3.0 API are not implemented, and glGetString(GL_VERSION) still
   reports OpenGL ES 2.0. See docs/fxOGL-roadmap.md. */

#include <GLES2/gl2.h>

#define GL_VERTEX_ARRAY_BINDING 0x85B5
#define GL_SAMPLER_BINDING 0x8919
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_COMPARE_MODE 0x884C
#define GL_TEXTURE_COMPARE_FUNC 0x884D
#define GL_COMPARE_REF_TO_TEXTURE 0x884E
#define GL_VERTEX_ATTRIB_ARRAY_DIVISOR 0x88FE
#define GL_PRIMITIVE_RESTART_FIXED_INDEX 0x8D69

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

/* Sampler objects (up to eight). A sampler bound to a texture unit replaces
   the bound texture's filters and wrap modes, and clamps the level of detail
   to [GL_TEXTURE_MIN_LOD, GL_TEXTURE_MAX_LOD]. GL_TEXTURE_WRAP_R and the
   compare mode/function are stored but have no effect: fxOGL has neither 3D
   nor depth textures. */
void glGenSamplers(GLsizei count, GLuint *samplers);
void glDeleteSamplers(GLsizei count, const GLuint *samplers);
GLboolean glIsSampler(GLuint sampler);
void glBindSampler(GLuint unit, GLuint sampler);
void glSamplerParameteri(GLuint sampler, GLenum pname, GLint param);
void glSamplerParameteriv(GLuint sampler, GLenum pname, const GLint *param);
void glSamplerParameterf(GLuint sampler, GLenum pname, GLfloat param);
void glSamplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param);
void glGetSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params);
void glGetSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params);

/* Instanced drawing. Attributes with a nonzero divisor (vertex array
   state) advance once per `divisor` instances. gl_InstanceID needs GLSL ES
   3.00, which is not implemented; per-instance data comes from divisors.
   glDrawElements and glDrawElementsInstanced also accept GL_UNSIGNED_INT
   indices, and glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX) makes the type's
   maximum index end the current primitive. */
void glDrawArraysInstanced(GLenum mode, GLint first, GLsizei count,
    GLsizei instancecount);
void glDrawElementsInstanced(GLenum mode, GLsizei count, GLenum type,
    const GLvoid *indices, GLsizei instancecount);
void glVertexAttribDivisor(GLuint index, GLuint divisor);

#ifdef __cplusplus
}
#endif
