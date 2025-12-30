#pragma once
#include <glad/glad.h>
#include "Core/Assert.h"

#define OPENGL_LOG_ENABLED 0
#define OPENGL_BIND_CHECKS 1

#if OPENGL_LOG_ENABLED
#define OPENGL_LOG_BUFFER_CREATED(n, buffers)                                 \
    do {                                                               \
        for (GLsizei _i = 0; _i < (n); ++_i) {                         \
            printf("[GL] Buffer %u created at %s:%d\n",                \
                   (unsigned)(buffers)[_i], __FILE__, __LINE__);       \
        }                                                              \
    } while (0)
#else
#define OPENGL_LOG_BUFFER_CREATED(n, buffers) (void)0
#endif

#if OPENGL_BIND_CHECKS
#define OPENGL_CHECK_BIND_TARGET(target_id, target_type) \
    do { \
        GLint current_bound; \
        glGetIntegerv(GetBindingQuery(target_type), &current_bound); \
        if (static_cast<GLuint>(current_bound) != (target_id)) { \
            std::cerr << "[OpenGL error]: Obkect " << (target_id) << " not bound to " << #target_type << "! Currently bound: " << current_bound << " File: " << __FILE__ << " Line: " << __LINE__<< "\n"; \
            DEBUG_BREAK(); \
        } \
    } while(0)

// Helper function to get the correct binding query for each buffer target
static inline GLenum GetBindingQuery(GLenum target)
{
    switch (target)
    {
    case GL_ARRAY_BUFFER: return GL_ARRAY_BUFFER_BINDING;
    case GL_ELEMENT_ARRAY_BUFFER: return GL_ELEMENT_ARRAY_BUFFER_BINDING;
    case GL_UNIFORM_BUFFER: return GL_UNIFORM_BUFFER_BINDING;
    case GL_SHADER_STORAGE_BUFFER: return GL_SHADER_STORAGE_BUFFER_BINDING;
    case GL_COPY_READ_BUFFER: return GL_COPY_READ_BUFFER_BINDING;
    case GL_COPY_WRITE_BUFFER: return GL_COPY_WRITE_BUFFER_BINDING;
    case GL_PIXEL_PACK_BUFFER: return GL_PIXEL_PACK_BUFFER_BINDING;
    case GL_PIXEL_UNPACK_BUFFER: return GL_PIXEL_UNPACK_BUFFER_BINDING;
    case GL_TRANSFORM_FEEDBACK_BUFFER: return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
    case GL_ATOMIC_COUNTER_BUFFER: return GL_ATOMIC_COUNTER_BUFFER_BINDING;
    case GL_DISPATCH_INDIRECT_BUFFER: return GL_DISPATCH_INDIRECT_BUFFER_BINDING;
    case GL_DRAW_INDIRECT_BUFFER: return GL_DRAW_INDIRECT_BUFFER_BINDING;
    case GL_QUERY_BUFFER: return GL_QUERY_BUFFER_BINDING;
    case GL_FRAMEBUFFER: return GL_FRAMEBUFFER_BINDING;

    case GL_VERTEX_ARRAY: return GL_VERTEX_ARRAY_BINDING;

    case GL_TEXTURE_1D: return GL_TEXTURE_BINDING_1D;
    case GL_TEXTURE_2D: return GL_TEXTURE_BINDING_2D;
    case GL_TEXTURE_3D: return GL_TEXTURE_BINDING_3D;
    case GL_TEXTURE_2D_ARRAY: return GL_TEXTURE_BINDING_2D_ARRAY;
    case GL_TEXTURE_CUBE_MAP: return GL_TEXTURE_BINDING_CUBE_MAP;

    default:
        std::cerr << "[GetBindingQuery]: Unknown opengl object target: " << target<< "\n";
        return 0;
    }
}

#else

#define OPENGL_CHECK_BIND_TARGET(buffer_id, target_type) ((void)0)

#endif