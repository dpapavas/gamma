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
#include <boost/mpl/list.hpp>

#include <iostream>

#include "options.h"
#include "kernel.h"
#include "macros.h"
#include "transformations.h"

#include "fixtures.h"
#include "polyhedron_tests.h"

#include <CGAL/Polygon_mesh_processing/measure.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

// Document: program

// # Chamfering Operation Tests

// We need to test result quality as well as correctnes.  The latter
// is taken care of by comparing the volume of the result, while the
// former is tested with the fixture below.

struct Chamfering_fixture: Evaluation_fixture {
    Chamfering_fixture() {
        Tolerances::curve = FT(FT::ET(1, 500));
    }

    template<typename T>
    void test_face_area(const T &P, const FT &A) {
        T Q(P);
        CGAL::Polygon_mesh_processing::triangulate_faces(Q);

        // We test for the absence of almost degenerate faces, by
        // requring a minimum face area.

        for (auto &x: CGAL::faces(Q)) {
            BOOST_TEST(CGAL::Polygon_mesh_processing::face_area(x, Q) > A * A);
        }
    }
};

BOOST_FIXTURE_TEST_SUITE(chamfer, Chamfering_fixture)

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(1e-9))
BOOST_AUTO_TEST_CASE(cube)
{
    // The vertices of the rectangular faces of an equilateral
    // chamfered cube with unit edges, are given by all permutations
    // and changes of sign of $(\frac{1}{2}, \frac{1}{2}, \frac{1}{2}
    // + \frac{2\sqrt{3}}{3})$.  This then means, that the distance
    // from the origin to the rectangular faces is $\frac{1}{2} +
    // \frac{2\sqrt{3}}{3}$, so that the edge length of the
    // unchamfered cube was:

    const FT a = 1.0 + 4.0 * sqrt(3.0) / 3.0;

    // The required chamfer length therefore is:

    const FT l = (a - 1.0) / 2.0;

    // We construct that cube and test that its edges are indeed of
    // unit length, after remeshing, as corefinement introduces other
    // edges via triangulation.

    const auto &result = evaluate(
        CHAMFER(
            CUBOID(a, a, a), nullptr, l, l,
            Chamfering_operation_mode::OUTER));

    evaluate_operations();

    auto &P = *result.value;
    std::remove_reference_t<decltype(P)> Q;

    CGAL::Polygon_mesh_processing::remesh_planar_patches(
        P, Q,
        CGAL::parameters::default_values(),
        CGAL::parameters::do_not_triangulate_faces(true));

    for (const auto &x: CGAL::edges(Q)) {
        BOOST_TEST(
            CGAL::to_double(
                CGAL::Polygon_mesh_processing::squared_edge_length(x, Q))
            == 1.0);
    }

    // We also test the volume of the chamfered cube. It is:

    test_polyhedron_volume(P, 9.0 + 52.0 / 3.0 / sqrt(3.0));
    test_face_area(P, FT(FT::ET(1, 10)));
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(1e-9))
BOOST_AUTO_TEST_CASE(cube_2)
{
    const auto &result = evaluate(
        FILLET(
            CUBOID(2, 2, 2), nullptr, FT::ET(1, 5),
            Chamfering_operation_mode::OUTER));

    evaluate_operations();

    const auto &P = *result.value;
    test_face_area(P, FT(FT::ET(1, 200)));
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.0025))
BOOST_AUTO_TEST_CASE(prism)
{
    // We chamfer the 90 degree edges of an octagonal prism.

    const auto &result = evaluate(
        CHAMFER(
            PRISM(8, 5, 2),
            EDGES_BY_SHARPNESS_ANGLE(90), FT::ET(1, 2), FT::ET(1, 2),
            Chamfering_operation_mode::OUTER));

    evaluate_operations();

    const auto &P = *result.value;

    // The volume of the prism, is $2 \sqrt{2} R^2 H$.  The volume
    // removed by the fillet, is roughly that swept by a profile of area
    // $\frac{r^2}{2}$, along the circumference of the octagon, which
    // is $8 R \sqrt{2 - \sqrt{2}}$.

    test_polyhedron_volume(
        P,
        2.0 * sqrt(2.0) * 25.0 * 2.0
        - 5.0 * sqrt(2.0 - sqrt(2.0)) * 8.0 * (0.25 / 2.0) * 2.0);

    test_face_area(P, FT(FT::ET(1, 10)));
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.002))
BOOST_AUTO_TEST_CASE(prism_2)
{
    // We fillet the 90 degree edges of an octagonal prism.

    const auto &result = evaluate(
        FILLET(
            PRISM(8, 5, 2),
            EDGES_BY_SHARPNESS_ANGLE(90), FT::ET(1, 2),
            Chamfering_operation_mode::OUTER));

    evaluate_operations();

    const auto &P = *result.value;

    // The volume of the prism, is $2 \sqrt{2} R^2 H$.  The volume
    // removed by the fillet, is roughly that swept by a profile of
    // area $r^2 - \frac{\pi r^2}{4}$^[The profile is a square of side
    // equal to the fillet radius $r$ minus the fillet quarter disk.],
    // along the circumference of the octagon, which is $8 R \sqrt{2 -
    // \sqrt{2}}$.

    test_polyhedron_volume(
        P,
        2.0 * sqrt(2.0) * 25.0 * 2.0
        - 5.0 * sqrt(2.0 - sqrt(2.0)) * 8.0 * (0.25 - M_PI * 0.25 / 4.0) * 2.0);

    test_face_area(P, FT(FT::ET(1, 200)));
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.001))
BOOST_AUTO_TEST_CASE(donut)
{
    // To test decomposition of disconnected edge components, we
    // chamfer a prism like above, but with a hole.

    const auto &result = evaluate(
        CHAMFER(
            DIFFERENCE(PRISM(8, 4, 2), PRISM(8, 2, 2)),
            EDGES_BY_SHARPNESS_ANGLE(90), FT::ET(1, 5), FT::ET(1, 5),
            Chamfering_operation_mode::OUTER));

    evaluate_operations();

    const auto &P = *result.value;

    // See `prism` for details on the volume calculation.

    test_polyhedron_volume(
        P,
        2.0 * sqrt(2.0) * 16.0 * 2.0
        - 4.0 * sqrt(2.0 - sqrt(2.0)) * 8.0 * (0.04 / 2.0) * 2.0

        - 2.0 * sqrt(2.0) * 4.0 * 2.0
        - 2.0 * sqrt(2.0 - sqrt(2.0)) * 8.0 * (0.04 / 2.0) * 2.0);

    test_face_area(P, FT(FT::ET(1, 200)));
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.001))
BOOST_AUTO_TEST_CASE(inner_cubes)
{
    auto A = CUBOID(4, 4, 4);

    // We test inner chamfers, on the following geometry:

    for (int i = -1; i < 2; i += 2) {
        for (int j = -1; j < 2; j += 2) {
            for (int k = -1; k < 2; k += 2) {
                A = DIFFERENCE(
                    A, TRANSFORM(
                        CUBOID(2, 2, 2), TRANSLATION_3(2 * i, 2 * j, 2 * k)));
            }
        }
    }

    const auto &result = evaluate(
        CHAMFER(
            A, nullptr, FT::ET(1, 4), FT::ET(1, 4),
            Chamfering_operation_mode::INNER));

    A.reset();
    evaluate_operations();

    // Since the volume we cut out from the large cube can be
    // assembled to form the original cube with side length 2, when we
    // add the inner chamfers, the cut out volume now becomes that of
    // a chamfered cube.  According to the Japanese Wikiepdia the
    // volume of a chamfered cube of original side length 2, where the
    // square faces after chamfering have side length $d$, is
    // $\frac{-3 d^3}{4} + \frac{3 d^2}{2} + 3 d + 2$.

    const auto &P = *result.value;
    const FT d = 2 - 2 * FT(FT::ET(1, 4));

    test_polyhedron_volume(P, 64 - (-3 * d * d * d / 4 + 3 * d * d / 2 + 3 * d + 2));
    test_face_area(P, FT(FT::ET(1, 500)));
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.001))
BOOST_AUTO_TEST_CASE(inner_cubes_2)
{
    auto A = CUBOID(4, 4, 4), B = CUBOID(2, 2, 2);

    // We test inner fillets, on the same geometry as for chamfers.
    // By the same reasoning, the removed volume is now that of a
    // filleted cube of side 2, but since we can't compute it
    // analytically, we simply add it back to the geometry and expect
    // the total to add up to the cube of side 4.

    for (int i = -1; i < 2; i += 2) {
        for (int j = -1; j < 2; j += 2) {
            for (int k = -1; k < 2; k += 2) {
                A = DIFFERENCE(
                    A, TRANSFORM(
                        B, TRANSLATION_3(2 * i, 2 * j, 2 * k)));
            }
        }
    }

    const auto &result = evaluate(
        JOIN(
            TRANSFORM(
                FILLET(
                    B, nullptr, FT::ET(1, 4),
                    Chamfering_operation_mode::OUTER), TRANSLATION_3(6, 0, 0)),
            FILLET(
                A, EDGES_BY_SHARPNESS_ANGLE(90), FT::ET(1, 4),
                Chamfering_operation_mode::INNER)));

    A.reset();
    B.reset();

    evaluate_operations();

    const auto &P = *result.value;

    test_polyhedron_volume(P, 64);
    test_face_area(P, FT(FT::ET(1, 5000)));
}

BOOST_AUTO_TEST_CASE(icosahedron)
{
    // As a sort of stress test on edge decomposition, we chamfer all
    // edges of an icosahedron.

    const auto &result = evaluate(
        CHAMFER(
            ICOSAHEDRON(2),
            nullptr, FT::ET(1, 5), FT::ET(1, 5),
            Chamfering_operation_mode::OUTER));

    evaluate_operations();

    const auto &P = *result.value;
    test_face_area(P, FT(FT::ET(1, 200)));
}

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES(icosahedron_2, 50)
BOOST_AUTO_TEST_CASE(icosahedron_2, * boost::unit_test::disabled())
{
    // Like above, but meant more as a stress test on result quality
    // in terms of geometry.  Some almost degenerate triangles are
    // produced, hence the expected failures.

    // We disable this by default as it can take a while to run.

    const auto &result = evaluate(
        FILLET(
            ICOSAHEDRON(2),
            nullptr, FT::ET(1, 5),
            Chamfering_operation_mode::OUTER));

    evaluate_operations();

    const auto &P = *result.value;
    test_face_area(P, FT(FT::ET(1, 200)));
}

BOOST_DATA_TEST_CASE(geometry,
                     (boost::unit_test::data::make({false, true})
                      * boost::unit_test::data::make({
                              static_cast<int>(Chamfering_operation_mode::OUTER),
                              static_cast<int>(Chamfering_operation_mode::INNER)})),
                     p, mode)
{
    auto A = JOIN(PRISM(5, 1, 4), PRISM(5, 2, 2));

    // We chamfer or fillet a simple object with both inner and outer
    // edges, testing the generated chamfer or fillet geometry.

    const auto &result = evaluate(
        p
        ? MAKE_FILLET(
            A, EDGES_BY_SHARPNESS_ANGLE(72), FT::ET(1, 5),
            static_cast<Chamfering_operation_mode>(mode))
        : MAKE_CHAMFER(
            A, EDGES_BY_SHARPNESS_ANGLE(72), FT::ET(1, 5), FT::ET(1, 5),
            static_cast<Chamfering_operation_mode>(mode)));

    A.reset();
    evaluate_operations();

    const auto &P = *result.value;
    test_face_area(P, FT(FT::ET(1, p ? 100000 : 100)));
}

// We test cases where the chamfering geometry would self-intersect if
// all segments were joined by mitering.  To do so we chamfer the top
// and bottom faces of an extruded chamfered rectangle.  The short
// oblique faces at the corners makes non-adjacent chamfer segments
// intersect past some chamfer length.

BOOST_AUTO_TEST_CASE(large)
{
    const auto &result = evaluate(
        CHAMFER(
            EXTRUSION(
                MINKOWSKI_SUM(REGULAR_POLYGON(4, 1), RECTANGLE(5, 5)),
                {TRANSLATION_3(0, 0, -3), TRANSLATION_3(0, 0, 3)}),
            EDGES_BY_SHARPNESS_ANGLE(90), 2, 2,
            Chamfering_operation_mode::OUTER));

    evaluate_operations();

    const auto &P = *result.value;
    test_face_area(P, FT(FT::ET(1, 200)));
}

// This is the same as above, only for filleting inner edges.

BOOST_AUTO_TEST_CASE(large_2)
{
    const auto &result = evaluate(
        FILLET(
            DIFFERENCE(
                EXTRUSION(
                    RECTANGLE(2, 2),
                    {TRANSLATION_3(0, 0, 0), TRANSLATION_3(0, 0, 2)}),
                EXTRUSION(
                    MINKOWSKI_SUM(
                        REGULAR_POLYGON(4, FT::ET(1, 8)), RECTANGLE(1, 1)),
                    {TRANSLATION_3(0, 0, 1), TRANSLATION_3(0, 0, 2)})),
            INTERSECTION({
                EDGES_BY_SHARPNESS_ANGLE(90),
                EDGES_IN(BOUNDING_PLANE(0, 0, 1, -1))}), FT::ET(3, 8),
            Chamfering_operation_mode::INNER));

    evaluate_operations();

    const auto &P = *result.value;
    test_face_area(P, FT(FT::ET(1, 200)));
}

BOOST_AUTO_TEST_SUITE_END()
