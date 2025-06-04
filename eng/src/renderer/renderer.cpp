#include "eng/renderer/renderer.hpp"
#include "eng/scene/assets.hpp"
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

    std::unordered_map<AssetID, std::vector<MeshInstance>> mesh_instances;
};

static Renderer s_renderer{};
static CameraData s_camera{};
static AssetPack s_default_pack{};

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
    s_renderer.default_shader.build("resources/shaders/vs.glsl",
                                    "resources/shaders/fs.glsl");

    s_default_pack = AssetPack::create("default");
    for (auto &[mesh_id, instances] : s_default_pack.meshes)
        s_renderer.mesh_instances[mesh_id].reserve(128);

    return true;
}

void shutdown() {
    s_default_pack.destroy();
    s_renderer.default_shader.destroy();
}

void scene_begin(const CameraData &camera) {
    s_camera = camera;

    for (auto &[mesh_id, instances] : s_renderer.mesh_instances)
        instances.clear();
}

void scene_end() {
    s_renderer.default_shader.bind();
    s_renderer.default_shader.set_uniform_mat4(
        "u_view_proj", s_camera.projection * s_camera.view);
    s_renderer.default_shader.set_uniform_1i("u_texture", 0);

    for (auto &[mesh_id, instances] : s_renderer.mesh_instances) {
        if (instances.empty())
            continue;

        Mesh &mesh = s_default_pack.meshes.at(mesh_id);
        mesh.vao.vbo_instanced.set_data(
            instances.data(), instances.size() * sizeof(MeshInstance));
        draw_indexed_instanced(s_renderer.default_shader, mesh.vao,
                               instances.size());
    }
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

void draw_indexed_instanced(const Shader &shader, const VertexArray &vao,
                            uint32_t count) {
    vao.bind();
    shader.bind();

    GL_CALL(glDrawElementsInstanced(GL_TRIANGLES, vao.ibo.indices_count,
                                    GL_UNSIGNED_INT, nullptr, count));
}

} // namespace eng::Renderer
