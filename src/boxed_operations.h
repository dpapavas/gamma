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
