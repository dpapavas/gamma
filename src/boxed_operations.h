// Copyright 2022, 2026 Dimitris Papavasiliou

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

// Document: program

// ## Boxed Operations

// CGAL has multiple types representing polygons and polyhedra.  Some
// operations necesseraily only operate on one of them
// (.e.g. minkowski sums only work on `Nef_polyhedron`) while some are
// defined for all types (e.g. transformations or boolean operations,
// which work for all kinds of polygons and polyhedra).

// We want to abstract such internal implementation details from the
// user so most operations can accept any type of operand that makes
// sense for them; for instance offsets only work on polygons but they
// accept any kind of polygon.  When necessary they convert their
// operand to the type they can work on.

// Similarly, they may return any type of operand, not even
// necessarily the same kind they operated on.  Sometimes this is a
// necessity.  For instance scaling a circle polygon non-uniformly
// turns it into an ellipse, so now it has to be represented as a conic
// polygon.  Mostly though, it's just a matter of efficiency: If a
// minkowski sum operation necessarily produces a Nef polyhedron, even
// if it accepted a `Polyhedron` as an opeand and converted it into
// `Nef_polyhedron` to do its work, there's no point in performing a
// costly conversion of the result back to `Polyhedron`, at least not
// until we determine it's necessary or convenient to do so, in order
// to perform any further operations on it.

// A "boxed" polygon or polyhedron, is simply an `std::variant` of all
// possible `Polygon_operation` or `Polyhedron_operation` types
// respectively.  This allows us to abstract away the underlying
// polygon or polyhedron type from the front end, by using
// `std::visit` as necessary.

typedef std::variant<
    std::shared_ptr<Polygon_operation<Polygon_set>>,
    std::shared_ptr<Polygon_operation<Circle_polygon_set>>,
    std::shared_ptr<Polygon_operation<Conic_polygon_set>>> Boxed_polygon;

typedef std::variant<
    std::shared_ptr<Polyhedron_operation<Polyhedron>>,
    std::shared_ptr<Polyhedron_operation<Nef_polyhedron>>,
    std::shared_ptr<Polyhedron_operation<Surface_mesh>>> Boxed_polyhedron;

// As hinted above, there's a special complication with polygons.
// Under transformation, circle polygons may transmute to conic
// polygons.  They may also stay circle polygons.  We handle this by
// making `TRANSFORM<>` return a pointer to `Operation` by default
// when operating on `Circle_polygon_set` polygons.

// To handle such a result, we then need to determine what sort of
// polygon it ended up as at run-time.  We do this with the following
// utility.

template<typename T>
Boxed_polygon make_boxed_transformed_polygon(const std::shared_ptr<T> &p) {
    if constexpr (std::is_same_v<T, Operation>) {
        if (auto x = std::dynamic_pointer_cast<
            Polygon_operation<Circle_polygon_set>>(p)) {
            return Boxed_polygon(x);
        }

        auto x = std::dynamic_pointer_cast<
            Polygon_operation<Conic_polygon_set>>(p);

        assert(x);
        return Boxed_polygon(x);
    } else {
        return Boxed_polygon(p);
    }
}

#endif
