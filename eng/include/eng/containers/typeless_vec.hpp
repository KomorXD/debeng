#ifndef VOIDVEC_HPP
#define VOIDVEC_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <typeindex>

namespace eng::cont {

/*  Typeless vector, used for storing data of arbitrary type. Methods can have
    templated and raw versions, where templated ones require that provided type
    is the same as the one used in create() call. They're there for convenience.
    Note that this container does not respect RAII, so data members' destructors
    won't be called. */
struct TypelessVector {
    /*  Return new vector created with given type T's properties. Future calls
        will rely on T's size, unless using templated versions (where it will
        have to match T used here). */
    template <typename T>
    [[nodiscard]] static TypelessVector create() {
        TypelessVector vv;
        vv.type_hash = typeid(T).hash_code();
        vv.element_size = sizeof(T);
        vv.reallocate(vv.capacity);

        return vv;
    }

    void clear() {
        reallocate(2);
        count = 0;
    }

    /* Return new vector with the same meta-data. The new vector holds no
       data. */
    [[nodiscard]] TypelessVector clone_empty() {
        TypelessVector clone;
        clone.type_hash = type_hash;
        clone.element_size = element_size;
        clone.reallocate(clone.capacity);

        return clone;
    }

    void reallocate(size_t new_capacity) {
        new_capacity = new_capacity < 2 ? 2 : new_capacity;
        storage = (uint8_t *)realloc(storage, new_capacity * element_size);
        capacity = new_capacity;
    }

    /*  Append to the end of array. Provided type T must be the same as T used
        in create() call. Return reference to appended element. */
    template <typename T>
    [[nodiscard]] T &append(const T &value) {
        assert(type_hash == typeid(T).hash_code() &&
               "Provided type T does not match assigned type used in create() "
               "call.");

        if (count >= capacity)
            reallocate(capacity * 2);

        T *data = (T *)storage;
        data[count] = value;
        count++;

        return data[count - 1];
    }

    /*  Append raw bytes to the end of array. Number of bytes copied is the same
        as size of type T used in create() call. Return pointer to appended
        bytes. */
    [[nodiscard]] uint8_t *append_raw(void *value_bytes) {
        if (count >= capacity)
            reallocate(capacity * 2);

        memcpy(storage + (count * element_size), value_bytes, element_size);
        count++;

        return &storage[(count - 1) * element_size];
    }

    /*  Insert data before idx-th element. Provided type T must be the same as T
        used in create() call. Return reference to inserted element. */
    template <typename T>
    [[nodiscard]] T &insert(const T &value, size_t idx) {
        assert(type_hash == typeid(T).hash_code() &&
               "Provided type T does not match assigned type used in create() "
               "call.");

        if (count >= capacity)
            reallocate(capacity * 2);

        T *data = (T *)storage;
        for (int32_t i = count; i > idx; i--)
            data[i] = data[i - 1];

        data[idx] = value;
        count++;

        return data[idx];
    }

    /*  Insert raw bytes before idx-th element. Number of bytes copied is the
        same as size of type T used in create() call. Return pointer to inserted
        bytes. */
    [[nodiscard]] uint8_t *insert_raw(void *value_bytes, size_t idx) {
        assert(idx <= count && "Access out of bounds");

        if (count >= capacity)
            reallocate(capacity * 2);

        if (idx == count)
            return append_raw(value_bytes);

        count++;
        memcpy(storage + (idx + 1) * element_size, storage + idx * element_size,
               (count - idx) * element_size);

        return (uint8_t *)memcpy(storage + (idx * element_size), value_bytes,
                                 element_size);
    }

    /*  Pop last element, where last element's size must be the same as size
        size of type T used in create() call. */
    void pop() {
        assert(count > 0 && "Trying to pop an empty storage");

        count--;
        if (count <= capacity / 2)
            reallocate(count);
    }

    /*  Erase element at index IDX. Type T must be the same as the one used in
        create() call. */
    template <typename T>
    void erase(size_t idx) {
        assert(type_hash == typeid(T).hash_code() &&
               "Provided type T does not match assigned type used in create() "
               "call.");
        assert(idx < count && "Access out of bounds");

        T *data = (T *)storage;
        for (int32_t i = idx; i < count; i++)
            data[i] = data[i + 1];

        count--;
        if (count <= capacity / 2)
            reallocate(count);
    }

    /*  Erase element at index IDX. Number of bytes erased is the same as size
        of type T used in create() call. */
    void erase_raw(size_t idx) {
        assert(idx < count && "Access out of bounds");

        if (idx == count - 1) {
            pop();
            return;
        }

        count--;
        memcpy(storage + idx * element_size, storage + (idx + 1) * element_size,
               (count - idx) * element_size);

        if (count <= capacity / 2)
            reallocate(count);
    }

    /*  Return a reference to element at IDX. Type T must be the same as the one
        used in create() call. */
    template <typename T>
    [[nodiscard]] T &at(size_t idx) {
        assert(type_hash == typeid(T).hash_code() &&
               "Provided type T does not match assigned type used in create() "
               "call.");
        assert(idx < count && "Access out of bounds");

        T *data = (T *)storage;
        return data[idx];
    }

    /*  Return a pointer to element at IDX. Actual index is going to be
        IDX * <size of type T used in create() call>. */
    [[nodiscard]] uint8_t *at_raw(size_t idx) {
        assert(idx < count && "Access out of bounds");

        return &storage[idx * element_size];
    }

    uint8_t *storage = nullptr;
    size_t count = 0;
    size_t capacity = 2;

    size_t element_size = 0;
    size_t type_hash = 0x00;
};

} // namespace eng::cont

#endif
