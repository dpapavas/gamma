#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/mpl/list.hpp>

#include "options.h"
#include "kernel.h"
#include "macros.h"
#include "transformations.h"

#include "circle_polygon_tests.h"

#include <CGAL/draw_polyhedron.h>

std::unordered_map<std::string, std::shared_ptr<Operation>> &_get_operations();

using polyhedron_types = boost::mpl::list<Polyhedron,
                                          Nef_polyhedron,
                                          Surface_mesh>;

template<typename T>
static bool compare(const std::shared_ptr<Polyhedron_operation<T>> &a,
                    const std::shared_ptr<Polyhedron_operation<T>> &b)
{
    return (Nef_polyhedron(*a->get_value())
            - Nef_polyhedron(*b->get_value())).is_empty();
}


template<typename T>
static bool compare(const std::shared_ptr<Polygon_operation<T>> &a,
                    const std::shared_ptr<Polygon_operation<T>> &b)
{
    T c;

    c.difference(*a->get_value(), *b->get_value());
    return c.is_empty();
}

template<typename T>
static auto compare(T lambda)
{
    using U = std::result_of_t<T()>;
    std::vector<U> v;
    v.reserve(2);

    int j[] = {Flags::fold_booleans, Flags::fold_flushes};

    for (int i = 0; i < 2; i++) {
        begin_unit("rewrite_test");
        Flags::fold_flushes = Flags::fold_booleans = i;

        v.push_back(lambda());
        evaluate_unit();

        for (auto &[k, x]: _get_operations()) {
            const std::string &l = x->get_tag();
            BOOST_TEST(l == k);
            BOOST_TEST(l == x->describe());
        }
    }

    Flags::fold_booleans = j[0];
    Flags::fold_flushes = j[1];

    BOOST_TEST(compare(v[0], v[1]));

    return v[1];
}

BOOST_AUTO_TEST_SUITE(rewrites)

/////////////////////////////
// Polygon boolean folding //
/////////////////////////////

BOOST_AUTO_TEST_CASE(polygon_difference)
{
    compare([]() {
        const int n = 8;
        auto p = RECTANGLE(0.5, 0.5);
        auto a = RECTANGLE(n + 1, 1);

        for (int i = -n / 2; i < (n + 1) / 2; i++) {
            auto b = TRANSFORM(p, TRANSLATION_2(i, 0));

            a = DIFFERENCE(a, b);
        }

        return a;
    });
}

BOOST_AUTO_TEST_CASE(polygon_union)
{
    compare([]() {
        auto p = CIRCLE(5);
        auto a = CIRCLE(1.25);

        for (int i = 0; i < 12; i++) {
            auto b = TRANSFORM_CS(
                a, (basic_rotation(30 * i)
                    * TRANSLATION_2(0, 5 * std::cos(std::acos(-1) / 12))));

            p = JOIN(p, b);
        }

        return p;
    });
}

BOOST_AUTO_TEST_CASE(polygon_intersection)
{
    compare([]() {
        auto p = CIRCLE(5);
        auto a = p;

        for (int i = 0; i < 12; i++) {
            auto b = TRANSFORM_CS(
                p, (basic_rotation(30 * i)
                    * TRANSLATION_2(0, 1)));

            a = INTERSECTION(a, b);
        }

        return p;
    });
}

///////////////////////////
// Polygon flush folding //
///////////////////////////

BOOST_DATA_TEST_CASE(flush_polygon,
                     (boost::unit_test::data::make({
                             FT(-1),
                             -FT(FT::ET(1, 4)),
                             FT(0),
                             FT(FT::ET(1, 5)),
                             FT(-1)})
                      * boost::unit_test::data::make({
                              FT(-1),
                              -FT(FT::ET(1, 3)),
                              FT(0),
                              FT(FT::ET(5, 6)),
                              FT(-1)})),
                     a, b)
{
    const auto p = compare([&a, &b]() {
        return FLUSH(
            FLUSH(
                RECTANGLE(1, 1), a, -a), b, -b);
    });

    BOOST_TEST(p->annotations["rewrites"] == "1");
}

//////////////////////////////
// Polyhedron flush folding //
//////////////////////////////

BOOST_DATA_TEST_CASE(flush_polyhedron,
                     (boost::unit_test::data::make({
                             FT(-1),
                             -FT(FT::ET(1, 4)),
                             FT(0),
                             FT(FT::ET(1, 5)),
                             FT(-1)})
                      * boost::unit_test::data::make({
                              FT(-1),
                              -FT(FT::ET(1, 3)),
                              FT(0),
                              FT(FT::ET(5, 6)),
                              FT(-1)})),
                     a, b)
{
    const auto p = compare([&a, &b]() {
        return FLUSH(
            FLUSH(
                CUBOID(1, 1, 1), a, -a, a), b, -b, -b);
    });

    BOOST_TEST(p->annotations["rewrites"] == "1");
}

////////////////////////////////
// Polyhedron boolean folding //
////////////////////////////////

BOOST_AUTO_TEST_CASE_TEMPLATE(polyhedron_difference, T, polyhedron_types)
{
    compare([]() {
        const int n = 8;
        auto p = CONVERT_TO<T>(CUBOID(0.5, 0.5, 11));
        auto a = CONVERT_TO<T>(CUBOID(n + 1, 1, 1));
        auto b = TRANSFORM(a, TRANSLATION_3(0, 0, 5));

        for (int i = -n / 2; i < (n + 1) / 2; i++) {
            auto c = TRANSFORM(p, TRANSLATION_3(i, 0, 0));

            a = DIFFERENCE(a, c);
            b = DIFFERENCE(b, c);
        }

        return JOIN(a, b);
    });
}

BOOST_AUTO_TEST_CASE_TEMPLATE(polyhedron_intersection, T, polyhedron_types)
{
    auto p = compare([]() {
        const int n = 8;
        auto a = CONVERT_TO<T>(CUBOID(n, n, n));

        for (int i = n - 1; i > 0; i--) {
            a = INTERSECTION(a, CUBOID(i, i, i));
        }

        return a;
    });

    BOOST_TEST(p->annotations["rewrites"] == "1");
}

BOOST_TEST_DECORATOR(* boost::unit_test::disabled())
BOOST_AUTO_TEST_CASE_TEMPLATE(large_polyhedron_difference, T, polyhedron_types)
{
    compare([]() {
        auto a = CONVERT_TO<T>(
            TRANSFORM(CUBOID(160, 2, 1), TRANSLATION_3(80, 0, 0)));

        for (int i = 0; i < 80; i++) {
            a = DIFFERENCE(
                a, TRANSFORM(CUBOID(1, 1, 1), TRANSLATION_3(2 * i, 0, 0)));
        }

        return a;
    });
}

#define TAPPED(OP)                                                      \
{                                                                       \
    compare([]() {                                                      \
        auto a = CONVERT_TO<T>(CUBOID(8, 1, 1));                        \
        auto b = CONVERT_TO<T>(TRANSFORM(CUBOID(8, 1, 1),               \
                                         TRANSLATION_3(0, 0, 5)));      \
                                                                        \
        for (int i = -4; i <= 4; i++) {                                 \
            auto x = TRANSFORM(CUBOID(0.5, 0.5, 1.5), TRANSLATION_3(i, 0, 0)); \
                                                                        \
            a = OP(a, x);                                               \
                                                                        \
            if (i == 0) {                                               \
                b = OP(b, TRANSFORM(a, TRANSLATION_3(0, 0, 5)));        \
            }                                                           \
        }                                                               \
                                                                        \
        return JOIN(a, b);                                              \
    });                                                                 \
}

BOOST_AUTO_TEST_CASE_TEMPLATE(tapped_polyhedron_join, T, polyhedron_types)
TAPPED(JOIN)

BOOST_AUTO_TEST_CASE_TEMPLATE(tapped_polyhedron_difference, T, polyhedron_types)
TAPPED(DIFFERENCE)

#undef TAPPED

/////////////////////////////
// Transformation folding. //
/////////////////////////////

BOOST_AUTO_TEST_CASE(polygon_transformation)
{
    int i = Flags::fold_transformations;
    Flags::fold_transformations = 1;

    begin_unit("test_case");

    auto a = POLYGON(
        std::vector<Point_2>({
                Point_2(0, 0),
                Point_2(1, 0),
                Point_2(0, 1)}));

    auto b =
        TRANSFORM(
            TRANSFORM(
                TRANSFORM(
                    TRANSFORM(
                        a, basic_rotation(90)),
                    TRANSLATION_2(1, 0)),
                basic_rotation(-90)),
            TRANSLATION_2(0, 1));

    evaluate_unit();

    BOOST_TEST(b->annotations["rewrites"] == "3");

    Polygon_set d;
    d.difference(*a->get_value(), *b->get_value());
    BOOST_TEST(d.is_empty());
    BOOST_TEST(
        b->describe() == ("transform(polygon(point(0,0),point(1,0),point(0,1)),"
                          "translation(0,0))"));

    Flags::fold_transformations = i;
}

BOOST_AUTO_TEST_CASE(sequential)
{
    begin_unit("test_case");

    auto p = TETRAHEDRON(1, 1, 1);
    auto t = TRANSFORM(
        TRANSFORM(p, TRANSLATION_3(1, 2, 3)),
        basic_rotation(90, 2));
    auto j = JOIN(t, t);

    // Dead code elimination ensures that the operation graph won't be
    // touched after postprocessing, so that we can inspect the
    // results of folding.

    int i = Flags::eliminate_dead_operations;
    int k = Flags::fold_transformations;
    Flags::eliminate_dead_operations = 1;
    Flags::fold_transformations = 1;

    evaluate_unit();

    // +---+ ----> +---+ ----> +---+ ----> +---+
    // | J |       | R |       | T |       | P |
    // +---+ <---- +---+ <---- +---+ <---- +---+

    // Folds to:

    //         1            3
    // +---+ ----> +----+ ----> +---+
    // | J |       | RT |       | P |
    // +---+ <---- +----+ <---- +---+
    //         2            4

    // Link 1

    BOOST_TEST_REQUIRE(j->predecessors.size() == 1);
    auto t_prime = dynamic_cast<Polyhedron_transform_operation<Polyhedron> *>(
        *j->predecessors.begin());
    BOOST_TEST_REQUIRE(t_prime);

    // Link 2

    BOOST_TEST_REQUIRE(t_prime->successors.size() == 1);
    BOOST_TEST_REQUIRE(*t_prime->successors.begin() == j.get());

    // Link 3

    BOOST_TEST_REQUIRE(t_prime->predecessors.size() == 1);
    BOOST_TEST_REQUIRE(*t_prime->predecessors.begin() == p.get());

    // Link 4

    BOOST_TEST_REQUIRE(p->successors.size() == 1);
    BOOST_TEST_REQUIRE(*p->successors.begin() == t_prime);

    Flags::eliminate_dead_operations = i;
    Flags::fold_transformations = k;
}

BOOST_AUTO_TEST_SUITE_END()
