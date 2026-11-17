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
#define GL_MAP_READ_BIT 0x0001
#define GL_MAP_WRITE_BIT 0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT 0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT 0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT 0x0020
#define GL_BUFFER_ACCESS_FLAGS 0x911F
#define GL_BUFFER_MAPPED 0x88BC
#define GL_BUFFER_MAP_POINTER 0x88BD
#define GL_BUFFER_MAP_LENGTH 0x9120
#define GL_BUFFER_MAP_OFFSET 0x9121
#define GL_UNSIGNED_INT_VEC2 0x8DC6
#define GL_UNSIGNED_INT_VEC3 0x8DC7
#define GL_UNSIGNED_INT_VEC4 0x8DC8

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

/* Buffer mapping for GL_ARRAY_BUFFER and GL_ELEMENT_ARRAY_BUFFER. The
   pointer addresses the buffer's store directly (rendering is synchronous),
   so invalidation keeps old contents and flushing copies nothing. Drawing
   from or calling glBufferSubData on a mapped buffer is GL_INVALID_OPERATION;
   glBufferData ends the mapping. GL_BUFFER_MAP_OFFSET and
   GL_BUFFER_MAP_LENGTH are queried with glGetBufferParameteriv (there is no
   glGetBufferParameteri64v). */
void *glMapBufferRange(GLenum target, GLintptr offset, GLsizeiptr length,
    GLbitfield access);
void glFlushMappedBufferRange(GLenum target, GLintptr offset,
    GLsizeiptr length);
GLboolean glUnmapBuffer(GLenum target);
void glGetBufferPointerv(GLenum target, GLenum pname, GLvoid **params);

/* Unsigned integer uniforms (GLSL ES 3.00 uint/uvec). As in GLES 3.0, the
   glUniform*i family cannot set them and glUniform*ui sets nothing else. */
void glUniform1ui(GLint location, GLuint v0);
void glUniform2ui(GLint location, GLuint v0, GLuint v1);
void glUniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2);
void glUniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3);
void glUniform1uiv(GLint location, GLsizei count, const GLuint *value);
void glUniform2uiv(GLint location, GLsizei count, const GLuint *value);
void glUniform3uiv(GLint location, GLsizei count, const GLuint *value);
void glUniform4uiv(GLint location, GLsizei count, const GLuint *value);
void glGetUniformuiv(GLuint program, GLint location, GLuint *params);

#ifdef __cplusplus
}
#endif
