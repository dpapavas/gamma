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
#include <boost/mpl/list.hpp>

#include <iostream>

#include "options.h"
#include "kernel.h"
#include "macros.h"
#include "transformations.h"

#include "fixtures.h"

#include <CGAL/draw_polyhedron.h>
#include <CGAL/draw_surface_mesh.h>
#include <CGAL/Polygon_mesh_processing/compute_normal.h>

// Document: program

// # Selection Tests

// These tests set up geometry and apply selections, testing the
// results either by counting or by examining their spatial
// characteristics.

using types = boost::mpl::list<Polyhedron, Surface_mesh>;

BOOST_FIXTURE_TEST_SUITE(selection, Coarse_evaluation_fixture)


// ## Bounded Selections

// We group tests by the geometric primitive their applied on.

static const FT epsilon(FT::ET(1, 1'000'000));
#define TEST_SELECTION(S, N) BOOST_TEST(S->apply(M).size() == N);

// ### Bounding Volume Tests on Cuboids

// We use a cuboid to test:

BOOST_AUTO_TEST_CASE_TEMPLATE(on_cuboid, T, types)
{
    const auto &result = evaluate(CONVERT_TO<T>(CUBOID(2, 2, 2)));
    evaluate_operations();
    auto &M = *result.value;

    //   1. bounding planes,

    {
        const auto V = BOUNDING_PLANE(0, 0, 1, -1);
        const auto W = BOUNDING_PLANE(0, 0, 1, -1 + epsilon);

        TEST_SELECTION(VERTICES_IN(V), 4);
        TEST_SELECTION(VERTICES_IN(W), 0);

        TEST_SELECTION(FACES_IN(V), 1);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 5);
        TEST_SELECTION(FACES_IN(W), 0);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 0);

        TEST_SELECTION(EDGES_IN(V), 4);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 8);
        TEST_SELECTION(EDGES_IN(W), 0);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 0);
    }

    //   2. bouding halfspaces and

    {
        const auto V = BOUNDING_HALFSPACE_INTERIOR(0, 0, 1, -1);
        const auto W = BOUNDING_HALFSPACE_INTERIOR(0, 0, 1, -1 + epsilon);

        TEST_SELECTION(VERTICES_IN(V), 4);
        TEST_SELECTION(VERTICES_IN(W), 4);

        TEST_SELECTION(FACES_IN(V), 1);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 5);
        TEST_SELECTION(FACES_IN(W), 1);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 5);

        TEST_SELECTION(EDGES_IN(V), 4);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 8);
        TEST_SELECTION(EDGES_IN(W), 4);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 8);
    }

    {
        const auto V = BOUNDING_HALFSPACE(0, 0, 1, -1);
        const auto W = BOUNDING_HALFSPACE(0, 0, 1, -1 + epsilon);

        TEST_SELECTION(VERTICES_IN(V), 8);
        TEST_SELECTION(VERTICES_IN(W), 4);

        TEST_SELECTION(FACES_IN(V), 6);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 6);
        TEST_SELECTION(FACES_IN(W), 1);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 5);

        TEST_SELECTION(EDGES_IN(V), 12);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 12);
        TEST_SELECTION(EDGES_IN(W), 4);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 8);
    }

    //   3. Bounding boxes.

    {
        const auto V = BOUNDING_BOX(2, 2, 2);
        const auto W = BOUNDING_BOX(2 - epsilon, 2 - epsilon, 2);

        TEST_SELECTION(VERTICES_IN(V), 8);
        TEST_SELECTION(VERTICES_IN(W), 0);

        TEST_SELECTION(FACES_IN(V), 6);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 6);
        TEST_SELECTION(FACES_IN(W), 0);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 0);

        TEST_SELECTION(EDGES_IN(V), 12);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 12);
        TEST_SELECTION(EDGES_IN(W), 0);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 0);
    }

    {
        const auto V = TRANSFORM(
            BOUNDING_BOX_INTERIOR(4, 4, 4), TRANSLATION_3(1, 1, 1));
        const auto W = TRANSFORM(
            BOUNDING_BOX_INTERIOR(4, 4, 4),
            TRANSLATION_3(1 + epsilon, 1 + epsilon, 1 + epsilon));

        TEST_SELECTION(VERTICES_IN(V), 1);
        TEST_SELECTION(VERTICES_IN(W), 1);

        TEST_SELECTION(FACES_IN(V), 0);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 3);
        TEST_SELECTION(FACES_IN(W), 0);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 3);

        TEST_SELECTION(EDGES_IN(V), 0);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 3);
        TEST_SELECTION(EDGES_IN(W), 0);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 3);
    }

    {
        const auto V = TRANSFORM(
            BOUNDING_BOX_BOUNDARY(4, 4, 4), TRANSLATION_3(1, 1, 1));
        const auto W = TRANSFORM(
            BOUNDING_BOX_BOUNDARY(4, 4, 4),
            TRANSLATION_3(1 + epsilon, 1 + epsilon, 1 + epsilon));

        TEST_SELECTION(VERTICES_IN(V), 7);
        TEST_SELECTION(VERTICES_IN(W), 0);

        TEST_SELECTION(FACES_IN(V), 3);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 6);
        TEST_SELECTION(FACES_IN(W), 0);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 0);

        TEST_SELECTION(EDGES_IN(V), 9);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 12);
        TEST_SELECTION(EDGES_IN(W), 0);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 0);
    }
}

// ### Bounding Volume Tests on Cylinder

// We use a cylinder to test selections using bounding cylinders:

BOOST_AUTO_TEST_CASE_TEMPLATE(on_cylinder, T, types)
{
    const auto &result = evaluate(
        CONVERT_TO<T>(DIFFERENCE(CYLINDER(2, 2), CYLINDER(1, 1))));
    evaluate_operations();
    auto &M = *result.value;

    //   1. Bounding cylinder,

    {
        const auto V = BOUNDING_CYLINDER(2, 2);
        const auto W = TRANSFORM(V, TRANSLATION_3(0, 0, epsilon));

        TEST_SELECTION(VERTICES_IN(V), 64 + 46);
        TEST_SELECTION(VERTICES_IN(W), 32 + 46);

        TEST_SELECTION(FACES_IN(V), 124 + 88);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 124 + 88);
        TEST_SELECTION(FACES_IN(W), 30 + 88);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 94 + 88);

        TEST_SELECTION(EDGES_IN(V), 186 + 132);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 186 + 132);
        TEST_SELECTION(EDGES_IN(W), 61 + 132);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 125 + 132);
    }

    //   2. interior and

    {
        const auto V = BOUNDING_CYLINDER_INTERIOR(2, 2);
        const auto W = TRANSFORM(V, TRANSLATION_3(0, 0, epsilon));

        TEST_SELECTION(VERTICES_IN(V), 46);
        TEST_SELECTION(VERTICES_IN(W), 46);

        TEST_SELECTION(FACES_IN(V), 88);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 88);
        TEST_SELECTION(FACES_IN(W), 88);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 88);

        TEST_SELECTION(EDGES_IN(V), 132);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 132);
        TEST_SELECTION(EDGES_IN(W), 132);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 132);
    }

    //   3. boundary.

    {
        const auto V = BOUNDING_CYLINDER_BOUNDARY(2, 2);
        const auto W = TRANSFORM(V, TRANSLATION_3(0, 0, epsilon));

        TEST_SELECTION(VERTICES_IN(V), 64);
        TEST_SELECTION(VERTICES_IN(W), 32);

        TEST_SELECTION(FACES_IN(V), 124);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 124);
        TEST_SELECTION(FACES_IN(W), 30);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 94);

        TEST_SELECTION(EDGES_IN(V), 186);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 186);
        TEST_SELECTION(EDGES_IN(W), 61);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 125);
    }
}

// ### Bounding Volume Tests on Sphere

// Similarly, a sphere is used to test selections using bounding
// spheres:

BOOST_AUTO_TEST_CASE_TEMPLATE(on_sphere, T, types)
{
    const auto &result = evaluate(
        CONVERT_TO<T>(DIFFERENCE(SPHERE(2), SPHERE(1))));
    evaluate_operations();
    auto &M = *result.value;

    //   1. Bounding sphere,

    {
        const auto V = BOUNDING_SPHERE(2);
        const auto W = BOUNDING_SPHERE(2 - epsilon);

        TEST_SELECTION(VERTICES_IN(V), 642 * 2);
        TEST_SELECTION(VERTICES_IN(W), 642);

        TEST_SELECTION(FACES_IN(V), 1280 * 2);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 1280 * 2);
        TEST_SELECTION(FACES_IN(W), 1280);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 1280);

        TEST_SELECTION(EDGES_IN(V), 1920 * 2);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 1920 * 2);
        TEST_SELECTION(EDGES_IN(W), 1920);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 1920);
    }

    //   2. interior and

    {
        const auto V = BOUNDING_SPHERE_INTERIOR(2);
        const auto W = BOUNDING_SPHERE_INTERIOR(2 + epsilon);

        TEST_SELECTION(VERTICES_IN(V), 642);
        TEST_SELECTION(VERTICES_IN(W), 642 * 2);

        TEST_SELECTION(FACES_IN(V), 1280);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 1280);
        TEST_SELECTION(FACES_IN(W), 1280 * 2);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 1280 * 2);

        TEST_SELECTION(EDGES_IN(V), 1920);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 1920);
        TEST_SELECTION(EDGES_IN(W), 1920 * 2);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 1920 * 2);
    }

    //   3. boundary.

    {
        const auto V = BOUNDING_SPHERE_BOUNDARY(2);
        const auto W = BOUNDING_SPHERE_BOUNDARY(2 + epsilon);

        TEST_SELECTION(VERTICES_IN(V), 642);
        TEST_SELECTION(VERTICES_IN(W), 0);

        TEST_SELECTION(FACES_IN(V), 1280);
        TEST_SELECTION(FACES_PARTIALLY_IN(V), 1280);
        TEST_SELECTION(FACES_IN(W), 0);
        TEST_SELECTION(FACES_PARTIALLY_IN(W), 0);

        TEST_SELECTION(EDGES_IN(V), 1920);
        TEST_SELECTION(EDGES_PARTIALLY_IN(V), 1920);
        TEST_SELECTION(EDGES_IN(W), 0);
        TEST_SELECTION(EDGES_PARTIALLY_IN(W), 0);
    }
}

#undef TEST_SELECTION

// ## Relative Selection Tests (Expansion and Contraction)

// Here we test relative selections, by performing contractions and
// expansions on basic selections and testing the results alone, or
// in combinations.

// ### Relative Vertex Selection Tests

// We create a a cuboid with sections for $z = -2 ... 2$, then:

BOOST_AUTO_TEST_CASE_TEMPLATE(relative_vertices, T, types)
{
    std::vector<Aff_transformation_3> v;

    for (int i = -2; i <= 2 ; i++) {
        v.push_back(TRANSLATION_3(0, 0, i));
    }

    const auto &result = evaluate(
        CONVERT_TO<T>(EXTRUSION(RECTANGLE(1, 1), std::move(v))));
    evaluate_operations();
    auto &P = *result.value;

    //   1. select the center vertices, on plane $z = 0$,

    const auto a = VERTICES_IN(BOUNDING_PLANE(0, 0, 1, 0));

    BOOST_TEST(a->apply(P).size() == 4);

    //   2. expand this to include planes $-1 <= z <= 1$,

    const auto b = RELATIVE_SELECTION(a, 1);
    auto bv = b->apply(P);

    std::sort(bv.begin(), bv.end());

    BOOST_TEST(bv.size() == 12);

    //   3. make the same selection, but through joining (so it should
    //   be sorted),

    const auto c = JOIN({
            a,
            VERTICES_IN(BOUNDING_PLANE(0, 0, 1, 1)),
            VERTICES_IN(BOUNDING_PLANE(0, 0, 1, -1))});

    BOOST_TEST(c->apply(P) == bv);

    //   4. expanded the selection to all vertices,

    const auto d = RELATIVE_SELECTION(a, 2);

    BOOST_TEST(d->apply(P).size() == 20);

    //   5. select planes $z = \pm 2$,

    const auto e = COMPLEMENT(b);
    const auto ev = e->apply(P);

    BOOST_TEST(ev.size() == 8);
    BOOST_TEST(INTERSECTION({b, e})->apply(P).size() == 0);

    //   6. select the same vertices as in as `b`, but by removing the end planes,

    const auto f = DIFFERENCE({d, e});

    BOOST_TEST(f->apply(P) == bv);

    //   7. select all planes with $z \neq 0$ and finally,

    const auto g = DIFFERENCE({d, a});

    BOOST_TEST(g->apply(P).size() == 16);
    BOOST_TEST(INTERSECTION({g, e})->apply(P) == ev);

    //   8. make the same selection as in `e`, but through contraction.

    const auto h = RELATIVE_SELECTION(g, -1);
    auto hv = h->apply(P);

    std::sort(hv.begin(), hv.end());

    BOOST_TEST(hv == ev);
}

// ### Relative Face Selection Tests

// Starting with the same sectioned cuboid, we:

BOOST_AUTO_TEST_CASE_TEMPLATE(relative_faces, T, types)
{
    std::vector<Aff_transformation_3> v;

    for (int i = -2; i <= 2 ; i++) {
        v.push_back(TRANSLATION_3(0, 0, i));
    }

    const auto &result = evaluate(
        CONVERT_TO<T>(EXTRUSION(RECTANGLE(1, 1), std::move(v))));
    evaluate_operations();
    auto &P = *result.value;

    //   1. select faces in planes $-1 <= z <= 1$,

    const auto a = FACES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, 0));

    BOOST_TEST(a->apply(P).size() == 8);

    //   2. expand to $-2 <= z <= 2$ (but without the caps),

    const auto b = RELATIVE_SELECTION(a, 1);
    auto bv = b->apply(P);

    std::sort(bv.begin(), bv.end());

    BOOST_TEST(bv.size() == 16);

    //   3. make the same selection, but through joining (so it should
    //   be sorted),

    const auto c = JOIN({
            a,
            FACES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, 1)),
            FACES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, -1))});

    BOOST_TEST(c->apply(P) == bv);

    //   4. expand it to all faces, then finally

    const auto d = RELATIVE_SELECTION(a, 2);
    auto dv = d->apply(P);

    std::sort(dv.begin(), dv.end());

    BOOST_TEST(dv.size() == 18);

    //   5. select only the caps.

    const auto e = RELATIVE_SELECTION(DIFFERENCE({d, a}), -1);
    auto ev = e->apply(P);

    std::sort(ev.begin(), ev.end());

    BOOST_TEST(ev.size() == 2);
    BOOST_TEST(INTERSECTION({b, e})->apply(P).size() == 0);
    BOOST_TEST(JOIN({b, e})->apply(P) == dv);
    BOOST_TEST(COMPLEMENT(b)->apply(P) == ev);
}

// ### Relative Edge Selection Tests

// Starting with the same geometry, we:

BOOST_AUTO_TEST_CASE_TEMPLATE(relative_edges, T, types)
{
    std::vector<Aff_transformation_3> v;

    for (int i = -2; i <= 2 ; i++) {
        v.push_back(TRANSLATION_3(0, 0, i));
    }

    const auto &result = evaluate(
        CONVERT_TO<T>(EXTRUSION(RECTANGLE(1, 1), std::move(v))));
    evaluate_operations();
    auto &P = *result.value;

    //   1. select edges between the planes $-1 < z < 1$ (not
    //   including edges on the planes),

    const auto a = EDGES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, 0));

    BOOST_TEST(a->apply(P).size() == 12);

    //   2. expand it to $-2 < z < 2$ (but without the caps),

    const auto b = RELATIVE_SELECTION(a, 1);
    auto bv = b->apply(P);

    std::sort(bv.begin(), bv.end());

    BOOST_TEST(bv.size() == 28);

    //   3. make the same selection, but through joining (so it should
    //   be sorted),

    const auto c = JOIN({
            a,
            EDGES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, 1)),
            EDGES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, -1))});

    BOOST_TEST(c->apply(P) == bv);

    //   4. expand it to all edges, the finally

    const auto d = RELATIVE_SELECTION(a, 2);
    auto dv = d->apply(P);

    std::sort(dv.begin(), dv.end());

    BOOST_TEST(dv.size() == 36);

    //   5. select only the edges of the caps.

    const auto e = RELATIVE_SELECTION(DIFFERENCE({d, a}), -1);
    auto ev = e->apply(P);

    std::sort(ev.begin(), ev.end());

    BOOST_TEST(ev.size() == 8);
    BOOST_TEST(INTERSECTION({b, e})->apply(P).size() == 0);
    BOOST_TEST(JOIN({b, e})->apply(P) == dv);
    BOOST_TEST(COMPLEMENT(b)->apply(P) == ev);
}

// ### Selection Conversion

// Here we test conversion between selections of different elements,
// such as vertices in selected faces, etc.


BOOST_AUTO_TEST_CASE_TEMPLATE(conversion, T, types)
{
    std::vector<Aff_transformation_3> v;

    // Using a cuboid we test:

    const auto &result = evaluate(CONVERT_TO<T>(CUBOID(2, 2, 2)));
    evaluate_operations();
    auto &P = *result.value;

    //   1. vertices on plane $z = 1$, converted to faces,

    const auto a = FACES_IN(VERTICES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(a->apply(P).size() == 1);

    //   2. vertices on plane $z = -1$, converted to faces (partial),

    const auto b = FACES_PARTIALLY_IN(VERTICES_IN(BOUNDING_PLANE(0, 0, 1, 1)));

    BOOST_TEST(b->apply(P).size() == 5);
    BOOST_TEST(INTERSECTION({a, b})->apply(P).size() == 0);

    //   3. vertices on plane $z = 1$, converted to edges,

    const auto c = EDGES_IN(VERTICES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(c->apply(P).size() == 4);

    //   4. vertices on plane $z = -1$, converted to edges (partial),

    const auto d = EDGES_PARTIALLY_IN(VERTICES_IN(BOUNDING_PLANE(0, 0, 1, 1)));

    BOOST_TEST(d->apply(P).size() == 8);
    BOOST_TEST(INTERSECTION({c, d})->apply(P).size() == 0);

    //   5. faces on plane $z = 1$, converted to vertices,

    const auto e = VERTICES_IN(FACES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(e->apply(P).size() == 4);

    //   6. faces on plane $z = 1$, converted to edges,

    const auto f = EDGES_IN(FACES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(f->apply(P).size() == 4);

    //   7. faces on plane $z = -1$, converted to edges (partial),

    const auto g = EDGES_PARTIALLY_IN(FACES_IN(BOUNDING_PLANE(0, 0, 1, 1)));

    BOOST_TEST(g->apply(P).size() == 8);
    BOOST_TEST(INTERSECTION({f, g})->apply(P).size() == 0);

    //   8. edges on plane $z = 1$, converted to vertices,

    const auto h = VERTICES_IN(EDGES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(h->apply(P).size() == 4);

    //   9. edges on plane $z = 1$, converted to faces,

    const auto i = FACES_IN(EDGES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(i->apply(P).size() == 1);

    //   10. edges on plane $z = -1$, converted to faces (partial),

    const auto j = FACES_PARTIALLY_IN(EDGES_IN(BOUNDING_PLANE(0, 0, 1, 1)));

    BOOST_TEST(j->apply(P).size() == 5);
    BOOST_TEST(INTERSECTION({i, j})->apply(P).size() == 0);
}

// ## Bounding Volume Flush Operations Tests

// We transform and then flush various shapes, then apply a matching
// transformation and flush to the corresponding bounding shape and
// use it to select vertices.  The result should be that all vertices
// get selected.  To make sure that the bounding volume exactly
// matches the shape, we then slightly dilate the latter and reapply
// the selection.  The result should be now that no vertices get
// selected as the bounding volume is now slightly smaller than the
// shape.

// We necessarily apply a simple 90 degree rotation instead of
// something more arbitrary, because otherwise the flushed geometry
// and volume would not match.  (Consider a sphere for instance, which
// is not really a sphere, but a polyhedral approxiamation.  Imagine
// it's rotated in such a way, that one of its facets happens to be
// parallel to the XY plane, so that flushing the sphere onto the
// plane, results in the fact resting on it.)

#define DEFINE_FLUSH_TEST_CASE(NAME, OP, VOLUME, N)                     \
BOOST_DATA_TEST_CASE(flush_bounding_## NAME,                            \
                     (boost::unit_test::data::make({false, true})       \
                      * boost::unit_test::data::make({-1, 1})           \
                      * boost::unit_test::data::make({-1, 1})           \
                      * boost::unit_test::data::make({-1, 1})),         \
                     p, lambda, mu, nu)                                 \
{                                                                       \
    /* This gets popped back on fixture destructon. */                  \
                                                                        \
    Tolerances::curve = FT::ET(1, 9);                                   \
                                                                        \
    const FT c = FT::ET(12345, 6789);                                   \
    const auto T = (                                                    \
        SCALING_3(c, c, c)                                              \
        * basic_rotation(90, 0)                                         \
        * TRANSLATION_3(1, 2, 3));                                      \
                                                                        \
    const auto &result = evaluate(                                      \
        p                                                               \
        ? CONVERT_TO<Polyhedron>(                                       \
            MINKOWSKI_SUM(                                              \
                FLUSH(TRANSFORM(OP, T), lambda, mu, nu),                \
                OCTAHEDRON(FT::ET(2, 1000),                             \
                           FT::ET(2, 1000),                             \
                           FT::ET(1, 1000))))                           \
        : FLUSH(TRANSFORM(OP, T), lambda, mu, nu));                     \
                                                                        \
    evaluate_operations();                                              \
                                                                        \
    auto &P = *result.value;                                            \
    const auto v = VERTICES_IN(                                         \
        (VOLUME)->transform(T)->flush(lambda, mu, nu));                 \
                                                                        \
    if (p) {                                                            \
        BOOST_TEST(v->apply(P).size() == 0);                            \
    } else {                                                            \
        const int n = N > 0 ? N : P.size_of_vertices();                 \
        BOOST_TEST(v->apply(P).size() == n);                            \
    }                                                                   \
}

DEFINE_FLUSH_TEST_CASE(box, CUBOID(1, 2, 3), BOUNDING_BOX(1, 2, 3), 0)
DEFINE_FLUSH_TEST_CASE(sphere, SPHERE(3), BOUNDING_SPHERE(3), 0)
DEFINE_FLUSH_TEST_CASE(cylinder, CYLINDER(3, 5), BOUNDING_CYLINDER(3, 5), 0)
DEFINE_FLUSH_TEST_CASE(union,
                       JOIN(
                           TRANSFORM(CYLINDER(3, 6), TRANSLATION_3(3, 0, 0)),
                           CUBOID(6, 6, 6)),
                       JOIN({
                           TRANSFORM(BOUNDING_CYLINDER(3, 6),
                                     TRANSLATION_3(3, 0, 0)),
                           BOUNDING_BOX(6, 6, 6)}), 0)

DEFINE_FLUSH_TEST_CASE(difference,
                       DIFFERENCE(
                           CUBOID(6, 6, 6),
                           TRANSFORM(CUBOID(6, 6, 6), TRANSLATION_3(3, 0, 0))),
                       DIFFERENCE({
                           BOUNDING_BOX(6, 6, 6),
                           TRANSFORM(BOUNDING_BOX(6, 6, 6),
                                     TRANSLATION_3(3, 0, 0))}), 4)

DEFINE_FLUSH_TEST_CASE(intersection,
                       INTERSECTION(
                           TRANSFORM(CYLINDER(3, 7), TRANSLATION_3(3, 3, 0)),
                           CUBOID(6, 6, 6)),
                       INTERSECTION({
                           TRANSFORM(BOUNDING_CYLINDER(3, 7),
                                     TRANSLATION_3(3, 3, 0)),
                           BOUNDING_BOX(6, 6, 6)}), 0)

#undef DEFINE_FLUSH_TEST_CASE

// ## Feature-Based Selection Tests

// To test sharp edge detection, we create a square bipyramid, with
// side length 2 and height 1.  The edges making up the base therefore
// share faces that form a right angle.  The edges that are incident
// to the tips share faces at 60 degree angles.

// We therefore test that:

BOOST_AUTO_TEST_CASE_TEMPLATE(sharp_angle_edges, T, types)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYHEDRON_HULL_OPEN();
            h->push_back(CUBOID(2, 2, 0));
            h->push_back(Point_3(0, 0, 1));
            h->push_back(Point_3(0, 0, -1));
            return CONVERT_TO<T>(POLYHEDRON_HULL_CLOSE(h));
        });

    evaluate_operations();
    auto &P = *result.value;
    const auto map = CGAL::get(CGAL::vertex_point, P);

    //   1. There are no edges sharper than 90 degrees.

    {
        auto v = EDGES_BY_SHARPNESS_ANGLE(91)->apply(P);

        BOOST_TEST(v.size() == 0);
    }

    //   2. For angles between 90 and 60 degrees, only the base edges
    //   are selected.

    for (int theta: {90, 61}) {
        auto v = EDGES_BY_SHARPNESS_ANGLE(theta)->apply(P);

        BOOST_TEST(v.size() == 4);
        for (auto &x: v) {
            BOOST_TEST(boost::get(map, CGAL::target(x, P)).z() == FT(0));
        }
    }

    //   3. All edges are sharper than 60 degrees.

    {
        auto v = EDGES_BY_SHARPNESS_ANGLE(60)->apply(P);

        BOOST_TEST(v.size() == 12);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(sharp_mode_edges, T, types)
{
    // An untriangulated octagonal prism will have two modes:

    const auto &result = evaluate(PRISM(8, 1, 1));

    evaluate_operations();
    auto &P = *result.value;
    const auto map = CGAL::get(CGAL::vertex_point, P);

    //   1. one at 90 degrees for its horizontal edges and

    {
        auto v = EDGES_BY_SHARPNESS_MODE(1)->apply(P);

        BOOST_TEST(v.size() == 16);
        for (auto &x: v) {
            BOOST_TEST(
                boost::get(map, CGAL::source(x, P)).z()
                == boost::get(map, CGAL::target(x, P)).z());
        }
    }

    //   2. the other at 45 degrees for the vertical edges:

    {
        auto v = DIFFERENCE({
                EDGES_BY_SHARPNESS_MODE(2),
                EDGES_BY_SHARPNESS_MODE(1)})->apply(P);

        BOOST_TEST(v.size() == 8);
        for (auto &x: v) {
            BOOST_TEST(
                boost::get(map, CGAL::source(x, P)).z()
                != boost::get(map, CGAL::target(x, P)).z());
        }
    }
}

// To test selection of sharp face patches, we use similar geometry,
// only now the central section of the bipyramid has some width.  Its
// edges therefore now have an angle of 45 degrees.

BOOST_AUTO_TEST_CASE_TEMPLATE(sharp_angle_faces, T, types)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYHEDRON_HULL_OPEN();
            h->push_back(CUBOID(2, 2, 2));
            h->push_back(Point_3(0, 0, 2));
            h->push_back(Point_3(0, 0, -2));
            return CONVERT_TO<T>(POLYHEDRON_HULL_CLOSE(h));
        });

    evaluate_operations();
    auto &P = *result.value;

    Vector_3 n(0, 0, 0);
    for (int i = 0; i < 4; i++) {
        // Using a threshold of 60 degrees therefore now splits the
        // geometry along the vertical edges into four parts, each
        // containing four faces (one on each tip plus two triangular
        // faces making up the square side).

        auto v = FACES_BY_SHARPNESS_ANGLE(
            60, std::vector<int> {i + 1})->apply(P);

        BOOST_TEST(v.size() == 4);

        Vector_3 n_i(0, 0, 0);

        for (auto &x: v) {
            n_i += CGAL::Polygon_mesh_processing::compute_face_normal(x, P);
        }

        // The vertical components of the tip face normals cancel out
        // and the sum thefore only has one non-zero component, along
        // the positive or negative X or Y axis.

        BOOST_TEST(n_i.z() == FT(0));
        BOOST_TEST((n_i.x() == FT(0) || n_i.y() == FT(0)));

        n += n_i;
    }

    // Since the four parts are pairwise opposite, the sum of the
    // normals should vanish.

    BOOST_TEST(n == Vector_3(0, 0, 0));

    // We also test selection of more than one patches.  Patches 1 and
    // 4 should be opposed and hence have complementary normals.

    for (auto &x: FACES_BY_SHARPNESS_ANGLE(
             60, std::vector<int> {1, 4})->apply(P)) {
        n += CGAL::Polygon_mesh_processing::compute_face_normal(x, P);
    }

    BOOST_TEST(n == Vector_3(0, 0, 0));
}

// This is similar, with the difference, that we select the sides of
// two prisms, by shooting a horizontal line through them, to select
// one side face in each and then selecting all others in the same
// components.

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(1e-15))
BOOST_AUTO_TEST_CASE_TEMPLATE(sharp_expanding_faces, T, types)
{
    const auto &result = evaluate(
        CONVERT_TO<T>(
            JOIN(
                TRANSFORM(
                    CONVERT_TO<Nef_polyhedron>(PRISM(8, 1, 3)),
                    TRANSLATION_3(2, 0, 0)),
                TRANSFORM(
                    CONVERT_TO<Nef_polyhedron>(PRISM(12, 1, 3)),
                    TRANSLATION_3(-2, 0, 0)))));

    evaluate_operations();
    auto &P = *result.value;

    auto v = FACES_BY_SHARPNESS_ANGLE(
        90, FACES_THROUGH(
            Line_3(Point_3(0, 0, 0), Point_3(1, 0, 0))))->apply(P);

    // We exepect as many quad faces as there are sides in a dodecagon
    // and an octagon.

    BOOST_TEST(v.size() == 20);

    Vector_3 n(0, 0, 0);

    for (auto &x: v) {
        const auto n_i = CGAL::Polygon_mesh_processing::compute_face_normal(x, P);

        // Each face should be vertical.

        BOOST_TEST(n_i.z() == FT(0));

        // We also accumulate the normals.  Their sum should vanish as
        // the sides are symmetrical around the z axis.

        n += n_i;
    }

    for (int i = 0; i < 3; i++) {
        BOOST_TEST(CGAL::to_double(n[i]) == 0.0);
    }
}

// For sharp mode face selection, we use an octagonal prism with a
// pyramidal bottom.

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(1e-17))
BOOST_AUTO_TEST_CASE_TEMPLATE(sharp_mode_faces, T, types)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYHEDRON_HULL_OPEN();
            h->push_back(PRISM(8, 1, 1));
            h->push_back(Point_3(0, 0, -1));
            return CONVERT_TO<T>(POLYHEDRON_HULL_CLOSE(h));
        });

    evaluate_operations();
    auto &P = *result.value;

    // The geometry has 4 modes at:

    //   1. 90 degrees for the top,
    //   2. about 60 degrees for the horizontal edges at the pointy
    //   bottom end,
    //   3. 45 degrees for the vertical edges of the octagon,
    //   4. about 20 degrees for the vertical edges of the bottom cap.

    // We select the second mode, which should give 3 components:

    for (int i = 1; i <= 3; i++) {
        for (const auto &x: FACES_BY_SHARPNESS_MODE(
                 2, std::vector<int> {i})->apply(P)) {
            const auto n_i = CGAL::Polygon_mesh_processing::compute_face_normal(x, P);

            //   1. a flat top with horizontal faces,

            BOOST_TEST((i == 3) == (CGAL::to_double(n_i.z()) == 1.0));

            //   2. an octagonal middle with vertical faces and

            BOOST_TEST((i == 2) == (CGAL::to_double(n_i.z()) == 0.0));

            //   3. an octagonal pyramid at the bottom with oblique
            //   faces, tested implicitly by the above.
        }
    }
}

// ## Intersection-Based Selection Tests

// To test intersection selection of faces or edges, we create a
// simple cuboid and apply various queries to it.

BOOST_AUTO_TEST_CASE_TEMPLATE(intersecting_edges, T, types)
{
    const auto &result = evaluate(
        [] {
            return CONVERT_TO<T>(CUBOID(2, 2, 2));
        });

    evaluate_operations();
    auto &P = *result.value;
    const auto map = CGAL::get(CGAL::vertex_point, P);

    // Queries include:

    //   1. segment, where we aim for the edge vertical edge through
    //   $(1, 1, 0)$,

    {
        auto v = EDGES_THROUGH(
            Segment_3(
                Point_3(0, 0, 0),
                Point_3(FT(FT::ET(9, 10)), FT(FT::ET(9, 10)), 0)))->apply(P);

        BOOST_TEST(v.size() == 0);

        auto u = EDGES_THROUGH(
            Segment_3(Point_3(0, 0, 0), Point_3(1, 1, 0)))->apply(P);

        BOOST_REQUIRE(u.size() == 1);
        for (const auto &x: {CGAL::source(u[0], P), CGAL::target(u[0], P)}) {
            const auto &p = boost::get(map, x);
            BOOST_TEST((p.x() == 1 && p.y() == 1));
        }
    }

    //   2. ray, aiming for the same,

    {
        auto v = EDGES_THROUGH(
            Ray_3(Point_3(0, 0, 0), Point_3(1, 0, 0)))->apply(P);

        BOOST_TEST(v.size() == 0);

        auto u = EDGES_THROUGH(
            Ray_3(
                Point_3(0, 0, 0),
                Point_3(FT(FT::ET(1, 2)), FT(FT::ET(1, 2)), 0)))->apply(P);

        BOOST_REQUIRE(u.size() == 1);
        for (const auto &x: {CGAL::source(u[0], P), CGAL::target(u[0], P)}) {
            const auto &p = boost::get(map, x);
            BOOST_TEST((p.x() == 1 && p.y() == 1));
        }
    }

    //   2. line, aiming for the opposing edge as well, or

    {
        auto v = EDGES_THROUGH(
            Line_3(Point_3(0, 0, 0), Point_3(1, 0, 0)))->apply(P);

        BOOST_TEST(v.size() == 0);

        auto u = EDGES_THROUGH(
            Line_3(
                Point_3(0, 0, 0),
                Point_3(FT(FT::ET(1, 2)), FT(FT::ET(1, 2)), 0)))->apply(P);

        BOOST_REQUIRE(u.size() == 2);
        for (const auto &x: u) {
            const auto p = boost::get(map, CGAL::source(x, P));
            const auto q = boost::get(map, CGAL::target(x, P));

            BOOST_TEST(
                (CGAL::abs(p.x()) == 1
                 && p.x() == p.y() && p.y() == q.x() && q.x() == q.y()));
        }
    }

    //   3. plane, cutting through all vertical edges.

    {
        auto v = EDGES_THROUGH(Plane_3(0, 0, 1, FT(FT::ET(11, 10))))->apply(P);

        BOOST_TEST(v.size() == 0);

        auto u = EDGES_THROUGH(Plane_3(0, 0, 1, 0))->apply(P);

        BOOST_REQUIRE(u.size() == 4);
        for (const auto &x: u) {
            const auto d = (boost::get(map, CGAL::source(x, P))
                            - boost::get(map, CGAL::target(x, P)));

            BOOST_TEST((d.x() == 0 && d.y() == 0));
        }
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(intersecting_faces, T, types)
{
    const auto &result = evaluate(
        [] {
            return CONVERT_TO<T>(CUBOID(2, 2, 2));
        });

    evaluate_operations();
    auto &P = *result.value;

    // Similarly to the test on edges, queries include:

    //   1. segment, where we aim for the face at $x = 1$,

    {
        auto v = FACES_THROUGH(
            Segment_3(
                Point_3(0, 0, 0),
                Point_3(FT(FT::ET(9, 10)), 0, 0)))->apply(P);

        BOOST_TEST(v.size() == 0);

        auto u = FACES_THROUGH(
            Segment_3(Point_3(0, 0, 0), Point_3(1, 0, 0)))->apply(P);

        BOOST_REQUIRE(u.size() == 1);
        BOOST_TEST(
            (CGAL::Polygon_mesh_processing::compute_face_normal(u[0], P)
             == Vector_3(1, 0, 0)));
    }

    //   2. ray, aiming for the same,

    {
        auto v = FACES_THROUGH(
            Ray_3(
                Point_3(FT(FT::ET(11, 10)), 0, 0), Point_3(2, 0, 0)))->apply(P);

        BOOST_TEST(v.size() == 0);

        auto u = FACES_THROUGH(
            Ray_3(
                Point_3(0, 0, 0), Point_3(FT(FT::ET(1, 2)), 0, 0)))->apply(P);

        BOOST_REQUIRE(u.size() == 1);
        BOOST_TEST(
            (CGAL::Polygon_mesh_processing::compute_face_normal(u[0], P)
             == Vector_3(1, 0, 0)));
    }

    //   2. line, aiming for the opposing face as well, or

    {
        auto v = FACES_THROUGH(
            Line_3(
                Point_3(0, FT(FT::ET(11, 10)), 0),
                Point_3(1, FT(FT::ET(11, 10)), 0)))->apply(P);

        BOOST_TEST(v.size() == 0);

        auto u = FACES_THROUGH(
            Line_3(
                Point_3(0, 0, 0), Point_3(FT(FT::ET(1, 2)), 0, 0)))->apply(P);

        BOOST_REQUIRE(u.size() == 2);
        for (const auto &x: u) {
            const auto n =
                CGAL::Polygon_mesh_processing::compute_face_normal(x, P);

            BOOST_TEST((n == Vector_3(1, 0, 0) || n == Vector_3(-1, 0, 0)));
        }
    }

    //   3. plane, cutting through all vertical faces.

    {
        auto v = FACES_THROUGH(Plane_3(0, 0, 1, FT(FT::ET(11, 10))))->apply(P);

        BOOST_TEST(v.size() == 0);

        auto u = FACES_THROUGH(Plane_3(0, 0, 1, 0))->apply(P);

        BOOST_REQUIRE(u.size() == 4);
        for (const auto &x: u) {
            const auto n =
                CGAL::Polygon_mesh_processing::compute_face_normal(x, P);

            BOOST_TEST(n.z() == 0);
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
