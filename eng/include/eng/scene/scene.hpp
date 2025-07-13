#ifndef SCENE_HPP
#define SCENE_HPP

#include "eng/scene/entity.hpp"
#include <string>

namespace eng {

struct Scene {
    [[nodiscard]] static Scene create(const std::string &name);

    void destroy();

    [[nodiscard]] Entity spawn_entity(const std::string &name);

    void destroy_entity(Entity ent);

    std::string name;
    ecs::Registry registry;
    std::vector<Entity> entities;
};

} // namespace eng

#endif
