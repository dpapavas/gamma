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

#include "kernel.h"
#include "transformations.h"
#include "macros.h"

#include "fixtures.h"
#include "circle_polygon_tests.h"

// Document: program

// # Circle Polygon Tests

// These are tests for circle polygon operations and related
// functionality.  We mainly test the results in two ways:

//   1. By testing the general "topology" of the results: A string of
//   the form "CC,CL LLLL" is provided which tests whether the result
//   is made up of two polygons, one having an outer boundary of two
//   circular (`C`) arcs and a hole made out of a circular arc (`C`)
//   and a line (`L`) segment and the other made up of 4 line (`L`)
//   segments and no holes.

//   2. By testing the total area of the resulting polygons
//   approximately.

// These tests are facilitated by the functions below.

FT polygon_area(const Circle_polygon_set &S)
{
    Polygon_set T;

    convert_circle_polygon_set(S, T, 0.001, FT::ET(1, 1000000));
    return polygon_area(T);
}

bool test_polygon_without_holes(const Circle_polygon &P,
                                const std::string_view &edges)
{
    std::string s;
    s.reserve(P.size());

    // Test if the given edge sequence is congruent (up to a circular
    // shift) to that of the given polygon.

    for (auto c = P.curves_begin(); c != P.curves_end(); c++) {
        s.push_back(c->is_linear() ? 'L' : 'C');
    }

    return (s.size() == edges.size()
            && (s + s).find(edges) != std::string::npos);
}

BOOST_FIXTURE_TEST_SUITE(circle_polygon, Evaluation_fixture)

// ## Primitive Circle Polygon Operation Tests

// These are tests for operations producing circle polygon primitives.

BOOST_AUTO_TEST_CASE(circle)
{
    const auto &result = evaluate(CIRCLE(2));

    evaluate_operations();

    BOOST_TEST(result.tag == "circle(2)");
    test_polygon(*result.value, "CC");
}

BOOST_DATA_TEST_CASE(segment,
                     (boost::unit_test::data::make({1, 3})
                      ^ boost::unit_test::data::make({"LC", "LCCC"})),
                     h, edges)
{
    const auto &result = evaluate(CIRCULAR_SEGMENT(2, FT::ET(h, 2)));

    evaluate_operations();

    BOOST_TEST(result.tag == tag("segment(2,", h, "/2)"));
    test_polygon(*result.value, edges);
}

BOOST_DATA_TEST_CASE(sector,
                     (boost::unit_test::data::make({70, 210})
                      ^ boost::unit_test::data::make({"LLC", "LLCC"})),
                     theta, edges)
{
    const auto &result = evaluate(CIRCULAR_SECTOR(2, theta));

    evaluate_operations();

    BOOST_TEST(result.tag == tag("sector(2,", theta, ")"));
    test_polygon(*result.value, edges);
}

// ## Circle Polygon Transformation Tests

// These are tests for transformations of circle polygons.

// Although a circle stays invariant under rotations, the following
// tests reassembly of x-monotone curves during transformation.  The
// important check is that each circle consists of 2 semicircular
// segments only.

BOOST_AUTO_TEST_CASE(transform_circle)
{
    const auto &result = evaluate(
        DIFFERENCE(
            TRANSFORM_CS(CIRCLE(2), basic_rotation(45)),
            TRANSFORM_CS(CIRCLE(FT::ET(1, 5)), basic_rotation(-45))));

    evaluate_operations();

    test_polygon(*result.value, "CC,CC");
}

BOOST_AUTO_TEST_CASE(transform_circles)
{
    const auto &result = evaluate(
        [] {
            Aff_transformation_2 X(-1, 0, 0, 1);
            Aff_transformation_2 Y(1, 0, 0, -1);

            auto a = INTERSECTION(
                TRANSFORM_CS(CIRCLE(2), TRANSLATION_2(-1, 0)),
                TRANSFORM_CS(CIRCLE(2), TRANSLATION_2(1, 0)));

            auto b = TRANSFORM_CS(
                a, basic_rotation(-45) * TRANSLATION_2(0, 2));
            auto c = JOIN(b, TRANSFORM_CS(b, X));

            return JOIN(c, TRANSFORM_CS(c, Y));
        });

    evaluate_operations();

    test_polygon(*result.value, "CCCC", "CCCC", "CCCC", "CCCC");
}

BOOST_AUTO_TEST_CASE(flush, * boost::unit_test::tolerance(0.0005))
{
    const auto &result = evaluate(
        DIFFERENCE(FLUSH(CIRCLE(2), -1, 0),
                   FLUSH(CIRCLE(1), -1, 0)));

    evaluate_operations();

    test_polygon_area(*result.value, std::acos(-1) * 3);
}

// ## Circle Polygon Coversion Tests

// These are tests for conversions between circle polygons and plain
// line polygons.

BOOST_AUTO_TEST_CASE(convert_rectangle)
{
    const auto &result = evaluate(
        CONVERT_TO<Circle_polygon_set>(RECTANGLE(3, 3)));

    evaluate_operations();

    BOOST_TEST(
        result.tag == ("circles(polygon(point(-3/2,-3/2),point(3/2,-3/2),"
                       "point(3/2,3/2),point(-3/2,3/2)))"));
    test_polygon(*result.value, "LLLL");
}

// Conversion from circle to plain polygons involves piecewise-linear
// approximation of the curves and the results are tested by area.

BOOST_AUTO_TEST_CASE(convert_segment, * boost::unit_test::tolerance(0.0025))
{
    const auto &result = evaluate(
        CONVERT_TO<Polygon_set>(CIRCULAR_SEGMENT(2, FT::ET(1, 2))));

    evaluate_operations();

    BOOST_TEST(result.tag == "segments(segment(2,1/2),1/1000,1/1000000)");
    test_polygon_area(*result.value, 1.5625 * std::acos(0.6) - 0.75);
}

BOOST_AUTO_TEST_CASE(convert_segments, * boost::unit_test::tolerance(0.001))
{
    const auto &result = evaluate(
        CONVERT_TO<Polygon_set>(
            DIFFERENCE(
                CIRCULAR_SEGMENT(3, 2), CIRCULAR_SEGMENT(2, FT::ET(1, 2)))));

    evaluate_operations();

    test_polygon_area(
        *result.value, ((1.5625 * 1.5625 * std::acos(-0.28) + 0.65625)
                          - (1.5625 * std::acos(0.6) - 0.75)));
}

BOOST_AUTO_TEST_CASE(convert_circles, * boost::unit_test::tolerance(0.001))
{
    const auto &result = evaluate(
        CONVERT_TO<Polygon_set>(
            JOIN(
                CIRCLE(FT::ET(1, 2)),
                DIFFERENCE(CONVERT_TO<Circle_polygon_set>(RECTANGLE(3, 3)),
                           CIRCLE(1)))));

    evaluate_operations();

    test_polygon_area(*result.value, 9 - 0.75 * std::acos(-1));
}

BOOST_AUTO_TEST_CASE(convert_sectors, * boost::unit_test::tolerance(0.001))
{
    const auto &result = evaluate(
        CONVERT_TO<Polygon_set>(
            JOIN(
                TRANSFORM_CS(
                    CIRCULAR_SECTOR(FT::ET(3, 2), 270),
                    basic_rotation(90)),
                TRANSFORM_CS(
                    CIRCULAR_SECTOR(FT(FT::ET(3, 2) - FT::ET(1, 10)), 90),
                    TRANSLATION_2(FT::ET(1, 10), FT::ET(1, 10))))));

    evaluate_operations();

    test_polygon_area(*result.value, std::atan(1) * 8.71);
}

// ## Circle Polygon Set Operation Tests

// These are tests for set operations on circle polygons.

BOOST_AUTO_TEST_CASE(join)
{
    const auto &result = evaluate(
        JOIN(CIRCLE(1),
             TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(-1, 0))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("join(circle(1),"
                              "circles(transform("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),"
                              "translation(-1,0))))"));
    test_polygon(*result.value, "CCLLL");
}

BOOST_AUTO_TEST_CASE(join_sectors)
{
    const auto &result = evaluate(
        JOIN(
            TRANSFORM_CS(CIRCULAR_SECTOR(FT::ET(3, 2), 270),
                         basic_rotation(90)),
            TRANSFORM_CS(CIRCULAR_SECTOR(FT(FT::ET(3, 2) - FT::ET(1, 10)), 90),
                         TRANSLATION_2(FT::ET(1, 10), FT::ET(1, 10)))));

    evaluate_operations();

    test_polygon(*result.value, "LLC", "LLCC");
}

BOOST_AUTO_TEST_CASE(join_mixed)
{
    const auto &result = evaluate(
        JOIN(
            TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(0, 1)),
            JOIN(
                TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(0, -1)),
                CONVERT_TO<Circle_polygon_set>(RECTANGLE(2, 2)))));

    evaluate_operations();

    test_polygon(*result.value, "CLCL");
}

BOOST_AUTO_TEST_CASE(difference)
{
    const auto &result = evaluate(
        DIFFERENCE(RECTANGLE(2, 2), CIRCLE(FT::ET(6, 5))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("difference("
                              "circles(polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1))),"
                              "circle(6/5))"));
    test_polygon(*result.value, "LLC", "LLC", "LLC", "LLC");
}

BOOST_AUTO_TEST_CASE(difference_segments)
{
    const auto &result = evaluate(
        DIFFERENCE(
            CIRCULAR_SEGMENT(3, 2), CIRCULAR_SEGMENT(2, FT::ET(1, 2))));

    evaluate_operations();

    test_polygon(*result.value, "LCLCCC");
}

BOOST_AUTO_TEST_CASE(intersection)
{
    const auto &result = evaluate(
        INTERSECTION(RECTANGLE(2, 2), CIRCLE(FT::ET(6, 5))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("intersection("
                              "circles(polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1))),"
                              "circle(6/5))"));
    test_polygon(*result.value, "LCLCLCLC");
}

BOOST_AUTO_TEST_CASE(symmetric_difference)
{
    const auto &result = evaluate(
        SYMMETRIC_DIFFERENCE(
            TRANSFORM_CS(CIRCLE(2), TRANSLATION_2(1, 0)),
            TRANSFORM_CS(CIRCLE(2), TRANSLATION_2(-1, 0))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("symmetric_difference("
                              "transform(circle(2),translation(1,0)),"
                              "transform(circle(2),translation(-1,0)))"));
    test_polygon(*result.value, "CCCC,CCCC");
}

BOOST_AUTO_TEST_CASE(
    symmetric_differences, * boost::unit_test::tolerance(0.001))
{
    const auto &result = evaluate(
        SYMMETRIC_DIFFERENCE(
            SYMMETRIC_DIFFERENCE(
                    TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(-1, 0)),
                    TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(1, 0))),
                SYMMETRIC_DIFFERENCE(
                    TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(0, -1)),
                    TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(0, 1)))));

    evaluate_operations();

    // The total area of the holes is $2(\pi - 2)$, so each of the
    // four parts has area 2.

    test_polygon_area(*result.value, 8);
}

BOOST_AUTO_TEST_CASE(mixed_operations)
{
    const auto &result = evaluate(
        JOIN(
            CIRCLE(FT::ET(1, 2)),
            DIFFERENCE(CONVERT_TO<Circle_polygon_set>(RECTANGLE(3, 3)),
                       CIRCLE(1))));

    evaluate_operations();

    test_polygon(*result.value, "LLLL,CC", "CC");
}

// ## Circle Polygon Components Operation Tests

// This test ensures that both outer boundaries and holes are properly
// extracted.

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.002))
BOOST_DATA_TEST_CASE(components, boost::unit_test::data::xrange(4), n)
{
    auto A = DIFFERENCE(CIRCLE(2), CIRCLE(1)), B = A;

    // We set up a set of three circular rings placed on the x axis,
    // symmetrically around the origin.

    for (int i = -1; i < 2; i += 2) {
        B = JOIN(B, TRANSFORM_CS(A, TRANSLATION_2(5 * i, 0)));
    }

    // The first two test cases select the first and last ring or hole
    // respectively.  The other two also perform a symmetric
    // difference with a rotated version, to test that the polygons
    // centered at $\pm 5$ have been selected.

    const std::initializer_list<int> v[2] = {{1, 2, 5, 6}, {6, 2}};
    B = COMPONENTS(B, v[n % 2]);

    if (n > 1) {
        B = SYMMETRIC_DIFFERENCE(B, TRANSFORM_CS(B, basic_rotation(180)));
    }

    const auto &result = evaluate(B);

    A.reset();
    B.reset();

    evaluate_operations();

    const auto &P = *result.value;

    // There first two cases are:

    switch (n) {
        //   1. two rings, i.e. two circles with circular holes and

    case 0:
        test_polygon(P, "CC,CC", "CC,CC");
        test_polygon_area(P, 2 * (4 - 1) * std::acos(-1.0));
        break;

        //   2. two "holes", i.e. two circles.

    case 1:
        test_polygon(P, "CC", "CC");
        test_polygon_area(P, 2 * std::acos(-1.0));
        break;

    // In the last two cases, due to symmetry, rotation by 180 degrees
    // should leave the result unchanged, and the symmetric difference
    // operation should return an empty set.

    default:
        BOOST_TEST(P.is_empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()
