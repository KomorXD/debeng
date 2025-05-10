#include "renderer/opengl.hpp"
#include "random_utils.hpp"
#include <alloca.h>
#include <cstdio>

void gl_clear_errors() {
    while (glGetError() != GL_NO_ERROR)
        ;
}

bool gl_check_errors(const char *func, const char *filename, int32_t line) {
    if (GLenum error = glGetError()) {
        fprintf(stderr, "OpenGL error %d in %s at line %d while calling %s\r\n",
                error, filename, line, func);

        return false;
    }

    return true;
}

VertexBuffer VertexBuffer::create() {
    VertexBuffer vbo;
    GL_CALL(glGenBuffers(1, &vbo.id));
    assert(vbo.id != 0 && "Couldn't generate vertex buffer");

    return vbo;
}

void VertexBuffer::allocate(const void *data, uint64_t size, uint32_t count) {
    assert(id != 0 && "Trying to allocate invalid vertex buffer");

    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, id));
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW));

    vertex_count = count;
}

void VertexBuffer::destroy() {
    assert(id != 0 && "Trying to deallocate invalid vertex buffer");

    GL_CALL(glDeleteBuffers(1, &id));
    id = 0;
}

void VertexBuffer::bind() const {
    assert(id != 0 && "Trying to bind invalid vertex buffer");

    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, id));
}

void VertexBuffer::unbind() const {
    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, 0));
}

void VertexBuffer::set_data(const void *data, uint64_t size,
                            uint64_t offset) const {
    assert(id != 0 && "Trying to update invalid vertex buffer");

    GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, id));
    GL_CALL(glBufferSubData(GL_ARRAY_BUFFER, (GLintptr)offset, size, data));
}

IndexBuffer IndexBuffer::create() {
    IndexBuffer ibo;
    GL_CALL(glGenBuffers(1, &ibo.id));
    assert(ibo.id != 0 && "Couldn't generate index buffer");

    return ibo;
}

void IndexBuffer::allocate(const uint32_t *data, uint32_t count) {
    assert(id != 0 && "Trying to allocate invalid index buffer");

    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id));
    GL_CALL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t),
                         data, GL_STATIC_DRAW));

    indices_count = count;
}

void IndexBuffer::destroy() {
    assert(id != 0 && "Trying to deallocate invalid index buffer");

    GL_CALL(glDeleteBuffers(1, &id));
    id = 0;
}

void IndexBuffer::bind() const {
    assert(id != 0 && "Trying to bind invalid index buffer");

    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id));
}

void IndexBuffer::unbind() const {
    GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
}

Shader Shader::create() {
    Shader shader;
    GL_CALL(shader.id = glCreateProgram());
    assert(shader.id != 0 && "Couldn't create program");

    return shader;
}

GLuint Shader::compile(GLenum type, const std::string &src) {
    GL_CALL(GLuint id = glCreateShader(type));
    assert(id != 0 && "Couldn't create shader");

    const char *source = src.c_str();
    GL_CALL(glShaderSource(id, 1, &source, nullptr));
    GL_CALL(glCompileShader(id));

    GLint success = 0;
    GL_CALL(glGetShaderiv(id, GL_COMPILE_STATUS, &success));
    if (success == GL_FALSE) {
        int len = 0;
        GL_CALL(glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len));

        char *msg = (char *)alloca(len * sizeof(char));
        GL_CALL(glGetShaderInfoLog(id, len, &len, msg));

        fprintf(stderr, "Failed to compile shader: %s", msg);
        GL_CALL(glDeleteShader(id));

        return 0;
    }

    return id;
}

bool Shader::build(const std::string &vs_path, const std::string &fs_path) {
    assert(id != 0 && "Trying to build shader on invalid shader object");

    std::string vs_src = get_file_content(vs_path).value();
    std::string fs_src = get_file_content(fs_path).value();

    GLuint vs_id = compile(GL_VERTEX_SHADER, vs_src);
    GLuint fs_id = compile(GL_FRAGMENT_SHADER, fs_src);

    GL_CALL(glAttachShader(id, vs_id));
    GL_CALL(glAttachShader(id, fs_id));
    GL_CALL(glLinkProgram(id));
    GL_CALL(glDeleteShader(vs_id));
    GL_CALL(glDeleteShader(fs_id));

    GLint success = 0;
    GL_CALL(glGetProgramiv(id, GL_LINK_STATUS, &success));
    if (success == GL_FALSE) {
        int len = 0;
        GL_CALL(glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len));

        char *msg = (char *)alloca(len * sizeof(char));
        GL_CALL(glGetProgramInfoLog(id, len, &len, msg));

        fprintf(stderr, "Failed to link shaders: %s", msg);
        GL_CALL(glDeleteProgram(id));

        return false;
    }

    GL_CALL(glValidateProgram(id));
    return true;
}

void Shader::destroy() {
    assert(id != 0 && "Trying to destroy invalid shader object");

    GL_CALL(glDeleteProgram(id));
    id = 0;
}

void Shader::bind() const {
    assert(id != 0 && "Trying to bind invalid shader object");

    GL_CALL(glUseProgram(id));
}

void Shader::unbind() const {
    GL_CALL(glUseProgram(0));
}

std::optional<GLint> Shader::get_uniform_location(const std::string &name) {
    assert(id != 0 && "Trying to get uniform of invalid shader object");

    if (uniform_cache.contains(name))
        return uniform_cache[name];

    GL_CALL(GLint loc = glGetUniformLocation(id, name.c_str()));
    if (loc == -1)
        return {};

    uniform_cache[name] = loc;
    return loc;
}

void Shader::set_uniform_1f(const std::string &name, float val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if(!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform1f(loc.value(), val));
}

size_t VertexBufferElement::get_size_of_type(GLenum type) {
    /*
        #define GL_BYTE 0x1400
        #define GL_UNSIGNED_BYTE 0x1401
        #define GL_SHORT 0x1402
        #define GL_UNSIGNED_SHORT 0x1403
        #define GL_INT 0x1404
        #define GL_UNSIGNED_INT 0x1405
        #define GL_FLOAT 0x1406
        #define GL_DOUBLE 0x140A
    */
    static size_t sizes[] = {sizeof(GLfloat),  sizeof(GLubyte), sizeof(GLshort),
                             sizeof(GLushort), sizeof(GLint),   sizeof(GLuint),
                             sizeof(GLfloat),  sizeof(GLdouble)};

    assert(type >= GL_BYTE && type <= GL_DOUBLE && "Invalid type passed");
    return sizes[type - GL_BYTE];
}

void VertexBufferLayout::push_float(uint32_t count, bool normalized) {
    GLboolean norm = (GLboolean)(normalized ? GL_TRUE : GL_FALSE);
    elements.push_back({GL_FLOAT, (GLint)count, norm});
    stride += count * sizeof(GLfloat);
}

void VertexBufferLayout::clear() {
    elements.clear();
    stride = 0;
}

VertexArray VertexArray::create() {
    VertexArray vao;
    GL_CALL(glGenVertexArrays(1, &vao.id));
    assert(vao.id != 0 && "Couldn't create vertex array");

    return vao;
}

void VertexArray::destroy() {
    assert(id != 0 && "Trying to destroy invalid vertex arrayy");

    GL_CALL(glDeleteVertexArrays(1, &id));
    id = 0;
}

void VertexArray::add_buffers(const VertexBuffer &vbo, const IndexBuffer &ibo,
                              const VertexBufferLayout &layout,
                              uint32_t attrib_offset) {
    bind();
    vbo.bind();
    ibo.bind();

    uint32_t len = layout.elements.size() + attrib_offset;
    size_t offset = 0;
    for (uint32_t i = attrib_offset; i < len; i++) {
        const VertexBufferElement &element = layout.elements[i - attrib_offset];

        GL_CALL(glEnableVertexAttribArray(i));
        GL_CALL(glVertexAttribPointer(i, element.count, element.type,
                                      element.normalized, layout.stride,
                                      (const void *)offset));

        offset +=
            element.count * VertexBufferElement::get_size_of_type(element.type);
    }

    vertex_count += vbo.vertex_count;
    this->ibo = &ibo;
}

void VertexArray::add_vertex_buffer(const VertexBuffer &vbo,
                                    const VertexBufferLayout &layout,
                                    uint32_t attrib_offset) {
    bind();
    vbo.bind();

    uint32_t len = layout.elements.size() + attrib_offset;
    size_t offset = 0;
    for (uint32_t i = attrib_offset; i < len; i++) {
        const VertexBufferElement &element = layout.elements[i - attrib_offset];

        GL_CALL(glEnableVertexAttribArray(i));
        GL_CALL(glVertexAttribPointer(i, element.count, element.type,
                                      element.normalized, layout.stride,
                                      (const void *)offset));

        offset +=
            element.count * VertexBufferElement::get_size_of_type(element.type);
    }

    vertex_count += vbo.vertex_count;
}

void VertexArray::add_instanced_vertex_buffer(const VertexBuffer &vbo,
                                              const VertexBufferLayout &layout,
                                              uint32_t attrib_offset) {
    bind();
    vbo.bind();

    uint32_t len = layout.elements.size() + attrib_offset;
    size_t offset = 0;
    for (uint32_t i = attrib_offset; i < len; i++) {
        const VertexBufferElement &element = layout.elements[i - attrib_offset];

        GL_CALL(glEnableVertexAttribArray(i));
        GL_CALL(glVertexAttribPointer(i, element.count, element.type,
                                      element.normalized, layout.stride,
                                      (const void *)offset));
        GL_CALL(glVertexAttribDivisor(i, 1));

        offset +=
            element.count * VertexBufferElement::get_size_of_type(element.type);
    }

    vertex_count += vbo.vertex_count;
}

void VertexArray::bind() const {
    assert(id != 0 && "Trying to bind invalid vertex array");

    GL_CALL(glBindVertexArray(id));
}

void VertexArray::unbind() const {
    GL_CALL(glBindVertexArray(0));
}
