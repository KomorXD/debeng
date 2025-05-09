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
