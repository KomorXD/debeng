#include "eng/renderer/renderer.hpp"
#include "eng/renderer/opengl.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "GLFW/glfw3.h"
#include <vector>

namespace eng::renderer {

struct Mesh {
    VertexArray vao;
};

struct MeshInstance {
    glm::mat4 transform;
};

struct GPU {
    GLint texture_units = 0;
};

struct Renderer {
    GPU gpu;

    Mesh quad_mesh;
    std::vector<MeshInstance> mesh_instances;

    Shader default_shader;
};

static Renderer s_renderer{};
static CameraData s_camera{};

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
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        return false;
    }

    GL_CALL(glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS,
                          &s_renderer.gpu.texture_units));

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

    {
	s_renderer.quad_mesh.vao = VertexArray::create();
	s_renderer.quad_mesh.vao.bind();

	float vertices[] = {
	    -0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
	     0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
	     0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
	    -0.5f,  0.5f, 0.0f, 0.0f, 1.0f
	};
	VertexBuffer vbo = VertexBuffer::create();
	vbo.allocate(vertices, 5 * 4 * sizeof(float), 5 * 4);

	uint32_t indices[] = {
	    0, 1, 2,
	    2, 3, 0
	};
	IndexBuffer ibo = IndexBuffer::create();
	ibo.allocate(indices, 3 * 2);

	VertexBufferLayout layout;
	layout.push_float(3);
	layout.push_float(2);
	s_renderer.quad_mesh.vao.add_buffers(vbo, ibo, layout);

	layout.clear();
	layout.push_float(4);
	layout.push_float(4);
	layout.push_float(4);
	layout.push_float(4);

	VertexBuffer ivbo = VertexBuffer::create();
	ivbo.allocate(nullptr, 4 * 4 * 16 * sizeof(float));
	s_renderer.quad_mesh.vao.add_instanced_vertex_buffer(ivbo, layout, 2);
	s_renderer.quad_mesh.vao.unbind();
    }

    // TODO: setup stuff when it comes

    return true;
}

void shutdown() {
    // TODO: clean stuff when it comes
}

void scene_begin(const CameraData &camera) {
    s_camera = camera;

    s_renderer.mesh_instances.clear();
}

void scene_end() {
    s_renderer.default_shader.bind();
    s_renderer.default_shader.set_uniform_mat4(
        "u_view_proj", s_camera.projection * s_camera.view);
    s_renderer.default_shader.set_uniform_1i("u_texture", 0);

    s_renderer.quad_mesh.vao.vbo_instanced.set_data(
        s_renderer.mesh_instances.data(),
        s_renderer.mesh_instances.size() * sizeof(MeshInstance));
    draw_indexed_instanced(s_renderer.default_shader, s_renderer.quad_mesh.vao,
                           s_renderer.mesh_instances.size());
}

void submit_quad(const glm::vec3 &position) {
    MeshInstance &ins = s_renderer.mesh_instances.emplace_back();
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
