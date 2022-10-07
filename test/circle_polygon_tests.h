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

#ifndef CIRCLE_POLYGON_TESTS_H
#define CIRCLE_POLYGON_TESTS_H

#include <forward_list>
#include <CGAL/draw_polygon_set_2.h>

inline void draw_circle_polygon_set(const Circle_polygon_set &S, double tau)
{
    Polygon_set T;
    convert_circle_polygon_set(S, T, tau);
    CGAL::draw(T);
}

inline std::shared_ptr<
    Polygon_operation<Circle_polygon_set>> TRANSFORM_CS(
    std::shared_ptr<Polygon_operation<Circle_polygon_set>> p,
    const Aff_transformation_2 &T)
{
    auto x = std::dynamic_pointer_cast<Polygon_operation<Circle_polygon_set>>(
        TRANSFORM(p, T));

    BOOST_TEST(x);
    return x;
}

bool test_polygon_without_holes(const Circle_polygon &P,
                                const std::string_view &edges);

// Test whether P is congruent to the given edge lists.  A
// specification of the form: "CC,LC,LLL" describes a polygon with a
// boundary made out of two circular edges, a hole made of a line
// segment and arc (hence a circular segment) and another triangular
// hole.

template<typename T, typename... Args>
bool test_polygon_with_holes(const T &P, const std::string_view polygon)

{
    std::list<std::string_view> l;

    for (size_t i = 0, j = 0; ; i = j + 1) {
        j = polygon.find_first_of(',', i);

        if (j == std::string_view::npos) {
            l.push_back(polygon.substr(i));
            break;
        }

        l.push_back(polygon.substr(i, j - i));
    }

    if (!test_polygon_without_holes(P.outer_boundary(), l.front())) {
        return false;
    }
    l.pop_front();

    for (auto H = P.holes_begin(); H != P.holes_end(); H++) {
        bool p = false;

        if (l.empty()) {
            return false;
        }

        for (auto it = l.begin(); it != l.end(); it++) {
            if ((p = test_polygon_without_holes(*H, *it))) {
                l.erase(it);
                break;
            }
        }

        if (!p) {
            return false;
        }
    }

    return l.empty();
}

template<typename T, typename... Args>
const T &test_polygon(const T &S, Args &... args)
{
    std::forward_list<typename T::Polygon_with_holes_2> p;
    std::list<std::string_view> s({args...});
    S.polygons_with_holes(std::front_inserter(p));

    while (!p.empty()) {
        for (auto it = s.begin(); it != s.end(); it++) {
            if (test_polygon_with_holes(p.front(), *it)) {
                s.erase(it);
                break;
            }
        }

        p.pop_front();
    }

    BOOST_TEST(p.empty());
    BOOST_TEST(s.empty());

    return S;
}

#endif
