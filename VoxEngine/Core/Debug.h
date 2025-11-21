#pragma once

#define PROFILING_ENABLED 1
#define OPENGL_LOG_ENABLED 0

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