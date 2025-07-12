#include "eng/containers/registry.hpp"
#include "eng/containers/typeless_vec.hpp"

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
        for (cont::TypelessVector &vec : atype.components)
            vec.clear();

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

    for (cont::TypelessVector &vec : atype->components)
        vec.erase_raw(row);
}

void extend_entity(Registry &reg, Archetype &curr_atype, Archetype &next_atype,
                   EntityID entity_id) {
    assert(&curr_atype != &next_atype &&
           "Trying to move to the same archetype");

    auto ent_itr = reg.entity_index.find(entity_id);
    assert(ent_itr != reg.entity_index.end() && "No such entity registered");

    EntityRecord &curr_record = ent_itr->second;
    size_t curr_row = curr_record.row;

    /*  Copy entity's data from CURR_ATYPE to NEXT_ATYPE. */
    for (auto &[hash, index] : curr_atype.column_index) {
        cont::TypelessVector &curr_vec = curr_atype.components[index];
        uint8_t *source = curr_vec.at_raw(curr_row);

        size_t target_idx = next_atype.column_index.at(hash);
        cont::TypelessVector &next_vec = next_atype.components[target_idx];

        (void)next_vec.append_raw(source);
        curr_vec.erase_raw(curr_row);
    }

    curr_record.archetype = &next_atype;
    reg.arch_entity_index.at(curr_atype.id).erase(entity_id);
    reg.arch_entity_index[next_atype.id].insert(entity_id);
}

void trim_entity(Registry &reg, Archetype &curr_atype, Archetype &next_atype,
                 EntityID entity_id) {
    assert(&curr_atype != &next_atype &&
           "Trying to move to the same archetype");

    auto ent_itr = reg.entity_index.find(entity_id);
    assert(ent_itr != reg.entity_index.end() && "No such entity registered");

    EntityRecord &curr_record = ent_itr->second;
    size_t curr_row = curr_record.row;

    /*  Copy entity's data from CURR_ATYPE to NEXT_ATYPE. */
    for (auto &[hash, index] : next_atype.column_index) {
        size_t curr_idx = curr_atype.column_index.at(hash);
        cont::TypelessVector &curr_vec = curr_atype.components[curr_idx];
        uint8_t *source = curr_vec.at_raw(curr_row);

        size_t next_idx = next_atype.column_index.at(hash);
        cont::TypelessVector &next_vec = next_atype.components[next_idx];

        (void)next_vec.append_raw(source);
        curr_vec.erase_raw(curr_row);

        curr_record.row = next_vec.count - 1;
    }

    curr_record.archetype = &next_atype;
    reg.arch_entity_index.at(curr_atype.id).erase(entity_id);
    reg.arch_entity_index[next_atype.id].insert(entity_id);

    /*  We probably removed an element in the middle, adjust rows for
        other entities. */
    for (EntityID ent : reg.arch_entity_index[curr_atype.id]) {
        EntityRecord &record = reg.entity_index[ent];
        if (record.row > curr_row)
            record.row--;
    }
}

} // namespace eng::ecs
