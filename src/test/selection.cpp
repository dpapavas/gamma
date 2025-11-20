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

#include <CGAL/draw_polyhedron.h>
#include <CGAL/draw_surface_mesh.h>

// These are (selector_type, mesh_type) tuples.

using types = boost::mpl::list<Polyhedron, Surface_mesh>;

BOOST_FIXTURE_TEST_SUITE(selection, Coarse_evaluation_fixture)

static const FT epsilon(FT::ET(1, 1'000'000));
#define TEST_SELECTION(S, N) BOOST_TEST(S->apply(M).size() == N);

BOOST_AUTO_TEST_CASE_TEMPLATE(on_cuboid, T, types)
{
    const auto &result = evaluate(CONVERT_TO<T>(CUBOID(2, 2, 2)));
    evaluate_operations();
    auto &M = *result.value;

    // Plane

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

    // Halfspace interior

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

    // Halfspace

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

    // Box

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

    // Box interior

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

    // Box boundary

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

BOOST_AUTO_TEST_CASE_TEMPLATE(on_cylinder, T, types)
{
    const auto &result = evaluate(
        CONVERT_TO<T>(DIFFERENCE(CYLINDER(2, 2), CYLINDER(1, 1))));
    evaluate_operations();
    auto &M = *result.value;

    // Cylinder

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

    // Cylinder interior

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

    // Cylinder boundary

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

BOOST_AUTO_TEST_CASE_TEMPLATE(on_sphere, T, types)
{
    const auto &result = evaluate(
        CONVERT_TO<T>(DIFFERENCE(SPHERE(2), SPHERE(1))));
    evaluate_operations();
    auto &M = *result.value;

    // Sphere

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

    // Sphere interior

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

    // Sphere boundary

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

// ### Relative Selection Tests (Expansion and Contraction)

BOOST_AUTO_TEST_CASE_TEMPLATE(relative_vertices, T, types)
{
    std::vector<Aff_transformation_3> v;

    // A cuboid with secctions for z = -2 .. 2.

    for (int i = -2; i <= 2 ; i++) {
        v.push_back(TRANSLATION_3(0, 0, i));
    }

    const auto &result = evaluate(
        CONVERT_TO<T>(EXTRUSION(RECTANGLE(1, 1), std::move(v))));
    evaluate_operations();
    auto &P = *result.value;

    // a: plane z = 0

    const auto a = VERTICES_IN(BOUNDING_PLANE(0, 0, 1, 0));

    BOOST_TEST(a->apply(P).size() == 4);

    // b: expanded to plane -1 <= z <= 1

    const auto b = RELATIVE_SELECTION(a, 1);
    auto bv = b->apply(P);

    std::sort(bv.begin(), bv.end());

    BOOST_TEST(bv.size() == 12);

    // c: same, but through joining (so it should be sorted)

    const auto c = JOIN({
            a,
            VERTICES_IN(BOUNDING_PLANE(0, 0, 1, 1)),
            VERTICES_IN(BOUNDING_PLANE(0, 0, 1, -1))});

    BOOST_TEST(c->apply(P) == bv);

    // d: expanded to all vertices

    const auto d = RELATIVE_SELECTION(a, 2);

    BOOST_TEST(d->apply(P).size() == 20);

    // e: planes z = +/- 2

    const auto e = COMPLEMENT(b);
    const auto ev = e->apply(P);

    BOOST_TEST(ev.size() == 8);
    BOOST_TEST(INTERSECTION({b, e})->apply(P).size() == 0);

    // f: same as b, but by removing the end planes

    const auto f = DIFFERENCE({d, e});

    BOOST_TEST(f->apply(P) == bv);

    // g: planes z != 0

    const auto g = DIFFERENCE({d, a});

    BOOST_TEST(g->apply(P).size() == 16);
    BOOST_TEST(INTERSECTION({g, e})->apply(P) == ev);

    // h: same as e, but through contraction

    const auto h = RELATIVE_SELECTION(g, -1);
    auto hv = h->apply(P);

    std::sort(hv.begin(), hv.end());

    BOOST_TEST(hv == ev);
}

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

    // a: planes -1 <= z <= 1

    const auto a = FACES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, 0));

    BOOST_TEST(a->apply(P).size() == 16);

    // b: expanded to -2 <= z <= 2 (but w/o the caps)

    const auto b = RELATIVE_SELECTION(a, 1);
    auto bv = b->apply(P);

    std::sort(bv.begin(), bv.end());

    BOOST_TEST(bv.size() == 32);

    // c: same, but through joining (so it should be sorted)

    const auto c = JOIN({
            a,
            FACES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, 1)),
            FACES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, -1))});

    BOOST_TEST(c->apply(P) == bv);

    // d: expanded to all faces

    const auto d = RELATIVE_SELECTION(a, 2);
    auto dv = d->apply(P);

    std::sort(dv.begin(), dv.end());

    BOOST_TEST(dv.size() == 36);

    // e: caps only

    const auto e = RELATIVE_SELECTION(DIFFERENCE({d, a}), -1);
    auto ev = e->apply(P);

    std::sort(ev.begin(), ev.end());

    BOOST_TEST(ev.size() == 4);
    BOOST_TEST(INTERSECTION({b, e})->apply(P).size() == 0);
    BOOST_TEST(JOIN({b, e})->apply(P) == dv);
    BOOST_TEST(COMPLEMENT(b)->apply(P) == ev);
}

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

    // a: planes -1 < z < 1 (not including edges on the planes)

    const auto a = EDGES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, 0));

    BOOST_TEST(a->apply(P).size() == 20);

    // b: expanded to -2 < z < 2 (but w/o the caps)

    const auto b = RELATIVE_SELECTION(a, 1);
    auto bv = b->apply(P);

    std::sort(bv.begin(), bv.end());

    BOOST_TEST(bv.size() == 44);

    // c: same, but through joining (so it should be sorted)

    const auto c = JOIN({
            a,
            EDGES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, 1)),
            EDGES_PARTIALLY_IN(BOUNDING_PLANE(0, 0, 1, -1))});

    BOOST_TEST(c->apply(P) == bv);

    // d: expanded to all vertices

    const auto d = RELATIVE_SELECTION(a, 2);
    auto dv = d->apply(P);

    std::sort(dv.begin(), dv.end());

    BOOST_TEST(dv.size() == 54);

    // e: caps only

    const auto e = RELATIVE_SELECTION(DIFFERENCE({d, a}), -1);
    auto ev = e->apply(P);

    std::sort(ev.begin(), ev.end());

    BOOST_TEST(ev.size() == 10);
    BOOST_TEST(INTERSECTION({b, e})->apply(P).size() == 0);
    BOOST_TEST(JOIN({b, e})->apply(P) == dv);
    BOOST_TEST(COMPLEMENT(b)->apply(P) == ev);
}

// ### Selection Conversion (Vertices in Selected Faces, etc.)

BOOST_AUTO_TEST_CASE_TEMPLATE(conversion, T, types)
{
    std::vector<Aff_transformation_3> v;

    const auto &result = evaluate(CONVERT_TO<T>(CUBOID(2, 2, 2)));
    evaluate_operations();
    auto &P = *result.value;

    // From vertices

    // a: vertices on plane z = 1, converted to faces

    const auto a = FACES_IN(VERTICES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(a->apply(P).size() == 1);

    // b: vertices on plane z = -1, converted to faces (partial)

    const auto b = FACES_PARTIALLY_IN(VERTICES_IN(BOUNDING_PLANE(0, 0, 1, 1)));

    BOOST_TEST(b->apply(P).size() == 5);
    BOOST_TEST(INTERSECTION({a, b})->apply(P).size() == 0);

    // c: vertices on plane z = 1, converted to edges

    const auto c = EDGES_IN(VERTICES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(c->apply(P).size() == 4);

    // d: vertices on plane z = -1, converted to edges (partial)

    const auto d = EDGES_PARTIALLY_IN(VERTICES_IN(BOUNDING_PLANE(0, 0, 1, 1)));

    BOOST_TEST(d->apply(P).size() == 8);
    BOOST_TEST(INTERSECTION({c, d})->apply(P).size() == 0);

    // From faces

    // e: faces on plane z = 1, converted to vertices

    const auto e = VERTICES_IN(FACES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(e->apply(P).size() == 4);

    // f: faces on plane z = 1, converted to edges

    const auto f = EDGES_IN(FACES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(f->apply(P).size() == 4);

    // g: faces on plane z = -1, converted to edges (partial)

    const auto g = EDGES_PARTIALLY_IN(FACES_IN(BOUNDING_PLANE(0, 0, 1, 1)));

    BOOST_TEST(g->apply(P).size() == 8);
    BOOST_TEST(INTERSECTION({f, g})->apply(P).size() == 0);

    // From edges

    // h: edges on plane z = 1, converted to vertices

    const auto h = VERTICES_IN(EDGES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(h->apply(P).size() == 4);

    // i: edges on plane z = 1, converted to faces

    const auto i = FACES_IN(EDGES_IN(BOUNDING_PLANE(0, 0, 1, -1)));

    BOOST_TEST(i->apply(P).size() == 1);

    // j: edges on plane z = -1, converted to faces (partial)

    const auto j = FACES_PARTIALLY_IN(EDGES_IN(BOUNDING_PLANE(0, 0, 1, 1)));

    BOOST_TEST(j->apply(P).size() == 5);
    BOOST_TEST(INTERSECTION({i, j})->apply(P).size() == 0);
}

// ### Bounding Volume Flush Operations Tests

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

BOOST_AUTO_TEST_SUITE_END()
