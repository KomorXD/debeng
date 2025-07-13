#ifndef ENTITY_HPP
#define ENTITY_HPP

#include "eng/containers/registry.hpp"
#include <string>

namespace eng {

struct Entity {
    template <typename T>
    [[nodiscard]] T &add_component() {
        return owning_reg->add_component<T>(handle);
    }

    template <typename T>
    void remove_component() {
        owning_reg->remove_component<T>(handle);
    }

    template <typename T>
    [[nodiscard]] T &get_component() {
        return owning_reg->get_component<T>(handle);
    }

    ecs::EntityID handle;
    ecs::Registry *owning_reg = nullptr;
    std::string name;
};

} // namespace eng

#endif
