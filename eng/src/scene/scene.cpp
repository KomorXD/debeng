#include "eng/scene/scene.hpp"
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

    entities.push_back(ent);

    return ent;
}

void Scene::destroy_entity(Entity ent) {
    auto ent_itr = std::find_if(
        entities.begin(), entities.end(),
        [&](const Entity &entity) { return ent.handle == entity.handle; });
    assert(ent_itr != entities.end() && "Trying to remove non-existent entity");

    registry.destroy_entity(ent_itr->handle);
    entities.erase(ent_itr);
}

} // namespace eng
