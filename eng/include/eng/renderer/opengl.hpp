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

struct VertexBufferElement {
    static size_t get_size_of_type(GLenum type);

    GLenum type = 0;
    GLint count = 0;
    GLboolean normalized = 0;
};

struct VertexBufferLayout {
    void push_float(uint32_t count, bool normalized = false);
    void clear();

    std::vector<VertexBufferElement> elements;
    uint32_t stride = 0;
};

struct VertexArray {
    static VertexArray create();

    void destroy();

    void add_buffers(const VertexBuffer &vbo, const IndexBuffer &ibo,
                     const VertexBufferLayout &layout,
                     uint32_t attrib_offset = 0);
    void add_vertex_buffer(const VertexBuffer &vbo,
                           const VertexBufferLayout &layout,
                           uint32_t attrib_offset = 0);
    void add_instanced_vertex_buffer(const VertexBuffer &vbo,
                                     const VertexBufferLayout &layout,
                                     uint32_t attrib_offset = 0);

    void bind() const;
    void unbind() const;

    GLuint id = 0;
    uint32_t vertex_count = 0;
    const IndexBuffer *ibo = nullptr;
};

#endif
