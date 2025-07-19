#ifndef VECTOR_WRAPPER_HPP
#define VECTOR_WRAPPER_HPP

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <typeinfo>
#include <vector>

namespace eng::cont {

template <typename T>
struct VectorWrapper;

/*  Generic version of VectorWrapper<T> class, made so we can store those in
    the same array, under the same type, even though implementations use various
    types.

    Virtual methods are exposed only to be able to use them without accesing
    underlying vector's type, which is useful when we don't know and don't care
    about this type. For type-aware operations (like pushing a value), use
    unerlying storage directly. */
struct GenericVectorWrapper {
    virtual ~GenericVectorWrapper() = default;

    virtual void clear() = 0;

    /*  Copies properties, without the data. */
    virtual GenericVectorWrapper *clone_empty() = 0;

    virtual void erase(size_t idx) = 0;

    virtual void pop_back() = 0;

    /*  Transfers element from this container to the other's back (hence no
        dst_idx). */
    [[nodiscard]] virtual size_t transfer_element(GenericVectorWrapper *other,
                                                  size_t src_idx) = 0;

    /*  Conv. method to convert to underlying vector. If types don't match, we
        assert. */
    template <typename T>
    VectorWrapper<T> &as_vec() {
        static_assert(
            std::is_base_of<GenericVectorWrapper, VectorWrapper<T>>::value,
            "Trying to cast to non-related type");

        VectorWrapper<T> *target_cont = (VectorWrapper<T> *)this;
        assert(typeid(T).hash_code() == target_cont->type_hash &&
               "Given container is of different type");

        return *target_cont;
    }

    /*  Used to avoid RTII. */
    size_t type_hash = 0x00;
};

/*  Literally just a wrapper around std::vector<T> + T's hash code. It exists
    only so we can have std::vector<T> functionality in a derived class - for
    context, see GenericVectorWrapper. */
template <typename T>
struct VectorWrapper : public GenericVectorWrapper {
    virtual void clear() {
        while (!storage.empty())
            storage.pop_back();
    }

    /*  Copies properties, without the data. */
    [[nodiscard]] virtual GenericVectorWrapper *clone_empty() {
        VectorWrapper<T> *new_vec = new VectorWrapper<T>;
        new_vec->type_hash = type_hash;

        return new_vec;
    }

    [[nodiscard]] static GenericVectorWrapper *create() {
        VectorWrapper<T> *new_vec = new VectorWrapper<T>;
        new_vec->type_hash = typeid(T).hash_code();

        return new_vec;
    }

    virtual void pop_back() { storage.pop_back(); }

    virtual void erase(size_t idx) { storage.erase(storage.begin() + idx); }

    /*  Transfers element from this container to the other's back (hence no
        dst_idx). */
    [[nodiscard]] virtual size_t transfer_element(GenericVectorWrapper *other,
                                                  size_t src_idx) {
        assert(other->type_hash == type_hash &&
               "Trying to move to container of different type");

        VectorWrapper<T> &dst_vec = other->as_vec<T>();
        dst_vec.storage.push_back(std::move(storage[src_idx]));
        erase(src_idx);

        return dst_vec.storage.size() - 1;
    }

    std::vector<T> storage;
};

} // namespace eng::cont

#endif
