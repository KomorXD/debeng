#ifndef PRIMITIVES_HPP
#define PRIMITIVES_HPP

#include <glm/glm.hpp>
#include <vector>

namespace eng {

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
    glm::vec2 texture_uv;
};

struct VertexData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

void calculate_tangents(Vertex &v0, Vertex &v1, Vertex &v2);

VertexData quad_vertex_data();
VertexData cube_vertex_data();

} // namespace eng

#endif
