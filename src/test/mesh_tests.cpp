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

#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <iostream>

#include "options.h"
#include "kernel.h"
#include "macros.h"
#include "transformations.h"

#include "fixtures.h"
#include "polyhedron_tests.h"

#include <CGAL/Polygon_mesh_processing/measure.h>

#include <CGAL/draw_polyhedron.h>

Point_3 mean_polyhedron_vertex(const Polyhedron &P)
{
    FT x(0), y(0), z(0);
    for (const auto &v: P.vertex_handles()) {
        const auto &p = v->point();
        x += p.x();
        y += p.y();
        z += p.z();
    }

    const int n = P.size_of_vertices();
    return Point_3(x / n, y / n, z / n);
}

Point_3 mean_polyhedron_vertex(const Surface_mesh &P)
{
    FT x(0), y(0), z(0);
    for (const auto &p: P.points()) {
        x += p.x();
        y += p.y();
        z += p.z();
    }

    const int n = P.number_of_vertices();
    return Point_3(x / n, y / n, z / n);
}

BOOST_FIXTURE_TEST_SUITE(mesh, Coarse_evaluation_fixture)

BOOST_AUTO_TEST_CASE(perturb, * boost::unit_test::tolerance(0.015))
{
    FT V, W;

    {
        const auto &result = evaluate(SPHERE(2));
        evaluate_operations();
        V = polyhedron_volume(*result.value);
    }

    {
        const auto &result = evaluate(
            PERTURB(SPHERE(2), nullptr, FT::ET(1, 100)));

        evaluate_operations();

        BOOST_TEST(result.tag == "perturb(sphere(2,1/100,1/1000000),1/100)");
        W = polyhedron_volume(*result.value);

        BOOST_TEST(V != W);
        BOOST_TEST(CGAL::abs(V - W) < FT(FT::ET(1, 10)));
    }
}

BOOST_AUTO_TEST_CASE(refine, * boost::unit_test::tolerance(0.015))
{
    const auto &result = evaluate(REFINE(SPHERE(2), nullptr, FT(5)));

    evaluate_operations();

    BOOST_TEST(result.tag == "refine(sphere(2,1/100,1/1000000),5)");

    const auto &P = *result.value;
    test_polyhedron_volume(P, std::acos(-1) * 4 / 3 * 8);
    BOOST_TEST(CGAL::to_double(
                   CGAL::squared_distance(
                       mean_polyhedron_vertex(P),
                       Point_3(CGAL::ORIGIN))) == 0.0);
}

BOOST_AUTO_TEST_CASE(refine_selected, * boost::unit_test::tolerance(0.015))
{
    const auto &result = evaluate(
        REFINE(
            JOIN(TRANSFORM(SPHERE(1), TRANSLATION_3(0, 0, 2)),
                 TRANSFORM(SPHERE(1), TRANSLATION_3(0, 0, -2))),
            FACES_IN(
                JOIN({
                        TRANSFORM(BOUNDING_SPHERE(FT::ET(1001, 1000)),
                                  TRANSLATION_3(0, 0, 2)),
                        TRANSFORM(BOUNDING_SPHERE(FT::ET(999, 1000)),
                                  TRANSLATION_3(0, 0, -2))})), FT(2)));

    evaluate_operations();

    BOOST_TEST(
        result.tag == ("refine(join("
                       "transform(sphere(1,1/100,1/1000000),"
                       "translation(0,0,2)),"
                       "transform(sphere(1,1/100,1/1000000),"
                       "translation(0,0,-2))),"
                       "faces_in(join("
                       "bounding_sphere(point(0,0,2),1001/1000),"
                       "bounding_sphere(point(0,0,-2),999/1000))),2)"));

    const auto &P = *result.value;
    test_polyhedron_volume(P, 2 * std::acos(-1) * 4 / 3);

    const auto C = mean_polyhedron_vertex(P);
    BOOST_TEST(CGAL::to_double(C.x()) == 0, boost::test_tools::tolerance(1e-3));
    BOOST_TEST(CGAL::to_double(C.y()) == 0, boost::test_tools::tolerance(1e-3));
    BOOST_TEST(CGAL::to_double(C.z()) > 0.75);
}

BOOST_AUTO_TEST_CASE(remesh, * boost::unit_test::tolerance(0.01))
{
    const FT l = FT::ET(1, 10);
    const auto &result = evaluate(
        REMESH(CUBOID(1, 1, 1), nullptr, nullptr, l, 1));

    evaluate_operations();

    BOOST_TEST(result.tag == "remesh(cuboid(1,1,1),1/10,1)");

    const auto &P = *result.value;
    test_polyhedron_volume(P, 1);

    for (const auto &e: P.edges()) {
        BOOST_TEST(std::sqrt(
                       CGAL::to_double(
                           CGAL::squared_distance(
                               e.prev()->vertex()->point(),
                               e.vertex()->point()))) <= CGAL::to_double(l),
                   boost::test_tools::tolerance(0.5));
    }
}

// Isotropic remeshing doesn't guarantee that the target will be
// reached, so we test that the edges are "short enough".

BOOST_AUTO_TEST_CASE(remesh_selected)
{
    const FT l = FT::ET(1, 10);
    const auto &result = evaluate(
        REMESH(
            CYLINDER(1, 2),
            FACES_IN(
                JOIN({
                        TRANSFORM(
                            BOUNDING_CYLINDER(1, 2),
                            TRANSLATION_3(0, 0, 1)),
                        TRANSFORM(
                            BOUNDING_CYLINDER(FT::ET(999, 1000), 2),
                            TRANSLATION_3(0, 0, -1))})), nullptr, l, 1));

    evaluate_operations();

    BOOST_TEST(
        result.tag == (
            "remesh(extrusion(regular_polygon(23,1,1/1000000),"
            "translation(0,0,-1),translation(0,0,1)),"
            "faces_in(join("
            "bounding_cylinder(point(0,0,0),vector(0,0,1),1,2),"
            "bounding_cylinder(point(0,0,-2),vector(0,0,1),999/1000,2))),"
            "1/10,1)"));

    for (const auto &e: result.value->edges()) {
        const auto &A = e.prev()->vertex()->point();
        const auto &B = e.vertex()->point();

        if (A.z() == 1 && B.z() == 1) {
            BOOST_TEST(CGAL::squared_distance(A, B) <= 4 * l * l);
        }
    }
}

BOOST_AUTO_TEST_CASE(remesh_constrained, * boost::unit_test::tolerance(5e-3))
{
    const FT l = FT::ET(1, 4);
    const auto &result = evaluate(
        REMESH(
            CUBOID(2, 2, 2),
            nullptr, EDGES_IN(BOUNDING_PLANE(0, 0, 1, 1)), l, 1));

    evaluate_operations();

    BOOST_TEST(
        result.tag == ("remesh(cuboid(2,2,2),"
                       "edges_in(bounding_plane(plane(0,0,1,1))),"
                       "1/4,1)"));

    const auto &P = *result.value;
    test_polyhedron_volume(P, 8);

    for (const auto &e: P.edges()) {
        BOOST_TEST(CGAL::squared_distance(
                       e.vertex()->point(),
                       e.opposite()->vertex()->point()) <= 4 * l * l);
    }
}

BOOST_AUTO_TEST_CASE(
    remesh_constrained_selected, * boost::unit_test::tolerance(6e-4))
{
    const FT l = FT::ET(1, 4);
    const auto &result = evaluate(
        REMESH(
            CUBOID(2, 2, 2),
            FACES_PARTIALLY_IN(
                TRANSFORM(BOUNDING_SPHERE(1), TRANSLATION_3(1, -1, -1))),
            EDGES_IN(BOUNDING_PLANE(0, 0, 1, 1)),
            l, 1));

    evaluate_operations();

    BOOST_TEST(
        result.tag == ("remesh(cuboid(2,2,2),"
                       "faces_partially_in("
                       "bounding_sphere(point(1,-1,-1),1)),"
                       "edges_in(bounding_plane(plane(0,0,1,1))),"
                       "1/4,1)"));

    const auto &P = *result.value;
    test_polyhedron_volume(P, 8);

    for (const auto &e: P.edges()) {
        const auto &A = e.vertex()->point();
        const auto &B = e.opposite()->vertex()->point();

        if ((1 - A.x()) + (A.y() + 1) + (A.z() + 1) <= 2
            && (1 - B.x()) + (B.y() + 1) + (B.z() + 1) <= 2) {
            BOOST_TEST(CGAL::squared_distance(A, B) <= 4 * l * l);
        }
    }
}

BOOST_AUTO_TEST_CASE(corefine)
{
    const auto &result = evaluate(
        COREFINE(CUBOID(2, 2, 2),
                 TRANSFORM(CUBOID(2, 2, 2), TRANSLATION_3(1, 1, 1))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("corefine(cuboid(2,2,2),"
                              "transform(cuboid(2,2,2),translation(1,1,1)))"));

    test_polyhedron(*result.value, 14, 72, 24, 8);
}

BOOST_DATA_TEST_CASE(corefine_plane,
                     (boost::unit_test::data::make({0, -2, -3, -4})
                      ^ boost::unit_test::data::make({20, 14, 8, 8})),
                     d, n)
{
    const auto &result = evaluate(
        COREFINE(CUBOID(2, 2, 2), Plane_3(1, 1, 1, d)));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag("corefine(cuboid(2,2,2),plane(1,1,1,", d, "))"));

    const auto &P = *result.value;
    test_polyhedron_volume(P, FT(8));
    BOOST_TEST(P.size_of_vertices() == n);
}

BOOST_DATA_TEST_CASE(components, boost::unit_test::data::xrange(3), n)
{
    auto A = CUBOID(2, 2, 2), B = A;

    // We set up a set of 5 cuboids, arranged in a cross pattern,
    // centered on the origin.

    for (auto [i, j]:
             std::initializer_list<std::pair<int, int>> {
             {-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
        B = JOIN(B, TRANSFORM(A, TRANSLATION_3(3 * i, 3 * j, 0)));
    }

    // We extract:

    //   1. the center cuboid,
    //   2. the left and right cuboids,
    //   3. the top and bottom cuboids,

    const std::initializer_list<int> v[3] = {{3}, {1, 5}, {2, 4}};
    const auto &result = evaluate(COMPONENTS(B, v[n]));

    A.reset();
    B.reset();

    evaluate_operations();

    // The first test case should consist of a single cuboid, the
    // others of two.

    const auto &P = *result.value;
    const int c = (n > 0) + 1;
    test_polyhedron(P, 8 * c, 36 * c, 12 * c, FT(8 * c));

    // Due to the symmetry of the setup, all cases should have their
    // centroid at the origin.

    const auto C = mean_polyhedron_vertex(P);
    BOOST_TEST(C == Point_3(0, 0, 0));
}

BOOST_DATA_TEST_CASE(components_with_holes, boost::unit_test::data::xrange(4), n)
{
    auto A = CUBOID(2, 2, 2), B = CUBOID(10, 4, 4);

    // We set up a large cuboid, with three smaller cuboidal holes,
    // all symmetric around the origin.

    for (int i = -1; i < 2; i++) {
        B = DIFFERENCE(B, TRANSFORM(A, TRANSLATION_3(3 * i, 0, 0)));
    }

    // The first two test cases select the outer boundary and one hole
    // and should therefore consist of two cuboids.  The other two
    // select only holes and should consist of a single cuboid.

    const std::initializer_list<int> v[4] = {{1, 2}, {1, 4}, {4}, {2}};
    const auto &result = evaluate(COMPONENTS(B, v[n]));

    A.reset();
    B.reset();

    evaluate_operations();

    const auto &P = *result.value;
    const int i = (n < 2), j = i + 1;
    test_polyhedron(P, 8 * j, 36 * j, 12 * j, FT(160 * i + (1 - 2 * i) * 8));

    // In cases 2 3, where we extract holes, the centroid should
    // coincide with that of the extracted hole (i.e. $x = \pm 3$).

    // In cases 0 and 1, the hole is on the other side.  Furthermore,
    // the outer boundary vertices cancel out due to symmetry, but
    // they still double the total vertex count, so the centroid is
    // now at $x = \mp 1.5$.

    const auto C = mean_polyhedron_vertex(P);
    BOOST_TEST(C == Point_3(FT(FT::ET(3 - 6 * (n % 2), 1 - 3 * i)), 0, 0));
}

BOOST_AUTO_TEST_SUITE_END()
