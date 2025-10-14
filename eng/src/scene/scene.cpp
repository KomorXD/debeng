#include "eng/scene/scene.hpp"
#include "eng/random_utils.hpp"
#include "eng/scene/components.hpp"
#include <algorithm>

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
    id_to_index[ent.handle] = entities.size() - 1;

    return ent;
}

Entity Scene::duplicate(Entity &ent) {
    Entity new_ent;
    new_ent.owning_reg = &registry;
    new_ent.handle = registry.duplicate(ent.handle);

    entities.push_back(new_ent);
    id_to_index[ent.handle] = entities.size() - 1;

    return new_ent;
}

static void remove_relation(Scene &scene, ecs::EntityID parent_id,
                            ecs::EntityID child_id) {
    int32_t parent_idx = scene.id_to_index[parent_id];
    Entity &parent = scene.entities[parent_idx];

    auto child_itr = std::find(parent.children_ids.begin(),
                               parent.children_ids.end(), child_id);
    parent.children_ids.erase(child_itr);

    int32_t child_idx = scene.id_to_index[child_id];
    scene.entities[child_idx].parent_id = std::nullopt;
}

static void remove_entity_tree_records(Scene &scene, ecs::EntityID root_id) {
    int32_t root_idx = scene.id_to_index[root_id];
    Entity &root = scene.entities[root_idx];

    scene.id_to_index.erase(root_id);
    scene.registry.destroy_entity(root_id);
    for (ecs::EntityID child_id : root.children_ids)
        remove_entity_tree_records(scene, child_id);
}

static int32_t related_entities_count(Scene &scene, ecs::EntityID root_id) {
    assert(scene.id_to_index.contains(root_id) &&
           "Root does not exist in this scene");

    int32_t root_idx = scene.id_to_index[root_id];
    Entity &root = scene.entities[root_idx];

    int32_t count = 1; // Account for itself
    for (ecs::EntityID child_id : root.children_ids)
        count += related_entities_count(scene, child_id);

    return count;
}

void Scene::destroy_entity(ecs::EntityID ent_id) {
    assert(id_to_index.contains(ent_id) &&
           "Entity does not exist in this scene");

    int32_t ent_idx = id_to_index[ent_id];
    Entity &ent = entities[ent_idx];

    if (ent.parent_id.has_value())
        remove_relation(*this, ent.parent_id.value(), ent.handle);

    int32_t entities_removed = related_entities_count(*this, ent.handle);
    remove_entity_tree_records(*this, ent.handle);

    auto ent_it = entities.begin() + ent_idx;
    entities.erase(ent_it, ent_it + entities_removed);

    for (int32_t i = 0; i < entities.size(); i++) {
        Entity &ent = entities[i];
        id_to_index[ent.handle] = i;
    }
}

void Scene::link_relation(Entity &parent, Entity &child) {
    assert(parent.handle != child.handle && "Linking entity to itself");
    assert(id_to_index.contains(parent.handle) &&
           "Parent does not exist in this scene");
    assert(id_to_index.contains(child.handle) &&
           "Child does not exist in this scene");

    if (child.parent_id.value_or(-1) == parent.handle)
        return;

    int32_t parent_idx = id_to_index[parent.handle];
    Entity &real_parent = entities[parent_idx];

    int32_t old_child_idx = id_to_index[child.handle];
    Entity &real_child = entities[old_child_idx];

    int32_t new_child_idx = parent_idx + 1;

    real_parent.children_ids.push_back(real_child.handle);
    if (real_child.parent_id.has_value())
        remove_relation(*this, real_child.parent_id.value(), real_child.handle);

    real_child.parent_id = real_parent.handle;

    bool child_after_parent = (old_child_idx > parent_idx);
    int32_t entities_moved = related_entities_count(*this, real_child.handle);

    auto first_itr = entities.begin();
    auto middle_itr = entities.begin();
    auto last_itr = entities.begin();
    if (child_after_parent) {
        first_itr = entities.begin() + new_child_idx;
        middle_itr = entities.begin() + old_child_idx;
        last_itr = entities.begin() + old_child_idx + entities_moved;
    } else {
        first_itr = entities.begin() + old_child_idx;
        middle_itr = entities.begin() + old_child_idx + entities_moved;
        last_itr = entities.begin() + new_child_idx;
    }

    std::rotate(first_itr, middle_itr, last_itr);

    for (int32_t i = 0; i < entities.size(); i++) {
        Entity &ent = entities[i];
        id_to_index[ent.handle] = i;
    }

    parent = entities[id_to_index[real_parent.handle]];
    child = entities[id_to_index[real_child.handle]];
}

void Scene::update_global_transforms() {
    for (int32_t i = 0; i < entities.size(); i++) {
        Entity &ent = entities[i];
        Transform &t = ent.get_component<Transform>();
        GlobalTransform &gt = ent.get_component<GlobalTransform>();
        gt.position = t.position;
        gt.rotation = t.rotation;
        gt.scale = t.scale;

        if (ent.parent_id.has_value()) {
            int32_t parent_index = id_to_index[ent.parent_id.value()];
            Entity &parent = entities[parent_index];

            GlobalTransform &pgt = parent.get_component<GlobalTransform>();
            glm::mat4 new_t = pgt.to_mat4() * gt.to_mat4();
            transform_decompose(new_t, gt.position, gt.rotation, gt.scale);
        }
    }
}

} // namespace eng
