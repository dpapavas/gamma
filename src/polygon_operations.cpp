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

#include <CGAL/ch_melkman.h>
#include <CGAL/convex_hull_2.h>
#include <CGAL/minkowski_sum_2.h>

#include "iterators.h"
#include "polygon_operations.h"
#include "tolerances.h"
#include "projection.h"

void Ngon_operation::evaluate()
{
    assert(!polygon);

    if (points.size() < 3) {
        CGAL_error_msg("cannot make polygon from less than three points");
    }

    polygon = std::make_shared<Polygon_set>(
        Polygon(points.begin(), points.end()));
}

void Regular_polygon_operation::evaluate()
{
    assert(!polygon);

    if (sides < 3) {
        CGAL_error_msg("cannot make polygon with less than three sides");
    }

    if (radius <= 0) {
        CGAL_error_msg("cannot make polygon with non-positive radius");
    }

    Polygon P;

    const double delta = 2 * std::acos(-1) / sides;
    for (auto [i, theta] = std::pair<int, double>(0, std::asin(1));
         i < sides;
         i++, theta += delta) {
        P.push_back(
            project_to_circle(
                std::cos(theta), std::sin(theta), radius, tolerance));
    }

    polygon = std::make_shared<Polygon_set>(P);
}

// The underlying transformation routine doesn't handle reflecting
// transformations, so we need to do that manually.

static Polygon transform_polygon(
    const Aff_transformation_2 &T, const Polygon &P)
{
    Polygon TP = transform(T, P);

    if (T.is_odd()) {
        TP.reverse_orientation();
    }

    return TP;
}

template <>
void Polygon_transform_operation<Polygon_set>::evaluate()
{
    assert(!polygon);

    polygon = std::make_shared<Polygon_set>();

    using namespace std::placeholders;
    auto f = std::bind(transform_polygon, transformation, _1);

    transform_polygon_set(*operand->get_value(), *polygon, f);
}

// Polygon flush operation

void Polygon_flush_operation::evaluate()
{
    assert(!polygon);

    polygon = std::make_shared<Polygon_set>();

    FT x_min, x_max, y_min, y_max;
    bool p = true;

    for_each_polygon(
        *operand->get_value(),
        [&x_min, &x_max, &y_min, &y_max, &p](const Polygon &G) {
            auto l = G.left_vertex(), r = G.right_vertex();
            auto b = G.bottom_vertex(), t = G.top_vertex();

            if (p || l->x() < x_min) {
                x_min = l->x();
            }

            if (p || r->x() > x_max) {
                x_max = r->x();
            }

            if (p || b->y() < y_min) {
                y_min = b->y();
            }

            if (p || t->y() > y_max) {
                y_max = t->y();
            }

            p = false;
        });

    const Aff_transformation_2 T(1, 0, (coefficients[0][0] * x_min
                                        - coefficients[0][1] * x_max),
                                 0, 1, (coefficients[1][0] * y_min
                                        - coefficients[1][1] * y_max));

    using namespace std::placeholders;
    auto f = std::bind(transform_polygon, T, _1);

    transform_polygon_set(*operand->get_value(), *polygon, f);
}

bool Polygon_flush_operation::fold_operand(
    const Polygon_operation<Polygon_set> *p)
{
    const Polygon_flush_operation *f =
        dynamic_cast<const Polygon_flush_operation *>(p);

    if (!f) {
        return false;
    }

    const FT (*b)[2] = this->coefficients, (*a)[2] = f->coefficients;

    // This computation can be done in place, since only one of
    // a[i][0], a[i][1] is non-zero at any given time.

    for (int i = 0; i < 2; i++) {
        this->coefficients[i][0] = (a[i][0] * (1 - b[i][1])
                                    + b[i][0] * (1 + a[i][0]));
        this->coefficients[i][1] = (a[i][1] * (1 + b[i][0])
                                    + b[i][1] * (1 - a[i][1]));
    }

    return true;
}

//////////
// Hull //
//////////

void Polygon_hull_operation::evaluate()
{
    assert(!polygon);

    std::vector<Point_2> v;
    std::vector<Polygon_with_holes> h;

    for (const auto &x: operands) {
        const Polygon_set &S = *x->get_value();
        S.polygons_with_holes(std::back_inserter(h));
    }

    // Use the linear-time Melkman algorithm, when dealing with a
    // single (simple) polygon.

    if (h.size() == 1 && points.size() == 0) {
        const Polygon &B = h[0].outer_boundary();
        CGAL::ch_melkman(
            B.vertices_begin(), B.vertices_end(), std::back_inserter(v));
    } else {
        iterator_chain<Polygon::Container::const_iterator> c;

        for (const Polygon_with_holes &x: h) {
            const Polygon &B = x.outer_boundary();
            c.push_back(B.vertices_begin(), B.vertices_end());
        }

        if (points.size() > 0) {
            c.push_back(points.cbegin(), points.cend());
        }

        CGAL::convex_hull_2(c.begin(), c.end(), std::back_inserter(v));
    }

    polygon = std::make_shared<Polygon_set>(Polygon(v.begin(), v.end()));
}

///////////////////
// Minkowski sum //
///////////////////

void Polygon_minkowski_sum_operation::evaluate()
{
    assert(!polygon);

    const Polygon_set &A = *first->get_value(), B = *second->get_value();

    if (A.number_of_polygons_with_holes() != 1
        || B.number_of_polygons_with_holes() != 1) {
        CGAL_error_msg("operand polygon set has more than one polygon");
    }

    Polygon_with_holes P[1], G[1];

    A.polygons_with_holes(P);
    B.polygons_with_holes(G);

    polygon = std::make_shared<Polygon_set>(CGAL::minkowski_sum_2(P[0], G[0]));
}

////////////////////
// Polygon offset //
////////////////////

#include <CGAL/create_offset_polygons_from_polygon_with_holes_2.h>

void Polygon_offset_operation::evaluate()
{
    assert(!polygon);

    Polygon_set &S = *operand->get_value();

    if (offset == 0) {
        polygon = std::make_shared<Polygon_set>(S);
        return;
    }

    polygon = std::make_shared<Polygon_set>();

    const int n = S.number_of_polygons_with_holes();
    std::vector<Polygon_with_holes> v;

    v.reserve(n);
    S.polygons_with_holes(std::back_inserter(v));

    for (const Polygon_with_holes &P: v) {
        auto w = offset < 0
            ? CGAL::create_interior_skeleton_and_offset_polygons_with_holes_2(
                -offset, P, Kernel())
            : CGAL::create_exterior_skeleton_and_offset_polygons_with_holes_2(
                offset, P, Kernel());

        for (const auto &p: w) {
            polygon->insert(*p);
        }
    }
}

bool Polygon_offset_operation::fold_operand(
    const Polygon_operation<Polygon_set> *p)
{
    const Polygon_offset_operation *o =
        dynamic_cast<const Polygon_offset_operation *>(p);

    if (!o) {
        return false;
    }

    this->offset += o->offset;

    return true;
}
