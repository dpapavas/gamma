#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <cmath>
#include <sstream>

#include <CGAL/draw_polygon_set_2.h>

#include "kernel.h"
#include "transformations.h"
#include "macros.h"

#include "fixtures.h"
#include "polygon_tests.h"

FT polygon_area(const Polygon_set &S)
{
    FT x(0);

    for_each_polygon(
        S, [&x](const Polygon &G) {
            x += G.area();
        });

    return x;
}

const Polygon_set &test_polygon_area(
    const Polygon_set &S, const double area)
{

    BOOST_TEST(CGAL::to_double(polygon_area(S)) == area);

    return S;
}

BOOST_FIXTURE_TEST_SUITE(polygon, Reset_operations)

////////////////
// Primitives //
////////////////

BOOST_AUTO_TEST_CASE(ngon)
{
    auto p = POLYGON(
        std::vector<Point_2>({
                Point_2(-1, 0),
                Point_2(1, 0),
                Point_2(0, 1)}));

    BOOST_TEST(p->describe()
               == "polygon(point(-1,0),point(1,0),point(0,1))");

    evaluate_unit();

    test_polygon(*p->get_value(), 1, 0, 3, 1);
}

BOOST_AUTO_TEST_CASE(regular, * boost::unit_test::tolerance(1e-9))
{
    Tolerances::projection = FT::ET(1, 1'000'000);

    auto p = REGULAR_POLYGON(12, 3);

    BOOST_TEST(p->describe() == "regular_polygon(12,3,1/1000000)");

    evaluate_unit();

    test_polygon(*p->get_value(), 1, 0, 12, 27.0);
}

BOOST_AUTO_TEST_CASE(rectangle)
{
    auto p = RECTANGLE(2, 3);

    BOOST_TEST(
        p->describe()
        == ("polygon(point(-1,-3/2),point(1,-3/2),"
            "point(1,3/2),point(-1,3/2))"));

    evaluate_unit();

    test_polygon(*p->get_value(), 1, 0, 4, 6);
}

////////////////////
// Set operations //
////////////////////

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
    auto a = JOIN(RECTANGLE(2, 2),
                  TRANSFORM(RECTANGLE(4, 4), TRANSLATION_2(2, 2)));

    auto b = JOIN(RECTANGLE(4, 4),
                  TRANSFORM(RECTANGLE(4, 4), TRANSLATION_2(2, 2)));

    auto c = JOIN(RECTANGLE(2, 2),
                  TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(1, 1)));

    BOOST_TEST(a->describe()
               == ("join("
                   "polygon(point(-1,-1),point(1,-1),"
                   "point(1,1),point(-1,1)),"
                   "transform("
                   "polygon(point(-2,-2),point(2,-2),"
                   "point(2,2),point(-2,2)),"
                   "translation(2,2)))"));

    evaluate_unit();

    test_polygon(*a->get_value(), 1, 0, 8, 19);
    test_polygon(*b->get_value(), 1, 0, 8, 28);
    test_polygon(*c->get_value(), 1, 0, 8, 7);
}

BOOST_AUTO_TEST_CASE(difference)
{
    auto a = DIFFERENCE(RECTANGLE(3, 4), RECTANGLE(1, 2));
    auto b = DIFFERENCE(RECTANGLE(1, 2), RECTANGLE(3, 4));

    BOOST_TEST(a->describe()
               == ("difference("
                   "polygon(point(-3/2,-2),point(3/2,-2),"
                   "point(3/2,2),point(-3/2,2)),"
                   "polygon(point(-1/2,-1),point(1/2,-1),"
                   "point(1/2,1),point(-1/2,1)))"));

    evaluate_unit();

    test_polygon(*a->get_value(), 1, 1, 8, 10);
    BOOST_TEST(b->get_value()->is_empty());
}

BOOST_AUTO_TEST_CASE(intersection)
{
    auto a = INTERSECTION(RECTANGLE(2, 2),
                          TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(1, 1)));

    BOOST_TEST(a->describe()
               == ("intersection("
                   "polygon(point(-1,-1),point(1,-1),"
                   "point(1,1),point(-1,1)),"
                   "transform("
                   "polygon(point(-1,-1),point(1,-1),"
                   "point(1,1),point(-1,1)),"
                   "translation(1,1)))"));

    evaluate_unit();

    test_polygon(*a->get_value(), 1, 0, 4, 1);
}

BOOST_AUTO_TEST_CASE(symmetric_difference)
{
    auto a = SYMMETRIC_DIFFERENCE(
        RECTANGLE(2, 2),
        TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(1, 1)));

    BOOST_TEST(a->describe()
               == ("symmetric_difference("
                   "polygon(point(-1,-1),point(1,-1),"
                   "point(1,1),point(-1,1)),"
                   "transform("
                   "polygon(point(-1,-1),point(1,-1),"
                   "point(1,1),point(-1,1)),"
                   "translation(1,1)))"));

    evaluate_unit();

    test_polygon(*a->get_value(), 1, 1, 12, 6);
}

BOOST_AUTO_TEST_CASE(complement)
{
    auto a = RECTANGLE(3, 4);
    auto b = RECTANGLE(1, 2);
    auto c = COMPLEMENT(b);
    auto d = INTERSECTION(a, c);
    auto e = DIFFERENCE(DIFFERENCE(a, b), d);

    BOOST_TEST(c->describe()
               == ("complement("
                   "polygon(point(-1/2,-1),point(1/2,-1),"
                   "point(1/2,1),point(-1/2,1)))"));

    evaluate_unit();

    BOOST_TEST(e->get_value()->is_empty());
}

/////////////////////
// Transformations //
/////////////////////

BOOST_AUTO_TEST_CASE(translation)
{
    auto a = RECTANGLE(2, 2);

    for (double x = -1.0; x <= 1.0; x += 2.0) {
        for (double y = -1.0; y <= 1.0; y += 2.0) {
            auto b = TRANSFORM(RECTANGLE(FT::ET(1, 2), FT::ET(1, 2)),
                               TRANSLATION_2(x, y));
            std::ostringstream s;

            s << ("transform("
                  "polygon(point(-1/4,-1/4),point(1/4,-1/4),"
                  "point(1/4,1/4),point(-1/4,1/4)),"
                  "translation(") << x << "," << y << "))";

            BOOST_TEST(b->describe() == s.str());

            a = DIFFERENCE(a, b);
        }
    }

    evaluate_unit();

    test_polygon(*a->get_value(), 1, 0, 12, FT::ET(15, 4));
}

BOOST_AUTO_TEST_CASE(rotation, * boost::unit_test::tolerance(0.0001))
{
    auto a = TRANSFORM(RECTANGLE(2, 2), basic_rotation(45));

    evaluate_unit();

    const Polygon_set &P = *a->get_value();
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
    auto a = POLYGON(
        std::vector<Point_2>({
                Point_2(-1, 0),
                Point_2(1, 0),
                Point_2(0, 1)}));

    auto b = JOIN(a, TRANSFORM(a, SCALING_2(1, -1)));

    BOOST_TEST(b->describe()
               == ("join("
                   "polygon(point(-1,0),point(1,0),"
                   "point(0,1)),"
                   "transform("
                   "polygon(point(-1,0),point(1,0),"
                   "point(0,1)),"
                   "scaling(1,-1)))"));

    evaluate_unit();

    test_polygon(*b->get_value(), 1, 0, 4, 2);
}

BOOST_DATA_TEST_CASE(flush,
                     (boost::unit_test::data::make({1, 0})
                      ^ boost::unit_test::data::make({0, 1})),
                     x, y)
{
    auto p = JOIN(FLUSH(RECTANGLE(2, 2), x, y),
                  FLUSH(RECTANGLE(2, 2), -x, -y));

    std::stringstream s;
    s << ("join(flush("
          "polygon(point(-1,-1),point(1,-1),"
          "point(1,1),point(-1,1)),0,") << x << ",0," << y
      << ("),flush(polygon(point(-1,-1),point(1,-1),"
          "point(1,1),point(-1,1)),") << -x << ",0," << -y << ",0))";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polygon(*p->get_value(), 1, 0, 6, 8);
}

/////////////////
// Convex hull //
/////////////////

BOOST_AUTO_TEST_CASE(hull)
{
    auto h = std::make_shared<Polygon_hull_operation>();
    h->push_back(RECTANGLE(2, 2));
    h->push_back(TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(1, 1)));
    auto a = HULL(h);

    // Make some more hulls, to test concurrent access.

    auto g = std::make_shared<Polygon_hull_operation>();
    g->push_back(RECTANGLE(2, 2));
    g->push_back(TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(-1, -1)));
    auto b = HULL(g);

    auto f = std::make_shared<Polygon_hull_operation>();
    f->push_back(TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(2, 2)));
    f->push_back(TRANSFORM(RECTANGLE(2, 2), TRANSLATION_2(1, 1)));
    auto c = HULL(f);

    BOOST_TEST(a->describe()
               == ("hull("
                   "polygon(point(-1,-1),point(1,-1),"
                   "point(1,1),point(-1,1)),"
                   "transform("
                   "polygon(point(-1,-1),point(1,-1),"
                   "point(1,1),point(-1,1)),"
                   "translation(1,1)))"));

    evaluate_unit();

    test_polygon(*a->get_value(), 1, 0, 6, 8);
    test_polygon(*b->get_value(), 1, 0, 6, 8);
    test_polygon(*c->get_value(), 1, 0, 6, 8);
}

BOOST_AUTO_TEST_CASE(hull_single)
{
    auto h = std::make_shared<Polygon_hull_operation>();
    h->push_back(DIFFERENCE(RECTANGLE(2, 2), RECTANGLE(1, 1)));
    auto p = HULL(h);

    evaluate_unit();

    test_polygon(*p->get_value(), 1, 0, 4, 4);
}

BOOST_AUTO_TEST_CASE(hull_points)
{
    auto h = std::make_shared<Polygon_hull_operation>();
    h->push_back(Point_2(CGAL::ORIGIN));
    h->push_back(Point_2(2, 0));
    h->push_back(Point_2(2, 2));
    h->push_back(Point_2(0, 2));
    auto p = HULL(h);

    BOOST_TEST(p->describe()
               == ("hull(point(0,0),point(2,0),"
                   "point(2,2),point(0,2))"));

    evaluate_unit();

    test_polygon(*p->get_value(), 1, 0, 4, 4);
}

///////////////////
// Minkowski sum //
///////////////////

BOOST_AUTO_TEST_CASE(minkowski_sum)
{
    Tolerances::projection = FT::ET(1, 1'000'000);

    auto p = MINKOWSKI_SUM(RECTANGLE(2, 2), REGULAR_POLYGON(4, FT::ET(1, 2)));

    BOOST_TEST(
        p->describe() == ("minkowski_sum(polygon("
                          "point(-1,-1),point(1,-1),"
                          "point(1,1),point(-1,1)),"
                          "regular_polygon(4,1/2,1/1000000))"));

    evaluate_unit();

    test_polygon(*p->get_value(), 1, 0, 8, FT::ET(17, 2));
}

////////////////////
// Polygon offset //
////////////////////

BOOST_DATA_TEST_CASE(offset,
                     boost::unit_test::data::make({-1, 0, 3}),
                     delta)
{
    auto p = OFFSET(RECTANGLE(4, 4), delta);

    std::stringstream s;
    s << ("offset(polygon(point(-2,-2),point(2,-2),"
          "point(2,2),point(-2,2)),") << delta << ")";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    const FT l = (4 + 2 * delta);
    test_polygon(*p->get_value(), 1, 0, 4, l * l);
}

BOOST_AUTO_TEST_SUITE_END()
