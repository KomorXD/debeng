#ifndef REGISTRY_HPP
#define REGISTRY_HPP

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "typeless_vec.hpp"

namespace eng::ecs {

using EntityID = uint32_t;
using EntitySet = std::unordered_set<EntityID>;

using ArchetypeID = uint32_t;

using ComponentHash = size_t;

/*  Entity's type - a set of it's components. */
using Type = std::vector<ComponentHash>;

struct Archetype;

/*  Links archetypes via a graph for fast lokups in case of adding or removing
    components from an entity - so we immediately know what is new entity's
    archetype. Filled lazily. */
struct ArchetypeEdge {
    Archetype *add;
    Archetype *remove;
};

struct Archetype {
    ArchetypeID id;
    Type type;

    /*  Components data stored in type erased storage. */
    std::vector<cont::TypelessVector> components;

    /*  Mapping component's hash <=> index in COMPONENTS vector for fast
        lookup. */
    std::unordered_map<ComponentHash, size_t> column_index;

    /*  Links to proper archetypes when adding/removing component with a given
        hash. Loaded lazily whenever this archetype is a source for a new one.
     */
    std::unordered_map<ComponentHash, ArchetypeEdge> edges;
};

struct EntityRecord {
    Archetype *archetype;

    /*  Row in archetype's internal data vectors. */
    size_t row = 0;
};

struct ArchetypeRecord {
    Archetype *atype;

    /*  This column denotes which element in archetype's components vector
        is for the given type (tied in component_index). */
    size_t column = 0;
};

using ArchetypeMap = std::unordered_map<ArchetypeID, ArchetypeRecord>;

struct Registry;

using exclude_fn = std::vector<ComponentHash> (*)(void);
template <typename... Components>
[[nodiscard]] std::vector<ComponentHash> exclude() {
    std::vector<ComponentHash> comp_hashes;
    ((comp_hashes.push_back(typeid(Components).hash_code())), ...);

    return comp_hashes;
}

struct RegistryView {
    template <typename T>
    [[nodiscard]] T &get(EntityID ent);

    Registry *reg = nullptr;
    std::vector<EntityID> entities;
    std::unordered_set<ComponentHash> queried_types;
};

/*  Creates new archetype based on SOURCE that additionally has component
    of type T. If such archetype already exists, it will return it. */
template <typename T>
[[nodiscard]] Archetype *extended_archetype(Registry &reg, Archetype &source);

/*  Creates new archetype based on SOURCE that has component of type T
    removed. If such archetype already exists, it will return it. */
template <typename T>
[[nodiscard]] Archetype *trimmed_archetype(Registry &reg, Archetype &source);

/*  Extends entity by moving it into an archetype with extra component
    with component with it. */
void extend_entity(Registry &reg, Archetype &curr_atype, Archetype &next_atype,
                   EntityID entity_id);

/*  Trims entity by moving it into an archetype with less components. */
void trim_entity(Registry &reg, Archetype &curr_atype, Archetype &next_atype,
                 EntityID entity_id);

/*  ECS registry, responsible for managing entities and their archetypes. */
struct Registry {

    /*  Creates a registry with an empty type and empty archetype which
        should be present for every entity. */
    [[nodiscard]] static Registry create();

    void destroy();

    /*  Registers a new entity with an empty type. */
    [[nodiscard]] EntityID create_entity();

    void destroy_entity(EntityID entity_id);

    /*  Add component of type T to an entity of ENTITY_ID id. Entity must exist
        and it mustn't have component T already. */
    template <typename T>
    [[nodiscard]] T &add_component(EntityID entity_id) {
        auto ent_itr = entity_index.find(entity_id);
        assert(ent_itr != entity_index.end() && "No such entity registered");
        assert(!has_component<T>(entity_id) &&
               "Entity already has a component of type T");

        const size_t comp_hash = typeid(T).hash_code();
        EntityRecord &record = ent_itr->second;
        Archetype &atype = *record.archetype;

        Archetype *next_atype = atype.edges[comp_hash].add;
        if (!next_atype)
            next_atype = extended_archetype<T>(*this, atype);

        extend_entity(*this, atype, *next_atype, entity_id);

        size_t new_comp_vec_idx = next_atype->column_index.at(comp_hash);
        cont::TypelessVector &new_comp_vec =
            next_atype->components.at(new_comp_vec_idx);

        T &new_data = new_comp_vec.append<T>(T{});
        record.row = new_comp_vec.count - 1;

        return new_data;
    }

    /*  Remove component of type T from an entity of ENTITY_ID id. Entity must
        exist and it must have component T already. */
    template <typename T>
    void remove_component(EntityID entity_id) {
        auto ent_itr = entity_index.find(entity_id);
        assert(ent_itr != entity_index.end() && "No such entity registered");
        assert(has_component<T>(entity_id) &&
               "Entity doesn't have a component of type T");

        const size_t comp_hash = typeid(T).hash_code();
        EntityRecord &record = ent_itr->second;
        Archetype &atype = *record.archetype;

        Archetype *next_atype = atype.edges[comp_hash].remove;
        if (!next_atype)
            next_atype = trimmed_archetype<T>(*this, atype);

        trim_entity(*this, atype, *next_atype, entity_id);
    }

    /*  Check if entity of ENTITY_ID id has component of type T and return
        appropriate boolean. Entity must exist. */
    template <typename T>
    [[nodiscard]] bool has_component(EntityID entity_id) {
        auto ent_itr = entity_index.find(entity_id);
        assert(ent_itr != entity_index.end() && "No such entity registered");

        const size_t comp_hash = typeid(T).hash_code();
        Archetype &archetype = *ent_itr->second.archetype;

        auto amap_itr = component_index.find(comp_hash);
        if (amap_itr == component_index.end())
            return false;

        ArchetypeMap &amap = amap_itr->second;
        return amap.count(archetype.id) != 0;
    }

    /*  Return reference to component data of type T that belongs to entity
        of ENTITY_ID id. Entity must exist and it must have component T. */
    template <typename T>
    [[nodiscard]] T &get_component(EntityID entity_id) {
        auto ent_itr = entity_index.find(entity_id);
        assert(ent_itr != entity_index.end() && "No such entity registered");
        assert(has_component<T>(entity_id) &&
               "Entity doesn't have a component of type T");

        const size_t comp_hash = typeid(T).hash_code();
        EntityRecord &record = ent_itr->second;
        Archetype &atype = *record.archetype;

        auto amap_itr = component_index.find(comp_hash);
        assert(amap_itr != component_index.end() &&
               "No such component registered");

        ArchetypeMap &amap = amap_itr->second;
        assert(amap.count(atype.id) != 0);

        ArchetypeRecord &arecord = amap.at(atype.id);
        cont::TypelessVector &column = atype.components.at(arecord.column);

        return column.at<T>(record.row);
    }

    /*  Get wrapped array of entites who have all components specified in
        the template arguments.

        TODO: come up with a faster/better way of doing this. */
    template <typename... Components>
    [[nodiscard]] RegistryView view(exclude_fn excl_fn = exclude<>) {
        std::vector<ComponentHash> comp_hashes;
        ((comp_hashes.push_back(typeid(Components).hash_code())), ...);

        RegistryView rview;
        rview.reg = this;

        if (comp_hashes.empty())
            return rview;

        for (ComponentHash comp_hash : comp_hashes)
            rview.queried_types.insert(comp_hash);

        /*  Get first component's archetype map to access IDs of archetypes
            who have this component. */
        const size_t first_comp_hash = comp_hashes[0];
        ArchetypeMap &amap = component_index[first_comp_hash];

        std::vector<ComponentHash> excluded_comp_hashes = excl_fn();
        auto archetype_excluded = [&](ArchetypeID aid) {
            for (ComponentHash curr_hash : excluded_comp_hashes) {
                ArchetypeMap &curr_amap = component_index[curr_hash];

                if (curr_amap.contains(aid)) {
                    return true;
                }
            }

            return false;
        };

        auto archetype_shares_components = [&](ArchetypeID aid) {
            /*  To check if archetype has other components, we look at
                component index and query entries with relevant components -
                then we check if current archetype ID exists in its
                archetype map. If it doesn't we know this archetype will not
                do. */
            for (size_t i = 1; i < comp_hashes.size(); i++) {
                size_t curr_comp_hash = comp_hashes[i];
                ArchetypeMap &curr_amap = component_index[curr_comp_hash];

                if (!curr_amap.contains(aid)) {
                    return false;
                }
            }

            return true;
        };

        std::vector<ArchetypeID> relevant_aids;
        /*  Iterate over every archetype ID and check if it also has all the
            other components this call requires. */
        for (auto &[aid, arecord] : amap) {
            if (archetype_excluded(aid) || !archetype_shares_components(aid))
                continue;

            relevant_aids.push_back(aid);
        }

        for (ArchetypeID aid : relevant_aids) {
            EntitySet &eset = arch_entity_index[aid];
            for (EntityID eid : eset)
                rview.entities.push_back(eid);
        }

        std::sort(rview.entities.begin(), rview.entities.end());
        return rview;
    }

    /*  Entity index mapping entity ID to entity's record. */
    std::unordered_map<EntityID, EntityRecord> entity_index;

    /*  Archetype index, mapping type vector to archetype. Actual archetype
        is stored here. */
    std::map<Type, Archetype> archetype_index;

    /*  Archetype <=> entity index, mapping archetype ID to list of entities
        whose archetype matched the ID. */
    std::unordered_map<ArchetypeID, EntitySet> arch_entity_index;

    /*  Component index, maps component hash to an archetype map, effectivly
        mapping component to every archetype that has that component as part
        of its type. */
    std::unordered_map<ComponentHash, ArchetypeMap> component_index;

    EntityID entity_id_counter = 1;
    ArchetypeID arch_id_counter = 1;
};

template <typename T>
Archetype *extended_archetype(Registry &reg, Archetype &source) {
    Type new_type = source.type;
    size_t comp_hash = typeid(T).hash_code();

    assert(std::find(source.type.begin(), source.type.end(), comp_hash) ==
               source.type.end() &&
           "Source archetype already has that type.");

    /*  We insert new type entry so that types are sorted. */
    if (!new_type.empty()) {
        auto type_ub =
            std::upper_bound(new_type.begin(), new_type.end(), comp_hash);
        new_type.insert(type_ub, comp_hash);
    } else {
        new_type.push_back(comp_hash);
    }

    /*  If there already exists an archetype we're trying to create,
        return it. */
    auto duplicate = reg.archetype_index.find(new_type);
    if (duplicate != reg.archetype_index.end())
        return &duplicate->second;

    Archetype new_archetype;
    new_archetype.id = reg.arch_id_counter++;
    new_archetype.type = new_type;

    /*  Copy existing storage for components into the new archetype,
        since it is based on SOURCE. */
    for (size_t i = 0; i < source.components.size(); i++)
        new_archetype.components.push_back(source.components[i].clone_empty());

    /*  Same as type entry, we want actual data vectors to be sorted in
        the same way. */
    if (!new_archetype.components.empty()) {
        cont::TypelessVector dummy;
        dummy.type_hash = comp_hash;

        auto vec_ub = std::upper_bound(new_archetype.components.begin(),
                                       new_archetype.components.end(), dummy,
                                       [&](const cont::TypelessVector &lhs,
                                           const cont::TypelessVector &rhs) {
                                           return lhs.type_hash < rhs.type_hash;
                                       });
        new_archetype.components.insert(vec_ub,
                                        cont::TypelessVector::create<T>());
    } else {
        new_archetype.components.push_back(cont::TypelessVector::create<T>());
    }

    /*  Fill in column_index for fast access to proper data given
        component's hash. */
    for (size_t i = 0; i < new_archetype.components.size(); i++) {
        size_t curr_comp_hash = new_archetype.components[i].type_hash;
        new_archetype.column_index.insert(std::make_pair(curr_comp_hash, i));
    }

    reg.archetype_index.insert(std::make_pair(new_type, new_archetype));
    Archetype *inserted_atype = &reg.archetype_index.at(new_type);

    /*  Register this new archetype as one that posseses its components. */
    for (size_t i = 0; i < new_archetype.type.size(); i++) {
        size_t curr_partype_hash = new_archetype.type[i];
        size_t curr_partype_index =
            new_archetype.column_index.at(curr_partype_hash);

        ArchetypeRecord new_arecord;
        new_arecord.atype = inserted_atype;
        new_arecord.column = curr_partype_index;

        reg.component_index[curr_partype_hash].insert(
            std::make_pair(new_archetype.id, new_arecord));
    }

    /*  Link SOURCE and new archetype for fast lookups. */
    new_archetype.edges[comp_hash].remove = &source;
    source.edges[comp_hash].add = &reg.archetype_index[new_type];

    return &reg.archetype_index.at(new_type);
}

template <typename T>
Archetype *trimmed_archetype(Registry &reg, Archetype &source) {
    Type new_type = source.type;
    size_t comp_hash = typeid(T).hash_code();

    assert(std::find(source.type.begin(), source.type.end(), comp_hash) !=
               source.type.end() &&
           "Source archetype doesn't have that type.");

    /*  Get rid of type hash we're trimming away. */
    auto type_itr = std::find(new_type.begin(), new_type.end(), comp_hash);
    new_type.erase(type_itr);

    /*  If there already exists an archetype we're trying to create,
        return it. */
    auto existing_archetype = reg.archetype_index.find(new_type);
    if (existing_archetype != reg.archetype_index.end())
        return &existing_archetype->second;

    Archetype new_archetype;
    new_archetype.id = reg.arch_id_counter++;
    new_archetype.type = new_type;

    /*  Copy existing storage for components into the new archetype,
        since it is based on SOURCE. Don't copy data for the component
        we're trying to remove. */
    for (size_t i = 0; i < source.components.size(); i++) {
        cont::TypelessVector &vv = source.components[i];
        if (vv.type_hash == comp_hash)
            continue;

        new_archetype.components.push_back(source.components[i].clone_empty());
    }

    /*  Fill in column_index for fast access to proper data given
        component's hash. */
    for (size_t i = 0; i < new_archetype.components.size(); i++)
        new_archetype.column_index.insert(
            std::make_pair(new_archetype.components[i].type_hash, i));

    reg.archetype_index.insert(std::make_pair(new_type, new_archetype));
    Archetype *inserted_atype = &reg.archetype_index.at(new_type);

    /*  Register this new archetype as one that posseses its components. */
    for (size_t i = 0; i < new_archetype.type.size(); i++) {
        size_t curr_partype_hash = new_archetype.type[i];
        size_t curr_partype_index =
            new_archetype.column_index.at(curr_partype_hash);

        ArchetypeRecord new_arecord;
        new_arecord.atype = inserted_atype;
        new_arecord.column = curr_partype_index;

        reg.component_index[curr_partype_hash].insert(
            std::make_pair(new_archetype.id, new_arecord));
    }

    /*  Link SOURCE and new archetype for fast lookups. */
    new_archetype.edges[comp_hash].add = &source;
    source.edges[comp_hash].remove = &reg.archetype_index[new_type];

    return &reg.archetype_index[new_type];
}

template <typename T>
T &RegistryView::get(EntityID ent) {
    assert(reg != nullptr && "Incomplete view");

    const size_t comp_hash = typeid(T).hash_code();
    assert(queried_types.contains(comp_hash) &&
           "Trying to access component that wasn't queried");

    return reg->get_component<T>(ent);
}

} // namespace eng::ecs

#endif
