#ifndef INTERPRETER_UTILS_HH
#define INTERPRETER_UTILS_HH

/// Make sure we don’t try to include this header from C.
#ifndef __cplusplus
#   error "This header is C++ only. Use <interpreter/utils.h> instead."
#endif

#include <fmt/format.h>
#include <interpreter/utils.h>

#define defer auto CAT($$defer_instance_, __COUNTER__) = $$defer{}, [&]()
#define tempset $$tempset_type CAT($$tempset_instance_, __COUNTER__) = $$tempset_stage_1{} %

template <typename callable>
struct $$defer_type {
    callable cb;
    explicit $$defer_type(callable&& _cb) : cb(std::forward<callable>(_cb)) {}
    ~$$defer_type() { cb(); }
};

struct $$defer {
    template <typename callable>
    $$defer_type<callable> operator,(callable&& cb) {
        return $$defer_type<callable>{std::forward<callable>(cb)};
    }
};

template <typename type>
struct $$tempset_type {
    type& ref;
    type t;
    type oldval;

    explicit $$tempset_type(type& var, std::convertible_to<type> auto&& cv) : ref(var), t(std::forward<decltype(cv)>(cv)) {
        oldval = std::move(ref);
        ref = std::move(t);
    }

    ~$$tempset_type() { ref = std::move(oldval); }
};

template <typename type>
struct $$tempset_stage_2 {
    type& ref;

    $$tempset_stage_2(type& var) : ref(var) {}
    $$tempset_type<type> operator=(std::convertible_to<type> auto&& value) {
        return $$tempset_type<type>{ref, std::forward<decltype(value)>(value)};
    }
};

struct $$tempset_stage_1 {
    template <typename type>
    $$tempset_stage_2<type> operator%(type& var) {
        return $$tempset_stage_2<type>{var};
    }
};

namespace interp {

template <typename ...arguments>
void die(fmt::format_string<arguments...> fmt, arguments&& ...args) {
    fmt::print(stderr, "Error: ");
    fmt::print(stderr, fmt, std::forward<arguments>(args)...);
    fmt::print(stderr, "\n");
    exit(1);
}

template <typename ...arguments>
void todo(fmt::format_string<arguments...> fmt = "", arguments&& ...args) {
    fmt::print(stderr, "TODO: ");
    if (fmt.get().size() != 0) fmt::print(stderr, fmt, std::forward<arguments>(args)...);
    else fmt::print (stderr, "Unimplemented");
    fmt::print(stderr, "\n");
    exit(42);
}

template <typename a, typename b>
constexpr inline bool is = std::is_same_v<std::remove_cvref_t<a>, std::remove_cvref_t<b>>;

}

#endif // INTERPRETER_UTILS_HH
