#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "eng/containers/registry.hpp"
#include "eng/scene/components.hpp"

namespace eng {

struct Entity {
    template <typename T>
    T &add_component() {
        return owning_reg->add_component<T>(handle);
    }

    template <typename T>
    void remove_component() {
        owning_reg->remove_component<T>(handle);
    }

    template <>
    void remove_component<Name>() {
        assert(false && "Can't remove Name component");
    }

    template <>
    void remove_component<Transform>() {
        assert(false && "Can't remove Transform component");
    }

    template <typename T>
    [[nodiscard]] T &get_component() {
        return owning_reg->get_component<T>(handle);
    }

    template <typename T>
    [[nodiscard]] bool has_component() {
        return owning_reg->has_component<T>(handle);
    }

    ecs::EntityID handle;
    ecs::Registry *owning_reg = nullptr;
};

} // namespace eng

#endif
