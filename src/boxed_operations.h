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

#ifndef BOXED_OPERATIONS_H
#define BOXED_OPERATIONS_H

#include "assertions.h"
#include "kernel.h"
#include "polygon_types.h"
#include "circle_polygon_types.h"
#include "conic_polygon_types.h"
#include "polyhedron_types.h"

typedef std::variant<
    std::shared_ptr<Polygon_operation<Polygon_set>>,
    std::shared_ptr<Polygon_operation<Circle_polygon_set>>,
    std::shared_ptr<Polygon_operation<Conic_polygon_set>>> Boxed_polygon;

template<typename T>
Boxed_polygon make_boxed_polygon(const std::shared_ptr<T> &p) {
    if constexpr (std::is_same_v<T, Operation>) {
        if (auto x = std::dynamic_pointer_cast<Polygon_operation<Polygon_set>>(p)) {
            return Boxed_polygon(x);
        } else if (auto x = std::dynamic_pointer_cast<Polygon_operation<Circle_polygon_set>>(p)) {
            return Boxed_polygon(x);
        } else if (auto x = std::dynamic_pointer_cast<Polygon_operation<Conic_polygon_set>>(p)) {
            return Boxed_polygon(x);
        } else {
            assert_not_reached();
        }
    } else {
        return Boxed_polygon(p);
    }
}

typedef std::variant<
    std::shared_ptr<Polyhedron_operation<Polyhedron>>,
    std::shared_ptr<Polyhedron_operation<Nef_polyhedron>>,
    std::shared_ptr<Polyhedron_operation<Surface_mesh>>> Boxed_polyhedron;

#endif
