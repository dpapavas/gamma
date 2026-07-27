// Copyright 2022, 2026 Dimitris Papavasiliou

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

#include <CGAL/draw_polyhedron.h>
#include <CGAL/draw_surface_mesh.h>

BOOST_FIXTURE_TEST_SUITE(deform, Evaluation_fixture)

BOOST_AUTO_TEST_CASE(fair)
{
    const auto &result = evaluate(
        FAIR(
            REMESH(
                CUBOID(2, 2, 2),
                nullptr, EDGES_IN(BOUNDING_HALFSPACE(0, 0, 1, -1)),
                FT(FT::ET(1, 3)), 1),
            VERTICES_IN(BOUNDING_HALFSPACE(0, 0, -1, 0)), 1));

    evaluate_operations();

    BOOST_TEST(
        result.tag == (
            "fair(remesh(cuboid(2,2,2),"
            "edges_in(bounding_halfspace(plane(0,0,1,-1))),1/3,1),"
            "vertices_in(bounding_halfspace(plane(0,0,-1,0))),1)"));

    const auto &P = *result.value;

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
    const auto &result = evaluate(
        SMOOTH_SHAPE(
            REMESH(CUBOID(2, 2, 2), nullptr, nullptr, FT::ET(1, 10), 1),
            i & 1 ? FACES_IN(BOUNDING_HALFSPACE(0, 1, 0, 0)) : nullptr,
            i & 2 ? VERTICES_IN(BOUNDING_HALFSPACE(0, 0, 1, 0)) : nullptr,
            FT::ET(1, 100), 1));

    evaluate_operations();

    std::stringstream s;

    s << "smooth_shape(remesh(cuboid(2,2,2),1/10,1),";

    if (i & 1) {
        s << "faces_in(bounding_halfspace(plane(0,1,0,0))),";
    }

    if (i & 2) {
        s << "vertices_in(bounding_halfspace(plane(0,0,1,0))),";
    }

    s << "1/100,1)";

    BOOST_TEST(result.tag == s.str());

    const FT V = polyhedron_volume(*result.value);
    BOOST_TEST((V > FT(FT::ET(15, 2)) && V < 8));
}

BOOST_AUTO_TEST_CASE(deform)
{
    Tolerances::curve = FT::ET(1, 100);

    const auto &result = evaluate(
        [] {
            auto h = POLYHEDRON_HULL_OPEN();
            h->push_back(
                TRANSFORM(SPHERE(FT::ET(3, 4)), TRANSLATION_3(0, 0, -5)));
            h->push_back(
                TRANSFORM(SPHERE(FT::ET(3, 4)), TRANSLATION_3(0, 0, 5)));

            return DEFORM(
                REMESH(
                    std::static_pointer_cast<Polyhedron_operation<Polyhedron>>(
                        POLYHEDRON_HULL_CLOSE(h)),
                    nullptr, nullptr, FT(FT::ET(1, 2)), 1),
                VERTICES_IN(BOUNDING_HALFSPACE(0, 0, 1, -3)),
                {std::pair(
                        VERTICES_IN(BOUNDING_HALFSPACE(0, 0, 1, 5)),
                        basic_rotation(90, 1))},
                FT::ET(1, 100), 1000);
        });

    evaluate_operations();

    BOOST_TEST(
        result.tag == (
            "deform(remesh(hull("
            "transform(sphere(3/4,1/100,1/1000000),translation(0,0,-5)),"
            "transform(sphere(3/4,1/100,1/1000000),translation(0,0,5))),1/2,1),"
            "vertices_in(bounding_halfspace(plane(0,0,1,-3))),"
            "vertices_in(bounding_halfspace(plane(0,0,1,5))),"
            "rotation(0,0,1,0,1,0,-1,0,0),1/100,1000)"));
}

BOOST_AUTO_TEST_CASE(deform_whole)
{
    auto b = TRANSFORM(BOUNDING_HALFSPACE(1, 0, 0, 0),
                       TRANSLATION_3(-2, 0, 0));
    const auto &result = evaluate(
        DEFORM(
            REMESH(
                CUBOID(5, 1, 1),
                nullptr, EDGES_IN(BOUNDING_HALFSPACE(0, 0, 1, -1)),
                FT(FT::ET(1, 4)), 1),
            {std::pair(VERTICES_IN(b), basic_rotation(90, 0)),
             std::pair(VERTICES_IN(TRANSFORM(b, basic_rotation(180, 1))),
                       basic_rotation(-90, 0))},
            FT::ET(1, 100), 1000));

    evaluate_operations();

    BOOST_TEST(
        result.tag == ("deform(remesh("
                       "cuboid(5,1,1),"
                       "edges_in(bounding_halfspace(plane(0,0,1,-1))),"
                       "1/4,1),"
                       "vertices_in(bounding_halfspace(plane(1,0,0,2))),"
                       "rotation(1,0,0,0,0,-1,0,1,0),"
                       "vertices_in(bounding_halfspace(plane(-1,0,0,2))),"
                       "rotation(1,0,0,0,0,1,0,-1,0),1/100,1000)"));
}

BOOST_AUTO_TEST_SUITE_END()
