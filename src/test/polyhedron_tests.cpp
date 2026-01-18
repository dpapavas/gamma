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

#include <filesystem>

#include <CGAL/boost/graph/convert_nef_polyhedron_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/measure.h>

#include "kernel.h"
#include "transformations.h"
#include "tolerances.h"
#include "macros.h"

#include "fixtures.h"
#include "polyhedron_tests.h"

namespace CGAL {
    FT exact(FT d)
    {
        return d;
    }
}

typedef boost::mpl::list<
    Polyhedron, Nef_polyhedron, Surface_mesh> polyhedron_types;

FT polyhedron_volume(const Nef_polyhedron &N)
{
    Surface_mesh M;

    CGAL::convert_nef_polyhedron_to_polygon_mesh(N, M, true);

    return CGAL::Polygon_mesh_processing::volume(M);
}

FT polyhedron_volume(const Surface_mesh &M)
{
    return CGAL::Polygon_mesh_processing::volume(M);
}

FT polyhedron_volume(const Polyhedron &P)
{
    Polyhedron Q(P);
    CGAL::Polygon_mesh_processing::triangulate_faces(Q.facet_handles(), Q);

    return CGAL::Polygon_mesh_processing::volume(Q);
}

const Polyhedron &test_polyhedron(
    const Polyhedron &P,
    const int vertices, const int halfedges, const int facets)
{
    maybe_output_polyhedron(P);

    BOOST_TEST(P.size_of_vertices() == vertices);
    BOOST_TEST(P.size_of_halfedges() == halfedges);
    BOOST_TEST(P.size_of_facets() == facets);

    return P;
}

const Nef_polyhedron &test_polyhedron(
    const Nef_polyhedron &N,
    const int vertices, const int halfedges, const int facets)
{
    maybe_output_polyhedron(N);

    BOOST_TEST(N.number_of_vertices() == vertices);
    BOOST_TEST(N.number_of_halfedges() == halfedges);
    BOOST_TEST(N.number_of_facets() == facets);

    return N;
}

const Surface_mesh &test_polyhedron(
    const Surface_mesh &M,
    const int vertices, const int halfedges, const int facets)
{
    maybe_output_polyhedron(M);

    BOOST_TEST(M.number_of_vertices() == vertices);
    BOOST_TEST(M.number_of_halfedges() == halfedges);
    BOOST_TEST(M.number_of_faces() == facets);

    return M;
}

template<typename T, typename U = Polyhedron>
static constexpr std::string conversion_tag(const std::string s)
{
    if constexpr(std::is_same_v<T, U>) {
        return s;
    } else if constexpr(std::is_same_v<T, Polyhedron>) {
        return "polyhedron(" + s + ")";
    } else if constexpr(std::is_same_v<T, Nef_polyhedron>) {
        return "nef(" + s + ")";
    } else {
        static_assert(std::is_same_v<T, Surface_mesh>);
        return "mesh(" + s + ")";
    }
}

#define UNIT_TETRAHEDRON TETRAHEDRON(1, 1, 1)
#define test_unit_tetrahedron(X) test_polyhedron(X, 4, 12, 4, FT(FT::ET(1, 6)))

BOOST_FIXTURE_TEST_SUITE(polyhedron, Evaluation_fixture)

////////////////
// Primitives //
////////////////

BOOST_DATA_TEST_CASE(tetrahedron,
                     (boost::unit_test::data::make({1, -1})
                      * boost::unit_test::data::make({1, -1})
                      * boost::unit_test::data::make({1, -1})),
                     i, j, k)
{
    const auto &result = evaluate(TETRAHEDRON(i, j, k));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag("tetrahedron(", i, ",", j, ",", k, ")"));
    test_unit_tetrahedron(*result.value);
}

BOOST_DATA_TEST_CASE(square_pyramid,
                     (boost::unit_test::data::make({3, -3})),
                     h)
{
    const auto &result = evaluate(SQUARE_PYRAMID(2, 2, h));

    evaluate_operations();

    BOOST_TEST(result.tag == tag("square_pyramid(2,2,", h, ")"));
    test_polyhedron(*result.value, 5, 16, 5, 2);
}

BOOST_DATA_TEST_CASE(octahedron,
                     (boost::unit_test::data::make({1, -1})
                      * boost::unit_test::data::make(
                          {FT(FT::ET(1, 2)), FT(FT::ET(-1, 2))})),
                     h_1, h_2)
{
    const auto &result = evaluate(OCTAHEDRON(2, 2, h_1, h_2));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag("octahedron(2,2,", h_1, ",", h_2.exact(), ")"));
    test_polyhedron(*result.value, 6, 24, 8, 2 * CGAL::abs(h_1 + h_2) / 3);
}

BOOST_AUTO_TEST_CASE(cuboid)
{
    const auto &result = evaluate(CUBOID(2, 3, 4));

    evaluate_operations();

    BOOST_TEST(result.tag == "cuboid(2,3,4)");
    test_polyhedron(*result.value, 8, 24, 6, 2 * 3 * 4);
    BOOST_TEST(
        CGAL::centroid(result.value->points_begin(), result.value->points_end())
        == Point_3(CGAL::ORIGIN));
}

BOOST_AUTO_TEST_CASE(icosahedron, * boost::unit_test::tolerance(0.0001))
{
    const auto &result = evaluate(ICOSAHEDRON(2));

    evaluate_operations();

    BOOST_TEST(result.tag == "icosahedron(2,1/1000000)");
    test_polyhedron(
        *result.value, 12, 60, 20,
        80 * (1 + sqrt(5) / 3) / std::pow(2 * sqrt(5) + 10, 1.5) * 8);
}

BOOST_AUTO_TEST_CASE(sphere, * boost::unit_test::tolerance(0.0025))
{
    const auto &result = evaluate(SPHERE(2));

    evaluate_operations();

    BOOST_TEST(result.tag == "sphere(2,1/1000,1/1000000)");
    test_polyhedron_volume(*result.value, std::acos(-1) * 4 / 3 * 8);
}

BOOST_AUTO_TEST_CASE(cylinder, * boost::unit_test::tolerance(0.0015))
{
    const auto &result = evaluate(CYLINDER(2, 10));

    evaluate_operations();

    test_polyhedron_volume(*result.value, std::acos(-1) * 4 * 10);
}

BOOST_DATA_TEST_CASE(regular_pyramid,
                     (boost::unit_test::data::make({3, -3})),
                     h)
{
    const auto &result = evaluate(REGULAR_PYRAMID(4, 1, h));

    evaluate_operations();

    BOOST_TEST(result.tag == tag("regular_pyramid(4,1,", h, ",1/1000000)"));
    test_polyhedron(*result.value, 5, 16, 5, 2);
}

BOOST_DATA_TEST_CASE(regular_bipyramid,
                     (boost::unit_test::data::make({1, -1})
                      * boost::unit_test::data::make({FT(FT::ET(1, 2)),
                              FT(FT::ET(-1, 2))})),
                     h_1, h_2)
{
    const auto &result = evaluate(REGULAR_BIPYRAMID(4, 1, h_1, h_2));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag(
            "regular_bipyramid(4,1,", h_1, ",", h_2.exact(), ",1/1000000)"));
    test_polyhedron(*result.value, 6, 24, 8, 2 * CGAL::abs(h_1 + h_2) / 3);
}

///////////////////////////
// Conversion operations //
///////////////////////////

#define DEFINE_CONVERSION_TEST_CASE(T)                                  \
BOOST_AUTO_TEST_CASE_TEMPLATE(to_## T, U, polyhedron_types)             \
{                                                                       \
    const auto &result = evaluate(                                      \
        CONVERT_TO<T>(CONVERT_TO<U>(UNIT_TETRAHEDRON)));                \
                                                                        \
    evaluate_operations();                                              \
    BOOST_TEST(                                                         \
        result.tag == (conversion_tag<T, U>(                            \
                           conversion_tag<U>("tetrahedron(1,1,1)"))));  \
    test_unit_tetrahedron(*result.value);                               \
}

DEFINE_CONVERSION_TEST_CASE(Polyhedron)
DEFINE_CONVERSION_TEST_CASE(Nef_polyhedron)
DEFINE_CONVERSION_TEST_CASE(Surface_mesh)

#undef DEFINE_CONVERSION_TEST_CASE

////////////////////
// Transformation //
////////////////////

BOOST_AUTO_TEST_CASE(rotation, * boost::unit_test::tolerance(0.0001))
{
    const auto &result = evaluate(
        [] {
            const double v[] = {1, 1, 1};

            return INTERSECTION(
                TRANSFORM(
                    UNIT_TETRAHEDRON,
                    basic_rotation(90, 1)
                    * basic_rotation(90, 2)),
                TRANSFORM(
                    UNIT_TETRAHEDRON,
                    axis_angle_rotation(120, v)));
        });

    evaluate_operations();

    test_polyhedron_volume(*result.value, 1.0 / 6.0);
}

BOOST_AUTO_TEST_CASE(reflection)
{
    const auto &result = evaluate(
        TRANSFORM(UNIT_TETRAHEDRON, SCALING_3(-1, 1, 1)));

    evaluate_operations();

    test_unit_tetrahedron(*result.value);
}

BOOST_AUTO_TEST_CASE(nef_reflection)
{
    const auto &result = evaluate(
        JOIN(
            CONVERT_TO<Nef_polyhedron>(UNIT_TETRAHEDRON),
            TRANSFORM(
                CONVERT_TO<Nef_polyhedron>(
                    UNIT_TETRAHEDRON), SCALING_3(-1, 1, 1))));

    evaluate_operations();

    test_polyhedron(*result.value, 4, 12, 4, FT(FT::ET(1, 3)));
}

BOOST_AUTO_TEST_CASE(mesh_reflection)
{
    const auto &result = evaluate(
        TRANSFORM(CONVERT_TO<Surface_mesh>(UNIT_TETRAHEDRON),
                  SCALING_3(-1, 1, 1)));

    evaluate_operations();

    test_unit_tetrahedron(*result.value);
}

BOOST_AUTO_TEST_CASE(polygon_transformation)
{
    const auto &result = evaluate(
        TRANSFORM(RECTANGLE(2, 2), basic_rotation(90, 0)));

    evaluate_operations();

    BOOST_TEST(result.tag == ("extrusion("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)),"
                              "rotation(1,0,0,0,0,-1,0,1,0))"));
    test_polyhedron(*result.value, 4, 8, 1);
}

BOOST_DATA_TEST_CASE(flush,
                     (boost::unit_test::data::make({1, 0, 0})
                      ^ boost::unit_test::data::make({0, 1, 0})
                      ^ boost::unit_test::data::make({0, 0, 1})),
                     x, y, z)
{
    const auto &result = evaluate(
        JOIN(
            FLUSH(CUBOID(2, 2, 2), x, y, z),
            FLUSH(CUBOID(2, 2, 2), -x, -y, -z)));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag(
            "join(flush(cuboid(2,2,2),0,", x, ",0,", y, ",0,", z,
            "),flush(cuboid(2,2,2),", -x, ",0,", -y, ",0,", -z, ",0))"));
    test_polyhedron(*result.value, 12, 60, 20, 16);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(flush_type, T, polyhedron_types)
{
    const auto &result = evaluate(
        JOIN(
        FLUSH(CONVERT_TO<T>(CUBOID(2, 2, 2)), 1, 0, 0),
        FLUSH(CONVERT_TO<T>(CUBOID(2, 2, 2)), -1, 0, 0)));

    evaluate_operations();

    if constexpr(std::is_same_v<T, Nef_polyhedron>) {
        test_polyhedron(*result.value, 8, 24, 6, 16);
    } else {
        test_polyhedron(*result.value, 12, 60, 20, 16);
    }
}

///////////////
// Extrusion //
///////////////

BOOST_AUTO_TEST_CASE(extrude)
{
    // Do a couple of extrusions to test concurrent access to the same
    // underlying polygon.

    const auto &result = evaluate(
        JOIN(
            EXTRUSION(
                RECTANGLE(2, 3),
                std::move(
                    std::vector(
                        {TRANSLATION_3(0, 0, 1), TRANSLATION_3(0, 0, 5)}))),
            EXTRUSION(
                TRANSFORM(RECTANGLE(2, 3), basic_rotation(45)),
                std::move(
                    std::vector(
                        {TRANSLATION_3(0, 0, -1), TRANSLATION_3(0, 0, -5)})))));

    evaluate_operations();

    BOOST_TEST(
        result.tag == (
            "join(extrusion("
            "polygon(point(-1,-3/2),point(1,-3/2),point(1,3/2),point(-1,3/2)),"
            "translation(0,0,1),translation(0,0,5)),"
            "extrusion(transform("
            "polygon(point(-1,-3/2),point(1,-3/2),point(1,3/2),point(-1,3/2)),"
            "rotation(803761/1136689,-803760/1136689,803760/1136689,803761/1136689)),"
            "translation(0,0,-1),translation(0,0,-5)))"));

    test_polyhedron(*result.value, 16, 72, 24, 48);
    BOOST_TEST(CGAL::is_valid_polygon_mesh(*result.value));
    BOOST_TEST(CGAL::is_closed(*result.value));
}

BOOST_AUTO_TEST_CASE(extrude_circle, * boost::unit_test::tolerance(0.001))
{
    const auto &result = evaluate(
        EXTRUSION(
            DIFFERENCE(CIRCLE(3), RECTANGLE(2, 2)),
            {TRANSLATION_3(0, 0, 0), TRANSLATION_3(0, 0, 1)}));

    evaluate_operations();

    BOOST_TEST(result.tag == ("extrusion("
                              "segments(difference(circle(3),circles("
                              "polygon(point(-1,-1),point(1,-1),"
                              "point(1,1),point(-1,1)))),"
                              "1/1000,1/1000000),"
                              "translation(0,0,0),"
                              "translation(0,0,1))"));

    test_polyhedron_volume(*result.value, std::acos(-1) * 9 - 4);
    BOOST_TEST(CGAL::is_valid_polygon_mesh(*result.value));
    BOOST_TEST(CGAL::is_closed(*result.value));
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.01))
BOOST_DATA_TEST_CASE(extrude_closed,
                     (boost::unit_test::data::make({1, 5}) *
                      boost::unit_test::data::make({0, 1})),
                     rho, q)
{
    const auto &result = evaluate(
        [&rho, &q] {
            std::vector<Aff_transformation_3> v;
            v.reserve(37);

            for (int i = 0; i < 37; i++) {
                v.push_back(
                    basic_rotation(i * 10, 1) * TRANSLATION_3(rho, 0, 0));
            }

            return EXTRUSION(
                (!q ? RECTANGLE(2, 2)
                 : DIFFERENCE(RECTANGLE(2, 2), RECTANGLE(1, 1))),
                std::move(v));
        });

    evaluate_operations();

    test_polyhedron_volume(*result.value, 2 * std::acos(-1) * rho * (4 - q));
    BOOST_TEST(CGAL::is_valid_polygon_mesh(*result.value));
    BOOST_TEST(CGAL::is_closed(*result.value));
}

BOOST_DATA_TEST_CASE(extrude_many,
                     (boost::unit_test::data::make({1, 2, 11})
                      * boost::unit_test::data::make({0, 1})),
                     n, q)
{
    const auto &result = evaluate(
        [&n, &q] {
            std::vector<Aff_transformation_3> v;
            v.reserve(n);

            for (int i = 0; i < n; i += 1) {
                v.push_back(
                    TRANSLATION_3(
                        FT::ET(i % 2 > 0 ? -1 : 1), FT::ET(0), FT::ET(i)));
            }

            return EXTRUSION(
                (!q
                 ? RECTANGLE(2, 2)
                 : DIFFERENCE(RECTANGLE(2, 2), RECTANGLE(1, 1))), std::move(v));
        });

    evaluate_operations();

    BOOST_TEST(CGAL::is_valid_polygon_mesh(*result.value));

    if (n > 1) {
        BOOST_TEST(CGAL::is_closed(*result.value));
    }

    if (n == 1 && q == 0) {
        test_polyhedron(*result.value, 4, 8, 1);
    } else {
        test_polyhedron(
            *result.value,
            4 * n * (q + 1),
            ((n == 1) * (q + 1) * 4 + (1 + (n > 1)) * (q == 0 ? 6 : 24)
             + 24 * (q + 1) * (n - 1)),
            (1 + (n > 1)) * (q == 0 ? 2 : 8) + 8 * (q + 1) * (n - 1));
    }

    if (n > 1) {
        test_polyhedron_volume(*result.value, FT((4 - q) * (n - 1)));
    }
}

/////////////////
// Convex hull //
/////////////////

BOOST_AUTO_TEST_CASE(hull)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYHEDRON_HULL_OPEN();
            h->push_back(TETRAHEDRON(FT::ET(1, 2), 1, 1));
            h->push_back(TETRAHEDRON(FT::ET(-1, 2), 1, 1));
            return POLYHEDRON_HULL_CLOSE(h);
        });

    evaluate_operations();

    BOOST_TEST(
        result.tag == "hull(tetrahedron(1/2,1,1),tetrahedron(-1/2,1,1))");
    test_unit_tetrahedron(*result.value);
}

BOOST_AUTO_TEST_CASE(hull_mixed)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYHEDRON_HULL_OPEN();
            h->push_back(TETRAHEDRON(FT::ET(1, 2), 1, 1));
            h->push_back(Point_3(FT(FT::ET(-1, 2)), 0, 0));
            return POLYHEDRON_HULL_CLOSE(h);
        });

    evaluate_operations();

    BOOST_TEST(result.tag == "hull(tetrahedron(1/2,1,1),point(-1/2,0,0))");
    test_unit_tetrahedron(*result.value);
}

BOOST_AUTO_TEST_CASE(hull_points)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYHEDRON_HULL_OPEN();
            h->push_back(Point_3(CGAL::ORIGIN));
            h->push_back(Point_3(1, 0, 0));
            h->push_back(Point_3(0, 1, 0));
            h->push_back(Point_3(0, 0, 1));
            return POLYHEDRON_HULL_CLOSE(h);
        });

    evaluate_operations();

    BOOST_TEST(result.tag == ("hull(point(0,0,0),point(1,0,0),"
                              "point(0,1,0),point(0,0,1))"));
    test_unit_tetrahedron(*result.value);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(hulls, T, polyhedron_types)
{
    const auto &result = evaluate(
        [] {
            auto h = POLYHEDRON_HULL_OPEN();
            h->push_back(UNIT_TETRAHEDRON);
            h->push_back(
                TRANSFORM(
                    CONVERT_TO<T>(UNIT_TETRAHEDRON), TRANSLATION_3(2, 0, 0)));
            return POLYHEDRON_HULL_CLOSE(h);
        });

    evaluate_operations();

    test_polyhedron(*result.value, 6, 24, 8, FT(FT::ET(7, 6)));
};

///////////////////
// Minkowski sum //
///////////////////

BOOST_AUTO_TEST_CASE(minkowski_sum)
{
    const auto &result = evaluate(
        MINKOWSKI_SUM(CUBOID(10, 10, 10), OCTAHEDRON(2, 2, 1)));

    evaluate_operations();

    BOOST_TEST(result.tag == ("minkowski_sum(nef(cuboid(10,10,10)),"
                              "nef(octahedron(2,2,1,1)))"));
    test_polyhedron(*result.value, 24, 96, 26, FT(FT::ET(4984, 3)));
}

/////////////////
// Subdivision //
/////////////////

BOOST_AUTO_TEST_CASE(loop)
{
    const auto &result = evaluate(LOOP(ICOSAHEDRON(1), 2));

    evaluate_operations();

    BOOST_TEST(result.tag == "loop(icosahedron(1,1/1000000),2)");
    test_polyhedron(
        *result.value, 12 + 60 / 2 + 2 * 60, 4 * 4 * 60, 4 * 4 * 20);
}

BOOST_AUTO_TEST_CASE(catmull_clark)
{
    const auto &result = evaluate(CATMULL_CLARK(CUBOID(1, 1, 1), 2));

    evaluate_operations();

    BOOST_TEST(result.tag == "catmull_clark(cuboid(1,1,1),2)");
    test_polyhedron(
        *result.value, 8 + 24 / 2 + 2 * 24 + 6 + 4 * 6, 4 * 4 * 24, 4 * 4 * 6);
}

BOOST_AUTO_TEST_CASE(doo_sabin)
{
    const auto &result = evaluate(DOO_SABIN(UNIT_TETRAHEDRON, 1));

    evaluate_operations();

    BOOST_TEST(result.tag == "doo_sabin(tetrahedron(1,1,1),1)");
    test_polyhedron(*result.value, 3 * 4, 2 * 12 + 6 * 4, 4 + 12 / 2 + 4);
}

BOOST_AUTO_TEST_CASE(sqrt_3)
{
    const auto &result = evaluate(SQRT_3(ICOSAHEDRON(1), 2));

    evaluate_operations();

    BOOST_TEST(result.tag == "sqrt_3(icosahedron(1,1/1000000),2)");
    test_polyhedron(*result.value, 12 + 20 + 60, 3 * 3 * 60, 3 * 60);
}

////////////////////
// Set operations //
////////////////////

BOOST_AUTO_TEST_CASE_TEMPLATE(join, T, polyhedron_types)
{
    const auto &result = evaluate(
        JOIN(
            CONVERT_TO<T>(UNIT_TETRAHEDRON),
            CONVERT_TO<T>(TETRAHEDRON(-1, 1, 1))));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag(
            "join(", conversion_tag<T>("tetrahedron(1,1,1)"), ",",
            conversion_tag<T>("tetrahedron(-1,1,1))")));
    test_polyhedron_volume(*result.value, FT(FT::ET(1, 3)));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(difference, T, polyhedron_types)
{
    const auto &result = evaluate(
        DIFFERENCE(
            CONVERT_TO<T>(UNIT_TETRAHEDRON),
            CONVERT_TO<T>(TETRAHEDRON(
                              FT::ET(1, 2), FT::ET(1, 2), FT::ET(1, 2)))));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag(
            "difference(", conversion_tag<T>("tetrahedron(1,1,1)"), ",",
            conversion_tag<T>("tetrahedron(1/2,1/2,1/2)"), ")"));
    test_polyhedron_volume(*result.value, FT(FT::ET(7, 48)));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(intersection, T, polyhedron_types)
{
    const auto &result = evaluate(
        INTERSECTION(
            CONVERT_TO<T>(UNIT_TETRAHEDRON),
            CONVERT_TO<T>(TRANSFORM(UNIT_TETRAHEDRON,
                                    TRANSLATION_3(FT::ET(1, 2), 0, 0)))));

    evaluate_operations();

    BOOST_TEST(
        result.tag == tag(
            "intersection(", conversion_tag<T>("tetrahedron(1,1,1)"), ",",
            conversion_tag<T>(
                "transform(tetrahedron(1,1,1),translation(1/2,0,0))"), ")"));
    test_polyhedron(*result.value, 4, 12, 4, FT(FT::ET(1, 48)));
}

BOOST_AUTO_TEST_CASE(symmetric_difference)
{
    const auto &result = evaluate(
        SYMMETRIC_DIFFERENCE(
            UNIT_TETRAHEDRON,
            TRANSFORM(UNIT_TETRAHEDRON, TRANSLATION_3(FT::ET(1, 2), 0, 0))));

    evaluate_operations();

    BOOST_TEST(result.tag == ("symmetric_difference("
                              "nef(tetrahedron(1,1,1)),"
                              "nef(transform("
                              "tetrahedron(1,1,1),"
                              "translation(1/2,0,0))))"));
    test_polyhedron(
        *result.value, 10, 34, 10, FT(2 * (FT::ET(1, 6) - FT::ET(1, 48))));
}

BOOST_AUTO_TEST_CASE(complement)
{
    const auto &result = evaluate(
        [] {
            auto a = CUBOID(4, 5, 6);
            auto b = CUBOID(1, 2, 3);

            return SYMMETRIC_DIFFERENCE(
                DIFFERENCE(a, b),
                INTERSECTION(a, COMPLEMENT(b)));
        });

    evaluate_operations();

    BOOST_TEST(
        result.tag == (
            "symmetric_difference(nef(difference(cuboid(4,5,6),cuboid(1,2,3))),"
            "nef(intersection(cuboid(4,5,6),complement(cuboid(1,2,3)))))"));
    BOOST_TEST(result.value->is_empty());
}

BOOST_AUTO_TEST_CASE(boundary)
{
    const auto &result = evaluate(
        [] {
            auto a = BOUNDARY(CUBOID(6, 7, 8));
            auto b = DIFFERENCE(CUBOID(7, 8, 9), CUBOID(5, 6, 7));
            auto c = MINKOWSKI_SUM(a, CUBOID(1, 1, 1));
            return DIFFERENCE(c, b);
        });

    evaluate_operations();

    BOOST_TEST(
        result.tag == ("difference(minkowski_sum("
                       "boundary(nef(cuboid(6,7,8))),nef(cuboid(1,1,1))),"
                       "nef(difference(cuboid(7,8,9),cuboid(5,6,7))))"));
    BOOST_TEST(result.value->is_empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(non_manifold_edge, T, polyhedron_types)
{
    const auto &result = evaluate(
        JOIN(
            CONVERT_TO<T>(TETRAHEDRON(1, 1, 1)),
            CONVERT_TO<T>(TETRAHEDRON(1, -1, -1))));

    evaluate_operations();

    if constexpr(std::is_same_v<T, Nef_polyhedron>) {
        test_polyhedron(*result.value, 6, 22, 8, FT(FT::ET(1, 3)));
    } else {
        // A resulting mesh with non-manifold edges should fail,
        // so the sink should remain unevaluated.

        BOOST_TEST(result.value == nullptr);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(non_manifold_vertex, T, polyhedron_types)
{
    const auto &result = evaluate(
        JOIN(
            CONVERT_TO<T>(TETRAHEDRON(1, 1, 1)),
            CONVERT_TO<T>(TETRAHEDRON(-1, -1, -1))));

    evaluate_operations();

    if constexpr(std::is_same_v<T, Nef_polyhedron>) {
        test_polyhedron(*result.value, 7, 24, 8, FT(FT::ET(1, 3)));
    } else {
        test_polyhedron(*result.value, 8, 24, 8, FT(FT::ET(1, 3)));
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(clip, T, polyhedron_types)
{
    const auto &result = evaluate(
        CLIP(CONVERT_TO<T>(TETRAHEDRON(2, 2, 2)), Plane_3(0, 0, -1, 1)));

    evaluate_operations();

    BOOST_TEST(result.tag == tag(
                   "clip(", conversion_tag<T>("tetrahedron(2,2,2)"),
                   ",plane(0,0,-1,1))"));
    test_polyhedron_volume(*result.value, FT(FT::ET(1, 6)));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(clip_non_manifold, T, polyhedron_types)
{
    const auto &result = evaluate(
        CLIP(
            CONVERT_TO<T>(
                DIFFERENCE(
                    CUBOID(4, 4, 4),
                    TRANSFORM(CUBOID(2, 2, 4), TRANSLATION_3(1, 1, 0)))),
            Plane_3(-1, -1, 0, 0)));

    evaluate_operations();

    if constexpr (std::is_same_v<T, Nef_polyhedron>) {
        test_polyhedron(*result.value, 10, 34, 10, 16);
    } else {
        BOOST_TEST(result.value == nullptr);
    }
}

// A set of spheres of the same or different radius joined together;
// meant to test parallel execution under some load.

BOOST_TEST_DECORATOR(* boost::unit_test::disabled())
BOOST_DATA_TEST_CASE(spheres,
                     boost::unit_test::data::make({false, true}),
                     q)
{
    const auto &result = evaluate(
        [&q] {
            std::shared_ptr<Polyhedron_operation<Polyhedron>> v[4];

            for (int i = 0; i < 4; i++) {
                v[i] = TRANSFORM(
                    SPHERE(FT(2 + (q ? FT::ET(i, 10) : 0))),
                    TRANSLATION_3(FT(i * FT::ET(3, 2)), 0, 0));
            }

            return JOIN(JOIN(v[0], v[1]), JOIN(v[2], v[3]));
        });

    evaluate_operations();

    BOOST_TEST(!result.value->is_empty());
}

// Intersections and differences of various small cubes with a larger
// one; meant as stress-test.

BOOST_AUTO_TEST_CASE(cube_tessellation, * boost::unit_test::disabled())
{
    const auto &result = evaluate(
        [] {
            const int n = 5;
            const int m = 2 * n + 1;
            std::vector<
                std::shared_ptr<Polyhedron_operation<Nef_polyhedron>>> v;

            auto a = CONVERT_TO<Nef_polyhedron>(CUBOID(m, m, m));
            const auto b = a;

            v.reserve(m * m * m);

            for (int i = -n; i <= n; i++) {
                for (int j = -n; j <= n; j++) {
                    for (int k = -n; k <= n; k++) {
                        auto x = ((i + j + k) % 2
                                  ? INTERSECTION(
                                      b, TRANSFORM(CONVERT_TO<Nef_polyhedron>(
                                                       CUBOID(1, 1, 1)),
                                                   TRANSLATION_3(i, j, k)))
                                  : INTERSECTION(
                                      TRANSFORM(CONVERT_TO<Nef_polyhedron>(
                                                    CUBOID(1, 1, 1)),
                                                TRANSLATION_3(i, j, k)), b));
                        v.push_back(x);

                        a = DIFFERENCE(a, x);
                    }
                }
            }

            return a;
        });

    evaluate_operations();

    BOOST_TEST(result.value->is_empty());
}

/////////////////////
// Sink operations //
/////////////////////

BOOST_AUTO_TEST_CASE(write_off)
{
    {
        auto p = WRITE_OFF(
            "test.off", {CONVERT_TO<Surface_mesh>(UNIT_TETRAHEDRON)});

        BOOST_TEST(
            p->describe()
            == "write_off(\"test.off\",mesh(tetrahedron(1,1,1)))");

        sink_operation(std::move(p));
    }

    evaluate_operations();

    Polyhedron P;
    std::ifstream("test.off") >> P;
    test_unit_tetrahedron(P);

    std::filesystem::remove("test.off");
}

BOOST_AUTO_TEST_CASE(merge_writes)
{
    {
        sink_operation(
            std::move(
                WRITE_OFF(
                    "test.off", {CONVERT_TO<Surface_mesh>(UNIT_TETRAHEDRON)})));

        sink_operation(
            std::move(
                WRITE_OFF(
                    "test.off", {CONVERT_TO<Surface_mesh>(
                            TETRAHEDRON(-1, 1, 1))})));
    }

    evaluate_operations();

    Polyhedron P;
    std::ifstream("test.off") >> P;
    test_polyhedron(P, 8, 24, 8, FT(FT::ET(1, 3)));

    std::filesystem::remove("test.off");
}

BOOST_AUTO_TEST_CASE(write_stl)
{
    {
        auto p = WRITE_STL(
            "test.stl", {CONVERT_TO<Surface_mesh>(UNIT_TETRAHEDRON)});

        BOOST_TEST(
            p->describe()
            == "write_stl(\"test.stl\",mesh(tetrahedron(1,1,1)))");

        sink_operation(std::move(p));
    }

    evaluate_operations();

    std::ifstream f = std::ifstream("test.stl");
    std::string s;

    f >> s;

    BOOST_TEST(f.good());
    BOOST_TEST(s == "solid");

    std::filesystem::remove("test.stl");
}

BOOST_AUTO_TEST_CASE(write_wrl)
{
    {
        auto p = WRITE_WRL(
            "test.wrl", {CONVERT_TO<Surface_mesh>(UNIT_TETRAHEDRON)});

        BOOST_TEST(
            p->describe()
            == "write_wrl(\"test.wrl\",mesh(tetrahedron(1,1,1)))");

        sink_operation(std::move(p));
    }

    evaluate_operations();

    BOOST_TEST(std::ifstream("test.wrl").good());
    std::filesystem::remove("test.wrl");
}

BOOST_AUTO_TEST_SUITE_END()
