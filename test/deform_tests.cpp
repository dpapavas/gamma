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

#include <CGAL/draw_polyhedron.h>
#include <CGAL/draw_surface_mesh.h>

BOOST_FIXTURE_TEST_SUITE(deform, Reset_operations)

BOOST_AUTO_TEST_CASE(fair)
{
    auto p = FAIR(
        REMESH(
            CUBOID(2, 2, 2),
            nullptr, EDGES_IN(BOUNDING_HALFSPACE(0, 0, 1, -1)),
            FT(FT::ET(1, 3)), 1),
        VERTICES_IN(BOUNDING_HALFSPACE(0, 0, -1, 0)), 1);

    BOOST_TEST(
        p->describe() == (
            "fair(remesh(cuboid(2,2,2),"
            "edges_in(bounding_halfspace(plane(0,0,1,-1))),1/3,1),"
            "vertices_in(bounding_halfspace(plane(0,0,-1,0))),1)"));

    evaluate_unit();

    const auto &P = *p->get_value();

    for (const auto &v: P.vertex_handles()) {
        const auto &A = v->point();

        if (A.z() > 0) {
            BOOST_TEST((CGAL::abs(A.x()) < FT(1)
                        && CGAL::abs(A.y()) < FT(1)
                        && A.z() < FT(1)));
        } else if (A.z() < -0.1) {
            // Some faired vertices (with initially positive z) might
            // end up with slightly negative z; let them slide.

            BOOST_TEST((CGAL::abs(A.x()) == 1
                        || CGAL::abs(A.y()) == 1
                        || A.z() == -1));
        }
    }
}

BOOST_DATA_TEST_CASE(smooth_shape, boost::unit_test::data::xrange(4), i)
{
    auto v = VERTICES_IN(BOUNDING_HALFSPACE(0, 0, 1, 0));
    auto f = FACES_IN(BOUNDING_HALFSPACE(0, 1, 0, 0));
    auto p = SMOOTH_SHAPE(
        REMESH(CUBOID(2, 2, 2), nullptr, nullptr, FT::ET(1, 10), 1),
        i & 1 ? f : nullptr, i & 2 ? v : nullptr,
        FT::ET(1, 100), 1);

    std::stringstream s;

    s << "smooth_shape(remesh(cuboid(2,2,2),1/10,1),";

    if (i & 1) {
        s << "faces_in(bounding_halfspace(plane(0,1,0,0))),";
    }

    if (i & 2) {
        s << "vertices_in(bounding_halfspace(plane(0,0,1,0))),";
    }

    s << "1/100,1)";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    const FT V = polyhedron_volume(*p->get_value());
    BOOST_TEST((V > FT(FT::ET(15, 2)) && V < 8));
}

BOOST_AUTO_TEST_CASE(deform)
{
    Tolerances::projection = FT::ET(1, 1'000'000);
    Tolerances::curve = FT::ET(1, 100);

    auto b = BOUNDING_HALFSPACE(0, 0, 1, -3);
    auto c = BOUNDING_HALFSPACE(0, 0, 1, 5);
    auto h = std::make_shared<Polyhedron_hull_operation>();
    h->push_back(TRANSFORM(SPHERE(1), TRANSLATION_3(0, 0, -5)));
    h->push_back(TRANSFORM(SPHERE(1), TRANSLATION_3(0, 0, 5)));
    auto a = HULL(h);

    auto p = DEFORM(
        REMESH(a, FACES_PARTIALLY_IN(b), nullptr, FT(FT::ET(1, 2)), 1),
        VERTICES_IN(b),
        {std::pair(VERTICES_IN(c), basic_rotation(90, 1))},
        FT::ET(1, 100), 1000);

    BOOST_TEST(
        p->describe() == ("deform(remesh(hull("
                          "transform(sphere(1,1/100,1/1000000),translation(0,0,-5)),"
                          "transform(sphere(1,1/100,1/1000000),translation(0,0,5))),"
                          "faces_partially_in("
                          "bounding_halfspace(plane(0,0,1,-3))),1/2,1),"
                          "vertices_in(bounding_halfspace(plane(0,0,1,-3))),"
                          "vertices_in(bounding_halfspace(plane(0,0,1,5))),"
                          "rotation(0,0,1,0,1,0,-1,0,0),1/100,1000)"));

    evaluate_unit();

#if 0
    CGAL::draw(*p->get_value());
#endif
}

BOOST_AUTO_TEST_CASE(deform_whole)
{
    Tolerances::curve = FT::ET(1, 100);

    auto b = TRANSFORM(BOUNDING_HALFSPACE(1, 0, 0, 0),
                       TRANSLATION_3(-2, 0, 0));
    auto p = DEFORM(
        REMESH(
            CUBOID(5, 1, 1),
            nullptr, EDGES_IN(BOUNDING_HALFSPACE(0, 0, 1, -1)),
            FT(FT::ET(1, 4)), 1),
        {std::pair(VERTICES_IN(b), basic_rotation(90, 0)),
         std::pair(VERTICES_IN(TRANSFORM(b, basic_rotation(180, 1))),
                   basic_rotation(-90, 0))},
        FT::ET(1, 100), 1000);

    BOOST_TEST(
        p->describe() == ("deform(remesh("
                          "cuboid(5,1,1),"
                          "edges_in(bounding_halfspace(plane(0,0,1,-1))),"
                          "1/4,1),"
                          "vertices_in(bounding_halfspace(plane(1,0,0,2))),"
                          "rotation(1,0,0,0,0,-1,0,1,0),"
                          "vertices_in(bounding_halfspace(plane(-1,0,0,2))),"
                          "rotation(1,0,0,0,0,1,0,-1,0),1/100,1000)"));

    evaluate_unit();

#if 0
    CGAL::draw(*p->get_value());
#endif
}

BOOST_AUTO_TEST_SUITE_END()
