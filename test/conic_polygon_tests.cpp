#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <CGAL/draw_polygon_set_2.h>

#include "kernel.h"
#include "transformations.h"
#include "macros.h"

#include "fixtures.h"
#include "conic_polygon_tests.h"
#include "circle_polygon_tests.h"
#include "polygon_tests.h"

[[maybe_unused]] static void draw(const Conic_polygon_set &S)
{
    Polygon_set T;
    convert_conic_polygon_set(S, T, 0.01);
    CGAL::draw(T);
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

BOOST_FIXTURE_TEST_SUITE(conic_polygon, Reset_operations)

////////////////////////////////////
// Conversion from other polygons //
////////////////////////////////////

BOOST_AUTO_TEST_CASE(convert_rectangle)
{
    auto p = CONVERT_TO<Conic_polygon_set>(RECTANGLE(3, 3));
    BOOST_TEST(p->describe()
               == ("conics(polygon(point(-3/2,-3/2),point(3/2,-3/2),"
                   "point(3/2,3/2),point(-3/2,3/2)))"));

    evaluate_unit();

    test_polygon(*p->get_value(), "LLLL");
}

BOOST_AUTO_TEST_CASE(convert_circle)
{
    auto p = CONVERT_TO<Conic_polygon_set>(CIRCLE(2));
    BOOST_TEST(p->describe() == "conics(circle(2))");

    evaluate_unit();

    test_polygon(*p->get_value(), "CC");
}

BOOST_DATA_TEST_CASE(convert_segment,
                     (boost::unit_test::data::make({1, 3})
                      ^ boost::unit_test::data::make({"LC", "LCCC"})),
                     h, edges)
{
    std::stringstream s;
    auto p = CONVERT_TO<Conic_polygon_set>(CIRCULAR_SEGMENT(1, FT::ET(h, 2)));

    s << "conics(segment(1," << h << "/2))";
    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polygon(*p->get_value(), edges);
}

BOOST_DATA_TEST_CASE(convert_sector,
                     (boost::unit_test::data::make({60, 200})
                      ^ boost::unit_test::data::make({"LLC", "LLCC"})),
                     theta, edges)
{
    std::stringstream s;
    auto p = CONVERT_TO<Conic_polygon_set>(CIRCULAR_SECTOR(1, theta));

    s << "conics(sector(1," << theta << "))";
    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polygon(*p->get_value(), edges);
}

/////////////////////
// Transformations //
/////////////////////

// Scaling transformations (and only those) of circles, should result
// in conics.

BOOST_AUTO_TEST_CASE(transform_circles)
{
    auto p = CIRCLE(1);
    Aff_transformation_2 S = SCALING_2(2, 3);
    Aff_transformation_2 T = TRANSLATION_2(2, 2);
    Aff_transformation_2 R = basic_rotation(1);
    std::vector<std::shared_ptr<Polygon_operation<Conic_polygon_set>>> v;
    std::vector<std::shared_ptr<Polygon_operation<Circle_polygon_set>>> w;

    for (const auto &X: {S, S * T, S * R, S * T * R}) {
        v.push_back(TRANSFORM_C(p, X));
    }

    for (const auto &X: {T, R, T * R}) {
        w.push_back(TRANSFORM_CS(p, X));
    }

    evaluate_unit();

    for (const auto x: v) {
        test_polygon(*x->get_value(), "EE");
    }

    for (const auto x: w) {
        test_polygon(*x->get_value(), "CC");
    }
}

BOOST_AUTO_TEST_CASE(transform_curves)
{
    auto p = DIFFERENCE(TRANSFORM(ELLIPSE(4, 2), basic_rotation(45)),
                        TRANSFORM(ELLIPTIC_SECTOR(1, 1, 180),
                                  basic_rotation(-45)));

    evaluate_unit();

    test_polygon(*p->get_value(), "EE,CCL");
}

BOOST_AUTO_TEST_CASE(flush, * boost::unit_test::tolerance(0.0005))
{
    Tolerances::curve = FT::ET(1, 1000);

    auto p = DIFFERENCE(FLUSH(ELLIPSE(2, 4), 0, -1),
                        FLUSH(ELLIPSE(1, 2), 0, -1));

    evaluate_unit();

    test_polygon_area(*p->get_value(), std::acos(-1) * 6);
}

// Primitives (converted and transformed)

BOOST_AUTO_TEST_CASE(ellipse)
{
    auto p = ELLIPSE(4, 2);

    BOOST_TEST(
        p->describe() == "transform(conics(circle(1)),scaling(4,2))");

    evaluate_unit();

    test_polygon(*p->get_value(), "EE");
}

BOOST_DATA_TEST_CASE(sector,
                     (boost::unit_test::data::make({100, 280})
                      ^ boost::unit_test::data::make({"LLE", "LLEE"})),
                     theta, edges)
{
    std::stringstream s;
    auto p = ELLIPTIC_SECTOR(4, 2, theta);
    s << "transform(conics(sector(1," << theta << ")),scaling(4,2))";
    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polygon(*p->get_value(), edges);
}

////////////////////
// Set operations //
////////////////////

BOOST_AUTO_TEST_CASE(join)
{
    auto p = JOIN(TRANSFORM(RECTANGLE(4, 4), TRANSLATION_2(2, 0)),
                  ELLIPSE(4, 2));

    BOOST_TEST(p->describe() ==
               "join("
               "conics(transform("
               "polygon(point(-2,-2),point(2,-2),"
               "point(2,2),point(-2,2)),"
               "translation(2,0))),"
               "transform(conics(circle(1)),scaling(4,2)))");

    evaluate_unit();

    test_polygon(*p->get_value(), "EELLL");
}

BOOST_AUTO_TEST_CASE(difference)
{
    auto p = DIFFERENCE(ELLIPSE(4, 2),
                        TRANSFORM(RECTANGLE(4, 4), TRANSLATION_2(2, 0)));

    BOOST_TEST(p->describe() ==
               "difference("
               "transform(conics(circle(1)),scaling(4,2)),"
               "conics(transform("
               "polygon(point(-2,-2),point(2,-2),"
               "point(2,2),point(-2,2)),"
               "translation(2,0))))");

    evaluate_unit();

    test_polygon(*p->get_value(), "EEL");
}

BOOST_AUTO_TEST_CASE(intersection)
{
    auto p = INTERSECTION(ELLIPSE(2, 4), CIRCLE(2));

    BOOST_TEST(p->describe() ==
               "intersection(transform(conics(circle(1)),scaling(2,4)),"
               "conics(circle(2)))");

    evaluate_unit();

    test_polygon(*p->get_value(), "CC");
}

BOOST_AUTO_TEST_CASE(intersections)
{
    auto p = INTERSECTION(
        INTERSECTION(
            TRANSFORM(ELLIPSE(2, 4), TRANSLATION_2(0, 3)),
            TRANSFORM_CS(CIRCLE(3), TRANSLATION_2(-1, 0))),
        INTERSECTION(
            TRANSFORM_CS(CIRCLE(3), TRANSLATION_2(1, 0)),
            TRANSFORM(RECTANGLE(6, 6), TRANSLATION_2(0, 3))));

    evaluate_unit();

    test_polygon(*p->get_value(), "CCELE");
}

BOOST_AUTO_TEST_CASE(egg)
{
    auto p = INTERSECTION(JOIN(TRANSFORM(RECTANGLE(4, 4), TRANSLATION_2(0, 2)),
                               CIRCLE(2)),
                          ELLIPSE(2, 4));

    BOOST_TEST(p->describe() ==
               "intersection(conics(join(circles("
               "transform(polygon(point(-2,-2),point(2,-2),"
               "point(2,2),point(-2,2)),"
               "translation(0,2))),circle(2))),"
               "transform(conics(circle(1)),scaling(2,4)))");

    evaluate_unit();

    test_polygon(*p->get_value(), "EC");
}

BOOST_AUTO_TEST_CASE(symmetric_difference)
{
    auto p = SYMMETRIC_DIFFERENCE(ELLIPTIC_SECTOR(4, 2, 270),
                                  DIFFERENCE(
                                      ELLIPSE(4, 2),
                                      TRANSFORM(RECTANGLE(4, 2),
                                                TRANSLATION_2(2, -1))));

    BOOST_TEST(p->describe().rfind("symmetric_difference(", 0) == 0);

    evaluate_unit();

    BOOST_TEST(p->get_value()->is_empty());
}

/////////////////////////////
// Conversions to segments //
/////////////////////////////

BOOST_AUTO_TEST_CASE(convert_ellipses, * boost::unit_test::tolerance(0.001))
{
    Tolerances::curve = FT::ET(1, 1000);

    auto a = JOIN(ELLIPSE(1, 1),
                  DIFFERENCE(ELLIPSE(6, 3), ELLIPSE(2, 2)));
    auto b = CONVERT_TO<Polygon_set>(a);

    evaluate_unit();

    test_polygon(*a->get_value(), "EE,CC", "CC");
    test_polygon_area(*b->get_value(),
                      std::acos(-1) * (6 * 3 - 4 + 1));
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.001))
BOOST_DATA_TEST_CASE(convert_elliptic_sector,
                     (boost::unit_test::data::make({0, 1})
                      ^ boost::unit_test::data::make({"LLE", "LLEE"})),
                     i, edges)
{
    Tolerances::curve = FT::ET(1, 1000);

    auto a = ELLIPTIC_SECTOR(4, 2, i * 180 + 45);
    auto b = CONVERT_TO<Polygon_set>(a);

    evaluate_unit();

    test_polygon(*a->get_value(), edges);
    test_polygon_area(*b->get_value(), std::acos(-1) * (4 * i + 1));
}

BOOST_AUTO_TEST_SUITE_END()
