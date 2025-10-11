#include "eng/renderer/opengl.hpp"
#include "eng/random_utils.hpp"
#include "glm/common.hpp"
#include "glm/gtc/integer.hpp"
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

bool Shader::build(const ShaderSpec &spec) {
    assert(id != 0 && "Trying to build shader on invalid shader object");

    std::string vs_src = get_file_content(spec.vertex_shader.path).value();
    for (const StringReplacement &rep : spec.vertex_shader.replacements)
        replace_all(vs_src, rep.pattern, rep.target);

    std::string fs_src = get_file_content(spec.fragment_shader.path).value();
    for (const StringReplacement &rep : spec.fragment_shader.replacements)
        replace_all(fs_src, rep.pattern, rep.target);

    GLuint vs_id = compile(GL_VERTEX_SHADER, vs_src);
    GLuint fs_id = compile(GL_FRAGMENT_SHADER, fs_src);
    GL_CALL(glAttachShader(id, vs_id));
    GL_CALL(glAttachShader(id, fs_id));

    GLuint gs_id = 0;
    if (spec.geometry_shader.has_value()) {
        ShaderDescriptor geom = spec.geometry_shader.value();
        std::string gs_src = get_file_content(geom.path).value();
        for (const StringReplacement &rep : geom.replacements)
            replace_all(gs_src, rep.pattern, rep.target);

        gs_id = compile(GL_GEOMETRY_SHADER, gs_src);
        GL_CALL(glAttachShader(id, gs_id));
    }

    GL_CALL(glLinkProgram(id));
    GL_CALL(glDeleteShader(vs_id));
    GL_CALL(glDeleteShader(fs_id));

    if (gs_id != 0)
        GL_CALL(glDeleteShader(gs_id));

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

bool Shader::build_compute(const ShaderDescriptor &desc) {
    assert(id != 0 && "Trying to build shader on invalid shader object");

    std::string comp_src = get_file_content(desc.path).value();
    for (const StringReplacement &rep : desc.replacements)
        replace_all(comp_src, rep.pattern, rep.target);

    GLuint comp_id = compile(GL_COMPUTE_SHADER, comp_src);
    GL_CALL(glAttachShader(id, comp_id));
    GL_CALL(glLinkProgram(id));
    GL_CALL(glDeleteShader(comp_id));

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

void Shader::dispatch_compute(const glm::ivec3 &group) {
    assert(id != 0 && "Trying to dispatch invalid shader object");

    GL_CALL(glUseProgram(id));
    GL_CALL(glDispatchCompute(group.x, group.y, group.z));
    GL_CALL(glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT));
}

std::optional<GLint> Shader::get_uniform_location(const std::string &name) {
    assert(id != 0 && "Trying to get uniform of invalid shader object");

    if (uniform_cache.contains(name))
        return uniform_cache[name];

    GL_CALL(GLint loc = glGetUniformLocation(id, name.c_str()));
    if (loc == -1) {
        uniform_cache[name] = std::nullopt;
        return std::nullopt;
    }

    uniform_cache[name] = loc;
    return loc;
}

void Shader::set_uniform_1i(const std::string &name, int32_t val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform1i(loc.value(), val));
}

void Shader::try_set_uniform_1i(const std::string &name, int32_t val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value())
        return;

    GL_CALL(glUniform1i(loc.value(), val));
}

void Shader::set_uniform_1f(const std::string &name, float val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform1f(loc.value(), val));
}

void Shader::try_set_uniform_1f(const std::string &name, float val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value())
        return;

    GL_CALL(glUniform1f(loc.value(), val));
}

void Shader::set_uniform_2f(const std::string &name, const glm::vec2 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform2f(loc.value(), val.x, val.y));
}

void Shader::try_set_uniform_2f(const std::string &name, const glm::vec2 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value())
        return;

    GL_CALL(glUniform2f(loc.value(), val.x, val.y));
}

void Shader::set_uniform_3f(const std::string &name, const glm::vec3 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform3f(loc.value(), val.x, val.y, val.z));
}

void Shader::try_set_uniform_3f(const std::string &name, const glm::vec3 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value())
        return;

    GL_CALL(glUniform3f(loc.value(), val.x, val.y, val.z));
}

void Shader::set_uniform_4f(const std::string &name, const glm::vec4 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniform4f(loc.value(), val.x, val.y, val.z, val.w));
}

void Shader::try_set_uniform_4f(const std::string &name, const glm::vec4 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value())
        return;

    GL_CALL(glUniform4f(loc.value(), val.x, val.y, val.z, val.w));
}

void Shader::set_uniform_mat4(const std::string &name, const glm::mat4 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value()) {
        fprintf(stderr, "Unable to get location of uniform '%s'\r\n",
                name.c_str());
        return;
    }

    GL_CALL(glUniformMatrix4fv(loc.value(), 1, GL_FALSE, &val[0][0]));
}

void Shader::try_set_uniform_mat4(const std::string &name,
                                  const glm::mat4 &val) {
    assert(id != 0 && "Trying to set uniform of invalid shader object");

    std::optional<GLint> loc = get_uniform_location(name);
    if (!loc.has_value())
        return;

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
    static size_t sizes[] = {sizeof(GLbyte),   sizeof(GLubyte), sizeof(GLshort),
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
    for (int32_t i = attrib_offset; i < len; i++) {
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
    for (int32_t i = attrib_offset; i < len; i++) {
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
    for (int32_t i = attrib_offset; i < len; i++) {
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
        GL_RGBA8, GL_RGB8, GL_RGBA16F, GL_RGB16F,
        GL_RG16F, GL_R8,   GL_RGB32F,  GL_R11F_G11F_B10F,
        GL_DEPTH_COMPONENT32F
    };
    static GLenum formats[] = {
        GL_RGBA, GL_RGB, GL_RGBA, GL_RGB,
        GL_RG,   GL_RED, GL_RGB,  GL_RGB,
        GL_DEPTH_COMPONENT
    };
    static GLenum types[] = {
        GL_UNSIGNED_BYTE, GL_UNSIGNED_BYTE, GL_FLOAT, GL_FLOAT,
        GL_FLOAT,         GL_UNSIGNED_BYTE, GL_FLOAT, GL_FLOAT,
        GL_FLOAT
    };
    static GLenum bpps[] = {
        4, 3, 4, 3,
        2, 1, 3, 3,
        1
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

Texture Texture::create(const std::string &path, TextureSpec spec) {
    Texture tex;
    tex.spec = spec;

    auto [internal, pixel_format, type, bpp] = format_details(spec.format);
    void *buffer = nullptr;
    stbi_set_flip_vertically_on_load(1);

    if (type == GL_FLOAT)
        buffer = stbi_loadf(path.c_str(), &tex.spec.size.x, &tex.spec.size.y,
                            nullptr, bpp);
    else
        buffer = buffer = stbi_load(path.c_str(), &tex.spec.size.x,
                                    &tex.spec.size.y, nullptr, bpp);

    GL_CALL(glGenTextures(1, &tex.id));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex.id));

    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, spec.min_filter));
    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, spec.mag_filter));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, spec.wrap));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, spec.wrap));

    if (pixel_format == GL_RED) {
        GLint swizzle_mask[4] = {GL_RED, GL_RED, GL_RED, GL_ONE};
        GL_CALL(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA,
                                 swizzle_mask));
    }

    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, internal, tex.spec.size.x,
                         tex.spec.size.y, 0, pixel_format, type, buffer));

    if (spec.gen_mipmaps) {
        GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));

        tex.spec.mips =
            1 +
            glm::floor(glm::log2(glm::max(tex.spec.size.x, tex.spec.size.y)));
    } else {
        for (int32_t mip = 1; mip < spec.mips; mip++) {
            int32_t w = tex.spec.size.x >> mip;
            int32_t h = tex.spec.size.y >> mip;

            GL_CALL(glTexImage2D(GL_TEXTURE_2D, mip, internal, w, h, 0,
                                 pixel_format, type, nullptr));
        }
    }

    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    if (buffer)
        stbi_image_free(buffer);

    std::filesystem::path tex_path = path;
    tex.path = path;
    tex.name = tex_path.filename().string();

    return tex;
}

Texture Texture::create(const void *data, TextureSpec spec) {
    Texture tex;
    tex.spec = spec;

    GL_CALL(glGenTextures(1, &tex.id));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex.id));

    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, spec.min_filter));
    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, spec.mag_filter));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, spec.wrap));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, spec.wrap));

    auto [internal, pixel_format, type, bpp] = format_details(spec.format);
    GL_CALL(glTexImage2D(GL_TEXTURE_2D, 0, internal, tex.spec.size.x,
                         tex.spec.size.y, 0, pixel_format, type, data));

    if (spec.gen_mipmaps) {
        GL_CALL(glGenerateMipmap(GL_TEXTURE_2D));

        tex.spec.mips =
            1 +
            glm::floor(glm::log2(glm::max(tex.spec.size.x, tex.spec.size.y)));
    } else {
        for (int32_t mip = 1; mip < spec.mips; mip++) {
            int32_t w = tex.spec.size.x >> mip;
            int32_t h = tex.spec.size.y >> mip;

            GL_CALL(glTexImage2D(GL_TEXTURE_2D, mip, internal, w, h, 0,
                                 pixel_format, type, nullptr));
        }
    }

    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));

    return tex;
}

Texture Texture::create_storage(TextureSpec spec) {
    Texture tex;
    tex.spec = spec;

    GL_CALL(glGenTextures(1, &tex.id));
    GL_CALL(glBindTexture(GL_TEXTURE_2D, tex.id));

    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, spec.min_filter));
    GL_CALL(
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, spec.mag_filter));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, spec.wrap));
    GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, spec.wrap));

    auto [internal, pixel_format, type, bpp] = format_details(spec.format);

    if (spec.gen_mipmaps)
        tex.spec.mips =
            1 +
            glm::floor(glm::log2(glm::max(tex.spec.size.x, tex.spec.size.y)));

    GL_CALL(glTexStorage2D(GL_TEXTURE_2D, tex.spec.mips, internal,
                           tex.spec.size.x, tex.spec.size.y));
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

void Texture::bind_image(int32_t mip, uint32_t binding,
                         ImageAccess access) const {
    assert(id != 0 && "Trying to bind invalid texture object");

    assert((int32_t)access <= (int32_t)ImageAccess::READ_WRITE &&
           "Invalid access");

    GLenum acc[] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};
    GLenum internal = format_details(spec.format).internal_format;
    GL_CALL(glBindImageTexture(binding, id, mip, GL_FALSE, 0,
                               acc[(int32_t)access], internal));
}

void Texture::unbind() const {
    GL_CALL(glBindTexture(GL_TEXTURE_2D, 0));
}

void Texture::clear_texture() {
    assert(id != 0 && "Trying to clear invalid texture object");

    auto [internal, pixel_format, type, bpp] = format_details(spec.format);
    for (int32_t mip = 0; mip < spec.mips; mip++) {
        GL_CALL(glClearTexImage(id, mip, pixel_format, type, 0));
    }
}

void Texture::change_params(TextureSpec spec) {
    bind();

    if (spec.min_filter != this->spec.min_filter) {
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                spec.min_filter));
        this->spec.min_filter = spec.min_filter;
    }

    if (spec.mag_filter != this->spec.mag_filter) {
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                spec.mag_filter));
        this->spec.mag_filter = spec.mag_filter;
    }

    if (spec.wrap != this->spec.wrap) {
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, spec.wrap));
        GL_CALL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, spec.wrap));

        this->spec.wrap = spec.wrap;
    }
}

bool Texture::has_mips() const {
    return spec.gen_mipmaps || spec.mips > 1;
}

const char *Texture::filter_str() {
    if (spec.mag_filter == GL_NEAREST)
        return "Point";

    if (spec.min_filter == GL_LINEAR ||
        spec.min_filter == GL_LINEAR_MIPMAP_NEAREST)
        return "Bilinear";

    if (spec.min_filter == GL_LINEAR_MIPMAP_LINEAR)
        return "Trilinear";

    __builtin_unreachable();
}

const char *Texture::wrap_str() {
    switch (spec.wrap) {
    case GL_REPEAT:
        return "Repeat";

    case GL_MIRRORED_REPEAT:
        return "Mirrored repeat";

    case GL_CLAMP_TO_EDGE:
        return "Clamp to edge";

    case GL_MIRROR_CLAMP_TO_EDGE:
        return "Mirror clamp to edge";

    case GL_CLAMP_TO_BORDER:
        return "Clamp to border";

    default:
        assert("Invalid wrap mode");
    }

    __builtin_unreachable();
}

const char *Texture::wrap_str(GLint wrap) {
    switch (wrap) {
    case GL_REPEAT:
        return "Repeat";

    case GL_MIRRORED_REPEAT:
        return "Mirrored repeat";

    case GL_CLAMP_TO_EDGE:
        return "Clamp to edge";

    case GL_MIRROR_CLAMP_TO_EDGE:
        return "Mirror clamp to edge";

    case GL_CLAMP_TO_BORDER:
        return "Clamp to border";

    default:
        assert("Invalid wrap mode");
    }

    __builtin_unreachable();
}

CubeTexture
CubeTexture::create(CubeTextureSpec spec) {
    CubeTexture tex;
    tex.spec = spec;

    TextureFormatDetails tex_details = format_details(spec.format);

    GL_CALL(glGenTextures(1, &tex.id));
    GL_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, tex.id));

    GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER,
                            spec.min_filter));
    GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER,
                            spec.mag_filter));
    GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, spec.wrap));
    GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, spec.wrap));
    GL_CALL(glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, spec.wrap));

    for (int32_t i = 0; i < 6; i++) {
        GL_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                             tex_details.internal_format, spec.face_dim,
                             spec.face_dim, 0, tex_details.format,
                             tex_details.type, nullptr));
    }

    if (spec.gen_mipmaps) {
        GL_CALL(glGenerateMipmap(GL_TEXTURE_CUBE_MAP));

        tex.spec.mips = 1 + glm::floor(glm::log2(spec.face_dim));
    } else {
        for (int32_t mip = 1; mip < spec.mips; mip++) {
            int32_t dim = spec.face_dim >> mip;

            for (int32_t i = 0; i < 6; i++) {
                GL_CALL(glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
                                     tex_details.internal_format, dim, dim, 0,
                                     tex_details.format, tex_details.type,
                                     nullptr));
            }
        }
    }

    GL_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));

    return tex;
}

void CubeTexture::destroy() {
    assert(id != 0 && "Trying to destroy invalid cube texture object");

    GL_CALL(glDeleteTextures(1, &id));
}

void CubeTexture::bind(int32_t slot) const {
    assert(id != 0 && "Trying to bind invalid cube texture object");

    GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    GL_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, id));
}

void CubeTexture::bind_face_image(int32_t face, int32_t mip, uint32_t binding,
                                  ImageAccess access) const {
    assert(id != 0 && "Trying to bind invalid cube texture object");

    assert((int32_t)access <= (int32_t)ImageAccess::READ_WRITE &&
           "Invalid access");

    GLenum acc[] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};
    GLenum internal = format_details(spec.format).internal_format;
    GL_CALL(glBindImageTexture(binding, id, mip, GL_FALSE, face,
                               acc[(int32_t)access], internal));
}

void CubeTexture::unbind() const {
    GL_CALL(glBindTexture(GL_TEXTURE_CUBE_MAP, 0));
}

DepthAttachmentDetails depth_attachment_details(const DepthAttachmentSpec &spec) {
    static GLint internal_format[] = {GL_DEPTH_COMPONENT32F,
                                      GL_DEPTH24_STENCIL8};
    static GLint format[] = {GL_DEPTH_COMPONENT, GL_DEPTH_STENCIL};
    static GLint type[] = {GL_FLOAT, GL_UNSIGNED_INT_24_8};
    static GLint attachment[] = {GL_DEPTH_ATTACHMENT,
                                 GL_DEPTH_STENCIL_ATTACHMENT};

    int32_t idx = (int32_t)spec.type;
    assert(idx < (int32_t)DepthAttachmentType::COUNT &&
           "Invalid renderbuffer type");

    return {
        internal_format[idx],
        format[idx],
        type[idx],
        attachment[idx]
    };
}

GLint opengl_texture_type(TextureType type) {
    switch (type) {
    case TextureType::TEX_2D:
        return GL_TEXTURE_2D;
    case TextureType::TEX_2D_ARRAY:
    case TextureType::TEX_2D_ARRAY_SHADOW:
        return GL_TEXTURE_2D_ARRAY;
    case TextureType::TEX_CUBE_ARRAY:
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

void Framebuffer::add_depth_attachment(DepthAttachmentSpec spec,
                                       std::optional<int32_t> target_index) {
    assert(id != 0 &&
           "Trying to create depth attachment for invalid framebuffer object");

    bind();

    GLuint tex_id{};
    GL_CALL(glGenTextures(1, &tex_id));

    DepthAttachmentDetails details = depth_attachment_details(spec);
    GLenum tex_type = opengl_texture_type(spec.tex_type);
    GL_CALL(glBindTexture(tex_type, tex_id));
    GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

    switch (spec.tex_type) {
    case TextureType::TEX_2D:
        GL_CALL(glTexImage2D(tex_type, 0, details.internal_format, spec.size.x,
                             spec.size.y, 0, details.format, details.type,
                             nullptr));
        break;

    case TextureType::TEX_2D_ARRAY_SHADOW:
        GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_COMPARE_MODE,
                                GL_COMPARE_REF_TO_TEXTURE));
        [[fallthrough]];

    case TextureType::TEX_2D_ARRAY:
        GL_CALL(glTexImage3D(tex_type, 0, details.internal_format, spec.size.x,
                             spec.size.y, spec.layers, 0, details.format,
                             details.type, nullptr));
        break;

    case TextureType::TEX_CUBE_ARRAY_SHADOW:
        GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_COMPARE_MODE,
                                GL_COMPARE_REF_TO_TEXTURE));
        [[fallthrough]];

    case TextureType::TEX_CUBE_ARRAY:
        GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE));
        GL_CALL(glTexStorage3D(tex_type, 1, details.internal_format,
                               spec.size.x, spec.size.y, 6 * spec.layers));
        break;

    default:
        assert("Invalid TextureType provided");
        __builtin_unreachable();
    }

    auto insert_itr = depth_attachments.end();
    if (target_index.has_value())
        insert_itr = depth_attachments.begin() + target_index.value();

    depth_attachments.insert(insert_itr, {tex_id, spec});
}

void Framebuffer::add_color_attachment(ColorAttachmentSpec spec,
                                       std::optional<int32_t> target_index) {
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
    GL_CALL(glTexParameterfv(tex_type, GL_TEXTURE_BORDER_COLOR,
                             &spec.border_color[0]));

    switch (tex_type) {
    case GL_TEXTURE_2D:
        GL_CALL(glTexImage2D(tex_type, 0, tex_details.internal_format,
                             spec.size.x, spec.size.y, 0, tex_details.format,
                             tex_details.type, nullptr));
        break;
    case GL_TEXTURE_2D_ARRAY:
        if (spec.type == TextureType::TEX_2D_ARRAY_SHADOW) {
            GL_CALL(glTexParameteri(tex_type, GL_TEXTURE_COMPARE_MODE,
                                    GL_COMPARE_REF_TO_TEXTURE));
        }

        GL_CALL(glTexImage3D(tex_type, 0, tex_details.internal_format,
                             spec.size.x, spec.size.y, spec.layers, 0,
                             tex_details.format, tex_details.type, nullptr));
        break;
    default:
        assert(true && "Unsupported texture type");
        break;
    }

    if (spec.gen_minmaps) {
        GL_CALL(glGenerateMipmap(tex_type));
    }

    auto insert_itr = color_attachments.end();
    if (target_index.has_value())
        insert_itr = color_attachments.begin() + target_index.value();

    color_attachments.insert(insert_itr, {tex_id, spec});
}

void Framebuffer::rebuild_depth_attachment(int32_t index,
                                           DepthAttachmentSpec spec) {
    assert(id != 0 &&
           "Trying to rebuild depth attachment for invalid framebuffer object");
    assert(index < depth_attachments.size() &&
           "Invalid depth attachment index");

    bind();
    remove_depth_attachment(index);
    add_depth_attachment(spec, index);
}

void Framebuffer::rebuild_color_attachment(int32_t index,
                                           ColorAttachmentSpec spec) {
    assert(id != 0 &&
           "Trying to rebuild color attachment for invalid framebuffer object");
    assert(index < color_attachments.size() &&
           "Invalid color attachment index");

    bind();
    remove_color_attachment(index);
    add_color_attachment(spec, index);
}

void Framebuffer::bind_depth_attachment(int32_t index, int32_t slot) const {
    assert(id != 0 &&
           "Trying to bind renderbuffer of invalid framebuffer object");
    assert(index < depth_attachments.size() &&
           "Invalid depth attachment index");

    const auto &[tex_id, spec] = depth_attachments[index];
    assert(tex_id != 0 && "Trying to bind invalid depth attachment");

    GLuint tex_type = opengl_texture_type(spec.tex_type);
    GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    GL_CALL(glBindTexture(tex_type, tex_id));
}

void Framebuffer::bind_color_attachment(int32_t index, int32_t slot) const {
    assert(id != 0 &&
           "Trying to bind color attachment of invalid framebuffer object");
    assert(index < color_attachments.size() &&
           "Invalid color attachment index");

    const auto &[tex_id, spec] = color_attachments[index];
    assert(tex_id != 0 && "Trying to bind invalid color attachment");

    GLuint tex_type = opengl_texture_type(spec.type);
    GL_CALL(glActiveTexture(GL_TEXTURE0 + slot));
    GL_CALL(glBindTexture(tex_type, tex_id));
}

void Framebuffer::bind_color_attachment_image(int32_t index, int32_t mip,
                                              int32_t binding,
                                              ImageAccess access) const {
    assert(id != 0 &&
           "Trying to bind color attachment of invalid framebuffer object");
    assert(index < color_attachments.size() &&
           "Invalid color attachment index");

    const auto &[tex_id, spec] = color_attachments[index];
    assert(tex_id != 0 && "Trying to bind invalid color attachment as image");

    assert((int32_t)access <= (int32_t)ImageAccess::READ_WRITE &&
           "Invalid access");

    GLenum acc[] = {GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE};
    GLenum internal = format_details(spec.format).internal_format;
    GL_CALL(glBindImageTexture(binding, tex_id, mip, GL_FALSE, 0,
                               acc[(int32_t)access], internal));
}

void Framebuffer::draw_to_depth_attachment(int32_t index, int32_t mip) {
    assert(index < depth_attachments.size() &&
           "Trying to draw to invalid depth map");

    const auto &[id, spec] = depth_attachments[index];
    bind();

    DepthAttachmentDetails details = depth_attachment_details(spec);
    if (spec.tex_type == TextureType::TEX_2D) {
        GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER, details.attachment_type,
                                       GL_TEXTURE_2D, id, 0));
    } else {
        GL_CALL(glFramebufferTexture(GL_FRAMEBUFFER, details.attachment_type,
                                     id, 0));
    }

    GL_CALL(glViewport(0, 0, spec.size.x, spec.size.y));
}

void Framebuffer::draw_to_color_attachment(int32_t index,
                                           int32_t target_attachment,
                                           int32_t mip) {
    assert(index < color_attachments.size() &&
           "Trying to draw to invalid color attachment");

    const auto &[id, spec] = color_attachments[index];
    bind();
    GL_CALL(glFramebufferTexture2D(GL_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0 + target_attachment,
                                   opengl_texture_type(spec.type), id, mip));
}

void Framebuffer::clear_color_attachment(int32_t attachment_index,
                                         int32_t mip) const {
    assert(attachment_index < color_attachments.size() &&
           "Trying to clear invalid color attachment");

    const auto &[id, spec] = color_attachments[attachment_index];
    TextureFormatDetails tex_details = format_details(spec.format);
    GL_CALL(glClearTexImage(id, mip, tex_details.format, GL_FLOAT, 0));
}

void Framebuffer::resize_depth_attachment(int32_t index,
                                          const glm::ivec2 &size) {
    assert(id != 0 && "Trying to alter invalid framebuffer object");
    assert(index < depth_attachments.size() &&
           "Invalid depth attachment index");

    DepthAttachment &attach = depth_attachments[index];
    assert(attach.id != 0 &&
           "Trying to resize invalid depth attachment object");

    if (attach.spec.size == size)
        return;

    DepthAttachmentSpec spec = attach.spec;
    spec.size = size;

    bind();
    remove_depth_attachment(index);
    add_depth_attachment(spec, index);
}

void Framebuffer::resize_color_attachment(int32_t index,
                                          const glm::ivec2 &size) {
    assert(id != 0 && "Trying to alter invalid framebuffer object");
    assert(index < color_attachments.size() &&
           "Invalid color attachment index");

    ColorAttachment &attach = color_attachments[index];
    assert(attach.id != 0 &&
           "Trying to resize invalid color attachment object");

    if (attach.spec.size == size)
        return;

    ColorAttachmentSpec spec = attach.spec;
    spec.size = size;

    bind();
    remove_color_attachment(index);
    add_color_attachment(spec, index);
}

void Framebuffer::resize_everything(const glm::ivec2 &size) {
    assert(id != 0 && "Trying to alter invalid framebuffer object");

    for (int32_t i = 0; i < depth_attachments.size(); i++)
        resize_depth_attachment(i, size);

    for (int32_t i = 0; i < color_attachments.size(); i++)
        resize_color_attachment(i, size);
}

void Framebuffer::remove_depth_attachment(int32_t index) {
    assert(id != 0 &&
           "Trying to remove depth attachment of invalid framebuffer object");
    assert(index < depth_attachments.size() &&
           "Invalid depth attachment index");

    bind();

    DepthAttachment &depth_attach = depth_attachments[index];
    GLint tex_type = opengl_texture_type(depth_attach.spec.tex_type);
    GL_CALL(glBindTexture(tex_type, 0));
    GL_CALL(glDeleteTextures(1, &depth_attach.id));

    depth_attachments.erase(depth_attachments.begin() + index);
}

void Framebuffer::remove_color_attachment(int32_t index) {
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

void Framebuffer::fill_color_draw_buffers() {
    std::vector<GLenum> buffers(color_attachments.size());
    for (int32_t i = 0; i < color_attachments.size(); i++)
        buffers[i] = GL_COLOR_ATTACHMENT0 + i;

    GL_CALL(glDrawBuffers(buffers.size(), buffers.data()));
}

glm::u8vec4 Framebuffer::pixel_at(const glm::vec2 &coords,
                                  int32_t attachment_idx) const {
    glm::u8vec4 pixel;

    bind();
    GL_CALL(glReadBuffer(GL_COLOR_ATTACHMENT0 + attachment_idx));
    GL_CALL(glReadPixels((GLint)coords.x, (GLint)coords.y, 1, 1, GL_RGBA,
                         GL_UNSIGNED_BYTE, &pixel[0]));

    return pixel;
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

ShaderStorage ShaderStorage::create(const void *data, uint32_t size) {
    ShaderStorage sso;
    GL_CALL(glGenBuffers(1, &sso.id));
    GL_CALL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, sso.id));
    GL_CALL(
        glBufferData(GL_SHADER_STORAGE_BUFFER, size, data, GL_DYNAMIC_DRAW));

    return sso;
}

void ShaderStorage::destroy() {
    assert(id != 0 && "Trying to destroy invalid shader storage object");

    GL_CALL(glDeleteBuffers(1, &id));
    id = 0;
}

void ShaderStorage::bind() const {
    assert(id != 0 && "Trying to bind invalid shader storage object");

    GL_CALL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, id));
}

void ShaderStorage::unbind() const {
    GL_CALL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0));
}

void ShaderStorage::bind_buffer_range(uint32_t index, uint32_t offset,
                                      uint32_t size) {
    assert(id != 0 && "Trying to bind range of invalid shader storage object");

    GL_CALL(glBindBufferRange(GL_SHADER_STORAGE_BUFFER, index, id,
                              (GLintptr)offset, size));
}

void ShaderStorage::set_data(const void *data, uint32_t size,
                             uint32_t offset) const {
    assert(id != 0 && "Trying to set data of invalid shader storage object");

    GL_CALL(glBindBuffer(GL_SHADER_STORAGE_BUFFER, id));
    GL_CALL(glBufferSubData(GL_SHADER_STORAGE_BUFFER, (GLintptr)offset, size,
                            data));
}

void ShaderStorage::realloc(uint32_t new_size) {
    assert(id != 0 && "Trying to realloc invalid shader storage object");

    GL_CALL(
        glBufferData(GL_SHADER_STORAGE_BUFFER, new_size, nullptr, GL_DYNAMIC_DRAW));
}
