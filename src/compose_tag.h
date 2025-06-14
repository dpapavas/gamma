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

#ifndef COMPOSE_TAG_H
#define COMPOSE_TAG_H

#include <sstream>
#include <ostream>
#include <tuple>

// ---

// ## Tag Composition

// We compose the operation's tag as a textual representation of the
// operation.  Roughly, we give each operation a unique name and then
// its tag is a string of the form `"name(arg_1, arg_2, ...)"`.  For
// example, a `Sphere_operation` with radius equal to 1, would be
// `"sphere(1)"` and a union of two spheres might be
// `"join(sphere(1),sphere(2))"`.

// The arguments can either be

//   * simple algebraic values, such as integers, rationals, etc.,
//   * strings, e.g. file names for output operations,
//   * geometric values, like points, vectors, transformations etc., or
//   * other operations.

// Implementations for "simple" values, can be found below; ref:
// Serialization of Simple Arguments.  Other implementations are
// bundled with the types they're meant for.

// Some declarations follow.

template<typename T, typename = void>
struct compose_tag_helper {
    static void compose(std::ostringstream &s, const T &x);
};

template<typename T, typename U>
struct compose_tag_helper<std::pair<T, U>> {
    static void compose(std::ostringstream &s, const std::pair<T, U> &p) {
        compose_tag_helper<T>::compose(s, p.first);
        compose_tag_helper<U>::compose(s, p.second);
    }
};

template<typename T>
struct compose_tag_helper<T, std::enable_if_t<std::is_array_v<T>>> {
    static void compose(std::ostringstream &s, const T &v) {
        for (auto x: v) {
            compose_tag_helper<std::decay_t<decltype(x)>>::compose(s, x);
        }
    }
};

template<typename T>
struct compose_tag_helper<T, std::void_t<decltype(std::declval<T>().begin()),
                                         decltype(std::declval<T>().end())>> {
    static void compose(std::ostringstream &s, const T &v) {
        for (auto x: v) {
            compose_tag_helper<std::decay_t<decltype(x)>>::compose(s, x);
        }
    }
};

// This template uses the various `compose_tag_helper` templates,
// which provide serializations for specific kinds of values, passed
// as arguments to an operation named `name`, to compose the tag.

template<typename... Args>
std::string compose_tag(const char *name, Args &&... args)
{
    std::ostringstream s;

    s << name << "(";
    if constexpr (sizeof...(Args) > 0) {
        const auto p = s.tellp();
        auto t = std::make_tuple(args...);

        (compose_tag_helper<std::remove_cv_t<
                                std::remove_reference_t<Args>>>::compose(
                                    s, args), ...);

        if (s.tellp() > p) {
            s.seekp(-1, std::ios_base::end);
        }

        s << ")";
    } else {
        s << ")";
    }

    return s.str();
}

// ---

#endif
