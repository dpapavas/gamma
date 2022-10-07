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

#ifndef CONIC_POLYGON_TESTS_H
#define CONIC_POLYGON_TESTS_H

static inline std::shared_ptr<
    Polygon_operation<Conic_polygon_set>> TRANSFORM_C(
    std::shared_ptr<Polygon_operation<Circle_polygon_set>> p,
    const Aff_transformation_2 &T)
{
    auto x = std::dynamic_pointer_cast<Polygon_operation<Conic_polygon_set>>(
        TRANSFORM(p, T));

    BOOST_TEST(x);
    return x;
}

bool test_polygon_without_holes(const Conic_polygon &P,
                                const std::string_view &edges);

#endif
