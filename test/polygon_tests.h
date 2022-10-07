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

#ifndef POLYGON_TESTS_H
#define POLYGON_TESTS_H

FT polygon_area(const Polygon_set &S);

const Polygon_set &test_polygon_area(
    const Polygon_set &S, const double area);

template<typename T>
const Polygon_set &test_polygon(const Polygon_set &S,
                                const int polygons, const int holes,
                                const int vertices, const T area)
{
    const int n = S.number_of_polygons_with_holes();
    BOOST_TEST(n == polygons);

    int h = 0, v = 0;
    FT A(0);

    for_each_polygon(
        S, [&h, &v, &A](const Polygon &G) {
               h += G.orientation();
               v += G.size();
               A += G.area();
           });

    BOOST_TEST(n - h == holes);
    BOOST_TEST(v == vertices);

    if constexpr(std::is_same_v<T, FT>) {
        BOOST_TEST(A == area);
    } else {
        BOOST_TEST(static_cast<T>(CGAL::to_double(A)) == area);
    }

    return S;
}

#endif
