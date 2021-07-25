#ifndef FRONTEND_H
#define FRONTEND_H

// These implement the various polyhedron boolean operations mode.  In NEF or
// COREFINE mode, we perform the minimum number of conversions necessary, to
// force the operation to be carried out with the specified method.  In AUTO
// mode, we prefer converting away from Nef, to avoid propagating conversions to
// Nef down the line, unless both operands are already Nef polyhedra, in which
// case we may as well do the operation with them.

#define PERFORM_POLYHEDRON_CLIP_OPERATION(PI)                   \
[&PI](auto &&x) {                                               \
    using T = typename std::remove_reference_t<decltype(*x)>;   \
                                                                \
    if (Options::polyhedron_booleans                            \
        == Polyhedron_booleans_mode::NEF) {                     \
        return Boxed_polyhedron(                                \
            CLIP(CONVERT_TO<Nef_polyhedron>(x), PI));           \
    } else if (                                                 \
        Options::polyhedron_booleans                            \
        == Polyhedron_booleans_mode::COREFINE                   \
        && std::is_same_v<T, Polyhedron_operation<Nef_polyhedron>>) {   \
        return Boxed_polyhedron(                                \
            CLIP(CONVERT_TO<Polyhedron>(x), PI));               \
    } else {                                                    \
        return Boxed_polyhedron(CLIP(x, PI));                   \
    }                                                           \
}

#define PERFORM_POLYHEDRON_SET_OPERATION(OP)                            \
[](auto &&x, auto &&y) {                                                \
    using T = typename std::remove_reference_t<decltype(*x)>;           \
    using U = typename std::remove_reference_t<decltype(*y)>;           \
                                                                        \
    if (Options::polyhedron_booleans                                    \
        == Polyhedron_booleans_mode::NEF) {                             \
        return Boxed_polyhedron(                                        \
            OP(                                                         \
                CONVERT_TO<Nef_polyhedron>(x),                          \
                CONVERT_TO<Nef_polyhedron>(y)));                        \
    } else {                                                            \
        if constexpr (                                                  \
            std::is_same_v<T, Polyhedron_operation<Nef_polyhedron>>     \
            && std::is_same_v<U, Polyhedron_operation<Nef_polyhedron>>) { \
            if (Options::polyhedron_booleans                            \
                == Polyhedron_booleans_mode::COREFINE) {                \
                return Boxed_polyhedron(                                \
                    OP(                                                 \
                        CONVERT_TO<Polyhedron>(x),                      \
                        CONVERT_TO<Polyhedron>(y)));                    \
            } else {                                                    \
                assert(Options::polyhedron_booleans                     \
                       == Polyhedron_booleans_mode::AUTO);              \
                                                                        \
                return Boxed_polyhedron(OP(x, y));                      \
            }                                                           \
        } else if constexpr (                                           \
            std::is_same_v<T, Polyhedron_operation<Nef_polyhedron>>) {  \
            return Boxed_polyhedron(OP(CONVERT_TO<Polyhedron>(x), y));  \
        } else if constexpr (                                           \
            std::is_same_v<U, Polyhedron_operation<Nef_polyhedron>>) {  \
            return Boxed_polyhedron(OP(x, CONVERT_TO<Polyhedron>(y)));  \
        } else {                                                        \
            return Boxed_polyhedron(OP(x, y));                          \
        }                                                               \
    }                                                                   \
}

void print_message(Operation::Message_level level, const char *s, const int n);
void add_output_operations(std::string name, std::vector<Boxed_polyhedron> &v);

#endif
