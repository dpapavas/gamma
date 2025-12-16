// Copyright 2022 Dimitris Papavasiliou

// This file is part of Gamma.

// Gamma is free software: you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.

// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
// details.

// You should have received a copy of the GNU General Public License along with
// this program. If not, see <https://www.gnu.org/licenses/>.

#ifndef FRONTEND_H
#define FRONTEND_H

// Document: program

// ## Selecting Between Nef and Corefinement Operations

// In boolean operations that can be implemented either on Nef
// polyhedra, or using corefinement on plain polyhedra, we decide what
// to do based on the polyhedron boolean operations mode.

// In NEF or COREFINE mode, we perform the minimum number of
// conversions necessary, to force the operation to be carried out
// with the specified method.  In AUTO mode, we prefer converting away
// from Nef, to avoid propagating conversions to Nef down the line,
// unless both operands are already Nef polyhedra, in which case we
// may as well do the operation with them.

// The following return lambdas for use with `std::visit`.  They're
// needed in every front end, so we define them here.

inline auto make_polyhedron_clip_visitor(const Plane_3 &Pi) {
    return [&Pi](auto &&x) {
        using T = typename std::remove_reference_t<decltype(*x)>;

        if (Options::polyhedron_booleans == Polyhedron_booleans_mode::NEF) {
            return Boxed_polyhedron(
                CLIP(CONVERT_TO<Nef_polyhedron>(x), Pi));
        } else if (
            Options::polyhedron_booleans == Polyhedron_booleans_mode::COREFINE
            && std::is_same_v<T, Polyhedron_operation<Nef_polyhedron>>) {
            return Boxed_polyhedron(
                CLIP(CONVERT_TO<Polyhedron>(x), Pi));
        } else {
            return Boxed_polyhedron(CLIP(x, Pi));
        }
    };
}

#define make_polyhedron_boolean_visitor(OP)                            \
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
void insert_output_operations(
    std::string name, std::vector<Boxed_polyhedron> &v);

// Document: none

#endif
