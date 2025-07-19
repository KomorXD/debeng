#include "eng/containers/registry.hpp"

namespace eng::ecs {

Registry Registry::create() {
    Registry reg;

    Type empty_type;
    Archetype empty_archetype;
    empty_archetype.id = reg.arch_id_counter++;
    empty_archetype.type = empty_type;

    reg.archetype_index.insert(std::make_pair(empty_type, empty_archetype));
    return reg;
}

void Registry::destroy() {
    entity_index.clear();
    component_index.clear();

    for (auto &[type, atype] : archetype_index) {
        for (cont::GenericVectorWrapper *cont : atype.components)
            delete cont;

        atype.column_index.clear();
    }

    archetype_index.clear();

    entity_id_counter = 1;
    arch_id_counter = 1;
}

EntityID Registry::create_entity() {
    EntityID id = entity_id_counter++;
    EntityRecord record;

    Type empty_type;
    record.archetype = &archetype_index.at(empty_type);

    entity_index.insert(std::make_pair(id, record));
    arch_entity_index[record.archetype->id].insert(id);

    return id;
}

void Registry::destroy_entity(EntityID entity_id) {
    auto ent_itr = entity_index.find(entity_id);
    assert(ent_itr != entity_index.end() &&
           "Trying to destroy non-registered entity");

    auto &[atype, row] = ent_itr->second;

    EntitySet &eset = arch_entity_index.at(atype->id);
    eset.erase(entity_id);

    for (EntityID ent : eset) {
        EntityRecord &record = entity_index[ent];
        if (record.row > row)
            record.row--;
    }

    for (cont::GenericVectorWrapper *cont : atype->components) {
        cont->erase(row);
    }
}

} // namespace eng::ecs
