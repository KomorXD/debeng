#ifndef OPENGL_HPP
#define OPENGL_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

#define GL_CALL(f)                                                             \
    gl_clear_errors();                                                         \
    f;                                                                         \
    assert(gl_check_errors(#f, __FILE__, __LINE__));

void gl_clear_errors();

bool gl_check_errors(const char *func, const char *filename, int32_t line);

struct VertexBuffer {
    static VertexBuffer create();

    void allocate(const void *data, uint64_t size, uint32_t count = 0);
    void destroy();

    void bind() const;
    void unbind() const;
    void set_data(const void *data, uint64_t size, uint64_t offset = 0) const;

    GLuint id = 0;
    uint32_t vertex_count = 0;
};

struct IndexBuffer {
    static IndexBuffer create();

    void allocate(const uint32_t *data, uint32_t count);
    void destroy();

    void bind() const;
    void unbind() const;

    GLuint id = 0;
    uint32_t indices_count = 0;
};

struct Shader {
    static Shader create();
    static GLuint compile(GLenum type, const std::string &src);

    bool build(const std::string &vs_path, const std::string &fs_path);
    void destroy();

    void bind() const;
    void unbind() const;

    GLuint id = 0;
};

#endif
