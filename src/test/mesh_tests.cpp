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

// Document: program

// # Mesh Operation Tests

// Below, we test operations that work at the mesh level.  As such,
// these operate only on `Polyhedron` and `Surface_mesh`.

// We first define helper functions to calculate the centroid of a
// mesh.

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
    // We perturb the vertices of a sphere.

    const FT l = FT::ET(1, 10);
    const auto &result = evaluate(PERTURB(SPHERE(2), nullptr, l));

    evaluate_operations();

    BOOST_TEST(result.tag == "perturb(sphere(2,1/100,1/1000000),1/10)");

    // We test the result by testing that the perturbed vertices fall
    // within the expected spherical shells.

    // Looking at the sources, we see that the distance parameter is
    // taken to mean the maximum displacement *along each coordinate*,
    // so the maximum displacement away from the original sphere point
    // is:

    const FT m = std::sqrt(3) * l;

    for (const auto &x: result.value->points()) {
        BOOST_TEST(
            CGAL::squared_distance(x, Point_3(0, 0, 0)) < (2 + m) * (2 + m));
        BOOST_TEST(
            CGAL::squared_distance(x, Point_3(0, 0, 0)) > (2 - m) * (2 - m));
    }
}

BOOST_AUTO_TEST_CASE(refine, * boost::unit_test::tolerance(0.015))
{
    // We refine a sphere and test that it has remainned a sphere in
    // terms of volume and vertex location.

    const auto &result = evaluate(REFINE(SPHERE(2), nullptr, FT(5)));

    evaluate_operations();

    BOOST_TEST(result.tag == "refine(sphere(2,1/100,1/1000000),5)");

    const auto &P = *result.value;
    test_polyhedron_volume(P, std::acos(-1) * 4.0 / 3.0 * 8.0);
    BOOST_TEST(CGAL::to_double(
                   CGAL::squared_distance(
                       mean_polyhedron_vertex(P),
                       Point_3(CGAL::ORIGIN))) == 0.0);
}

BOOST_AUTO_TEST_CASE(refine_selected, * boost::unit_test::tolerance(0.025))
{
    // Here we refine one of two identical spheres.

    const auto &result = evaluate(
        REFINE(
            JOIN(TRANSFORM(SPHERE(1), TRANSLATION_3(0, 0, 2)),
                 TRANSFORM(SPHERE(1), TRANSLATION_3(0, 0, -2))),
            FACES_IN(BOUNDING_HALFSPACE(0, 0, -1, 1)), FT(2)));

    evaluate_operations();

    BOOST_TEST(
        result.tag == ("refine(join("
                       "transform(sphere(1,1/100,1/1000000),"
                       "translation(0,0,2)),"
                       "transform(sphere(1,1/100,1/1000000),"
                       "translation(0,0,-2))),"
                       "faces_in(bounding_halfspace(plane(0,0,-1,1))),2)"));

    // We check the volume as before.

    const auto &P = *result.value;
    test_polyhedron_volume(P, 2.0 * std::acos(-1) * 4.0 / 3.0);

    // Since we increase one sphere's density by 2, it should now have
    // rougly 3 times the vertices of the other.  We test it, by
    // checking that the centroid has shifted towards it accordingly.

    const auto C = mean_polyhedron_vertex(P);
    BOOST_TEST(CGAL::to_double(C.x()) == 0, boost::test_tools::tolerance(1e-3));
    BOOST_TEST(CGAL::to_double(C.y()) == 0, boost::test_tools::tolerance(1e-3));
    BOOST_TEST(CGAL::to_double(C.z()) == 0.75);
}

BOOST_AUTO_TEST_CASE(remesh, * boost::unit_test::tolerance(0.01))
{
    // We remesh a cube and check that the edge length after the
    // opration is close to the specified target.

    const FT l = FT::ET(1, 10);
    const auto &result = evaluate(
        REMESH(CUBOID(1, 1, 1), nullptr, nullptr, l, 1));

    evaluate_operations();

    BOOST_TEST(result.tag == "remesh(cuboid(1,1,1),1/10,1)");

    const auto &P = *result.value;

    // We test that the mesh has more or less retained its shape by
    // checking that it has approximately retained its volume.

    test_polyhedron_volume(P, 1);

    for (const auto &e: CGAL::edges(P)) {
        BOOST_TEST(
            CGAL::Polygon_mesh_processing::squared_edge_length(e, P)
            <= 4 * l * l);
    }
}

BOOST_AUTO_TEST_CASE(remesh_selected)
{
    // To test partial remeshing, we remesh just the top face of a
    // cylinder.

    const FT l = FT::ET(1, 10);
    const auto &result = evaluate(
        REMESH(
            CYLINDER(1, 2),
            FACES_IN(BOUNDING_PLANE(0, 0, -1, 1)), nullptr, l, 1));

    evaluate_operations();

    BOOST_TEST(
        result.tag == (
            "remesh(extrusion(regular_polygon(23,1,1/1000000),"
            "translation(0,0,-1),translation(0,0,1)),"
            "faces_in(bounding_plane(plane(0,0,-1,1))),"
            "1/10,1)"));

    // We should test that edge length is at target only for the top face,
    // but since isotropic remeshing doesn't guarantee that the target
    // will be reached, we test that the edges are "short enough".

    for (const auto &e: result.value->edges()) {
        const auto &a = e.prev()->vertex()->point();
        const auto &b = e.vertex()->point();

        BOOST_TEST(
            (CGAL::squared_distance(a, b) <= 4 * l * l)
            == (a.z() == 1 && b.z() == 1));
    }
}

BOOST_AUTO_TEST_CASE(remesh_constrained, * boost::unit_test::tolerance(5e-3))
{
    // To test remeshing with constrained edges, we again use a cube
    // and approximately test edge length and volume.

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

    for (const auto &e: result.value->edges()) {
        const auto &a = e.prev()->vertex()->point();
        const auto &b = e.vertex()->point();

        BOOST_TEST(CGAL::squared_distance(a, b) <= 4 * l * l);

        // Additionally, we test that the bondary of the constrained
        // face has not been deformed.

        if (a.z() == -1 && b.z() == -1) {
            BOOST_TEST((
                a.x() <= 1 && a.x() >= -1 && b.x() <= 1 && b.x() >= -1
                && a.y() <= 1 && a.y() >= -1 && b.y() <= 1 && b.y() >= -1));
        }
    }
}

BOOST_AUTO_TEST_CASE(remesh_constrained_selected)
{
    // This is like `remesh_selected`, but now we constrain the edges
    // on the remeshed face.

    const FT l = FT::ET(1, 4);
    const auto &result = evaluate(
        REMESH(
            CUBOID(2, 2, 2),
            FACES_IN(BOUNDING_PLANE(0, 0, -1, 1)),
            EDGES_IN(BOUNDING_PLANE(0, 0, -1, 1)), l, 1));

    evaluate_operations();

    BOOST_TEST(
        result.tag == ("remesh(cuboid(2,2,2),"
                       "faces_in(bounding_plane(plane(0,0,-1,1))),"
                       "edges_in(bounding_plane(plane(0,0,-1,1))),"
                       "1/4,1)"));

    const auto &P = *result.value;

    // We can simply test that the selected edges have in fact been
    // constrined, by checking that the volume is now *exactly*
    // retained.

    test_polyhedron_volume(P, 8);

    for (const auto &e: result.value->edges()) {
        const auto &a = e.prev()->vertex()->point();
        const auto &b = e.vertex()->point();

        BOOST_TEST(
            (CGAL::squared_distance(a, b) <= 4 * l * l)
            == (a.z() == 1 && b.z() == 1));
    }
}

BOOST_AUTO_TEST_CASE(corefine)
{
    // We corefine a cube with another cube and test the resulting
    // geometry.

    const auto &result = evaluate(
        COREFINE(CUBOID(2, 2, 2),
                 TRANSFORM(CUBOID(2, 2, 2), TRANSLATION_3(1, 1, 1))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("corefine(cuboid(2,2,2),"
                              "transform(cuboid(2,2,2),translation(1,1,1)))"));

    test_polyhedron(*result.value, 14, 72, 24, 8);
}

BOOST_AUTO_TEST_CASE(corefine_plane)
{
    // We corefine a cube with the XY plane and test that it has
    // retained its volume and all newly introduced vertices lie on
    // the plane.

    const auto &result = evaluate(
        COREFINE(CUBOID(2, 2, 2), Plane_3(0, 0, 1, 0)));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag("corefine(cuboid(2,2,2),plane(0,0,1,0))"));

    const auto &P = *result.value;
    test_polyhedron_volume(P, FT(8));

    std::size_t n = 0;
    for(const auto &x: P.points()) {
        n += (x.z() == 0);
    }

    BOOST_TEST(n == (P.size_of_vertices() - 8));
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
