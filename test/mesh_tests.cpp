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

BOOST_FIXTURE_TEST_SUITE(mesh, Reset_operations)

BOOST_AUTO_TEST_CASE(perturb, * boost::unit_test::tolerance(0.015))
{
    Tolerances::projection = FT::ET(1, 1'000'000);
    Tolerances::curve = FT::ET(1, 100);

    auto a = SPHERE(2);
    auto b = PERTURB(SPHERE(2), nullptr, FT::ET(1, 100));
    BOOST_TEST(b->describe() == "perturb(sphere(2,1/100,1/1000000),1/100)");

    evaluate_unit();

    const FT V = polyhedron_volume(*a->get_value());
    const FT W = polyhedron_volume(*b->get_value());

    BOOST_TEST(V != W);
    BOOST_TEST(CGAL::abs(V - W) < FT(FT::ET(1, 10)));
}

BOOST_AUTO_TEST_CASE(refine, * boost::unit_test::tolerance(0.015))
{
    Tolerances::projection = FT::ET(1, 1'000'000);
    Tolerances::curve = FT::ET(1, 100);

    auto p = REFINE(SPHERE(2), nullptr, FT(5));
    BOOST_TEST(p->describe() == "refine(sphere(2,1/100,1/1000000),5)");

    evaluate_unit();

    const auto &P = *p->get_value();

    test_polyhedron_volume(P, std::acos(-1) * 4 / 3 * 8);
    BOOST_TEST(CGAL::to_double(
                   CGAL::squared_distance(
                       mean_polyhedron_vertex(P),
                       Point_3(CGAL::ORIGIN))) == 0.0);
}

BOOST_AUTO_TEST_CASE(refine_selected, * boost::unit_test::tolerance(0.015))
{
    Tolerances::projection = FT::ET(1, 1'000'000);
    Tolerances::curve = FT::ET(1, 100);

    auto b = FACES_IN(
        JOIN({
                TRANSFORM(BOUNDING_SPHERE(FT::ET(1001, 1000)),
                          TRANSLATION_3(0, 0, 2)),
                TRANSFORM(BOUNDING_SPHERE(FT::ET(999, 1000)),
                          TRANSLATION_3(0, 0, -2))}));
    auto p = REFINE(
        JOIN(TRANSFORM(SPHERE(1), TRANSLATION_3(0, 0, 2)),
             TRANSFORM(SPHERE(1), TRANSLATION_3(0, 0, -2))), b, FT(2));

    BOOST_TEST(
        p->describe() == ("refine(join("
                          "transform(sphere(1,1/100,1/1000000),"
                          "translation(0,0,2)),"
                          "transform(sphere(1,1/100,1/1000000),"
                          "translation(0,0,-2))),"
                          "faces_in(join("
                          "bounding_sphere(point(0,0,2),1001/1000),"
                          "bounding_sphere(point(0,0,-2),999/1000))),2)"));

    evaluate_unit();

    const auto &P = *p->get_value();

    test_polyhedron_volume(P, 2 * std::acos(-1) * 4 / 3);
    const auto C = mean_polyhedron_vertex(P);

    BOOST_TEST(CGAL::to_double(C.x()) == 0, boost::test_tools::tolerance(1e-3));
    BOOST_TEST(CGAL::to_double(C.y()) == 0, boost::test_tools::tolerance(1e-3));
    BOOST_TEST(CGAL::to_double(C.z()) > 0.75);
}

BOOST_AUTO_TEST_CASE(remesh, * boost::unit_test::tolerance(0.01))
{
    const FT l = FT::ET(1, 10);
    auto p = REMESH(CUBOID(1, 1, 1), nullptr, nullptr, l, 1);

    BOOST_TEST(p->describe() == "remesh(cuboid(1,1,1),1/10,1)");

    evaluate_unit();

    const auto &P = *p->get_value();

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
    Tolerances::projection = FT::ET(1, 1'000'000);
    Tolerances::curve = FT::ET(1, 100);

    const FT l = FT::ET(1, 10);
    auto b = FACES_IN(
        JOIN({
                TRANSFORM(
                    BOUNDING_CYLINDER(1, 2),
                    TRANSLATION_3(0, 0, 1)),
                TRANSFORM(
                    BOUNDING_CYLINDER(FT::ET(999, 1000), 2),
                    TRANSLATION_3(0, 0, -1))}));
    auto p = REMESH(CYLINDER(1, 2), b, nullptr, l, 1);

    BOOST_TEST(
        p->describe() == ("remesh(extrusion(regular_polygon(23,1,1/1000000),"
                          "translation(0,0,-1),translation(0,0,1)),"
                          "faces_in(join("
                          "bounding_cylinder(point(0,0,0),vector(0,0,1),1,2),"
                          "bounding_cylinder(point(0,0,-2),vector(0,0,1),999/1000,2))),"
                          "1/10,1)"));

    evaluate_unit();

    for (const auto &e: p->get_value()->edges()) {
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
    auto p = REMESH(
        CUBOID(2, 2, 2),
        nullptr, EDGES_IN(BOUNDING_PLANE(0, 0, 1, 1)), l, 1);

    BOOST_TEST(
        p->describe() == ("remesh(cuboid(2,2,2),"
                          "edges_in(bounding_plane(plane(0,0,1,1))),"
                          "1/4,1)"));

    evaluate_unit();

    const auto &P = *p->get_value();

    test_polyhedron_volume(P, 8);
    for (const auto &e: P.edges()) {
        BOOST_TEST(CGAL::squared_distance(
                       e.vertex()->point(),
                       e.opposite()->vertex()->point()) <= 4 * l * l);
    }
}

BOOST_AUTO_TEST_CASE(
    remesh_constrained_selected, * boost::unit_test::tolerance(5e-4))
{
    const FT l = FT::ET(1, 4);
    auto p = REMESH(
        CUBOID(2, 2, 2),
        FACES_PARTIALLY_IN(
            TRANSFORM(BOUNDING_SPHERE(1), TRANSLATION_3(1, -1, -1))),
        EDGES_IN(BOUNDING_PLANE(0, 0, 1, 1)),
        l, 1);

    BOOST_TEST(
        p->describe() == ("remesh(cuboid(2,2,2),"
                          "faces_partially_in("
                          "bounding_sphere(point(1,-1,-1),1)),"
                          "edges_in(bounding_plane(plane(0,0,1,1))),"
                          "1/4,1)"));

    evaluate_unit();

    const auto &P = *p->get_value();
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
    auto p = COREFINE(CUBOID(2, 2, 2),
                      TRANSFORM(CUBOID(2, 2, 2), TRANSLATION_3(1, 1, 1)));

    BOOST_TEST(p->describe() == ("corefine(cuboid(2,2,2),"
                                 "transform(cuboid(2,2,2),translation(1,1,1)))"));

    evaluate_unit();

    const auto &P = *p->get_value();

    test_polyhedron(P, 14, 72, 24, 8);
}

BOOST_DATA_TEST_CASE(corefine_plane,
                     (boost::unit_test::data::make({0, -2, -3, -4})
                      ^ boost::unit_test::data::make({21, 12, 8, 8})),
                     d, n)
{
    auto p = COREFINE(CUBOID(2, 2, 2), Plane_3(1, 1, 1, d));
    std::stringstream s;

    s << "corefine(cuboid(2,2,2),plane(1,1,1," << d << "))";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    const auto &P = *p->get_value();

    BOOST_TEST(P.size_of_vertices() == n);
    test_polyhedron_volume(P, FT(8));
}

BOOST_AUTO_TEST_SUITE_END()
