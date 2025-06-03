#include "eng/renderer/primitives.hpp"

namespace eng {

void calculate_tangents(Vertex &v0, Vertex &v1, Vertex &v2) {
    glm::vec3 edge1 = v1.position - v0.position;
    glm::vec3 edge2 = v2.position - v0.position;

    glm::vec2 d_uv1 = v1.texture_uv - v0.texture_uv;
    glm::vec2 d_uv2 = v2.texture_uv - v0.texture_uv;

    float f = 1.0f / (d_uv1.x * d_uv2.y - d_uv2.x * d_uv1.y);
    v0.tangent = f * (d_uv2.y * edge1 - d_uv1.y * edge2);
    v0.bitangent = f * (d_uv2.x * edge1 - d_uv1.x * edge2);

    glm::vec3 normal = glm::normalize(v0.normal);
    v0.tangent =
        glm::normalize(v0.tangent - normal * glm::dot(normal, v0.tangent));
    v0.bitangent =
        glm::normalize(v0.bitangent - normal * glm::dot(normal, v0.bitangent));

    v2.tangent = v1.tangent = v0.tangent;
    v2.bitangent = v1.bitangent = v0.bitangent;
}

VertexData quad_vertex_data() {
    std::vector<Vertex> vertices = {
        {{  0.5f, -0.5f,  0.0f }, {  0.0f,  0.0f, -1.0f }, {}, {}, { 0.0f, 0.0f }}, // Bottom-right
        {{ -0.5f, -0.5f,  0.0f }, {  0.0f,  0.0f, -1.0f }, {}, {}, { 1.0f, 0.0f }}, // Bottom-left
        {{ -0.5f,  0.5f,  0.0f }, {  0.0f,  0.0f, -1.0f }, {}, {}, { 1.0f, 1.0f }}, // Top-left
        {{  0.5f,  0.5f,  0.0f }, {  0.0f,  0.0f, -1.0f }, {}, {}, { 0.0f, 1.0f }}, // Top-right
    };

    std::vector<uint32_t> indices = {
        0, 1, 2,
        2, 3, 0
    };

    calculate_tangents(vertices[0], vertices[1], vertices[2]);
    calculate_tangents(vertices[0], vertices[2], vertices[3]);

    return { vertices, indices };
}

VertexData cube_vertex_data() {
    std::vector<Vertex> vertices = {
        // Front
        {{ -0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, {}, {}, { 0.0f, 0.0f }}, // Bottom-left
        {{  0.5f, -0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, {}, {}, { 1.0f, 0.0f }}, // Bottom-right
        {{  0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, {}, {}, { 1.0f, 1.0f }}, // Top-right
        {{ -0.5f,  0.5f,  0.5f }, {  0.0f,  0.0f,  1.0f }, {}, {}, { 0.0f, 1.0f }}, // Top-left

        // Back
        {{  0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, {}, {}, { 0.0f, 0.0f }}, // Bottom-right
        {{ -0.5f, -0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, {}, {}, { 1.0f, 0.0f }}, // Bottom-left
        {{ -0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, {}, {}, { 1.0f, 1.0f }}, // Top-left
        {{  0.5f,  0.5f, -0.5f }, {  0.0f,  0.0f, -1.0f }, {}, {}, { 0.0f, 1.0f }}, // Top-right

        // Top
        {{ -0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, {}, {}, { 0.0f, 0.0f }}, // Bottom-left
        {{  0.5f,  0.5f,  0.5f }, {  0.0f,  1.0f,  0.0f }, {}, {}, { 1.0f, 0.0f }}, // Bottomm-right
        {{  0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, {}, {}, { 1.0f, 1.0f }}, // Top-right
        {{ -0.5f,  0.5f, -0.5f }, {  0.0f,  1.0f,  0.0f }, {}, {}, { 0.0f, 1.0f }}, // Top-left

        // Bottom
        {{  0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, {}, {}, { 1.0f, 0.0f }}, // Bottomm-right
        {{ -0.5f, -0.5f,  0.5f }, {  0.0f, -1.0f,  0.0f }, {}, {}, { 0.0f, 0.0f }}, // Bottom-left
        {{ -0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, {}, {}, { 0.0f, 1.0f }}, // Top-left
        {{  0.5f, -0.5f, -0.5f }, {  0.0f, -1.0f,  0.0f }, {}, {}, { 1.0f, 1.0f }}, // Top-right

        // Left
        {{ -0.5f, -0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, {}, {}, { 0.0f, 0.0f }}, // Bottom-left
        {{ -0.5f, -0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, {}, {}, { 1.0f, 0.0f }}, // Bottom-right
        {{ -0.5f,  0.5f,  0.5f }, { -1.0f,  0.0f,  0.0f }, {}, {}, { 1.0f, 1.0f }}, // Top-right
        {{ -0.5f,  0.5f, -0.5f }, { -1.0f,  0.0f,  0.0f }, {}, {}, { 0.0f, 1.0f }}, // Top-left

        // Right
        {{  0.5f, -0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, {}, {}, { 1.0f, 0.0f }}, // Bottom-right
        {{  0.5f, -0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, {}, {}, { 0.0f, 0.0f }}, // Bottom-left
        {{  0.5f,  0.5f, -0.5f }, {  1.0f,  0.0f,  0.0f }, {}, {}, { 0.0f, 1.0f }}, // Top-left
        {{  0.5f,  0.5f,  0.5f }, {  1.0f,  0.0f,  0.0f }, {}, {}, { 1.0f, 1.0f }}  // Top-right
    };

    std::vector<uint32_t> indices =
    {
        // Front
        0, 1, 2, 2, 3, 0,
        // Back
        4, 5, 6, 6, 7, 4,
        // Top
        8, 9, 10, 10, 11, 8,
        // Bottom
        12, 13, 14, 14, 15, 12,
        // Left
        16, 17, 18, 18, 19, 16,
        // Right
        20, 21, 22, 22, 23, 20
    };

    for (size_t i = 0; i < indices.size(); i += 6) {
        calculate_tangents(vertices[indices[i + 0]], vertices[indices[i + 1]],
                           vertices[indices[i + 2]]);
        calculate_tangents(vertices[indices[i + 3]], vertices[indices[i + 4]],
                           vertices[indices[i + 5]]);
    }

    return { vertices, indices };
}

} // namespace eng
