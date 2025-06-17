#include "eng/renderer/opengl.hpp"
#include "eng/random_utils.hpp"
#include "stb/stb_image.hpp"
#include <alloca.h>
#include <cstdio>
#include <filesystem>

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
    GL_CALL(glBufferData(GL_ARRAY_BUFFER, size, data, GL_DYNAMIC_DRAW));

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
                         data, GL_DYNAMIC_DRAW));

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

void Shader::set_uniform_1i(const std::string &name, int32_t val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if(!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform1i(loc.value(), val));
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

void Shader::set_uniform_2f(const std::string &name, const glm::vec2 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if(!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform2f(loc.value(), val.x, val.y));
}

void Shader::set_uniform_3f(const std::string &name, const glm::vec3 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if(!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform3f(loc.value(), val.x, val.y, val.z));
}

void Shader::set_uniform_4f(const std::string &name, const glm::vec4 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if(!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform4f(loc.value(), val.x, val.y, val.z, val.w));
}

void Shader::set_uniform_mat4(const std::string &name, const glm::mat4 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if(!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniformMatrix4fv(loc.value(), 1, GL_FALSE, &val[0][0]));
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

    if (vbo.id != 0)
        vbo.destroy();

    if (ibo.id != 0)
        ibo.destroy();

    if (vbo_instanced.id != 0)
        vbo_instanced.destroy();

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

    this->vbo = vbo;
    this->ibo = ibo;
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

    this->vbo = vbo;
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

    this->vbo_instanced = vbo;
}

void VertexArray::bind() const {
    assert(id != 0 && "Trying to bind invalid vertex array");

    GL_CALL(glBindVertexArray(id));
}

void VertexArray::unbind() const {
    GL_CALL(glBindVertexArray(0));
}

TextureFormatDetails format_details(TextureFormat format) {
    static GLenum internal_formats[] = {
        GL_RGBA8, GL_RGB8,   GL_RGBA16F,        GL_RGB16F,
        GL_RG16F, GL_RGB32F, GL_R11F_G11F_B10F, GL_DEPTH_COMPONENT32F
    };
    static GLenum formats[] = {
        GL_RGBA, GL_RGB, GL_RGBA, GL_RGB,
        GL_RG,   GL_RGB, GL_RGB,  GL_DEPTH_COMPONENT
    };
    static GLenum types[] = {
        GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE, GL_FLOAT, GL_FLOAT,
        GL_FLOAT,         GL_FLOAT,         GL_FLOAT, GL_FLOAT
    };
    static GLenum bpps[] = {
        4, 3, 4, 3,
        2, 3, 3, 1
    };

    int32_t idx = (int32_t)format;
    assert(idx < (int32_t)TextureFormat::COUNT && "Invalid texture format");

    return {
        internal_formats[idx],
        formats[idx],
        types[idx],
        bpps[idx]
    };
}

Texture Texture::create(const std::string &path, TextureFormat format) {
    Texture tex;

    auto [internal, pixel_format, type, bpp] = format_details(format);
    void *buffer = nullptr;
    stbi_set_flip_vertically_on_load(1);

    if (type == GL_FLOAT)
        buffer =
            stbi_loadf(path.c_str(), &tex.width, &tex.height, &tex.bpp, bpp);
    else
        buffer =
            stbi_load(path.c_str(), &tex.width, &tex.height, &tex.bpp, bpp);

    GL_CALL(glGenTextures(1, &tex.id));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex.id));

    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_NEAREST_MIPMAP_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));

    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, internal, tex.width, tex.height, 0,
                         pixel_format, type, buffer));
    GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    if (buffer)
        stbi_image_free(buffer);

    std::filesystem::path tex_path = path;
    tex.path = path;
    tex.name = tex_path.filename().string();

    return tex;
}

Texture Texture::create(const void *data, int32_t width, int32_t height,
                        TextureFormat format) {
    Texture tex;

    GL_CALL(glGenTextures(1, &tex.id));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex.id));

    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_NEAREST_MIPMAP_LINEAR));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));


    auto [internal, pixel_format, type, bpp] = format_details(format);
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, internal, tex.width, tex.height, 0,
                         pixel_format, type, data));
    GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    return tex;
}

void Texture::destroy() {
    assert(id != 0 && "Trying to destroy invalid texture object");

    GL_CALL(glDeleteTextures(1, &id));
}

void Texture::bind(uint32_t slot) const {
    assert(id != 0 && "Trying to bind invalid texture object");

    GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, id));
}

void Texture::unbind() const {
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

RenderbufferDetails rbo_details(const RenderbufferSpec &spec) {
    static GLint type[] = {GL_DEPTH_COMPONENT, GL_STENCIL_COMPONENTS,
                           GL_DEPTH24_STENCIL8};
    static GLint attachment[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT,
                                 GL_DEPTH_STENCIL_ATTACHMENT};

    int32_t idx = (int32_t)spec.type;
    assert(idx < (int32_t)RenderbufferType::COUNT &&
           "Invalid renderbuffer type");

    return {
        type[idx],
        attachment[idx]
    };
}

GLint opengl_texture_type(ColorAttachmentType type) {
    switch (type) {
    case ColorAttachmentType::TEX_2D:
        return GL_TEXTURE_2D;
    case ColorAttachmentType::TEX_2D_MULTISAMPLE:
        return GL_TEXTURE_2D_MULTISAMPLE;
    case ColorAttachmentType::TEX_2D_ARRAY:
    case ColorAttachmentType::TEX_2D_ARRAY_SHADOW:
        return GL_TEXTURE_2D_ARRAY;
    case ColorAttachmentType::TEX_CUBEMAP:
        return GL_TEXTURE_CUBE_MAP;
    case ColorAttachmentType::TEX_CUBEMAP_ARRAY:
        return GL_TEXTURE_CUBE_MAP_ARRAY;
    default:
        assert(true && "Unsupported texture type passed");
        return {};
    }
}

Framebuffer Framebuffer::create() {
    Framebuffer fbo;
    GL_CALL(glGenFramebuffers(1, &fbo.id));
    return fbo;
}

void Framebuffer::destroy() {
    assert(id != 0 && "Trying to destroy invalid framebuffer object");

    GL_CALL(glDeleteFramebuffers(1, &id));
}

void Framebuffer::bind() const {
    assert(id != 0 && "Trying to bind invalid framebuffer object");

    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, id));
}

void Framebuffer::unbind() const {
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
}

void Framebuffer::add_renderbuffer(RenderbufferSpec spec) {
    assert(id != 0 &&
           "Trying to create renderbuffer for invalid framebuffer object");
    assert(rbo.id == 0 && "Trying to overwrite existing renderbuffer without "
                          "destroying it first");

    bind();
    GL_CALL(glGenRenderbuffers(1, &rbo.id));
    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, rbo.id));

    RenderbufferDetails details = rbo_details(spec);
    GL_CALL(glRenderbufferStorage(GL_RENDERBUFFER, details.type, spec.size.x,
                                  spec.size.y));
    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, details.attachment_type,
                                      GL_RENDERBUFFER, rbo.id));

    rbo.spec = spec;
}

void Framebuffer::add_color_attachment(ColorAttachmentSpec spec) {
    assert(id != 0 &&
           "Trying to create color attachment for invalid framebuffer object");

    bind();

    GLuint tex_id{};
    GL_CALL(glGenTextures(1, &tex_id));

    TextureFormatDetails tex_details = format_details(spec.format);
    GLint tex_type = opengl_texture_type(spec.type);
    GL_CALL(glBindTexture(tex_type, tex_id));
    GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_MIN_FILTER, spec.min_filter));
    GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_MAG_FILTER, spec.mag_filter));
    GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_WRAP_S, spec.wrap));
    GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_WRAP_T, spec.wrap));

    switch (tex_type) {
    case GL_TEXTURE_2D:
        GL_CALL(glTexParameterfv(tex_type, GL_TEXTURE_BORDER_COLOR,
                                 &spec.border_color[0]));
        GL_CALL(glTexImage2D(tex_type, 0, tex_details.internal_format,
                             spec.size.x, spec.size.y, 0, tex_details.format,
                             tex_details.type, nullptr));
        break;
    case GL_TEXTURE_2D_ARRAY:
        if (spec.type == ColorAttachmentType::TEX_2D_ARRAY_SHADOW)
            GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_COMPARE_MODE,
                                    GL_COMPARE_REF_TO_TEXTURE));

        GL_CALL(glTexParameterfv(tex_type, GL_TEXTURE_BORDER_COLOR,
                                 &spec.border_color[0]));
        GL_CALL(glTexImage3D(tex_type, 0, tex_details.internal_format,
                             spec.size.x, spec.size.y, spec.layers, 0,
                             tex_details.format, tex_details.type, nullptr));
        break;
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        GL_CALL(glTexParameterfv(tex_type, GL_TEXTURE_BORDER_COLOR,
                                 &spec.border_color[0]));
        GL_CALL(glTexStorage3D(tex_type, 1, tex_details.internal_format,
                               spec.size.x, spec.size.y, spec.layers * 6));
        break;
    case GL_TEXTURE_CUBE_MAP:
        GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_WRAP_R, spec.wrap));
        for (uint32_t i = 0; i < 6; i++) {
            GL_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                                 tex_details.internal_format, spec.size.x,
                                 spec.size.y, 0, tex_details.format,
                                 tex_details.type, nullptr));
        }
        break;
    default:
        assert(true && "Unsupported texture type");
        break;
    }

    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   tex_type, tex_id, 0));
    if (spec.gen_minmaps)
        GL_CALL(glGenerateMipmap(tex_type));

    color_attachments.push_back({tex_id, spec});
}

void Framebuffer::bind_renderbuffer() const {
    assert(id != 0 &&
           "Trying to bind renderbuffer of invalid framebuffer object");
    assert(rbo.id != 0 && "Trying to bind invalid renderbuffer object");

    bind();
    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, rbo.id));
    GL_CALL(glViewport(0, 0, rbo.spec.size.x, rbo.spec.size.y));
}

void Framebuffer::bind_color_attachment(uint32_t index, uint32_t slot) const {
    assert(id != 0 &&
           "Trying to bind color attachment of invalid framebuffer object");
    assert(index < color_attachments.size() &&
           "Invalid color attachment index");

    bind();
    const auto &[tex_id, spec] = color_attachments[index];
    assert(tex_id != 0 && "Trying to bind invalid color attachment");

    GLuint tex_type = opengl_texture_type(spec.type);
    GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    GL_CALL(glBindTexture(tex_type, tex_id));
}

void Framebuffer::resize_renderbuffer(const glm::ivec2 &size) {
    assert(id != 0 && "Trying to alter invalid framebuffer object");
    assert(rbo.id != 0 && "Trying to resize invalid renderbuffer object");

    if (size == rbo.spec.size)
        return;

    bind();
    bind_renderbuffer();

    RenderbufferDetails details = rbo_details(rbo.spec);
    GL_CALL(
        glRenderbufferStorage(GL_RENDERBUFFER, details.type, size.x, size.y));
    GL_CALL(glFramebufferRenderbuffer(GL_FRAMEBUFFER, details.attachment_type,
                                      GL_RENDERBUFFER, rbo.id));

    rbo.spec.size = size;
}

void Framebuffer::resize_color_attachment(uint32_t index,
                                          const glm::ivec2 &size) {
    assert(id != 0 && "Trying to alter invalid framebuffer object");
    assert(index < color_attachments.size() &&
           "Invalid color attachment index");

    ColorAttachment &attach = color_attachments[index];
    assert(attach.id != 0 &&
           "Trying to resize invalid color attachment object");

    if (attach.spec.size == size)
        return;

    bind();
    TextureFormatDetails tex_details = format_details(attach.spec.format);
    GLint tex_type = opengl_texture_type(attach.spec.type);
    GL_CALL(glBindTexture(tex_type, attach.id));

    switch (tex_type) {
    case GL_TEXTURE_2D:
        GL_CALL(glTexImage2D(tex_type, 0, tex_details.internal_format, size.x,
                             size.y, 0, tex_details.format, tex_details.type,
                             nullptr));
        break;
    case GL_TEXTURE_2D_ARRAY:
        GL_CALL(glTexImage3D(tex_type, 0, tex_details.internal_format, size.x,
                             size.y, attach.spec.layers, 0, tex_details.format,
                             tex_details.type, nullptr));
        break;
    case GL_TEXTURE_CUBE_MAP_ARRAY:
        GL_CALL(glTexStorage3D(tex_type, 1, tex_details.internal_format, size.x,
                               size.y, attach.spec.layers * 6));
        break;
    case GL_TEXTURE_CUBE_MAP:
        for (uint32_t i = 0; i < 6; i++) {
            GL_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                                 tex_details.internal_format, size.x, size.y, 0,
                                 tex_details.format, tex_details.type,
                                 nullptr));
        }
        break;
    default:
        assert(true && "Unsupported texture type");
        break;
    }

    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   tex_type, attach.id, 0));
    if (attach.spec.gen_minmaps)
        GL_CALL(glGenerateMipmap(tex_type));

    attach.spec.size = size;
}

void Framebuffer::resize_everything(const glm::ivec2 &size) {
    assert(id != 0 && "Trying to alter invalid framebuffer object");

    resize_renderbuffer(size);

    for (size_t i = 0; i < color_attachments.size(); i++)
        resize_color_attachment(i, size);
}

void Framebuffer::remove_renderbuffer() {
    assert(id != 0 &&
           "Trying to remove renderbuffer of invalid framebuffer object");
    assert(rbo.id != 0 && "Trying to remove invalid renderbuffer object");

    bind();
    GL_CALL(glBindRenderbuffer(GL_RENDERBUFFER, 0));
    GL_CALL(glDeleteRenderbuffers(1, &rbo.id));

    rbo = {};
}

void Framebuffer::remove_color_attachment(uint32_t index) {
    assert(id != 0 &&
           "Trying to remove color attachment of invalid framebuffer object");
    assert(index < color_attachments.size() &&
           "Invalid color attachment index");

    bind();

    ColorAttachment &color_attach = color_attachments[index];
    GLint tex_type = opengl_texture_type(color_attach.spec.type);
    GL_CALL(glBindTexture(tex_type, 0));
    GL_CALL(glDeleteTextures(1, &color_attach.id));

    color_attachments.erase(color_attachments.begin() + index);
}

bool Framebuffer::is_complete() const {
    GL_CALL(return glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                   GL_FRAMEBUFFER_COMPLETE);
}

UniformBuffer UniformBuffer::create(const void *data, uint32_t size) {
    UniformBuffer ubo;
    GL_CALL(glGenBuffers(1, &ubo.id));
    GL_CALL(glBindBuffer(GL_UNIFORM_BUFFER, ubo.id));
    GL_CALL(glBufferData(GL_UNIFORM_BUFFER, size, data, GL_DYNAMIC_DRAW));

    return ubo;
}

void UniformBuffer::destroy() {
    assert(id != 0 && "Trying to destroy invalid uniform buffer object");

    GL_CALL(glDeleteBuffers(1, &id));
    id = 0;
}

void UniformBuffer::bind() const {
    assert(id != 0 && "Trying to bind invalid uniform buffer object");

    GL_CALL(glBindBuffer(GL_UNIFORM_BUFFER, id));
}

void UniformBuffer::unbind() const {
    GL_CALL(glBindBuffer(GL_UNIFORM_BUFFER, 0));
}

void UniformBuffer::bind_buffer_range(uint32_t index, uint32_t offset,
                                      uint32_t size) {
    assert(id != 0 && "Trying to bind range of invalid uniform buffer object");

    GL_CALL(glBindBufferRange(GL_UNIFORM_BUFFER, index, id, (GLintptr)offset,
                              size));
}

void UniformBuffer::set_data(const void *data, uint32_t size,
                             uint32_t offset) const {
    assert(id != 0 && "Trying to set data of invalid uniform buffer object");

    GL_CALL(glBindBuffer(GL_UNIFORM_BUFFER, id));
    GL_CALL(glBufferSubData(GL_UNIFORM_BUFFER, (GLintptr)offset, size, data));
}
