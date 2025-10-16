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

static Entity &build_duplicate_children(Scene &scene, Entity &root) {
    Entity &new_ent = scene.entities.emplace_back();
    new_ent.owning_reg = &scene.registry;
    new_ent.handle = scene.registry.duplicate(root.handle);

    int32_t new_ent_idx = scene.entities.size() - 1;
    scene.id_to_index[new_ent.handle] = new_ent_idx;

    for (ecs::EntityID child_id : root.children_ids) {
        Entity dummy_child = scene.entities[scene.id_to_index[child_id]];
        Entity &new_child = build_duplicate_children(scene, dummy_child);
        new_ent = scene.entities[new_ent_idx];
        scene.link_relation(new_ent, new_child);
    }

    return scene.entities[new_ent_idx];
}

Entity &Scene::duplicate(Entity ent) {
    Entity &new_root = build_duplicate_children(*this, ent);
    ecs::EntityID new_root_id = new_root.handle;
    if (ent.parent_id.has_value()) {
        Entity &parent = entities[id_to_index[ent.parent_id.value()]];
        link_relation(parent, new_root);
    }

    return entities[id_to_index[new_root_id]];
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

void Scene::link_relation(Entity parent, Entity child) {
    assert(parent.handle != child.handle && "Linking entity to itself");
    assert(id_to_index.contains(parent.handle) &&
           "Parent does not exist in this scene");
    assert(id_to_index.contains(child.handle) &&
           "Child does not exist in this scene");

    if (child.parent_id.value_or(-1) == parent.handle)
        return;

    int32_t parent_idx = id_to_index[parent.handle];
    Entity &rparent = entities[parent_idx];

    int32_t child_idx = id_to_index[child.handle];
    Entity &rchild = entities[child_idx];

    if (rchild.parent_id.has_value())
        remove_relation(*this, rchild.parent_id.value(), rchild.handle);

    rparent.children_ids.insert(rparent.children_ids.begin(), rchild.handle);
    rchild.parent_id = rparent.handle;

    int32_t new_child_idx = parent_idx + 1;
    int32_t child_subtree_size = related_entities_count(*this, rchild.handle);

    int32_t first{};
    int32_t mid{};
    int32_t last{};
    if (parent_idx < child_idx) {
        first = new_child_idx;
        mid = child_idx;
        last = mid + child_subtree_size;
    } else {
        first = child_idx;
        mid = first + child_subtree_size;
        last = new_child_idx;
    }

    std::rotate(entities.begin() + first, entities.begin() + mid,
                entities.begin() + last);

    for (int32_t i = first; i < last; i++) {
        Entity &ent = entities[i];
        id_to_index[ent.handle] = i;
    }

    new_child_idx = id_to_index[child.handle];

    for (int32_t i = new_child_idx; i < new_child_idx + child_subtree_size; i++) {
        Entity &local_child = entities[i];
        assert(local_child.parent_id.has_value() &&
               "Child in a subtree should have a parent");

        int32_t local_parent_idx = id_to_index[local_child.parent_id.value()];
        Entity &local_parent = entities[local_parent_idx];

        GlobalTransform &pgt = local_parent.get_component<GlobalTransform>();
        glm::mat4 parent_inv = glm::inverse(pgt.to_mat4());

        GlobalTransform &cgt = local_child.get_component<GlobalTransform>();
        glm::mat4 adjusted_child_local = parent_inv * cgt.to_mat4();

        Transform &ct = local_child.get_component<Transform>();
        transform_decompose(adjusted_child_local, ct.position, ct.rotation,
                            ct.scale);
    }
}

bool Scene::is_ascendant_of(Entity &child, Entity &ascendant) {
    assert(id_to_index.contains(child.handle) &&
           "Child does not exist in this scene");
    assert(id_to_index.contains(ascendant.handle) &&
           "Ascendant does not exist in this scene");

    Entity ent = child;
    while (ent.parent_id.has_value()) {
        ecs::EntityID ent_p_id = ent.parent_id.value();
        if (ent_p_id == ascendant.handle)
            return true;

        ent = entities[id_to_index[ent_p_id]];
    }

    return false;
}

bool Scene::is_descendant_of(Entity &parent, Entity &descendant) {
    return is_ascendant_of(descendant, parent);
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
