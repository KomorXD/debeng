#include "eng/renderer/renderer.hpp"
#include "eng/renderer/opengl.hpp"
#include "eng/scene/assets.hpp"
#include "eng/scene/components.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "GLFW/glfw3.h"
#include <vector>

namespace eng::renderer {

struct GPU {
    char *vendor;
    char *opengl_version;
    char *device_name;

    GLint texture_units = 0;
};

struct Renderer {
    GPU gpu;
    Shader default_shader;

    UniformBuffer camera_uni_buffer;

    UniformBuffer point_lights_uni_buffer;
    std::vector<PointLightData> point_lights;

    VertexArray screen_quad_vao;
    Shader screen_quad_shader;

    std::unordered_map<AssetID, std::vector<MeshInstance>> mesh_instances;
};

static Renderer s_renderer{};
static const AssetPack *s_asset_pack{};

void opengl_msg_cb(unsigned source, unsigned type, unsigned id,
                   unsigned severity, int length, const char *msg,
                   const void *user_param) {
    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        fprintf(stderr, "%s\r\n", msg);
        return;
    case GL_DEBUG_SEVERITY_MEDIUM:
        fprintf(stderr, "%s\r\n", msg);
        return;
    case GL_DEBUG_SEVERITY_LOW:
        fprintf(stderr, "%s\r\n", msg);
        return;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        printf("%s\r\n", msg);
        return;
    }
}

bool init() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        return false;

    GPU &gpu_spec = s_renderer.gpu;
    GL_CALL(glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                          &gpu_spec.texture_units));
    GL_CALL(gpu_spec.vendor = (char *)glGetString(GL_VENDOR));
    GL_CALL(gpu_spec.device_name = (char *)glGetString(GL_RENDERER));
    GL_CALL(gpu_spec.opengl_version = (char *)glGetString(GL_VERSION));

    printf("GPU Vendor: %s\n", gpu_spec.vendor);
    printf("GPU Device: %s\n", gpu_spec.device_name);
    printf("OpenGL version: %s\n", gpu_spec.opengl_version);
    printf("Max texture units: %d\n", gpu_spec.texture_units);

    GL_CALL(glEnable(GL_DEBUG_OUTPUT));
    GL_CALL(glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS));
    GL_CALL(glDebugMessageCallback(opengl_msg_cb, nullptr));
    GL_CALL(glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                                  GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr,
                                  GL_FALSE));

    GL_CALL(glEnable(GL_DEPTH_TEST));
    GL_CALL(glDepthFunc(GL_LESS));
    GL_CALL(glEnable(GL_BLEND));
    GL_CALL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL_CALL(glEnable(GL_CULL_FACE));
    GL_CALL(glCullFace(GL_BACK));
    GL_CALL(glEnable(GL_LINE_SMOOTH));
    GL_CALL(glEnable(GL_MULTISAMPLE));
    GL_CALL(glEnable(GL_STENCIL_TEST));
    GL_CALL(glStencilFunc(GL_NOTEQUAL, 1, 0xFF));
    GL_CALL(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));
    GL_CALL(glStencilMask(0x00));
    GL_CALL(glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS));

    s_renderer.default_shader = Shader::create();
    assert(s_renderer.default_shader.build("resources/shaders/base.vert",
                                           "resources/shaders/base.frag") &&
           "Default shaders not found");

    s_renderer.mesh_instances[eng::AssetPack::QUAD_ID].reserve(128);
    s_renderer.mesh_instances[eng::AssetPack::CUBE_ID].reserve(128);
    s_renderer.mesh_instances[eng::AssetPack::SPHERE_ID].reserve(128);

    float screen_quad_vertices[] = {
    //    position      texture_uv
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f,

         1.0f,  1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,   0.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f
    };

    s_renderer.screen_quad_vao = VertexArray::create();
    s_renderer.screen_quad_vao.bind();

    VertexBuffer vbo = VertexBuffer::create();
    vbo.allocate(screen_quad_vertices, sizeof(screen_quad_vertices));

    VertexBufferLayout layout;
    layout.push_float(2); // 0 - position
    layout.push_float(2); // 1 - texture UV
    s_renderer.screen_quad_vao.add_vertex_buffer(vbo, layout);

    s_renderer.screen_quad_shader = Shader::create();
    assert(s_renderer.screen_quad_shader.build(
               "resources/shaders/screen_quad.vert",
               "resources/shaders/screen_quad.frag") &&
           "Screen quad shader not found");
    s_renderer.screen_quad_shader.bind();
    s_renderer.screen_quad_shader.set_uniform_1i("u_screen_texture", 0);

    s_renderer.camera_uni_buffer =
        UniformBuffer::create(nullptr, sizeof(CameraData));
    s_renderer.camera_uni_buffer.bind_buffer_range(0, 0, sizeof(CameraData));

    uint32_t size = 128 * sizeof(PointLightData) + sizeof(int32_t);
    s_renderer.point_lights_uni_buffer = UniformBuffer::create(nullptr, size);
    s_renderer.point_lights_uni_buffer.bind_buffer_range(1, 0, size);

    return true;
}

void shutdown() {
    s_renderer.default_shader.destroy();
    s_renderer.camera_uni_buffer.destroy();
    s_renderer.point_lights_uni_buffer.destroy();
}

void scene_begin(const CameraData &camera, AssetPack &asset_pack) {
    s_asset_pack = &asset_pack;

    assert(s_asset_pack && "Empty asset pack object");

    s_renderer.point_lights.clear();

    for (auto &[mesh_id, instances] : s_renderer.mesh_instances)
        instances.clear();

    s_renderer.camera_uni_buffer.bind();
    s_renderer.camera_uni_buffer.set_data(&camera, sizeof(CameraData));
}

void scene_end() {
    s_renderer.point_lights_uni_buffer.bind();

    int32_t count = s_renderer.point_lights.size();
    s_renderer.point_lights_uni_buffer.set_data(s_renderer.point_lights.data(),
                                                count * sizeof(PointLightData));

    int32_t offset = 128 * sizeof(PointLightData);
    s_renderer.point_lights_uni_buffer.set_data(&count, sizeof(int32_t), offset);

    s_renderer.default_shader.bind();
    s_renderer.default_shader.set_uniform_1i("u_texture", 0);

    for (auto &[mesh_id, instances] : s_renderer.mesh_instances) {
        if (instances.empty())
            continue;

        const Mesh &mesh = s_asset_pack->meshes.at(mesh_id);
        mesh.vao.vbo_instanced.set_data(
            instances.data(), instances.size() * sizeof(MeshInstance));
        draw_elements_instanced(s_renderer.default_shader, mesh.vao,
                               instances.size());
    }
}

void submit_mesh(const glm::mat4 &transform, AssetID mesh_id) {
    std::vector<MeshInstance> &instances = s_renderer.mesh_instances[mesh_id];
    MeshInstance &instance = instances.emplace_back();
    instance.transform = transform;
}

void submit_quad(const glm::vec3 &position) {
    std::vector<MeshInstance> &instances =
        s_renderer.mesh_instances.at(AssetPack::QUAD_ID);
    MeshInstance &ins = instances.emplace_back();
    ins.transform = glm::translate(glm::mat4(1.0f), position) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
}

void submit_cube(const glm::vec3 &position) {
    std::vector<MeshInstance> &instances =
        s_renderer.mesh_instances.at(AssetPack::CUBE_ID);
    MeshInstance &ins = instances.emplace_back();
    ins.transform = glm::translate(glm::mat4(1.0f), position) *
                    glm::scale(glm::mat4(1.0f), glm::vec3(1.0f));
}

void submit_point_light(const glm::vec3 &position, const PointLight &light) {
    PointLightData &light_data = s_renderer.point_lights.emplace_back();
    light_data.position_and_linear =
        glm::vec4(position, light.linear);
    light_data.color_and_quadratic =
        glm::vec4(light.color * light.intensity, light.quadratic);
}

void draw_to_screen_quad() {
    GL_CALL(glDisable(GL_DEPTH_TEST));
    draw_arrays(s_renderer.screen_quad_shader, s_renderer.screen_quad_vao, 6);
    GL_CALL(glEnable(GL_DEPTH_TEST));
}

void draw_arrays(const Shader &shader, const VertexArray &vao,
                 uint32_t vertices_count) {
    vao.bind();
    shader.bind();

    GL_CALL(glDrawArrays(GL_TRIANGLES, 0, vertices_count));
}

void draw_arrays_instanced(const Shader &shader, const VertexArray &vao,
                           uint32_t vertices_count, uint32_t instances_count) {
    vao.bind();
    shader.bind();

    GL_CALL(glDrawArraysInstanced(GL_TRIANGLES, 0, vertices_count,
                                  instances_count));
}

void draw_elements(const Shader &shader, const VertexArray &vao) {
    vao.bind();
    shader.bind();

    GL_CALL(glDrawElements(GL_TRIANGLES, vao.ibo.indices_count, GL_UNSIGNED_INT,
                           nullptr));
}

void draw_elements_instanced(const Shader &shader, const VertexArray &vao,
                             uint32_t instances_count) {
    vao.bind();
    shader.bind();

    GL_CALL(glDrawElementsInstanced(GL_TRIANGLES, vao.ibo.indices_count,
                                    GL_UNSIGNED_INT, nullptr, instances_count));
}

} // namespace eng::Renderer
