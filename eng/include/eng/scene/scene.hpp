#ifndef SCENE_HPP
#define SCENE_HPP

#include "eng/scene/entity.hpp"
#include <string>

namespace eng {

struct Scene {
    [[nodiscard]] static Scene create(const std::string &name);

    void destroy();

    [[nodiscard]] Entity spawn_entity(const std::string &name);
    [[nodiscard]] Entity &duplicate(Entity ent);

    void destroy_entity(ecs::EntityID ent_id);

    void link_relation(Entity parent, Entity child);

    bool is_ascendant_of(Entity &child, Entity &ascendant);
    bool is_descendant_of(Entity &parent, Entity &descendant);

    void update_global_transforms();

    std::string name;
    ecs::Registry registry;

    /* Entities are sorted in a hierarchical order - in other words,
     * they are ordered in a way that we would encounter them if they were
     * stored in a tree and we traversed it by depth.
     * The reason to make it a vector instead of a tree is to optimize the most
     * common use case: iterating over all of them (for UI rendering and
     * updating global transforms). That way we iterate in hierarchical order
     * and we get cache benefits. The price for that is more expensive
     * hierarchy-changing operations (like changing parent), but it's not done
     * every frame. */
    std::vector<Entity> entities;

    /* For fast entity access. Be aware that changing the hierarchy might
     * change indices, so you might need to fetch it again. */
    std::map<ecs::EntityID, int32_t> id_to_index;
};

} // namespace eng

#endif
