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

#include <cmath>
#include <sstream>

#include "kernel.h"
#include "transformations.h"
#include "macros.h"

#include "fixtures.h"
#include "polygon_tests.h"

// Document: program

// # Polygon Tests

// These are tests for plain polygon operations and related
// functionality.  We mainly test the results in two ways:

//   1. By testing total number of polygons, holes and vertices in the
//   resulting polygon set.

//   2. By testing the total area.

// The latter test is facilitated by the function below.

FT polygon_area(const Polygon_set &S)
{
    FT x(0);

    for_each_polygon(
        S, [&x](const Polygon &G) {
            x += G.area();
        });

    return x;
}

BOOST_FIXTURE_TEST_SUITE(polygon, Evaluation_fixture)

// ## Primitive Polygon Operation Tests

// These are tests for operations producing polygon primitives.

BOOST_AUTO_TEST_CASE(simple)
{
    const auto &result = evaluate(
        POLYGON(
            std::vector<Point_2>({
                    Point_2(-1, 0),
                    Point_2(1, 0),
                    Point_2(0, 1)})));

    evaluate_operations();

    BOOST_TEST(result.tag == "polygon(point(-1,0),point(1,0),point(0,1))");
    test_polygon(*result.value, 1, 0, 3, 1);
}

BOOST_AUTO_TEST_CASE(regular, * boost::unit_test::tolerance(1e-9))
{
    const auto &result = evaluate(REGULAR_POLYGON(12, 3));

    evaluate_operations();

    BOOST_TEST(result.tag == "regular_polygon(12,3,1/1000000)");
    test_polygon(*result.value, 1, 0, 12, 27.0);
}

BOOST_AUTO_TEST_CASE(rectangle)
{
    const auto &result = evaluate(RECTANGLE(2, 3));

    evaluate_operations();

    BOOST_TEST(result.tag == ("polygon(point(-1,-3/2),point(1,-3/2),"
                              "point(1,3/2),point(-1,3/2))"));
    test_polygon(*result.value, 1, 0, 4, 6);
}

// ## Polygon Set Operation Tests

// These are tests for set operations on polygons.

BOOST_AUTO_TEST_CASE(set_operation_result)
{
    using T = std::tuple<Polygon_set, Circle_polygon_set, Conic_polygon_set>;

#define TEST_2(I, J)                                                    \
    static_assert(std::is_same_v<                                       \
                  Polygon_set_operation_result<                         \
                  std::tuple_element_t<I, T>, std::tuple_element_t<J, T>>, \
                  Polygon_operation<std::tuple_element_t<std::max(I, J), T>>>);

#define TEST_1(I) TEST_2(I, 0) TEST_2(I, 1) TEST_2(I, 2)

    TEST_1(0) TEST_1(1) TEST_1(2)

#undef TEST_1
#undef TEST_2
}

BOOST_AUTO_TEST_CASE(join)
{
    const auto &result = evaluate(
        JOIN(RECTANGLE(2, 2),
             TRANSFORM(RECTANGLE(4, 4), TRANSLATION_2(2, 2))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("join("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),"
                              "transform("
                              "polygon(point(-2,-2),point(2,-2),"
                              "point(2,2),point(-2,2)),"
                              "translation(2,2)))"));
    test_polygon(*result.value, 1, 0, 8, 19);
}

BOOST_AUTO_TEST_CASE(difference)
{
    const auto &result = evaluate(DIFFERENCE(RECTANGLE(3, 4), RECTANGLE(1, 2)));

    evaluate_operations();

    BOOST_TEST(result.tag == ("difference("
                              "polygon(point(-3/2,-2),point(3/2,-2),"
                              "point(3/2,2),point(-3/2,2)),"
                              "polygon(point(-1/2,-1),point(1/2,-1),"
                              "point(1/2,1),point(-1/2,1)))"));

    test_polygon(*result.value, 1, 1, 8, 10);
}

BOOST_AUTO_TEST_CASE(intersection)
{
    const auto &result = evaluate(
        INTERSECTION(RECTANGLE(2, 2),
                     TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(1, 1))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("intersection("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),"
                              "transform("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),"
                              "translation(1,1)))"));
    test_polygon(*result.value, 1, 0, 4, 1);
}

BOOST_AUTO_TEST_CASE(symmetric_difference)
{
    const auto &result = evaluate(
        SYMMETRIC_DIFFERENCE(
            RECTANGLE(2, 2),
            TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(1, 1))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("symmetric_difference("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),"
                              "transform("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),"
                              "translation(1,1)))"));
    test_polygon(*result.value, 1, 1, 12, 6);
}

BOOST_AUTO_TEST_CASE(complement_simple)
{
    const auto &result = evaluate(COMPLEMENT(RECTANGLE(1, 2)));

    evaluate_operations();

    BOOST_TEST(result.tag == ("complement("
                              "polygon(point(-1/2,-1),point(1/2,-1),"
                              "point(1/2,1),point(-1/2,1)))"));
}

// This tests the polygon complement (along with intersection) through
// the identity $(A - B) = (A \cap B')$.

BOOST_AUTO_TEST_CASE(complement_identity)
{
    const auto &result = evaluate(
        [] {
            auto a = RECTANGLE(3, 4);
            auto b = RECTANGLE(1, 2);
            auto c = COMPLEMENT(b);
            auto d = INTERSECTION(a, c);
            return SYMMETRIC_DIFFERENCE(DIFFERENCE(a, b), d);
        });

    evaluate_operations();

    BOOST_TEST(result.value->is_empty());
}

// ## Polygon Transformation Tests

// These are tests for transformations of polygons.

BOOST_AUTO_TEST_CASE(translation)
{
    const auto &result = evaluate(
        TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(1, 2)));

    evaluate_operations();

    BOOST_TEST(result.tag == ("transform("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),translation(1,2))"));
}

BOOST_AUTO_TEST_CASE(translations)
{
    const auto &result = evaluate(
        [] {
            auto p = RECTANGLE(2, 2);

            for (double x = -1.0; x <= 1.0; x += 2.0) {
                for (double y = -1.0; y <= 1.0; y += 2.0) {
                    p = DIFFERENCE(
                        p, TRANSFORM(
                            RECTANGLE(FT::ET(1, 2), FT::ET(1, 2)),
                            TRANSLATION_2(x, y)));
                }
            }

            return p;
        });

    evaluate_operations();

    test_polygon(*result.value, 1, 0, 12, FT::ET(15, 4));
}

BOOST_AUTO_TEST_CASE(rotation, * boost::unit_test::tolerance(0.0001))
{
    const auto &result = evaluate(
        TRANSFORM(RECTANGLE(2, 2), basic_rotation(45)));

    evaluate_operations();

    const Polygon_set &P = *result.value;
    test_polygon(P, 1, 0, 4, 4);

    BOOST_TEST(P.number_of_polygons_with_holes() == 1);
    Polygon_with_holes G[1];
    P.polygons_with_holes(G);
    auto b = G[0].bbox();

    for (int i = 0; i < 2; i++) {
        BOOST_TEST(CGAL::to_double(b.max(i) * b.min(i)) == -2.0);
    }
}

BOOST_AUTO_TEST_CASE(reflection)
{
    const auto &result = evaluate(
        [] {
            auto a = POLYGON(
                std::vector<Point_2>({
                        Point_2(-1, 0),
                        Point_2(1, 0),
                        Point_2(0, 1)}));

            return JOIN(a, TRANSFORM(a, SCALING_2(1, -1)));
        });

    evaluate_operations();

    BOOST_TEST(result.tag == ("join("
                              "polygon(point(-1,0),point(1,0),"
                              "point(0,1)),"
                              "transform("
                              "polygon(point(-1,0),point(1,0),"
                              "point(0,1)),"
                              "scaling(1,-1)))"));
    test_polygon(*result.value, 1, 0, 4, 2);
}

BOOST_DATA_TEST_CASE(flush,
                     (boost::unit_test::data::make({1, 0})
                      ^ boost::unit_test::data::make({0, 1})),
                     x, y)
{
    const auto &result = evaluate(
        JOIN(FLUSH(RECTANGLE(2, 2), x, y),
             FLUSH(RECTANGLE(2, 2), -x, -y)));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag(
            ("join(flush("
             "polygon(point(-1,-1),point(1,-1),"
             "point(1,1),point(-1,1)),0,"), x, ",0,", y,
            ("),flush(polygon(point(-1,-1),point(1,-1),"
             "point(1,1),point(-1,1)),"), -x, ",0,", -y, ",0))"));
    test_polygon(*result.value, 1, 0, 6, 8);
}

// ## Polygon Hull Tests

// These are tests for polygons hulls, which can operate on points,
// polygons or a mixture of them.

BOOST_AUTO_TEST_CASE(hull_single)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYGON_HULL_OPEN();
            h->push_back(
                DIFFERENCE(RECTANGLE(2, 2),
                           TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(1, 1))));

            return POLYGON_HULL_CLOSE(h);
        });

    evaluate_operations();

    test_polygon(*result.value, 1, 0, 5, FT::ET(7, 2));
}

BOOST_AUTO_TEST_CASE(hull_mixed)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYGON_HULL_OPEN();
            h->push_back(RECTANGLE(2, 2));
            h->push_back(Point_2(-3, 0));
            h->push_back(Point_2(3, 0));
            return POLYGON_HULL_CLOSE(h);
        });

    evaluate_operations();

    BOOST_TEST(result.tag == ("hull("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),"
                              "point(-3,0),point(3,0))"));

    test_polygon(*result.value, 1, 0, 6, 8);
}

BOOST_AUTO_TEST_CASE(hull_points)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYGON_HULL_OPEN();
            h->push_back(Point_2(CGAL::ORIGIN));
            h->push_back(Point_2(2, 0));
            h->push_back(Point_2(2, 2));
            h->push_back(Point_2(0, 2));

            return POLYGON_HULL_CLOSE(h);
        });

    evaluate_operations();

    BOOST_TEST(result.tag == ("hull(point(0,0),point(2,0),"
                              "point(2,2),point(0,2))"));
    test_polygon(*result.value, 1, 0, 4, 4);
}

// ## Polygon Minkowski Sum Tests

// The minkowski sum of the rectangle and rhombus below should be an
// octagon with an area equal to that of the rectange + rhombus + four
// parallelograms = $4 + \frac{1}{2} + 4 \times \frac{1}{2} \times 2 = 8.5$.

BOOST_AUTO_TEST_CASE(minkowski_sum)
{
    const auto &result = evaluate(
        MINKOWSKI_SUM(RECTANGLE(2, 2), REGULAR_POLYGON(4, FT::ET(1, 2))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("minkowski_sum(polygon("
                              "point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),"
                              "regular_polygon(4,1/2,1/1000000))"));
    test_polygon(*result.value, 1, 0, 8, FT::ET(17, 2));
}

// ## Polygon Offset Sum Tests

// The tests below straightforwardly test offseting a rectangle by a
// positive or negative amount.  They also test the identity offset.

BOOST_DATA_TEST_CASE(offset,
                     boost::unit_test::data::make({-1, 0, 3}),
                     delta)
{
    const auto &result = evaluate(
        OFFSET(RECTANGLE(4, 4), delta));

    evaluate_operations();

    BOOST_TEST(result.tag == tag(
                   ("offset(polygon(point(-2,-2),point(2,-2),"
                    "point(2,2),point(-2,2)),"), delta, ")"));

    const FT l = 4 + 2 * delta;
    test_polygon(*result.value, 1, 0, 4, l * l);
}

// ## Polygon Components Operation Tests

// This components test mimics the corresponding one for polyhedra.

BOOST_DATA_TEST_CASE(components, boost::unit_test::data::xrange(3), n)
{
    auto A = RECTANGLE(2, 2), B = A;

    // We set up a set of 5 rectangles, arranged in a cross pattern,
    // centered on the origin.

    for (auto [i, j]:
             std::initializer_list<std::pair<int, int>> {
             {-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
        B = JOIN(B, TRANSFORM(A, TRANSLATION_2(3 * i, 3 * j)));
    }

    // We extract symmetric combinations of rectangles.

    const std::initializer_list<int> v[3] = {{3}, {1, 5}, {2, 4}};
    const auto &result = evaluate(COMPONENTS(B, v[n]));

    A.reset();
    B.reset();

    evaluate_operations();

    // The first test case should consist of a single rectangle, the
    // others of two.

    auto &S = *result.value;

    const int k = (n > 0) + 1;
    test_polygon(S, k, 0, 4 * k, 4 * k);

    // In all cases, due to symmetry, rotation by 180 degrees should
    // leave the result unchanged.

    Polygon_set T;
    transform_polygon_set(S, T, [](const Polygon &x) {
        return transform(basic_rotation(180), x);
    });

    T.symmetric_difference(S);
    BOOST_TEST(T.is_empty());
}

// Below we test that out of bounds components indices are handled
// correctly.

BOOST_AUTO_TEST_CASE(components_2)
{
    // We request the first component, plus the out-of-bounds
    // components -1, 0, and 3.  The result should be component 1
    // only.

    const auto &result = evaluate(
        COMPONENTS(
            JOIN(
                POLYGON(
                    std::vector<Point_2>({
                            Point_2(-2, -1),
                            Point_2(-1, 0),
                            Point_2(-2, 1)})),

                POLYGON(
                    std::vector<Point_2>({
                            Point_2(2, 1),
                            Point_2(1, 0),
                            Point_2(2, -1)}))), {3, 1, -1, 0}));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag(
            ("components(join(polygon(point(-2,-1),point(-1,0),point(-2,1)),"
             "polygon(point(2,1),point(1,0),point(2,-1))),1,3)")));

    test_polygon(*result.value, 1, 0, 3, 1);
}

BOOST_AUTO_TEST_SUITE_END()
