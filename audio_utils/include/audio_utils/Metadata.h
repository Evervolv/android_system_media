/*
 * Copyright 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_AUDIO_METADATA_H
#define ANDROID_AUDIO_METADATA_H

#ifdef __cplusplus

#include <any>
#include <map>
#include <string>
#include <vector>

/**
 * Audio Metadata: a C++ Object based map.
 *
 * Data is a map of strings to Datum Objects.
 *
 * Datum is a C++ "Object", a direct instance of std::any, but limited
 * to only the following allowed types:
 *
 * Native                              Java equivalent
 * int32                               (int)
 * int64                               (long)
 * float                               (float)
 * double                              (double)
 * std::string                         (String)
 * Data (std::map<std::string, Datum>) (Map<String, Object>)
 *
 * TEST ONLY std::vector<Datum>        (Object[])
 * TEST ONLY std::pair<Datum, Datum>   (Pair<Object, Object>)
 *
 * As can be seen, std::map, std::vector, std::pair are
 * recursive types on Datum.
 *
 * The Data map accepts typed Keys, which designate the type T of the
 * value associated with the Key<T> in the template parameter.
 *
 * CKey<T> is the constexpr version suitable for fixed compile-time constants.
 * Key<T> is the non-constexpr version.
 */

namespace android::audio_utils::metadata {

// Set up type comparison system
// see std::variant for the how the type_index() may be used.

/*
 * returns the index of type T in the type parameter list Ts.
 */
template <typename T, typename... Ts>
inline constexpr ssize_t type_index() {
    constexpr bool checks[] = {std::is_same_v<std::decay_t<T>, std::decay_t<Ts>>...};
    for (size_t i = 0; i < sizeof...(Ts); ++i) {
        if (checks[i]) return i; // the index in Ts.
    }
    return -1; // none found.
}

// compound_type is a holder of types. There are concatenation tricks of type lists
// but we don't need them here.
template <typename... Ts>
struct compound_type {
    inline static constexpr size_t size_v = sizeof...(Ts);
    template <typename T>
    inline static constexpr bool contains_v = type_index<T, Ts...>() >= 0;
    template <typename T>
    inline static constexpr ssize_t index_of() { return type_index<T, Ts...>(); }

    /**
     * Applies function f to a datum pointer a.
     *
     * \param f is the function to apply.  It should take one argument,
     *     which is a (typed) pointer to the value stored in a.
     * \param a is the Datum object (derived from std::any).
     * \param result if non-null stores the return value of f (if f has a return value).
     *     result may be nullptr if one does not care about the return value of f.
     * \return true on success, false if there is no applicable data stored in a.
     */
    template <typename F, typename A>
    static bool apply(F f, A *a, std::any *result = nullptr) {
        return apply_impl<F, A, Ts...>(f, a, result);
    }

    // helper
    // Linear search in the number of types because of non-cached std:any_cast
    // lookup.  See std::visit for std::variant for constant time implementation.
    template <typename F, typename A, typename T, typename... Ts2>
    static bool apply_impl(F f, A *a, std::any *result) {
        auto t = std::any_cast<T>(a); // may be const ptr or not.
        if (t == nullptr) {
            return apply_impl<F, A, Ts2...>(f, a, result);
        }

        // only save result if the function has a non-void return type.
        // and result is not nullptr.
        using result_type = std::invoke_result_t<F, T*>;
        if constexpr (!std::is_same_v<result_type, void>) {
            if (result != nullptr) {
                *result = (result_type)f(t);
                return true;
            }
        }

        f(t); // discard the result
        return true;
    }

    // helper base class
    template <typename F, typename A>
    static bool apply_impl(F f __unused, A *a __unused, std::any *result __unused) {
        return false;
    }
};

#ifdef METADATA_TESTING

// This is a helper struct to verify that we are moving Datums instead
// of copying them.
struct MoveCount {
    int32_t mMoveCount = 0;
    int32_t mCopyCount = 0;

    MoveCount() = default;
    MoveCount(MoveCount&& other) {
        mMoveCount = other.mMoveCount + 1;
        mCopyCount = other.mCopyCount;
    }
    MoveCount(const MoveCount& other) {
        mMoveCount = other.mMoveCount;
        mCopyCount = other.mCopyCount + 1;
    }
    MoveCount &operator=(MoveCount&& other) {
        mMoveCount = other.mMoveCount + 1;
        mCopyCount = other.mCopyCount;
        return *this;
    }
    MoveCount &operator=(const MoveCount& other) {
        mMoveCount = other.mMoveCount;
        mCopyCount = other.mCopyCount + 1;
        return *this;
    }
};
#endif

class Data;
class Datum;

// The order of this list must be maintained for binary compatibility
using metadata_types = compound_type<
    int32_t,
    int64_t,
    float,
    double,
    std::string,
    Data /* std::map<std::string, Datum> */
    // OK to add at end.
#ifdef METADATA_TESTING
    , std::vector<Datum>      // another complex object for testing
    , std::pair<Datum, Datum> // another complex object for testing
    , MoveCount
#endif
    >;

// A subset of the metadata types may be directly copied.
using primitive_metadata_types = compound_type<int32_t, int64_t, float, double
#ifdef METADATA_TESTING
    , MoveCount
#endif
    >;

template <typename T>
inline constexpr bool is_primitive_metadata_type_v =
    primitive_metadata_types::contains_v<T>;

template <typename T>
inline constexpr bool is_metadata_type_v =
    metadata_types::contains_v<T>;

/**
 * Datum is the C++ version of Object, based on std::any
 * to be portable to other Data Object systems on std::any. For C++, there
 * are two forms of generalized Objects, std::variant and std::any.
 *
 * What is a variant?
 * std::variant is like a std::pair<type_index, union>, where the types
 * are kept in the template parameter list, and you only need to store
 * the type_index of the current value's type in the template parameter
 * list to find the value's type (to access data in the union).
 *
 * What is an any?
 * std::any is a std::pair<type_func, pointer> (though the standard encourages
 * small buffer optimization of the pointer for small data types,
 * so the pointer might actually be data).  The type_func is cleverly
 * implemented templated, so that one type_func exists per type.
 *
 * For datum, we use std::any, which is different than mediametrics::Item
 * (which uses std::variant).
 *
 * std::any is the C++ version of Java's Object.  One benefit of std::any
 * over std::variant is that it is portable outside of this package as a
 * std::any, to another C++ Object system based on std::any
 * (as we any_cast to discover the type). std::variant does not have this
 * portability (without copy conversion) because it requires an explicit
 * type list to be known in the template, so you can't exchange them freely
 * as the union size and the type/type ordering will be different in general
 * between two variant-based Object systems.
 *
 * std::any may work better with some recursive types than variants,
 * as it uses pointers so that physical size need not be known for type
 * definition.
 *
 * This is a design choice: mediametrics::Item as a closed system,
 * metadata::Datum as an open system.
 *
 * CAUTION:
 * For efficiency, prefer the use of std::any_cast<T>(std::any *)
 *       which returns a pointer to T (no extra copies.)
 *
 * std::any_cast<T>(std::any) returns an instance of T (copy constructor).
 * std::get<N>(std::variant) returns a reference (no extra copies).
 *
 * The Data map operations are optimized to return references to
 * avoid unnecessary copies.
 */

class Datum : public std::any {
public:
    // Don't add any virtual functions or non-static member variables.

    Datum() = default;

    // Do not make these explicit
    // Force type of std::any to exactly the values we permit to be parceled.
    template <typename T, typename = std::enable_if_t<is_metadata_type_v<T>>>
    Datum(T && t) : std::any(std::forward<T>(t)) {};

    template <typename T, typename = std::enable_if_t<is_metadata_type_v<T>>>
    Datum& operator=(T&& t) {
        static_cast<std::any *>(this)->operator=(std::forward<T>(t));
    }

    Datum(const char *t) : std::any(std::string(t)) {}; // special string handling
};

// PREVENT INCORRECT MODIFICATIONS
// Datum is a helping wrapper on std::any
// Don't add any non-static members
static_assert(sizeof(Datum) == sizeof(std::any));
// Nothing is virtual
static_assert(!std::is_polymorphic_v<Datum>);

/**
 * Keys
 *
 * Audio Metadata keys are typed.  Similar to variant's template typenames,
 * which directly indicate possible types in the union, the Audio Metadata
 * Keys contain the Value's Type in the Key's template type parameter.
 *
 * Example:
 *
 * inline constexpr CKey<int64_t> MY_BIGINT("bigint_is_mine");
 * inline constexpr CKey<Data> TABLE("table");
 *
 * Thus if we have a Data object d:
 *
 * decltype(d[TABLE]) is Data
 * decltype(d[MY_BIGINT) is int64_t
 */

/**
 * Key is a non-constexpr key which has local storage in a string.
 */
template <typename T, typename = std::enable_if_t<is_metadata_type_v<T>>>
class Key : private std::string {
public:
    using std::string::string; // base constructor
    const char *getName() const { return c_str(); }
};

/**
 * CKey is a constexpr key, which is preferred.
 *
 * inline constexpr CKey<int64_t> MY_BIGINT("bigint_is_mine");
 */
template <typename T, typename = std::enable_if_t<is_metadata_type_v<T>>>
class CKey  {
    const char * const mName;
public:
    explicit constexpr CKey(const char *name) : mName(name) {}
    CKey(const Key<T> &key) : mName(key.getName()) {}
    const char *getName() const { return mName; }
};

/**
 * Data is the storage for our Datums.
 *
 * It is implemented on top of std::map<std::string, Datum>
 * but we augment it with typed Key
 * getters and setters, as well as operator[] overloads.
 */
class Data : public std::map<std::string, Datum> {
public:
    // Don't add any virtual functions or non-static member variables.

    // We supplement the raw form of map with
    // the following typed form using Key.

    // Intentionally there is no get(), we suggest *get_ptr()
    template <template <typename /* T */, typename... /* enable-ifs */> class K, typename T>
    T* get_ptr(const K<T>& key, bool allocate = false) {
        auto it = find(key.getName());
        if (it == this->end()) {
            if (!allocate) return nullptr;
            it = emplace(key.getName(), T{}).first;
        }
        return std::any_cast<T>(&it->second);
    }

    template <template <typename, typename...> class K, typename T>
    const T* get_ptr(const K<T>& key) const {
        auto it = find(key.getName());
        if (it == this->end()) return nullptr;
        return std::any_cast<T>(&it->second);
    }

    template <template <typename, typename...> class K, typename T>
    void put(const K<T>& key, T && t) {
        (*this)[key.getName()] = std::forward<T>(t);
    }

    template <template <typename, typename...> class K>
    void put(const K<std::string>& key, const char *value) {
        (*this)[key.getName()] = value;
    }

    // We overload our operator[] so we unhide the one in the base class.
    using std::map<std::string, Datum>::operator[];

    template <template <typename, typename...> class K, typename T>
    T& operator[](const K<T> &key) {
        return *get_ptr(key, /* allocate */ true);
    }

    template <template <typename, typename...> class K, typename T>
    const T& operator[](const K<T> &key) const {
        return *get_ptr(key);
    }
};

// PREVENT INCORRECT MODIFICATIONS
// Data is a helping wrapper on std::map
// Don't add any non-static members
static_assert(sizeof(Data) == sizeof(std::map<std::string, Datum>));
// Nothing is virtual
static_assert(!std::is_polymorphic_v<Data>);

/**
 * Parceling of Datum by recursive descent to a ByteString
 */

// Platform Apex compatibility note:
// type_size_t may not change.
using type_size_t = uint16_t;

// Platform Apex compatibility note:
// index_size_t must not change.
using index_size_t = uint16_t;

// Platform Apex compatibility note:
// datum_size_t must not change.
using datum_size_t = uint32_t;

// The particular implementation of ByteString may change
// without affecting compatibility.
using ByteString = std::basic_string<uint8_t>;

/*
    These should correspond to the Java AudioMetadata.java

    TYPE_NONE = 0,
    TYPE_INT32 = 1,
    TYPE_INT64 = 2,
    TYPE_FLOAT = 3,
    TYPE_DOUBLE = 4,
    TYPE_STRING = 5,
    TYPE_DATA = 6,
*/

template <typename T>
inline constexpr type_size_t get_type_as_value() {
    return (type_size_t)(metadata_types::index_of<T>() + 1);
}

template <typename T>
inline constexpr type_size_t type_as_value = get_type_as_value<T>();

// forward decl for recursion.
bool copyToByteString(const Datum& datum, ByteString &bs);

template <typename T,  typename = std::enable_if_t<
    is_primitive_metadata_type_v<T> || std::is_arithmetic_v<std::decay_t<T>>
    >>
bool copyToByteString(const T *t, ByteString& bs) {
    bs.append((uint8_t*)t, sizeof(*t));
    return true;
}

bool copyToByteString(const std::string *s, ByteString&bs) {
    if (s->size() > std::numeric_limits<index_size_t>::max()) return false;
    index_size_t size = s->size();
    if (!copyToByteString(&size, bs)) return false;
    bs.append((uint8_t*)s->c_str());
    return true;
}

bool copyToByteString(const Data *d, ByteString& bs) {
    if (d->size() > std::numeric_limits<index_size_t>::max()) return false;
    index_size_t size = d->size();
    if (!copyToByteString(&size, bs)) return false;
    for (const auto &[name, datum2] : *d) {
        if (!copyToByteString(&name, bs) || !copyToByteString(datum2, bs))
            return false;
    }
    return true;
}

#ifdef METADATA_TESTING

bool copyToByteString(const std::vector<Datum> *v, ByteString& bs) {
    if (v->size() > std::numeric_limits<index_size_t>::max()) return false;
    index_size_t size = v->size();
    if (!copyToByteString(&size, bs)) return false;
    for (index_size_t i = 0; i < size; ++i) {
        if (!copyToByteString((*v)[i], bs)) return false;
    }
    return true;
}

bool copyToByteString(const std::pair<Datum, Datum> *p, ByteString& bs) {
    return copyToByteString(p->first, bs) && copyToByteString(p->second, bs);
}

#endif // METADATA_TESTING

// Now the actual code to use.
bool copyToByteString(const Datum& datum, ByteString &bs) {
    bool success = false;
    return metadata_types::apply([&bs, &success](auto ptr) {
             // save type
             const type_size_t type = type_as_value<decltype(*ptr)>;
             if (!copyToByteString(&type, bs)) return;

             // get current location
             const size_t idx = bs.size();

             // save size (replaced later)
             datum_size_t datum_size = 0;
             if (!copyToByteString(&datum_size, bs)) return;

             // copy data
             if (!copyToByteString(ptr, bs)) return;

             // save correct size
             const size_t diff = bs.size() - idx - sizeof(datum_size);
             if (diff > std::numeric_limits<datum_size_t>::max()) return;
             datum_size = diff;
             bs.replace(idx, sizeof(datum_size), (uint8_t*)&datum_size, sizeof(datum_size));
             success = true;
         }, &datum) && success;
}

/**
 * Obtaining the Datum back from ByteString
 */

// forward declaration
Datum datumFromByteString(const ByteString& bs, size_t& idx);

template <typename T, typename = std::enable_if_t<
        is_primitive_metadata_type_v<T> ||
        std::is_arithmetic_v<std::decay_t<T>>
        >>
bool copyFromByteString(T *dest, const ByteString& bs, size_t& idx) {
    if (idx + sizeof(T) > bs.size()) return false;
    bs.copy((uint8_t*)dest, sizeof(T), idx);
    idx += sizeof(T);
    return true;
}

bool copyFromByteString(std::string *s, const ByteString& bs, size_t& idx) {
    index_size_t size;
    if (!copyFromByteString(&size, bs, idx)) return false;
    if (idx + size > bs.size()) return false;
    s->resize(size);
    for (index_size_t i = 0; i < size; ++i) {
        (*s)[i] = bs[idx++];
    }
    return true;
}

bool copyFromByteString(Data *d, const ByteString& bs, size_t& idx) {
    index_size_t size;
    if (!copyFromByteString(&size, bs, idx)) return false;
    for (index_size_t i = 0; i < size; ++i) {
        std::string s;
        if (!copyFromByteString(&s, bs, idx)) return false;
        Datum value = datumFromByteString(bs, idx);
        if (!value.has_value()) return false;
        (*d)[s] = std::move(value);
    }
    return true;
}

// TODO: consider a more elegant implementation, e.g. std::variant visit.
// perhaps using integer sequences.
Datum datumFromByteString(const ByteString &bs, size_t& idx) {
    type_size_t type;
    if (!copyFromByteString(&type, bs, idx)) return {};

    datum_size_t datum_size;
    if (!copyFromByteString(&datum_size, bs, idx)) return {};
    if (datum_size > bs.size() - idx) return {};
    const size_t finalIdx = idx + datum_size; // TODO: always check this

    switch (type) {
    case type_as_value<int32_t>: {
        int32_t i32;
        if (!copyFromByteString(&i32, bs, idx)) return {};
        return i32;
    }
    case type_as_value<int64_t>: {
        int64_t i64;
        if (!copyFromByteString(&i64, bs, idx)) return {};
        return i64;
    }
    case type_as_value<float>: {
        float f;
        if (!copyFromByteString(&f, bs, idx)) return {};
        return f;
    }
    case type_as_value<double>: {
        double d;
        if (!copyFromByteString(&d, bs, idx)) return {};
        return d;
    }
    case type_as_value<std::string>: {
        std::string s;
        if (!copyFromByteString(&s, bs, idx)) return {};
        return s;
    }
    case type_as_value<Data>: {
        Data d;
        if (!copyFromByteString(&d, bs, idx)) return {};
        return d;
    }
#ifdef METADATA_TESTING
    case type_as_value<std::vector<Datum>>: {
        index_size_t size;
        if (!copyFromByteString(&size, bs, idx)) return {};
        std::vector<Datum> v(size);
        for (index_size_t i = 0; i < size; ++i) {
            Datum d = datumFromByteString(bs, idx);
            if (!d.has_value()) return {};
            v[i] = std::move(d);
        }
        return v;
    }
    case type_as_value<std::pair<Datum, Datum>>: {
        Datum d1 = datumFromByteString(bs, idx);
        if (!d1.has_value()) return {};
        Datum d2 = datumFromByteString(bs, idx);
        if (!d2.has_value()) return {};
        return std::make_pair(std::move(d1), std::move(d2));
    }
    case type_as_value<MoveCount>: {
        MoveCount mc;
        if (!copyFromByteString(&mc, bs, idx)) return {};
        return mc;
    }
#endif
    case 0: // This is excluded to have compile failure. We do not allow undefined types.
    default:
        idx = finalIdx; // skip unknown type.
        return {};
    }
}

// Handy helpers - these are the most efficient ways to parcel Data.
Data dataFromByteString(const ByteString &bs) {
    Data d;
    size_t idx = 0;
    copyFromByteString(&d, bs, idx);
    return d; // copy elision
}

ByteString byteStringFromData(const Data &data) {
    ByteString bs;
    copyToByteString(&data, bs);
    return bs; // copy elision
}

} // namespace android::audio_utils::metadata

#endif // __cplusplus

// C API (see C++ API above for details)

// TOOO: use C Generics
// https://en.cppreference.com/w/c/language/generic

/** \cond */
__BEGIN_DECLS
/** \endcond */

typedef struct audio_metadata_t audio_metadata_t;

/**
 * \brief Creates a metadata object
 *
 * \return the metadata object or NULL on failure.
 */
audio_metadata_t *audio_metadata_create();


/**
 * \brief Destroys the metadata object.
 *
 * \param metadata         object returned by create, if NULL nothing happens.
 */
void audio_metadata_destroy(audio_metadata_t *metadata);

/** \cond */
__END_DECLS
/** \endcond */

#endif // !ANDROID_AUDIO_METADATA_H
