#include "eng/renderer/primitives.hpp"
#include "glm/gtc/constants.hpp"

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

    for (int32_t i = 0; i < indices.size(); i += 6) {
        calculate_tangents(vertices[indices[i + 0]], vertices[indices[i + 1]],
                           vertices[indices[i + 2]]);
        calculate_tangents(vertices[indices[i + 3]], vertices[indices[i + 4]],
                           vertices[indices[i + 5]]);
    }

    return { vertices, indices };
}

VertexData uv_sphere_vertex_data() {
    constexpr float RADIUS = 0.5f;
    constexpr int32_t slices = 48;
    constexpr int32_t stacks = 48;

    std::vector<Vertex> vertices;
    for (int32_t stack = 0; stack <= stacks; stack++) {
        float phi = glm::pi<float>() * (float)stack / stacks;
        float sin_phi = glm::sin(phi);
        float cos_phi = glm::cos(phi);

        for (int32_t slice = 0; slice <= slices; slice++) {
            float theta = 2.0f * glm::pi<float>() * (float)slice / slices;
            float sin_theta = glm::sin(theta);
            float cos_theta = glm::cos(theta);

            Vertex vertex{};
            vertex.position = vertex.normal =
                RADIUS *
                glm::vec3(cos_theta * sin_phi, cos_phi, sin_theta * sin_phi);
            vertex.tangent =
                glm::normalize(glm::vec3(-sin_theta, 0.0f, cos_theta));
            vertex.bitangent =
                glm::normalize(glm::cross(vertex.normal, vertex.tangent));
            vertex.texture_uv = {(float)slice / slices,
                                 1.0f - (float)stack / stacks};
            vertices.push_back(vertex);
        }
    }

    std::vector<uint32_t> indices;
    for (int32_t stack = 0; stack < stacks; stack++) {
        for (int32_t slice = 0; slice < slices; slice++) {
            int32_t next_slice = slice + 1;
            int32_t next_stack = (stack + 1) % (stacks + 1);

            indices.push_back(next_stack * (slices + 1) + next_slice);
            indices.push_back(next_stack * (slices + 1) + slice);
            indices.push_back(stack * (slices + 1) + slice);

            indices.push_back(stack * (slices + 1) + next_slice);
            indices.push_back(next_stack * (slices + 1) + next_slice);
            indices.push_back(stack * (slices + 1) + slice);
        }
    }

    return {vertices, indices};
}

} // namespace eng
