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

#include "options.h"
#include "kernel.h"
#include "macros.h"
#include "transformations.h"

#include "fixtures.h"
#include "circle_polygon_tests.h"

#include <CGAL/draw_polyhedron.h>
#include <CGAL/draw_nef_3.h>
#include <CGAL/draw_surface_mesh.h>

// ---

// # Graph Rewrite Tests

// These tests exercise operation rewrites (ref: Graph Rewriting).
// When working with these, it can be useful to inspect the resulting
// graphs.  One way to accomplish this, is the following:

// ```
// test -l nothing -t 'rewrites/...' -- --dump-graph=- | dot -O -Tpdf
// xpdf noname.gv.*pdf
// ```

// ## Coarse-Grained Rewrite Tests

// A simple, but effective way to test rewrites, is to set up a
// computation which should trigger one or more rewrites and evaluate
// it twice: once with rewrites disabled and again after enabling
// them.  The criterion of success is then, that the resulting
// geometry should be the same, but the operations by which it was
// produced should not.

template<typename T>
static void draw_result(const T &r)
{
    if constexpr (std::is_same_v<T, Circle_polygon_set>) {
        Polygon_set p;
        convert_circle_polygon_set(r, p, 0.001, FT::ET(1, 1000000));
        CGAL::draw(p);
    } else if constexpr (std::is_same_v<T, Conic_polygon_set>) {
        Polygon_set p;
        convert_conic_polygon_set(r, p, 0.001);
        CGAL::draw(p);
    } else {
        CGAL::draw(r);
    }
}

struct Rewrites_fixture: public Evaluation_fixture {
    // We compare two geometries, by taking their symmetric difference and
    // requiring the result to be empty.

    template<typename T>
    bool compare_results(const T &a, const T &b) {
        if (std::getenv("DRAW")) {
            draw_result(a);
            draw_result(b);
        }

        if constexpr (std::is_same_v<T, Polyhedron>
                      || std::is_same_v<T, Nef_polyhedron>
                      || std::is_same_v<T, Surface_mesh>) {
            return Nef_polyhedron(a).symmetric_difference(
                Nef_polyhedron(b)).is_empty();
        } else {
            static_assert(std::is_same_v<T, Polygon_set>
                          || std::is_same_v<T, Circle_polygon_set>
                          || std::is_same_v<T, Conic_polygon_set>);

            T c;
            c.symmetric_difference(a, b);
            return c.is_empty();
        }
    }

    // This function is generally passed a lambda that will produce a
    // computation to be tested.  This is then evaluated twice and the
    // results tested as described above.

    template<typename T>
    auto test_rewrite(bool expect, T f) {
        push(Flags::fold_transformations, 0);
        push(Flags::fold_offsets, 0);
        push(Flags::fold_flushes, 0);
        push(Flags::fold_booleans, 0);

        const auto &a = evaluate(f);
        evaluate_operations();

        Flags::fold_transformations = Flags::fold_offsets =
            Flags::fold_flushes = Flags::fold_booleans = 1;

        const auto &b = evaluate(f);
        evaluate_operations();

        pop(Flags::fold_booleans);
        pop(Flags::fold_flushes);
        pop(Flags::fold_offsets);
        pop(Flags::fold_transformations);

        if (expect) {
            BOOST_TEST(a.operations != b.operations, "no rewrite took place");
        } else {
            BOOST_TEST(a.operations == b.operations, "a rewrite took place");
        }

        if (!Flags::dry_run) {
            BOOST_TEST(compare_results(*a.value, *b.value), "results differ");
        }

        return b;
    }

    std::weak_ptr<Operation> sink(std::shared_ptr<Operation> op) {
        std::weak_ptr<Operation> w = op;
        sink_operation(std::move(op));
        return w;
    }
};

BOOST_FIXTURE_TEST_SUITE(rewrites, Rewrites_fixture)

// ### Polygon Boolean Folding

// For associative operations, we test:

//   1. left-asscociative chains, like the ones that would have been
//   produced by the front-end, with the succeeding union operation
//   always on the first operand, i.e. $(((p + q) + r) + ...)$,
//   2. right-asscociative chains, with the succeeding operation on
//   the second operand, i.e. $(p + (q + (r + ...)))$ and
//   3. mixed associativity, i.e. with the parentheses placed here or
//   there randomly.

#define DEFINE_CHAIN_TEST(NAME, N, F, A, B, ...)                        \
BOOST_DATA_TEST_CASE(NAME,                                              \
                     boost::unit_test::data::xrange(3) ^                \
                     boost::unit_test::data::make(__VA_ARGS__),         \
                     sample, expect)                                    \
{                                                                       \
    test_rewrite(                                                       \
        expect,                                                         \
        [&sample]() {                                                   \
            auto p = A;                                                 \
            std::srand(1);                                              \
                                                                        \
            for (int i = 0; i < N; i++) {                               \
                auto q = B;                                             \
                p = (sample == 2 ?                                      \
                     (std::rand() % 100) >= 50                          \
                     : sample)                                          \
                    ? F(q, p) : F(p, q);                                \
            }                                                           \
                                                                        \
            return p;                                                   \
        });                                                             \
}

// These test definition read as "define test `polygon_union` with a
// chain of 12 unions, of a central circle of radius 5 with suitable
// translated and rotated smaller circles.  All three associativity
// cases listed above should result in rewrites".

DEFINE_CHAIN_TEST(
    polygon_union,

    12, JOIN,
    CIRCLE(5),
    TRANSFORM_CS(
        CIRCLE(FT(FT::ET(5, 4))),
        (basic_rotation(30 * i)
         * TRANSLATION_2(0, 5 * std::cos(std::acos(-1) / 12)))),

    true, true, true)

DEFINE_CHAIN_TEST(
    polygon_intersection,

    12, INTERSECTION,
    CIRCLE(5),
    TRANSFORM_CS(
        CIRCLE(5), (basic_rotation(30 * i)
                    * TRANSLATION_2(0, FT(FT::ET(5, 2))))),

    true, true, true);

// In the case of difference chains, the right-associative test should
// not result in a rewrite.  For the random test, we expect several
// rewrites, one for each left-associative sub-chain.

DEFINE_CHAIN_TEST(
    polygon_difference,

    15, DIFFERENCE,
    RECTANGLE(2 * 15, 2),
    TRANSFORM(RECTANGLE(1, 1), TRANSLATION_2(2 * i - 14, 0)),

    true, false, true)

// ### Polyhedron Boolean Folding

// These tests follow much the same pattern as for polygons above.

// anchor: `polyhedron_union` test

// The union test in particular produces a sensible result with a
// chain of simple (i.e. untransformed) tetrahedrons, so the resulting
// graph illustrates the result of associative folding rewrites quite
// clearly.

DEFINE_CHAIN_TEST(
    polyhedron_union,

    7, JOIN,
    CONVERT_TO<Nef_polyhedron>(TETRAHEDRON(1, 1, 1)),
    CONVERT_TO<Nef_polyhedron>(
        TETRAHEDRON(
            (i & 4) / 2 - 1, (i & 2) - 1, (i & 1) * 2 - 1)),

    true, true, true);

DEFINE_CHAIN_TEST(
    polyhedron_intersection,

    4, JOIN,
    CUBOID(1, 1, 1),
    TRANSFORM(CUBOID(1, 1, 10), basic_rotation(20 * (i + 1), 2)),

    true, true, true);

DEFINE_CHAIN_TEST(
    polyhedron_difference,

    15, DIFFERENCE,
    CUBOID(2 * 15, 2, 2),
    TRANSFORM(CUBOID(1, 1, 10), TRANSLATION_3(2 * i - 14, 0, 0)),

    true, false, true)

// anchor: `large_polyhedron_union` test

// These test are larger versions of the above.  They're mostly meant
// to showcase the effect of rewrites in practive and are disabled by
// default, since they take a long time.

BOOST_TEST_DECORATOR(* boost::unit_test::disabled())
DEFINE_CHAIN_TEST(
    large_polyhedron_union,

    1000, JOIN,
    CUBOID(1, 1, 1),
    TRANSFORM(CUBOID(1, 1, 1), TRANSLATION_3(i + 1, 0, 0)),

    true, true, true)

BOOST_TEST_DECORATOR(* boost::unit_test::disabled())
DEFINE_CHAIN_TEST(
    large_polyhedron_difference,

    80, DIFFERENCE,
    CUBOID(2 * 80, 2, 2),
    TRANSFORM(CUBOID(1, 1, 10), TRANSLATION_3(2 * i - 79, 0, 0)),

    true, false, true)

#undef DEFINE_CHAIN_TEST

// We add some tests to ensure that rewrites work with all kinds of
// polygons and polyhedra.  We don't really care about the results
// here; these have been tested previously.  We only test that
// rewrites take place.

#define DEFINE_TYPE_TEST(NAME, TYPES, F, P)                     \
BOOST_AUTO_TEST_CASE_TEMPLATE(NAME, T, TYPES)                   \
{                                                               \
    push(Flags::dry_run, 1);                                    \
                                                                \
    test_rewrite(                                               \
        true,                                                   \
        []() {                                                  \
            auto p = CONVERT_TO<T>(P(8));                       \
                                                                \
            for (int i = 1; i < 8; i++) {                       \
                p = F(p, CONVERT_TO<T>(P(i)));                  \
            }                                                   \
                                                                \
            return p;                                           \
        });                                                     \
                                                                \
    pop(Flags::dry_run);                                        \
}

using polygon_types = boost::mpl::list<Polygon_set,
                                       Circle_polygon_set,
                                       Conic_polygon_set>;

DEFINE_TYPE_TEST(polygon_joins, polygon_types, JOIN, CIRCLE)
DEFINE_TYPE_TEST(polygon_intersections, polygon_types, INTERSECTION, CIRCLE)
DEFINE_TYPE_TEST(polygon_differences, polygon_types, DIFFERENCE, CIRCLE)

using polyhedron_types = boost::mpl::list<Polyhedron,
                                          Nef_polyhedron,
                                          Surface_mesh>;

DEFINE_TYPE_TEST(polyhedron_joins, polyhedron_types, JOIN, SPHERE)
DEFINE_TYPE_TEST(
    polyhedron_intersections, polyhedron_types, INTERSECTION, SPHERE)
DEFINE_TYPE_TEST(polyhedron_differences, polyhedron_types, DIFFERENCE, SPHERE)

#undef DEFINE_TYPE_TEST

// ### Unary Operation Chains

// Here we test folding of unary operation chains.  These include
// offsets and flushes of both polygons and polyhedra.

BOOST_AUTO_TEST_CASE(polygon_offset)
{
    const auto &result = test_rewrite(
        true,
        []() {
            return OFFSET(OFFSET(RECTANGLE(1, 1), 5), -2);
        });
}

BOOST_DATA_TEST_CASE(polygon_flush,
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
    const auto &result = test_rewrite(
        true,
        [&a, &b]() {
            return FLUSH(FLUSH(RECTANGLE(1, 1), a, -a), b, -b);
        });
}

BOOST_DATA_TEST_CASE(polyhedron_flush,
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
    const auto &result = test_rewrite(
        true,
        [&a, &b]() {
            return FLUSH(FLUSH(CUBOID(1, 1, 1), a, -a, a), b, -b, -b);
        });
}

BOOST_AUTO_TEST_CASE(polygon_transformation)
{
    test_rewrite(
        true,
        []() {
            return TRANSFORM(
                TRANSFORM(
                    TRANSFORM(RECTANGLE(1, 1), TRANSLATION_2(1, 0)),
                    basic_rotation(-90)),
                SCALING_2(2, 2));
        });
}

BOOST_AUTO_TEST_CASE(polyhedron_transformation)
{
    test_rewrite(
        true,
        []() {
            return TRANSFORM(
                TRANSFORM(
                    TRANSFORM(CUBOID(1, 1, 1), TRANSLATION_3(0, 1, 0)),
                    basic_rotation(-90, 0)),
                SCALING_3(2, 2, 2));
        });
}

// Again we test that unary operation rewriting works with all types
// of polygons and polyhedra.

#define DEFINE_TYPE_TEST(NAME, TYPES, P, ...)                   \
BOOST_AUTO_TEST_CASE_TEMPLATE(NAME, T, TYPES)                   \
{                                                               \
    push(Flags::dry_run, 1);                                    \
                                                                \
    test_rewrite(                                               \
        true,                                                   \
        []() {                                                  \
            auto p = CONVERT_TO<T>(P(1));                       \
                                                                \
            for (int i = 1; i < 8; i++) {                       \
                p = __VA_ARGS__(p, i);                          \
            }                                                   \
                                                                \
            return p;                                           \
        });                                                     \
                                                                \
    pop(Flags::dry_run);                                        \
}

// We need not test polygon flushes and offsets agains all types, as
// they're defined only for `Polygon_set`.

DEFINE_TYPE_TEST(
    polyhedron_flushes, polyhedron_types, SPHERE,

    [](auto p, int i) {
        return FLUSH(CONVERT_TO<T>(p), !(i & 4), !(i & 2), !(i & 1));
    })

DEFINE_TYPE_TEST(
    polygon_transformations, polygon_types, CIRCLE,

    [](auto p, int i) {
        return TRANSFORM<T, Polygon_operation<T>>(
            CONVERT_TO<T>(p), TRANSLATION_2(i, 0));
    })

DEFINE_TYPE_TEST(
    polyhedron_transformations, polyhedron_types, SPHERE,

    [](auto p, int i) {
        return TRANSFORM(CONVERT_TO<T>(p), TRANSLATION_3(i, 0, 0));
    })

#undef DEFINE_TYPE_TEST

// ## Fine-Grained Rewrite Tests

// In these tests, we perform rewrites on a graph and stop short of
// actual evaluation.  Disabling evaluation ensures that the operation
// graph won't be touched after postprocessing, so that we can then
// inspect the rewritten graph for proper structure.

// At the end we perform one more evaulation pass, with evaluation
// enabled (albeit in dry run mode).  This ensures the graph is
// properly flushed and prevents interference with following tests.

BOOST_AUTO_TEST_CASE(unary)
{

    push(Flags::evaluate, 0);
    push(Flags::fold_transformations, 1);

    // We set up a chain of two transformations.

    // ```graph
    // { op -> op_r -> op_t -> p }
    // ```

    const auto w = sink(
        CLIP(
            TRANSFORM(
                TRANSFORM(SPHERE(1), TRANSLATION_3(1, 2, 3)),
                basic_rotation(90, 2)), Plane_3(0, 0, 1, 0)));

    evaluate_operations();

    // This should fold to a single transformation.

    // ```graph
    // { op -> op_rt -> p }
    // ```

    // Link 1

    const auto &op = w.lock().get();
    BOOST_TEST_REQUIRE(op);
    BOOST_TEST_REQUIRE(op->predecessors.size() == 1);
    const Operation *op_rt = *op->predecessors.cbegin();

    BOOST_TEST(
        dynamic_cast<const Polyhedron_transform_operation<Polyhedron> *>(
            op_rt));

    // Link 2

    BOOST_TEST_REQUIRE(op_rt->successors.size() == 1);
    BOOST_TEST(*op_rt->successors.cbegin() == op);

    // Link 3

    BOOST_TEST_REQUIRE(op_rt->predecessors.size() == 1);
    const Operation *s = *op_rt->predecessors.cbegin();
    BOOST_TEST(dynamic_cast<const Sphere_operation *>(s));

    // Link 4

    BOOST_TEST_REQUIRE(s->successors.size() == 1);
    BOOST_TEST(*s->successors.cbegin() == op_rt);

    pop(Flags::fold_transformations);
    pop(Flags::evaluate);

    push(Flags::dry_run, 1);
    evaluate_operations();
    pop(Flags::dry_run);
}

BOOST_AUTO_TEST_CASE(associative)
{
    push(Flags::evaluate, 0);
    push(Flags::fold_booleans, 1);

    // We set up a chain of 8 intersections `op_n` of nested spheres,
    // clipped by `op_c`.

    // ```graph
    // { op_c -> op_1 -> ... -> op_8 -> p }
    // op_8 -> q
    // op_1 -> r
    // ```

    const int n = 8;
    auto x = SPHERE(n);

    for (int i = n - 1; i > 0; i--) {
        x = INTERSECTION(x, SPHERE(i));
    }

    x = CLIP(x, Plane_3(0, 0, 1, 0));

    const auto w = sink(std::move(x));

    evaluate_operations();

    // After folding, we should end up with a binary tree with the
    // spheres at the leaves and 3 levels of intersections down to the
    // root.

    const auto &op_c = w.lock().get();
    BOOST_TEST_REQUIRE(op_c);
    BOOST_TEST(op_c->predecessors.size() == 1);

    auto test = [](const Operation *op, int n, auto &&test) {
        if (n == 0) {
            BOOST_TEST(dynamic_cast<const Sphere_operation *>(op));
            return;
        }

        BOOST_TEST(
            dynamic_cast<
                const Polyhedron_intersection_operation<Polyhedron> *>(op));

        auto it = op->predecessors.cbegin();
        test(*it++, n - 1, test);
        test(*it++, n - 1, test);
        BOOST_TEST((it == op->predecessors.cend()));

    };

    test(*op_c->predecessors.cbegin(), 3, test);

    pop(Flags::fold_booleans);
    pop(Flags::evaluate);

    push(Flags::dry_run, 1);
    evaluate_operations();
    pop(Flags::dry_run);
}

BOOST_AUTO_TEST_CASE(difference)
{
    push(Flags::evaluate, 0);
    push(Flags::fold_booleans, 1);

    // We set up a chain of two difference operations between nested
    // spheres.

    // ```graph
    // edge: 1
    // { op_2 -> op_1 -> p }
    // edge: 2
    // op_1 -> q
    // op_2 -> r
    // ```

    std::vector v = {SPHERE(3), SPHERE(2), SPHERE(1)};
    const auto p = v[0].get(), q = v[1].get(), r = v[2].get();
    const auto w = sink(DIFFERENCE(DIFFERENCE(v[0], v[1]), v[2]));

    v.clear();

    evaluate_operations();

    // After folding, `op_2` should have remained a difference.

    const auto &op_2 = dynamic_cast<
        const Polyhedron_difference_operation<Polyhedron> *>(w.lock().get());

    BOOST_TEST_REQUIRE(op_2);
    BOOST_TEST(op_2->first.get() == p);

    // On the other hand, `op_1` should have become a union of `q` and
    // `r`.

    const auto op_1 = dynamic_cast<
        const Polyhedron_join_operation<Polyhedron> *>(op_2->second.get());

    BOOST_TEST_REQUIRE(op_1);

    BOOST_TEST(op_1->first.get() == q);
    BOOST_TEST(op_1->second.get() == r);

    pop(Flags::fold_booleans);
    pop(Flags::evaluate);

    push(Flags::dry_run, 1);
    evaluate_operations();
    pop(Flags::dry_run);
}

BOOST_AUTO_TEST_SUITE_END()
