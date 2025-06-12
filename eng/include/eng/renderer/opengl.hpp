#ifndef OPENGL_HPP
#define OPENGL_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <optional>
#include <string>
#include <unordered_map>

#define GL_CALL(f)                                                             \
    gl_clear_errors();                                                         \
    f;                                                                         \
    assert(gl_check_errors(#f, __FILE__, __LINE__));

void gl_clear_errors();

[[nodiscard]] bool gl_check_errors(const char *func, const char *filename, int32_t line);

struct VertexBuffer {
    [[nodiscard]] static VertexBuffer create();

    void allocate(const void *data, uint64_t size, uint32_t count = 0);
    void destroy();

    void bind() const;
    void unbind() const;
    void set_data(const void *data, uint64_t size, uint64_t offset = 0) const;

    GLuint id = 0;
    uint32_t vertex_count = 0;
};

struct IndexBuffer {
    [[nodiscard]] static IndexBuffer create();

    void allocate(const uint32_t *data, uint32_t count);
    void destroy();

    void bind() const;
    void unbind() const;

    GLuint id = 0;
    uint32_t indices_count = 0;
};

struct Shader {
    [[nodiscard]] static Shader create();
    [[nodiscard]] static GLuint compile(GLenum type, const std::string &src);

    [[nodiscard]] bool build(const std::string &vs_path,
                             const std::string &fs_path);
    void destroy();

    void bind() const;
    void unbind() const;

    [[nodiscard]] std::optional<GLint>
    get_uniform_location(const std::string &name);
    void set_uniform_1i(const std::string &name, int32_t val);
    void set_uniform_1f(const std::string &name, float val);
    void set_uniform_2f(const std::string &name, const glm::vec2 &val);
    void set_uniform_3f(const std::string &name, const glm::vec3 &val);
    void set_uniform_4f(const std::string &name, const glm::vec4 &val);
    void set_uniform_mat4(const std::string &name, const glm::mat4 &val);

    GLuint id = 0;
    std::unordered_map<std::string, GLint> uniform_cache;
};

struct VertexBufferElement {
    [[nodiscard]] static size_t get_size_of_type(GLenum type);

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
    [[nodiscard]] static VertexArray create();

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
    VertexBuffer vbo;
    VertexBuffer vbo_instanced;
    IndexBuffer ibo;
};

enum class TextureFormat {
    RGBA8,
    RGB8,
    RGBA16F,
    RGB16F,
    RG16F,
    RGB32F,
    R11_G11_B10,
    DEPTH_32F,

    COUNT
};

struct TextureFormatDetails {
    GLenum internal_format;
    GLenum format;
    GLenum type;
    GLenum bpp;
};

[[nodiscard]] TextureFormatDetails format_details(TextureFormat format);

struct Texture {
    [[nodiscard]] static Texture create(const std::string &path,
                                        TextureFormat format);
    [[nodiscard]] static Texture create(const void *data, int32_t width,
                                        int32_t height, TextureFormat format);

    void destroy();

    void bind(uint32_t slot = 0) const;
    void unbind() const;

    GLuint id = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t bpp = 0;

    std::string path;
    std::string name;
};

enum class RenderbufferType {
    DEPTH,
    STENICL,
    DEPTH_STENCIL,

    COUNT
};

struct RenderbufferDetails {
    GLint type;
    GLint attachment_type;
};

struct RenderbufferSpec {
    RenderbufferType type;
    glm::ivec2 size;
};

struct Renderbuffer {
    GLuint id = 0;
    RenderbufferSpec spec;
};

[[nodiscard]] RenderbufferDetails rbo_details(const RenderbufferSpec &spec);

enum class ColorAttachmentType {
    TEX_2D,
    TEX_2D_MULTISAMPLE,
    TEX_2D_ARRAY,
    TEX_2D_ARRAY_SHADOW,
    TEX_CUBEMAP,
    TEX_CUBEMAP_ARRAY,

    COUNT
};

[[nodiscard]] GLint opengl_texture_type(ColorAttachmentType type);

struct ColorAttachmentSpec {
    ColorAttachmentType type;
    TextureFormat format;

    GLint wrap;
    GLint min_filter;
    GLint mag_filter;

    glm::vec4 border_color;
    glm::ivec2 size;

    int32_t layers = 1;
    bool gen_minmaps = false;
};

struct ColorAttachment {
    GLuint id = 0;
    ColorAttachmentSpec spec;
};

struct Framebuffer {
    [[nodiscard]] static Framebuffer create();

    void destroy();

    void bind() const;
    void unbind() const;

    void add_renderbuffer(RenderbufferSpec spec);
    void add_color_attachment(ColorAttachmentSpec spec);

    void bind_renderbuffer() const;
    void bind_color_attachment(uint32_t index, uint32_t slot = 0) const;

    void resize_renderbuffer(const glm::ivec2 &size);
    void resize_color_attachment(uint32_t index, const glm::ivec2 &size);
    void resize_everything(const glm::ivec2 &size);

    void remove_renderbuffer();
    void remove_color_attachment(uint32_t index);

    [[nodiscard]] bool is_complete() const;

    GLuint id = 0;
    Renderbuffer rbo;
    std::vector<ColorAttachment> color_attachments;
};

#endif
