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

#include <CGAL/draw_polyhedron.h>
#include <CGAL/draw_nef_3.h>
#include <CGAL/draw_surface_mesh.h>

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
    BOOST_TEST(P.size_of_vertices() == vertices);
    BOOST_TEST(P.size_of_halfedges() == halfedges);
    BOOST_TEST(P.size_of_facets() == facets);

    return P;
}

const Nef_polyhedron &test_polyhedron(
    const Nef_polyhedron &N,
    const int vertices, const int halfedges, const int facets)
{
    BOOST_TEST(N.number_of_vertices() == vertices);
    BOOST_TEST(N.number_of_halfedges() == halfedges);
    BOOST_TEST(N.number_of_facets() == facets);

    return N;
}

const Surface_mesh &test_polyhedron(
    const Surface_mesh &M,
    const int vertices, const int halfedges, const int facets)
{
    BOOST_TEST(M.number_of_vertices() == vertices);
    BOOST_TEST(M.number_of_halfedges() == halfedges);
    BOOST_TEST(M.number_of_faces() == facets);

    return M;
}

#define UNIT_TETRAHEDRON() TETRAHEDRON(1, 1, 1)
#define test_unit_tetrahedron(X) test_polyhedron(X, 4, 12, 4, FT(FT::ET(1, 6)))

BOOST_FIXTURE_TEST_SUITE(polyhedron, Reset_operations)

////////////////
// Primitives //
////////////////

BOOST_DATA_TEST_CASE(tetrahedron,
                     (boost::unit_test::data::make({1, -1})
                      * boost::unit_test::data::make({1, -1})
                      * boost::unit_test::data::make({1, -1})),
                     i, j, k)
{
    auto p = TETRAHEDRON(i, j, k);
    std::stringstream s;

    s << "tetrahedron(" << i << "," << j << "," << k << ")";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_unit_tetrahedron(*p->get_value());
}

BOOST_DATA_TEST_CASE(square_pyramid,
                     (boost::unit_test::data::make({3, -3})),
                     h)
{
    auto p = SQUARE_PYRAMID(2, 2, h);
    std::stringstream s;

    s << "square_pyramid(2,2," << h << ")";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polyhedron(*p->get_value(), 5, 16, 5, 2);
}

BOOST_DATA_TEST_CASE(octahedron,
                     (boost::unit_test::data::make({1, -1})
                      * boost::unit_test::data::make({FT(FT::ET(1, 2)),
                                                      FT(FT::ET(-1, 2))})),
                     h_1, h_2)
{
    auto p = OCTAHEDRON(2, 2, h_1, h_2);
    std::stringstream s;

    s << "octahedron(2,2," << h_1 << "," << h_2.exact() << ")";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polyhedron(
        *p->get_value(), 6, 24, 8, 2 * CGAL::abs(h_1 + h_2) / 3);
}

BOOST_AUTO_TEST_CASE(cuboid)
{
    auto p = CUBOID(2, 3, 4);
    BOOST_TEST(p->describe() == "cuboid(2,3,4)");

    evaluate_unit();

    const Polyhedron &P = *p->get_value();
    test_polyhedron(P, 8, 24, 6, 2 * 3 * 4);
    BOOST_TEST(CGAL::centroid(P.points_begin(), P.points_end())
               == Point_3(CGAL::ORIGIN));
}

BOOST_AUTO_TEST_CASE(icosahedron, * boost::unit_test::tolerance(0.0001))
{
    Tolerances::projection = FT::ET(1, 1'000'000);

    auto p = ICOSAHEDRON(2);
    BOOST_TEST(p->describe() == "icosahedron(2,1/1000000)");

    evaluate_unit();

    test_polyhedron(
        *p->get_value(), 12, 60, 20,
        80 * (1 + sqrt(5) / 3) / std::pow(2 * sqrt(5) + 10, 1.5) * 8);
}

BOOST_AUTO_TEST_CASE(sphere, * boost::unit_test::tolerance(0.0025))
{
    Tolerances::projection = FT::ET(1, 1'000'000);
    Tolerances::curve = FT::ET(1, 500);

    auto p = SPHERE(2);
    BOOST_TEST(p->describe() == "sphere(2,1/500,1/1000000)");

    evaluate_unit();

    test_polyhedron_volume(*p->get_value(), std::acos(-1) * 4 / 3 * 8);
}

BOOST_AUTO_TEST_CASE(cylinder, * boost::unit_test::tolerance(0.0015))
{
    Tolerances::curve = FT::ET(1, 500);

    auto p = CYLINDER(2, 10);

    evaluate_unit();

    test_polyhedron_volume(*p->get_value(), std::acos(-1) * 4 * 10);
}

BOOST_DATA_TEST_CASE(regular_pyramid,
                     (boost::unit_test::data::make({3, -3})),
                     h)
{
    Tolerances::projection = FT::ET(1, 1'000'000);

    auto p = REGULAR_PYRAMID(4, 1, h);
    std::stringstream s;

    s << "regular_pyramid(4,1," << h << ",1/1000000)";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polyhedron(*p->get_value(), 5, 16, 5, 2);
}

BOOST_DATA_TEST_CASE(regular_bipyramid,
                     (boost::unit_test::data::make({1, -1})
                      * boost::unit_test::data::make({FT(FT::ET(1, 2)),
                                                      FT(FT::ET(-1, 2))})),
                     h_1, h_2)
{
    Tolerances::projection = FT::ET(1, 1'000'000);

    auto p = REGULAR_BIPYRAMID(4, 1, h_1, h_2);
    std::stringstream s;

    s << "regular_bipyramid(4,1," << h_1 << "," << h_2.exact() << ",1/1000000)";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polyhedron(
        *p->get_value(), 6, 24, 8, 2 * CGAL::abs(h_1 + h_2) / 3);
}

///////////////////////////
// Conversion operations //
///////////////////////////

#define DEFINE_CONVERSION_TEST_CASE_1(T, NAME)                          \
    BOOST_AUTO_TEST_CASE(to_## T)                                       \
    {                                                                   \
        auto p = CONVERT_TO<T>(UNIT_TETRAHEDRON());                     \
                                                                        \
        BOOST_TEST(p->describe() == #NAME "(tetrahedron(1,1,1))");      \
                                                                        \
        evaluate_unit();                                                \
        test_unit_tetrahedron(*p->get_value());                         \
    }

#define DEFINE_CONVERSION_TEST_CASE_2(T, U)             \
    BOOST_AUTO_TEST_CASE(T ##_to_## U)                  \
    {                                                   \
        auto a = CONVERT_TO<T>(UNIT_TETRAHEDRON());     \
        auto b = CONVERT_TO<U>(a);                      \
                                                        \
        evaluate_unit();                                \
                                                        \
        if constexpr (std::string_view(#T) == #U) {     \
            BOOST_TEST(a == b);                         \
        } else {                                        \
            test_unit_tetrahedron(*b->get_value());     \
        }                                               \
    }

DEFINE_CONVERSION_TEST_CASE_1(Nef_polyhedron, nef)
DEFINE_CONVERSION_TEST_CASE_1(Surface_mesh, mesh)

DEFINE_CONVERSION_TEST_CASE_2(Nef_polyhedron, Polyhedron)
DEFINE_CONVERSION_TEST_CASE_2(Nef_polyhedron, Nef_polyhedron)
DEFINE_CONVERSION_TEST_CASE_2(Nef_polyhedron, Surface_mesh)

DEFINE_CONVERSION_TEST_CASE_2(Surface_mesh, Polyhedron)
DEFINE_CONVERSION_TEST_CASE_2(Surface_mesh, Nef_polyhedron)
DEFINE_CONVERSION_TEST_CASE_2(Surface_mesh, Surface_mesh)

#undef DEFINE_CONVERSION_TEST_CASE_1
#undef DEFINE_CONVERSION_TEST_CASE_2

////////////////////
// Transformation //
////////////////////

BOOST_AUTO_TEST_CASE(rotation, * boost::unit_test::tolerance(0.0001))
{
    const double v[3] = {1, 1, 1};

    auto p = INTERSECTION(
        TRANSFORM(
            UNIT_TETRAHEDRON(),
            basic_rotation(90, 1)
            * basic_rotation(90, 2)),
        TRANSFORM(
            UNIT_TETRAHEDRON(),
            axis_angle_rotation(120, v)));

    evaluate_unit();

    test_polyhedron_volume(*p->get_value(), 1.0 / 6.0);
}

BOOST_AUTO_TEST_CASE(reflection)
{
    auto p = TRANSFORM(UNIT_TETRAHEDRON(), SCALING_3(-1, 1, 1));

    evaluate_unit();

    test_unit_tetrahedron(*p->get_value());
}

BOOST_AUTO_TEST_CASE(nef_reflection)
{
    auto a = CONVERT_TO<Nef_polyhedron>(UNIT_TETRAHEDRON());
    auto b = JOIN(a, TRANSFORM(a, SCALING_3(-1, 1, 1)));

    evaluate_unit();

    test_polyhedron(*b->get_value(), 4, 12, 4, FT::ET(1, 3));
}

BOOST_AUTO_TEST_CASE(mesh_reflection)
{
    auto p = TRANSFORM(CONVERT_TO<Surface_mesh>(UNIT_TETRAHEDRON()),
                       SCALING_3(-1, 1, 1));

    evaluate_unit();

    test_unit_tetrahedron(*p->get_value());
}

BOOST_AUTO_TEST_CASE(polygon_transformation)
{
    auto p = TRANSFORM(RECTANGLE(2, 2),
                       basic_rotation(90, 0));

    BOOST_TEST(p->describe()
               == ("extrusion("
                   "polygon(point(-1,-1),point(1,-1),"
                   "point(1,1),point(-1,1)),"
                   "rotation(1,0,0,0,0,-1,0,1,0))"));

    evaluate_unit();

    test_polyhedron(*p->get_value(), 4, 8, 1);
}

BOOST_DATA_TEST_CASE(flush,
                     (boost::unit_test::data::make({1, 0, 0})
                      ^ boost::unit_test::data::make({0, 1, 0})
                      ^ boost::unit_test::data::make({0, 0, 1})),
                     x, y, z)
{
    auto p = JOIN(FLUSH(CUBOID(2, 2, 2), x, y, z),
                  FLUSH(CUBOID(2, 2, 2), -x, -y, -z));

    std::stringstream s;
    s << ("join(flush("
          "cuboid(2,2,2),0,")
      << x << ",0," << y << ",0," << z
      << ("),flush(cuboid(2,2,2),")
      << -x << ",0," << -y << ",0," << -z<< ",0))";

    BOOST_TEST(p->describe() == s.str());

    evaluate_unit();

    test_polyhedron(*p->get_value(), 12, 60, 20, 16);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(flush_type, T, polyhedron_types)
{
    auto p = JOIN(
        FLUSH(CONVERT_TO<T>(CUBOID(2, 2, 2)), 1, 0, 0),
        FLUSH(CONVERT_TO<T>(CUBOID(2, 2, 2)), -1, 0, 0));

    evaluate_unit();

    if constexpr(std::is_same_v<T, Nef_polyhedron>) {
        test_polyhedron(*p->get_value(), 8, 24, 6, 16);
    } else {
        test_polyhedron(*p->get_value(), 12, 60, 20, 16);
    }
}

///////////////
// Extrusion //
///////////////

BOOST_AUTO_TEST_CASE(extrude)
{
    std::vector<Aff_transformation_3> u({
            TRANSLATION_3(0, 0, 0),
            TRANSLATION_3(0, 0, 4)});

    std::vector<Aff_transformation_3> v({
            TRANSLATION_3(0, 0, 0),
            TRANSLATION_3(0, 0, -4)});

    // Do a couple of extrusions to test concurrent access to the same
    // underlying polygon.

    auto a = EXTRUSION(RECTANGLE(2, 3), std::move(u));
    auto b = EXTRUSION(
        TRANSFORM(RECTANGLE(2, 3), basic_rotation(45)),
        std::move(v));

    BOOST_TEST(a->describe()
               == ("extrusion("
                   "polygon(point(-1,-3/2),point(1,-3/2),"
                   "point(1,3/2),point(-1,3/2)),"
                   "translation(0,0,0),"
                   "translation(0,0,4))"));

    evaluate_unit();

    BOOST_TEST_REQUIRE(CGAL::is_valid_polygon_mesh(*a->get_value()));
    BOOST_TEST_REQUIRE(CGAL::is_closed(*a->get_value()));

    test_polyhedron(*a->get_value(), 8, 36, 12, 24);
    test_polyhedron(*b->get_value(), 8, 36, 12, 24);
}

BOOST_AUTO_TEST_CASE(extrude_circe, * boost::unit_test::tolerance(0.001))
{
    Tolerances::projection = FT::ET(1, 1'000'000);
    Tolerances::curve = FT::ET(1, 1000);

    auto a = DIFFERENCE(CIRCLE(3), RECTANGLE(2, 2));
    auto b = EXTRUSION(a, {TRANSLATION_3(0, 0, 0), TRANSLATION_3(0, 0, 1)});

    BOOST_TEST(b->describe()
               == ("extrusion("
                   "segments(difference(circle(3),circles("
                   "polygon(point(-1,-1),point(1,-1),"
                   "point(1,1),point(-1,1)))),"
                   "1/1000,1/1000000),"
                   "translation(0,0,0),"
                   "translation(0,0,1))"));

    evaluate_unit();

    BOOST_TEST_REQUIRE(CGAL::is_valid_polygon_mesh(*b->get_value()));
    BOOST_TEST_REQUIRE(CGAL::is_closed(*b->get_value()));

    test_polyhedron_volume(*b->get_value(), std::acos(-1) * 9 - 4);
}

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.01))
BOOST_DATA_TEST_CASE(extrude_closed,
                     (boost::unit_test::data::make({1, 5}) *
                      boost::unit_test::data::make({0, 1})),
                     rho, p)
{
    std::vector<Aff_transformation_3> v;
    v.reserve(37);

    for (int i = 0; i < 37; i++) {
        v.push_back(basic_rotation(i * 10, 1) * TRANSLATION_3(rho, 0, 0));
    }

    auto a = (!p ? RECTANGLE(2, 2)
                 : DIFFERENCE(RECTANGLE(2, 2), RECTANGLE(1, 1)));
    auto b = EXTRUSION(a, std::move(v));

    evaluate_unit();

    BOOST_TEST_REQUIRE(CGAL::is_valid_polygon_mesh(*b->get_value()));
    BOOST_TEST_REQUIRE(CGAL::is_closed(*b->get_value()));

    test_polyhedron_volume(*b->get_value(), 2 * std::acos(-1) * rho * (4 - p));
}

BOOST_DATA_TEST_CASE(extrude_many,
                     (boost::unit_test::data::make({1, 2, 11})
                      * boost::unit_test::data::make({0, 1})),
                     n, p)
{
    auto a = (!p ? RECTANGLE(2, 2)
                 : DIFFERENCE(RECTANGLE(2, 2), RECTANGLE(1, 1)));

    std::vector<Aff_transformation_3> v;

    for (int i = 0; i < n; i += 1) {
        v.push_back(TRANSLATION_3(FT::ET(i % 2 > 0 ? -1 : 1), FT::ET(0), FT::ET(i)));
    }

    auto b = EXTRUSION(a, std::move(v));

    BOOST_TEST(v.empty());

    evaluate_unit();

    BOOST_TEST_REQUIRE(CGAL::is_valid_polygon_mesh(*b->get_value()));

    if (n > 1) {
        BOOST_TEST_REQUIRE(CGAL::is_closed(*b->get_value()));
    }

    if (n == 1 && p == 0) {
        test_polyhedron(*b->get_value(), 4, 8, 1);
    } else {
        test_polyhedron(*b->get_value(),
                        4 * n * (p + 1),
                        ((n == 1) * (p + 1) * 4
                         + (1 + (n > 1)) * (p == 0 ? 6 : 24)
                         + 24 * (p + 1) * (n - 1)),
                        (1 + (n > 1)) * (p == 0 ? 2 : 8) + 8 * (p + 1) * (n - 1));
    }

    if (n > 1) {
        test_polyhedron_volume(*b->get_value(), FT((4 - p) * (n - 1)));
    }
}

/////////////////
// Convex hull //
/////////////////

BOOST_AUTO_TEST_CASE(hull)
{
    auto h = std::make_shared<Polyhedron_hull_operation>();
    h->push_back(TETRAHEDRON(FT::ET(1, 2), 1, 1));
    h->push_back(TETRAHEDRON(FT::ET(-1, 2), 1, 1));
    auto a = HULL(h);

    auto g = std::make_shared<Polyhedron_hull_operation>();
    g->push_back(
        TRANSFORM(TETRAHEDRON(FT::ET(-1, 2), 1, 1),
            SCALING_3(-1, 1, 1)));
    g->push_back(TETRAHEDRON(FT::ET(-1, 2), 1, 1));
    auto b = HULL(g);

    auto f = std::make_shared<Polyhedron_hull_operation>();
    f->push_back(TETRAHEDRON(FT::ET(1, 2), 1, 1));
    f->push_back(Point_3(FT(FT::ET(-1, 2)), 0, 0));
    auto c = HULL(f);

    BOOST_TEST(a->describe()
               == "hull(tetrahedron(1/2,1,1),tetrahedron(-1/2,1,1))");

    evaluate_unit();

    test_unit_tetrahedron(*a->get_value());
    test_unit_tetrahedron(*b->get_value());
    test_unit_tetrahedron(*c->get_value());
}

BOOST_AUTO_TEST_CASE(hull_points)
{
    auto h = std::make_shared<Polyhedron_hull_operation>();
    h->push_back(Point_3(CGAL::ORIGIN));
    h->push_back(Point_3(1, 0, 0));
    h->push_back(Point_3(0, 1, 0));
    h->push_back(Point_3(0, 0, 1));
    auto p = HULL(h);

    BOOST_TEST(p->describe()
               == ("hull(point(0,0,0),point(1,0,0),"
                   "point(0,1,0),point(0,0,1))"));

    evaluate_unit();

    test_unit_tetrahedron(*p->get_value());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(hulls, T, polyhedron_types)
{
    auto h = std::make_shared<Polyhedron_hull_operation>();
    h->push_back(TETRAHEDRON(1, 1, 1));
    h->push_back(
        TRANSFORM(CONVERT_TO<T>(TETRAHEDRON(1, 1, 1)), TRANSLATION_3(2, 0, 0)));
    auto p = HULL(h);

    BOOST_TEST(
        p->describe().rfind(
            "hull(tetrahedron(1,1,1),transform(", 0) == 0);

    evaluate_unit();

    test_polyhedron(*p->get_value(), 6, 24, 8, FT::ET(7, 6));
}

///////////////////
// Minkowski sum //
///////////////////

BOOST_AUTO_TEST_CASE(minkowski_sum)
{
    auto p = MINKOWSKI_SUM(CUBOID(10, 10, 10), OCTAHEDRON(2, 2, 1));

    BOOST_TEST(
        p->describe() == ("minkowski_sum(nef(cuboid(10,10,10)),"
                          "nef(octahedron(2,2,1,1)))"));

    evaluate_unit();

    test_polyhedron(*p->get_value(), 24, 96, 26, FT::ET(4984, 3));
}

/////////////////
// Subdivision //
/////////////////

BOOST_AUTO_TEST_CASE(loop)
{
    Tolerances::projection = FT::ET(1, 1'000'000);

    auto p = LOOP(ICOSAHEDRON(1), 2);
    auto q = LOOP(
        TRANSFORM(ICOSAHEDRON(1), basic_rotation(45, 2)), 1);

    BOOST_TEST(p->describe() == "loop(icosahedron(1,1/1000000),2)");

    evaluate_unit();

    test_polyhedron(
        *p->get_value(), 12 + 60 / 2 + 2 * 60, 4 * 4 * 60, 4 * 4 * 20);
    test_polyhedron(
        *q->get_value(), 12 + 60 / 2, 4 * 60, 4 * 20);
}

BOOST_AUTO_TEST_CASE(catmull_clark)
{
    auto p = CATMULL_CLARK(CUBOID(1, 1, 1), 2);

    BOOST_TEST(
        p->describe() == ("catmull_clark(cuboid(1,1,1),2)"));

    evaluate_unit();

    test_polyhedron(*p->get_value(),
                    8 + 24 / 2 + 2 * 24 + 6 + 4 * 6,
                    4 * 4 * 24,
                    4 * 4 * 6);
}

BOOST_AUTO_TEST_CASE(doo_sabin)
{
    auto p = DOO_SABIN(UNIT_TETRAHEDRON(), 1);

    BOOST_TEST(p->describe() == "doo_sabin(tetrahedron(1,1,1),1)");

    evaluate_unit();

    test_polyhedron(
        *p->get_value(), 3 * 4, 2 * 12 + 6 * 4, 4 + 12 / 2 + 4);
}

BOOST_AUTO_TEST_CASE(sqrt_3)
{
    Tolerances::projection = FT::ET(1, 1'000'000);

    auto p = SQRT_3(ICOSAHEDRON(1), 2);

    BOOST_TEST(p->describe() == "sqrt_3(icosahedron(1,1/1000000),2)");

    evaluate_unit();

    test_polyhedron(
        *p->get_value(), 12 + 20 + 60, 3 * 3 * 60, 3 * 60);
}

////////////////////
// Set operations //
////////////////////

BOOST_AUTO_TEST_CASE_TEMPLATE(join, T, polyhedron_types)
{
    auto p = JOIN(
        CONVERT_TO<T>(UNIT_TETRAHEDRON()),
        CONVERT_TO<T>(TETRAHEDRON(-1, 1, 1)));

    if constexpr (std::is_same_v<T, Polyhedron>) {
        BOOST_TEST(p->describe()
                   == "join(tetrahedron(1,1,1),tetrahedron(-1,1,1))");
    }

    evaluate_unit();

    test_polyhedron_volume(*p->get_value(), FT::ET(1, 3));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(difference, T, polyhedron_types)
{
    auto p = DIFFERENCE(
        CONVERT_TO<T>(UNIT_TETRAHEDRON()),
        CONVERT_TO<T>(TETRAHEDRON(FT::ET(1, 2), FT::ET(1, 2), FT::ET(1, 2))));

    if constexpr (std::is_same_v<T, Polyhedron>) {
        BOOST_TEST(
            p->describe()
            == ("difference(tetrahedron(1,1,1),tetrahedron(1/2,1/2,1/2))"));
    }

    evaluate_unit();

    test_polyhedron_volume(*p->get_value(), FT::ET(7, 48));
}

BOOST_AUTO_TEST_CASE_TEMPLATE(intersection, T, polyhedron_types)
{
    auto p = INTERSECTION(
        CONVERT_TO<T>(UNIT_TETRAHEDRON()),
        CONVERT_TO<T>(TRANSFORM(UNIT_TETRAHEDRON(),
                                    TRANSLATION_3(FT::ET(1, 2), 0, 0))));

    if constexpr (std::is_same_v<T, Polyhedron>) {
        BOOST_TEST(p->describe()
                   == ("intersection("
                       "tetrahedron(1,1,1),"
                       "transform("
                       "tetrahedron(1,1,1),"
                       "translation(1/2,0,0)))"));
    }

    evaluate_unit();

    test_polyhedron(*p->get_value(), 4, 12, 4, FT::ET(1, 48));
}

BOOST_AUTO_TEST_CASE(symmetric_difference)
{
    auto p = SYMMETRIC_DIFFERENCE(UNIT_TETRAHEDRON(),
                                  TRANSFORM(UNIT_TETRAHEDRON(),
                                            TRANSLATION_3(FT::ET(1, 2), 0, 0)));

    BOOST_TEST(p->describe()
               == ("symmetric_difference("
                   "nef(tetrahedron(1,1,1)),"
                   "nef(transform("
                   "tetrahedron(1,1,1),"
                   "translation(1/2,0,0))))"));

    evaluate_unit();

    test_polyhedron(
        *p->get_value(), 10, 34, 10, FT(2 * (FT::ET(1, 6) - FT::ET(1, 48))));
}

BOOST_AUTO_TEST_CASE(complement)
{
    auto a = CUBOID(3, 4, 5);
    auto b = CUBOID(1, 2, 3);
    auto c = COMPLEMENT(b);
    auto d = INTERSECTION(a, c);
    auto e = DIFFERENCE(DIFFERENCE(a, b), d);

    BOOST_TEST(c->describe() == ("complement(cuboid(1,2,3))"));

    evaluate_unit();

    BOOST_TEST(e->get_value()->is_empty());
}

BOOST_AUTO_TEST_CASE(boundary)
{
    auto a = BOUNDARY(CUBOID(6, 7, 8));
    auto b = DIFFERENCE(CUBOID(7, 8, 9), CUBOID(5, 6, 7));
    auto c = MINKOWSKI_SUM(a, CUBOID(1, 1, 1));
    auto d = DIFFERENCE(c, b);

    BOOST_TEST(a->describe() == ("boundary(nef(cuboid(6,7,8)))"));

    evaluate_unit();

    BOOST_TEST(d->get_value()->is_empty());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(non_manifold_edge, T, polyhedron_types)
{
    auto p = JOIN(
        CONVERT_TO<T>(TETRAHEDRON(1, 1, 1)),
        CONVERT_TO<T>(TETRAHEDRON(1, -1, -1)));

    evaluate_unit();

    if constexpr(std::is_same_v<T, Nef_polyhedron>) {
        test_polyhedron(*p->get_value(), 6, 22, 8, FT(FT::ET(1, 3)));
    } else {
        bool failed = p->annotations.find("failure") != p->annotations.end();
        BOOST_TEST(failed);
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(non_manifold_vertex, T, polyhedron_types)
{
    auto p = JOIN(
        CONVERT_TO<T>(TETRAHEDRON(1, 1, 1)),
        CONVERT_TO<T>(TETRAHEDRON(-1, -1, -1)));

    evaluate_unit();

    if constexpr(std::is_same_v<T, Nef_polyhedron>) {
        test_polyhedron(*p->get_value(), 7, 24, 8, FT(FT::ET(1, 3)));
    } else {
        test_polyhedron(*p->get_value(), 8, 24, 8, FT(FT::ET(1, 3)));
    }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(clip, T, polyhedron_types)
{
    auto p = CLIP(CONVERT_TO<T>(TETRAHEDRON(2, 2, 2)), Plane_3(0, 0, -1, 1));

    if constexpr (std::is_same_v<T, Polyhedron>) {
        BOOST_TEST(p->describe()
                   == ("clip(tetrahedron(2,2,2),plane(0,0,-1,1))"));
    }

    evaluate_unit();

    test_unit_tetrahedron(*p->get_value());
}

BOOST_AUTO_TEST_CASE_TEMPLATE(clip_non_manifold, T, polyhedron_types)
{
    auto p = CLIP(
        CONVERT_TO<T>(
            DIFFERENCE(CUBOID(4, 4, 4),
                       TRANSFORM(CUBOID(2, 2, 4), TRANSLATION_3(1, 1, 0)))),
        Plane_3(-1, -1, 0, 0));

    evaluate_unit();

    if constexpr (std::is_same_v<T, Nef_polyhedron>) {
        test_polyhedron(*p->get_value(), 10, 34, 10, 16);
    }
}

// A set of spheres of the same or different radius joined together;
// meant to test parallel execution under some load.

BOOST_TEST_DECORATOR(* boost::unit_test::disabled())
BOOST_DATA_TEST_CASE(spheres,
                     boost::unit_test::data::make({false, true}),
                     p)
{
    Tolerances::curve = FT::ET(1, 500);
    std::shared_ptr<Polyhedron_operation<Polyhedron>> v[4];

    for (int i = 0; i < 4; i++) {
        v[i] = TRANSFORM(
            SPHERE(FT(2 + (p ? FT::ET(i, 10) : 0))),
            TRANSLATION_3(FT(i * FT::ET(3, 2)), 0, 0));
    }

    auto a = JOIN(JOIN(v[0], v[1]), JOIN(v[2], v[3]));

    evaluate_unit();

    BOOST_TEST(a->get_value());
}

// Intersections and differences of various small cubes with a larger
// one; meant as stress-test.

BOOST_AUTO_TEST_CASE(cube_tessellation, * boost::unit_test::disabled())
{
    const int n = 5;
    const int m = 2 * n + 1;
    std::vector<std::shared_ptr<Polyhedron_operation<Nef_polyhedron>>> v;

    auto a = CONVERT_TO<Nef_polyhedron>(CUBOID(m, m, m));
    const auto b = a;

    v.reserve(m * m * m);

    for (int i = -n; i <= n ; i++) {
        for (int j = -n; j <= n ; j++) {
            for (int k = -n; k <= n ; k++) {
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

    evaluate_unit();

    BOOST_TEST(a->get_value()->is_empty());

    for (const auto &x: v) {
        test_polyhedron(*x->get_value(), 8, 24, 6, FT(1));
    }
}

/////////////////////
// Sink operations //
/////////////////////

BOOST_AUTO_TEST_CASE(write_off)
{
    auto p = WRITE_OFF("test.off",
                       {CONVERT_TO<Surface_mesh>(UNIT_TETRAHEDRON())});

    BOOST_TEST(p->describe()
               == "write_off(\"test.off\",mesh(tetrahedron(1,1,1)))");

    evaluate_unit();

    Polyhedron P;
    std::ifstream("test.off") >> P;
    test_unit_tetrahedron(P);

    std::filesystem::remove("test.off");
}

BOOST_AUTO_TEST_CASE(write_stl)
{
    auto p = WRITE_STL("test.stl",
                       {CONVERT_TO<Surface_mesh>(UNIT_TETRAHEDRON())});

    BOOST_TEST(p->describe()
               == "write_stl(\"test.stl\",mesh(tetrahedron(1,1,1)))");

    evaluate_unit();

    std::ifstream f = std::ifstream("test.stl");
    std::string s;

    f >> s;

    BOOST_TEST(f.good());
    BOOST_TEST(s == "solid");

    std::filesystem::remove("test.stl");
}

BOOST_AUTO_TEST_CASE(write_wrl)
{
    auto p = WRITE_WRL("test.wrl",
                       {CONVERT_TO<Surface_mesh>(UNIT_TETRAHEDRON())});

    BOOST_TEST(p->describe()
               == "write_wrl(\"test.wrl\",mesh(tetrahedron(1,1,1)))");

    evaluate_unit();

    BOOST_TEST(std::ifstream("test.wrl").good());

    std::filesystem::remove("test.wrl");
}

BOOST_AUTO_TEST_SUITE_END()
