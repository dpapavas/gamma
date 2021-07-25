#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include "kernel.h"
#include "transformations.h"
#include "macros.h"

#include "fixtures.h"
#include "circle_polygon_tests.h"
#include "polygon_tests.h"

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

BOOST_FIXTURE_TEST_SUITE(circle_polygon, Reset_operations)

////////////////
// Primitives //
////////////////

BOOST_AUTO_TEST_CASE(circle)
{
    auto p = CIRCLE(2);

    BOOST_TEST(p->describe() == "circle(2)");

    evaluate_unit();

    test_polygon(*p->get_value(), "CC");
}

BOOST_DATA_TEST_CASE(segment,
                     (boost::unit_test::data::make({1, 3})
                      ^ boost::unit_test::data::make({"LC", "LCCC"})),
                     h, edges)
{
    std::stringstream s;
    auto p = CIRCULAR_SEGMENT(2, FT::ET(h, 2));

    s << "segment(2," << h << "/2)";
    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polygon(*p->get_value(), edges);
}

BOOST_DATA_TEST_CASE(sector,
                     (boost::unit_test::data::make({70, 210})
                      ^ boost::unit_test::data::make({"LLC", "LLCC"})),
                     theta, edges)
{
    std::stringstream s;
    auto p = CIRCULAR_SECTOR(2, theta);

    s << "sector(2," << theta << ")";
    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polygon(*p->get_value(), edges);
}

/////////////////////
// Transformations //
/////////////////////

// Although a circle stays invariant under rotations, the following
// tests reassembly of x-monotone curves during transformation.  The
// important check is that each circle consists of 2 semicircular
// segments only.

BOOST_AUTO_TEST_CASE(transform_circle)
{
    auto p = DIFFERENCE(
        TRANSFORM_CS(CIRCLE(2), basic_rotation(45)),
        TRANSFORM_CS(CIRCLE(FT::ET(1, 5)), basic_rotation(-45)));

    evaluate_unit();

    test_polygon(*p->get_value(), "CC,CC");
}

BOOST_AUTO_TEST_CASE(transform_circles)
{
    Aff_transformation_2 X(-1, 0, 0, 1);
    Aff_transformation_2 Y(1, 0, 0, -1);

    auto a = INTERSECTION(
        TRANSFORM_CS(CIRCLE(2), TRANSLATION_2(-1, 0)),
        TRANSFORM_CS(CIRCLE(2), TRANSLATION_2(1, 0)));

    auto b = TRANSFORM_CS(
        a, basic_rotation(-45) * TRANSLATION_2(0, 2));
    auto c = JOIN(b, TRANSFORM_CS(b, X));
    auto d = JOIN(c, TRANSFORM_CS(c, Y));

    evaluate_unit();

    test_polygon(*d->get_value(), "CCCC", "CCCC", "CCCC", "CCCC");
}

BOOST_AUTO_TEST_CASE(flush, * boost::unit_test::tolerance(0.0005))
{
    Tolerances::curve = FT::ET(1, 1000);

    auto p = DIFFERENCE(FLUSH(CIRCLE(2), -1, 0),
                        FLUSH(CIRCLE(1), -1, 0));

    evaluate_unit();

    test_polygon_area(*p->get_value(), std::acos(-1) * 3);
}

/////////////////
// Conversions //
/////////////////

BOOST_AUTO_TEST_CASE(convert_rectangle)
{
    auto p = CONVERT_TO<Circle_polygon_set>(RECTANGLE(3, 3));
    BOOST_TEST(p->describe()
               == ("circles(polygon(point(-3/2,-3/2),point(3/2,-3/2),"
                   "point(3/2,3/2),point(-3/2,3/2)))"));

    evaluate_unit();

    test_polygon(*p->get_value(), "LLLL");
}

BOOST_AUTO_TEST_CASE(convert_segment, * boost::unit_test::tolerance(0.0025))
{
    Tolerances::projection = FT::ET(1, 1'000'000);
    Tolerances::curve = FT::ET(1, 1000);

    auto a = CIRCULAR_SEGMENT(2, FT::ET(1, 2));
    auto b = CONVERT_TO<Polygon_set>(a);
    BOOST_TEST(b->describe() == "segments(segment(2,1/2),1/1000,1/1000000)");

    evaluate_unit();

    test_polygon(*a->get_value(), "LC");
    test_polygon_area(*b->get_value(),
                      1.5625 * std::acos(0.6) - 0.75);
}

BOOST_AUTO_TEST_CASE(convert_segments, * boost::unit_test::tolerance(0.001))
{
    Tolerances::curve = FT::ET(1, 1000);

    auto a = DIFFERENCE(CIRCULAR_SEGMENT(3, 2), CIRCULAR_SEGMENT(2, FT::ET(1, 2)));
    auto b = CONVERT_TO<Polygon_set>(a);

    evaluate_unit();

    test_polygon(*a->get_value(), "LCLCCC");
    test_polygon_area(
        *b->get_value(), ((1.5625 * 1.5625 * std::acos(-0.28) + 0.65625)
                          - (1.5625 * std::acos(0.6) - 0.75)));
}

BOOST_AUTO_TEST_CASE(convert_circles, * boost::unit_test::tolerance(0.001))
{
    Tolerances::curve = FT::ET(1, 1000);

    auto a = JOIN(
        CIRCLE(FT::ET(1, 2)),
        DIFFERENCE(CONVERT_TO<Circle_polygon_set>(RECTANGLE(3, 3)),
                   CIRCLE(1)));
    auto b = CONVERT_TO<Polygon_set>(a);

    evaluate_unit();

    test_polygon(*a->get_value(), "LLLL,CC", "CC");
    test_polygon_area(*b->get_value(), 9 - 0.75 * std::acos(-1));
}

BOOST_AUTO_TEST_CASE(convert_sectors, * boost::unit_test::tolerance(0.001))
{
    Tolerances::curve = FT::ET(1, 1000);

    auto a = JOIN(
        TRANSFORM_CS(CIRCULAR_SECTOR(FT::ET(3, 2), 270),
                     basic_rotation(90)),
        TRANSFORM_CS(CIRCULAR_SECTOR(FT(FT::ET(3, 2) - FT::ET(1, 10)), 90),
                     TRANSLATION_2(FT::ET(1, 10), FT::ET(1, 10))));
    auto b = CONVERT_TO<Polygon_set>(a);

    evaluate_unit();

    test_polygon(*a->get_value(), "LLC", "LLCC");
    test_polygon_area(*b->get_value(), std::atan(1) * 8.71);
}

////////////////////
// Set operations //
////////////////////

BOOST_AUTO_TEST_CASE(join)
{
    auto p = JOIN(CIRCLE(1),
                  TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(-1, 0)));

    BOOST_TEST(p->describe() ==
               "join(circle(1),"
               "circles(transform("
               "polygon(point(-1,-1),point(1,-1),"
               "point(1,1),point(-1,1)),"
               "translation(-1,0))))");

    evaluate_unit();

    test_polygon(*p->get_value(), "CCLLL");
}

BOOST_AUTO_TEST_CASE(joins)
{
    auto p = JOIN(
        TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(0, 1)),
        JOIN(
            TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(0, -1)),
            CONVERT_TO<Circle_polygon_set>(RECTANGLE(2, 2))));

    evaluate_unit();

    test_polygon(*p->get_value(), "CLCL");
}

BOOST_AUTO_TEST_CASE(difference)
{
    auto p = DIFFERENCE(RECTANGLE(2, 2), CIRCLE(FT::ET(6, 5)));

    BOOST_TEST(p->describe() ==
               "difference("
               "circles(polygon(point(-1,-1),point(1,-1),"
               "point(1,1),point(-1,1))),"
               "circle(6/5))");

    evaluate_unit();

    test_polygon(*p->get_value(), "LLC", "LLC", "LLC", "LLC");
}

BOOST_AUTO_TEST_CASE(intersection)
{
    auto p = INTERSECTION(RECTANGLE(2, 2), CIRCLE(FT::ET(6, 5)));

    BOOST_TEST(p->describe() ==
               "intersection("
               "circles(polygon(point(-1,-1),point(1,-1),"
               "point(1,1),point(-1,1))),"
               "circle(6/5))");

    evaluate_unit();

    test_polygon(*p->get_value(), "LCLCLCLC");
}

BOOST_AUTO_TEST_CASE(symmetric_difference)
{
    auto p = SYMMETRIC_DIFFERENCE(
        TRANSFORM_CS(CIRCLE(2), TRANSLATION_2(1, 0)),
        TRANSFORM_CS(CIRCLE(2), TRANSLATION_2(-1, 0)));

    BOOST_TEST(p->describe() ==
               "symmetric_difference("
               "transform(circle(2),translation(1,0)),"
               "transform(circle(2),translation(-1,0)))");

    evaluate_unit();

    test_polygon(*p->get_value(), "CCCC,CCCC");
}

BOOST_AUTO_TEST_CASE(
    symmetric_differences, * boost::unit_test::tolerance(0.001))
{
    Tolerances::curve = FT::ET(1, 1000);

    auto a = SYMMETRIC_DIFFERENCE(
        SYMMETRIC_DIFFERENCE(
            TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(-1, 0)),
            TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(1, 0))),
        SYMMETRIC_DIFFERENCE(
            TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(0, -1)),
            TRANSFORM_CS(CIRCLE(1), TRANSLATION_2(0, 1))));

    BOOST_REQUIRE(a);

    auto b = CONVERT_TO<Polygon_set>(a);

    evaluate_unit();

    // The total area of the holes is 2 * (pi - 2), so each of the
    // four parts has area 2.

    test_polygon_area(*b->get_value(), 8);
}

BOOST_AUTO_TEST_SUITE_END()
