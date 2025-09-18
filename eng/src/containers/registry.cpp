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
    arch_entity_index[record.archetype->id].push_back(id);

    return id;
}

EntityID Registry::duplicate(EntityID entity_id) {
    auto ent_itr = entity_index.find(entity_id);
    assert(ent_itr != entity_index.end() &&
           "Trying to duplicate non-registered entity");

    EntityID id = create_entity();

    /*  If type is empty, there's nothing left to do. */
    if (ent_itr->second.archetype->type == Type{})
        return id;

    auto &[atype, row] = ent_itr->second;
    EntityRecord &new_record = entity_index.find(id)->second;
    Archetype &new_atype = *new_record.archetype;
    extend_entity<void>(*this, new_atype, *atype, id);

    for (auto &[hash, index] : atype->column_index) {
        cont::GenericVectorWrapper *cont = atype->components[index];
        new_record.row = cont->copy_element(cont, row);
    }

    return id;
}

void Registry::destroy_entity(EntityID entity_id) {
    auto ent_itr = entity_index.find(entity_id);
    assert(ent_itr != entity_index.end() &&
           "Trying to destroy non-registered entity");

    auto &[atype, row] = ent_itr->second;

    EntitySet &eset = arch_entity_index.at(atype->id);
    auto id_itr = std::find(eset.begin(), eset.end(), entity_id);
    eset.erase(id_itr);

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
