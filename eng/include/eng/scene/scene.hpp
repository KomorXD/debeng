#ifndef SCENE_HPP
#define SCENE_HPP

#include "eng/scene/entity.hpp"
#include <string>

namespace eng {

struct Scene {
    [[nodiscard]] static Scene create(const std::string &name);

    void destroy();

    [[nodiscard]] Entity spawn_entity(const std::string &name);
    [[nodiscard]] Entity duplicate(Entity &ent);

    void destroy_entity(Entity &ent);

    void update_global_transforms();

    std::string name;
    ecs::Registry registry;
    std::vector<Entity> entities;

    std::map<ecs::EntityID, int32_t> id_to_index;
};

} // namespace eng

#endif
