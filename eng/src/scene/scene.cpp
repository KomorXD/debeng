#include "eng/scene/scene.hpp"
#include "eng/random_utils.hpp"
#include "eng/scene/components.hpp"

namespace eng {

Scene Scene::create(const std::string &name) {
    Scene scene;
    scene.registry = ecs::Registry::create();
    scene.name = name;

    return scene;
}

void Scene::destroy() {
    name.clear();
    registry.destroy();
    entities.clear();
}

Entity Scene::spawn_entity(const std::string &name) {
    Entity ent;
    ent.owning_reg = &registry;
    ent.handle = registry.create_entity();
    ent.add_component<Name>().name = name;
    ent.add_component<Transform>();
    ent.add_component<GlobalTransform>();

    entities.push_back(ent);

    return ent;
}

Entity Scene::duplicate(Entity &ent) {
    Entity new_ent;
    new_ent.owning_reg = &registry;
    new_ent.handle = registry.duplicate(ent.handle);

    entities.push_back(new_ent);

    return new_ent;
}

void Scene::destroy_entity(Entity &ent) {
    auto ent_itr = std::find_if(
        entities.begin(), entities.end(),
        [&](const Entity &entity) { return ent.handle == entity.handle; });
    assert(ent_itr != entities.end() && "Trying to remove non-existent entity");

    registry.destroy_entity(ent_itr->handle);
    entities.erase(ent_itr);
}

void Scene::update_global_transforms() {
    for (int32_t i = 0; i < entities.size(); i++) {
        Entity &ent = entities[i];
        Transform &t = ent.get_component<Transform>();
        GlobalTransform &gt = ent.get_component<GlobalTransform>();
        gt.position = t.position;
        gt.rotation = t.rotation;
        gt.scale = t.scale;

        if (ent.parent_index.has_value()) {
            Entity &parent = entities[ent.parent_index.value()];
            GlobalTransform &pgt = parent.get_component<GlobalTransform>();
            glm::mat4 new_t = pgt.to_mat4() * gt.to_mat4();
            transform_decompose(new_t, gt.position, gt.rotation, gt.scale);
        }
    }
}

} // namespace eng
