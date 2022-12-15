#ifndef INTERPRETER_INTERNAL_HH
#define INTERPRETER_INTERNAL_HH

#include <interpreter/interp.hh>

#define defer auto INTERP_CAT($$defer_instance_, __COUNTER__) = $$defer{}, [&]()
#define tempset $$tempset_type INTERP_CAT($$tempset_instance_, __COUNTER__) = $$tempset_stage_1{} %

#define REP(n, var) for (usz var = 0; var < (n); var++)
#define repeat(n) REP(n, INTERP_CAT($$rep, __COUNTER__))

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

#endif // INTERPRETER_INTERNAL_HH
