#pragma once

#include <any>
#include <memory>
#include <variant>
#include <cstdint>
#include <optional>


namespace zinc {

template <class T>
struct is_variant : std::false_type {
};

template <class ...Ts>
struct is_variant<std::variant<Ts...>> : std::true_type {
};

template <class T>
struct is_shared_ptr : std::false_type {
};

template <class T>
struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {
    using type = T;
};

using scalar_type_variant = std::variant
        < bool
        , uint8_t
        , uint16_t
        , uint32_t
        , uint64_t
        , int8_t
        , int16_t
        , int32_t
        , int64_t
        , float
        , double
        >;

#ifdef ZINC_ANY_WITH_VECTOR
template <size_t N>
using vector_type_variant = std::variant
        < vec<N, bool>
        , vec<N, uint8_t>
        , vec<N, uint16_t>
        , vec<N, uint32_t>
        , vec<N, uint64_t>
        , vec<N, int8_t>
        , vec<N, int16_t>
        , vec<N, int32_t>
        , vec<N, int64_t>
        , vec<N, float>
        , vec<N, double>
        >;
#endif

template <class T, class = void>
struct any_traits {
    using underlying_type = T;
};

template <class T>
struct any_traits<T, std::void_t<decltype(
        std::declval<scalar_type_variant &>() = std::declval<T>()
        )>> {
    using underlying_type = scalar_type_variant;
};

#ifdef ZINC_ANY_WITH_VECTOR
template <size_t N, class T>
struct any_traits<vec<N, T>, std::void_t<decltype(
        std::declval<scalar_type_variant &>() = std::declval<T>()
        )>> {
    using underlying_type = vector_type_variant<N>;
};
#endif

template <class T>
struct any_traits<T *, std::void_t<typename T::polymorphic_base_type>> {
    using underlying_type = typename T::polymorphic_base_type *;
};

template <class T>
struct any_traits<T const *, std::void_t<typename T::polymorphic_base_type>> {
    using underlying_type = typename T::polymorphic_base_type const *;
};

template <class T>
struct any_traits<std::unique_ptr<T>, std::void_t<typename T::polymorphic_base_type>> {
    using underlying_type = std::unique_ptr<typename T::polymorphic_base_type>;
};

template <class T>
struct any_traits<std::shared_ptr<T>, std::void_t<typename T::polymorphic_base_type>> {
    using underlying_type = std::shared_ptr<typename T::polymorphic_base_type>;
};

template <class T>
using any_underlying_type_t = typename any_traits<std::decay_t<T>>::underlying_type;

struct any : std::any {
    any() = default;

    any(any const &a) = default;

    template <class T>
    any(T const &t)
    : std::any(static_cast<any_underlying_type_t<T> const &>(t))
    {}

    any &operator=(any const &a) = default;

    template <class T>
    any &operator=(T const &t) {
        std::any::operator=(
                static_cast<any_underlying_type_t<T>>(t));
        return *this;
    }
};

template <class T>
T smart_any_cast(any const &a) {
    if constexpr (std::is_same_v<T, any>) {
        return a;
    } else {
        using V = any_underlying_type_t<T>;
        decltype(auto) v = std::any_cast<V const &>(a);
        if constexpr (std::is_pointer_v<T>) {
            using U = std::remove_pointer_t<T>;
            auto ptr = dynamic_cast<U *>(v);
            if (!ptr) throw std::bad_cast();
            return ptr;
        } else if constexpr (is_shared_ptr<T>::value) {
            using U = typename is_shared_ptr<T>::type;
            auto ptr = std::dynamic_pointer_cast<U>(v);
            if (!ptr) throw std::bad_cast();
            return ptr;
        } else if constexpr (is_variant<V>::value && !is_variant<T>::value) {
            return std::visit([] (auto const &x) {
                return (T)x;
            }, v);
        } else {
            return v;
        }
    }
}

template <class T>
std::optional<T> silent_any_cast(any const &a) {
    if constexpr (std::is_same_v<T, any>) {
        return std::make_optional(a);
    } else {
        using V = any_underlying_type_t<T>;
        if (typeid(V) != a.type()) {
            return std::nullopt;
        }
        decltype(auto) v = std::any_cast<V const &>(a);
        if constexpr (std::is_pointer_v<T>) {
            auto ptr = dynamic_cast<T>(v);
            if (!ptr) return std::nullopt;
            return std::make_optional(ptr);
        } else if constexpr (is_shared_ptr<T>::value) {
            using U = typename is_shared_ptr<T>::type;
            auto ptr = std::dynamic_pointer_cast<U>(v);
            if (!ptr) return std::nullopt;
            return std::make_optional(ptr);
        } else if constexpr (is_variant<V>::value && !is_variant<T>::value) {
            return std::make_optional(std::visit([] (auto const &x) {
                return (T)x;
            }, v));
        } else {
            return std::make_optional(v);
        }
    }
}

template <class T>
T &&exact_any_cast(any &&a) {
    if constexpr (std::is_same_v<T, any>) {
        return a;
    } else {
        using V = any_underlying_type_t<T>;
        decltype(auto) v = std::any_cast<V &&>(a);
        if constexpr (std::is_pointer_v<T>) {
            return static_cast<T>(v);
        } else if constexpr (is_shared_ptr<T>::value) {
            using U = typename is_shared_ptr<T>::type;
            return std::static_pointer_cast<U>(v);
        } else if constexpr (is_variant<V>::value) {
            return std::get<T>(v);
        } else {
            return v;
        }
    }
}

}
