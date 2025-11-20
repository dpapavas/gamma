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
#include "conic_polygon_tests.h"

FT polygon_area(const Conic_polygon_set &S)
{
    Polygon_set T;

    convert_conic_polygon_set(S, T, 0.001);
    return polygon_area(T);
}

bool test_polygon_without_holes(const Conic_polygon &P,
                                const std::string_view &edges)
{
    std::string s;
    s.reserve(P.size());

    // Test the given edge sequence (L for linear, C for circular and
    // E for elliptic segment) against the given polygon.

    for (auto c = P.curves_begin(); c != P.curves_end(); c++) {
        if (c->orientation() == CGAL::COLLINEAR) {
            s.push_back('L');
        } else if (CGAL::sign(4 * c->r() * c->s() - c->t() * c->t())
                   == CGAL::POSITIVE) {
            s.push_back(
                (c->r() == c->s()
                 && CGAL::sign(c->t()) == CGAL::ZERO) ? 'C' : 'E');
        } else {
            s.push_back('?');
        }
    }

    return (s + s).find(edges) != std::string::npos;
}

BOOST_FIXTURE_TEST_SUITE(conic_polygon, Evaluation_fixture)

////////////////////////////////////
// Conversion from other polygons //
////////////////////////////////////

BOOST_AUTO_TEST_CASE(convert_rectangle)
{
    const auto &result = evaluate(
        CONVERT_TO<Conic_polygon_set>(RECTANGLE(3, 3)));;

    evaluate_operations();

    BOOST_TEST(result.tag == ("conics(polygon(point(-3/2,-3/2),point(3/2,-3/2),"
                              "point(3/2,3/2),point(-3/2,3/2)))"));
    test_polygon(*result.value, "LLLL");
}

BOOST_AUTO_TEST_CASE(convert_circle)
{
    const auto &result = evaluate(CONVERT_TO<Conic_polygon_set>(CIRCLE(2)));

    evaluate_operations();

    BOOST_TEST(result.tag == "conics(circle(2))");
    test_polygon(*result.value, "CC");
}

BOOST_DATA_TEST_CASE(convert_segment,
                     (boost::unit_test::data::make({1, 3})
                      ^ boost::unit_test::data::make({"LC", "LCCC"})),
                     h, edges)
{
    const auto &result = evaluate(
        CONVERT_TO<Conic_polygon_set>(CIRCULAR_SEGMENT(1, FT::ET(h, 2))));

    evaluate_operations();

    BOOST_TEST(result.tag == tag("conics(segment(1,", h, "/2))"));
    test_polygon(*result.value, edges);
}

BOOST_DATA_TEST_CASE(convert_sector,
                     (boost::unit_test::data::make({60, 200})
                      ^ boost::unit_test::data::make({"LLC", "LLCC"})),
                     theta, edges)
{
    const auto &result = evaluate(
        CONVERT_TO<Conic_polygon_set>(CIRCULAR_SECTOR(1, theta)));

    evaluate_operations();

    BOOST_TEST(result.tag == tag("conics(sector(1,", theta, "))"));
    test_polygon(*result.value, edges);
}

/////////////////////
// Transformations //
/////////////////////

BOOST_DATA_TEST_CASE(transform_circles,
                     boost::unit_test::data::make({
                             TRANSLATION_2(2, 2),
                             basic_rotation(15),
                             TRANSLATION_2(2, 2) * basic_rotation(1)}), T)
{
    // Transformations resulting in non-uniform scaling of circles
    // (and only those), should result in conics.  We start with plain
    // translation and/or rotation.

    {
        const auto &result = evaluate(TRANSFORM_CS(CIRCLE(1), T));

        evaluate_operations();

        test_polygon(*result.value, "CC");
    }

    // Add uniforma scaling; we should still end up with a circle.

    {
        const auto &result = evaluate(
            TRANSFORM_CS(CIRCLE(1), SCALING_2(2, 2) * T));

        evaluate_operations();

        test_polygon(*result.value, "CC");
    }

    // Non-uniform scaling; expect an ellipse.

    {
        const auto &result = evaluate(
            TRANSFORM_C(CIRCLE(1), SCALING_2(2, 3) * T));

        evaluate_operations();

        test_polygon(*result.value, "EE");
    }

    // Shearing; again, expect an ellipse.

    {
        const auto &result = evaluate(
            TRANSFORM_C(CIRCLE(1), Aff_transformation_2(1, 1, 0, 1) * T));

        evaluate_operations();

        test_polygon(*result.value, "EE");
    }
}

BOOST_AUTO_TEST_CASE(transform_curves)
{
    const auto &result = evaluate(
        DIFFERENCE(TRANSFORM(ELLIPSE(4, 2), basic_rotation(45)),
                   TRANSFORM(ELLIPTIC_SECTOR(1, 1, 180),
                             basic_rotation(-45))));

    evaluate_operations();

    test_polygon(*result.value, "EE,CCL");
}

BOOST_AUTO_TEST_CASE(flush, * boost::unit_test::tolerance(0.0005))
{
    const auto &result = evaluate(
        DIFFERENCE(FLUSH(ELLIPSE(2, 4), 0, -1),
                   FLUSH(ELLIPSE(1, 2), 0, -1)));

    evaluate_operations();

    test_polygon_area(*result.value, std::acos(-1) * 6);
}

// Primitives (converted and transformed)

BOOST_AUTO_TEST_CASE(ellipse)
{
    const auto &result = evaluate(ELLIPSE(4, 2));

    evaluate_operations();

    BOOST_TEST(result.tag == "transform(conics(circle(1)),scaling(4,2))");
    test_polygon(*result.value, "EE");
}

BOOST_DATA_TEST_CASE(sector,
                     (boost::unit_test::data::make({100, 280})
                      ^ boost::unit_test::data::make({"LLE", "LLEE"})),
                     theta, edges)
{
    const auto &result = evaluate(ELLIPTIC_SECTOR(4, 2, theta));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag(
            "transform(conics(sector(1,", theta, ")),scaling(4,2))"));
    test_polygon(*result.value, edges);
}

////////////////////
// Set operations //
////////////////////

BOOST_AUTO_TEST_CASE(join)
{
    const auto &result = evaluate(
        JOIN(TRANSFORM(RECTANGLE(4, 4), TRANSLATION_2(2, 0)),
             ELLIPSE(4, 2)));

    evaluate_operations();

    BOOST_TEST(result.tag == ("join("
                              "conics(transform("
                              "polygon(point(-2,-2),point(2,-2),"
                              "point(2,2),point(-2,2)),"
                              "translation(2,0))),"
                              "transform(conics(circle(1)),scaling(4,2)))"));
    test_polygon(*result.value, "EELLL");
}

BOOST_AUTO_TEST_CASE(difference)
{
    const auto &result = evaluate(
        DIFFERENCE(
            ELLIPSE(4, 2), TRANSFORM(RECTANGLE(4, 4), TRANSLATION_2(2, 0))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("difference("
                              "transform(conics(circle(1)),scaling(4,2)),"
                              "conics(transform("
                              "polygon(point(-2,-2),point(2,-2),"
                              "point(2,2),point(-2,2)),"
                              "translation(2,0))))"));
    test_polygon(*result.value, "EEL");
}

BOOST_AUTO_TEST_CASE(intersection)
{
    const auto &result = evaluate(
        INTERSECTION(ELLIPSE(2, 4), CIRCLE(2)));

    evaluate_operations();

    BOOST_TEST(result.tag == ("intersection(transform(conics(circle(1)),scaling(2,4)),"
                              "conics(circle(2)))"));
    test_polygon(*result.value, "CC");
}

BOOST_AUTO_TEST_CASE(intersections)
{
    const auto &result = evaluate(
        INTERSECTION(
            INTERSECTION(
                TRANSFORM(ELLIPSE(2, 4), TRANSLATION_2(0, 3)),
                TRANSFORM_CS(CIRCLE(3), TRANSLATION_2(-1, 0))),
            INTERSECTION(
                TRANSFORM_CS(CIRCLE(3), TRANSLATION_2(1, 0)),
                TRANSFORM(RECTANGLE(6, 6), TRANSLATION_2(0, 3)))));

    evaluate_operations();

    test_polygon(*result.value, "CCELE");
}

BOOST_AUTO_TEST_CASE(egg)
{
    const auto &result = evaluate(
        INTERSECTION(JOIN(TRANSFORM(RECTANGLE(4, 4), TRANSLATION_2(0, 2)),
                          CIRCLE(2)),
                     ELLIPSE(2, 4)));

    evaluate_operations();

    BOOST_TEST(result.tag == ("intersection(conics(join(circles("
                              "transform(polygon(point(-2,-2),point(2,-2),"
                              "point(2,2),point(-2,2)),"
                              "translation(0,2))),circle(2))),"
                              "transform(conics(circle(1)),scaling(2,4)))"));
    test_polygon(*result.value, "EC");
}

BOOST_AUTO_TEST_CASE(symmetric_difference)
{
    const auto &result = evaluate(
        SYMMETRIC_DIFFERENCE(
            ELLIPTIC_SECTOR(4, 2, 270),
            DIFFERENCE(
                ELLIPSE(4, 2),
                TRANSFORM(RECTANGLE(4, 2),
                          TRANSLATION_2(2, -1)))));

    evaluate_operations();

    BOOST_TEST(
        result.tag == (
            "symmetric_difference("
            "transform(conics(sector(1,270)),scaling(4,2)),"
            "difference(transform(conics(circle(1)),scaling(4,2)),"
            "conics(transform("
            "polygon(point(-2,-1),point(2,-1),point(2,1),point(-2,1)),"
            "translation(2,-1)))))"));
    BOOST_TEST(result.value->is_empty());
}

BOOST_AUTO_TEST_CASE(mixed_operations)
{
    const auto &result = evaluate(
        JOIN(
            ELLIPSE(1, 1),
            DIFFERENCE(ELLIPSE(6, 3), ELLIPSE(2, 2))));

    evaluate_operations();

    test_polygon(*result.value, "EE,CC", "CC");
}

/////////////////////////////
// Conversions to segments //
/////////////////////////////

BOOST_AUTO_TEST_CASE(convert_ellipses, * boost::unit_test::tolerance(0.001))
{
    const auto &result = evaluate(
        CONVERT_TO<Polygon_set>(
            JOIN(ELLIPSE(1, 1),
                 DIFFERENCE(ELLIPSE(6, 3), ELLIPSE(2, 2)))));

    evaluate_operations();

    test_polygon_area(*result.value, std::acos(-1) * (6 * 3 - 4 + 1));
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.001))
BOOST_DATA_TEST_CASE(convert_elliptic_sector,
                     (boost::unit_test::data::make({0, 1})
                      ^ boost::unit_test::data::make({"LLE", "LLEE"})),
                     i, edges)
{
    const auto &result = evaluate(
        CONVERT_TO<Polygon_set>(ELLIPTIC_SECTOR(4, 2, i * 180 + 45)));

    evaluate_operations();

    test_polygon_area(*result.value, std::acos(-1) * (4 * i + 1));
}

BOOST_AUTO_TEST_SUITE_END()
