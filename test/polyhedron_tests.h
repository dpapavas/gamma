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

#ifndef POLYHEDRON_TESTS_H
#define POLYHEDRON_TESTS_H

FT polyhedron_volume(const Nef_polyhedron &N);
FT polyhedron_volume(const Surface_mesh &M);
FT polyhedron_volume(const Polyhedron &P);

const Polyhedron &test_polyhedron(
    const Polyhedron &P,
    const int vertices, const int halfedges, const int facets);

const Nef_polyhedron &test_polyhedron(
    const Nef_polyhedron &N,
    const int vertices, const int halfedges, const int facets);

const Surface_mesh &test_polyhedron(
    const Surface_mesh &M,
    const int vertices, const int halfedges, const int facets);

template<typename T>
const T &test_polyhedron_volume(const T &P, const FT &volume)
{
    BOOST_TEST(polyhedron_volume(P) == volume);

    return P;
}

template<typename T>
const T &test_polyhedron_volume(const T &P, const double volume)
{
    BOOST_TEST(CGAL::to_double(polyhedron_volume(P)) == volume);

    return P;
}

template<typename T, typename U>
const T &test_polyhedron(
    const T &P,
    const int vertices, const int halfedges, const int facets, const U &volume)
{
    test_polyhedron(P, vertices, halfedges, facets);
    test_polyhedron_volume(P, volume);

    return P;
}

#endif
