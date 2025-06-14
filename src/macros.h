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

#ifndef MACROS_H
#define MACROS_H

#include "basic_operations.h"
#include "transformation_types.h"
#include "selection.h"
#include "polygon_operations.h"
#include "circle_polygon_operations.h"
#include "conic_polygon_operations.h"
#include "polyhedron_operations.h"
#include "extrusion_operation.h"
#include "sink_operations.h"
#include "mesh_operations.h"
#include "deform_operations.h"
#include "evaluation.h"

template<typename T, typename U = Operation, typename... Args>
static std::shared_ptr<U> make_and_map(Args &&... args) {
    std::shared_ptr<Operation> p = map_operation(
        make_operation<T>(std::forward<Args>(args)...));

    if constexpr (std::is_same_v<U, Operation>) {
        return p;
    }

    return std::static_pointer_cast<U>(p);
}

/////////////////////
// Transformations //
/////////////////////

inline Aff_transformation_2 SCALING_2(const FT &x, const FT &y) {
    return Aff_transformation_2(x, 0, 0, y);
}

inline Aff_transformation_3 SCALING_3(
    const FT &x, const FT &y, const FT &z) {
    return Aff_transformation_3(x, 0, 0, 0, y, 0, 0, 0, z);
}

inline Aff_transformation_2 TRANSLATION_2(const FT &x, const FT &y) {
    return Aff_transformation_2(CGAL::TRANSLATION, Vector_2(x, y));
}

inline Aff_transformation_3 TRANSLATION_3(
    const FT &x, const FT &y, const FT &z) {
    return Aff_transformation_3(CGAL::TRANSLATION, Vector_3(x, y, z));
}

////////////////
// Selections //
////////////////

template<typename R = Face_selector>
inline std::shared_ptr<R> RELATIVE_SELECTION(
    const std::shared_ptr<Face_selector> &p, int n)
{
    return std::make_shared<Relative_face_selector>(p, n);
}

template<typename R = Vertex_selector>
inline std::shared_ptr<R> RELATIVE_SELECTION(
    const std::shared_ptr<Vertex_selector> &p, int n)
{
    return std::make_shared<Relative_vertex_selector>(p, n);
}

template<typename R = Edge_selector>
inline std::shared_ptr<R> RELATIVE_SELECTION(
    const std::shared_ptr<Edge_selector> &p, int n)
{
    return std::make_shared<Relative_edge_selector>(p, n);
}

template<typename R = Face_selector, typename T>
inline std::shared_ptr<R> FACES_IN(const std::shared_ptr<T> &p)
{
    if constexpr (std::is_same_v<T, Bounding_volume>) {
        return std::make_shared<Bounded_face_selector>(p, false);
    } else if constexpr (std::is_same_v<T, Vertex_selector>) {
        return std::make_shared<Vertex_to_face_selector>(p, false);
    } else {
        static_assert(std::is_same_v<T, Edge_selector>);
        return std::make_shared<Edge_to_face_selector>(p, false);
    }
}

template<typename R = Face_selector, typename T>
inline std::shared_ptr<R> FACES_PARTIALLY_IN(const std::shared_ptr<T> &p)
{
    if constexpr (std::is_same_v<T, Bounding_volume>) {
        return std::make_shared<Bounded_face_selector>(p, true);
    } else if constexpr (std::is_same_v<T, Vertex_selector>) {
        return std::make_shared<Vertex_to_face_selector>(p, true);
    } else {
        static_assert(std::is_same_v<T, Edge_selector>);
        return std::make_shared<Edge_to_face_selector>(p, true);
    }
}

template<typename R = Vertex_selector, typename T>
inline std::shared_ptr<R> VERTICES_IN(const std::shared_ptr<T> &p)
{
    if constexpr (std::is_same_v<T, Bounding_volume>) {
        return std::make_shared<Bounded_vertex_selector>(p);
    } else if constexpr (std::is_same_v<T, Face_selector>) {
        return std::make_shared<Face_to_vertex_selector>(p);
    } else {
        static_assert(std::is_same_v<T, Edge_selector>);
        return std::make_shared<Edge_to_vertex_selector>(p);
    }
}

template<typename R = Edge_selector, typename T>
inline std::shared_ptr<R> EDGES_IN(const std::shared_ptr<T> &p)
{
    if constexpr (std::is_same_v<T, Bounding_volume>) {
        return std::make_shared<Bounded_edge_selector>(p, false);
    } else if constexpr (std::is_same_v<T, Vertex_selector>) {
        return std::make_shared<Vertex_to_edge_selector>(p, false);
    } else {
        static_assert(std::is_same_v<T, Face_selector>);
        return std::make_shared<Face_to_edge_selector>(p, false);
    }
}

template<typename R = Edge_selector, typename T>
inline std::shared_ptr<R> EDGES_PARTIALLY_IN(const std::shared_ptr<T> &p)
{
    if constexpr (std::is_same_v<T, Bounding_volume>) {
        return std::make_shared<Bounded_edge_selector>(p, true);
    } else if constexpr (std::is_same_v<T, Vertex_selector>) {
        return std::make_shared<Vertex_to_edge_selector>(p, true);
    } else {
        static_assert(std::is_same_v<T, Face_selector>);
        return std::make_shared<Face_to_edge_selector>(p, true);
    }
}

#define DEFINE_SELECTOR_SET_OPERATIONS(WHAT, WHICH)                     \
template<typename R = WHAT ##_selector>                                 \
inline std::shared_ptr<R> JOIN(                                         \
    std::vector<std::shared_ptr<WHAT ##_selector>> &&v)                 \
{                                                                       \
    return std::make_shared<Set_union_## WHICH ##_selector>(            \
        std::move(v));                                                  \
}                                                                       \
                                                                        \
template<typename R = WHAT ##_selector>                                 \
inline std::shared_ptr<R> INTERSECTION(                                 \
    std::vector<std::shared_ptr<WHAT ##_selector>> &&v)                 \
{                                                                       \
    return std::make_shared<Set_intersection_## WHICH ##_selector>(     \
        std::move(v));                                                  \
}                                                                       \
                                                                        \
template<typename R = WHAT ##_selector>                                 \
inline std::shared_ptr<R> DIFFERENCE(                                   \
    std::vector<std::shared_ptr<WHAT ##_selector>> &&v)                 \
{                                                                       \
    return std::make_shared<Set_difference_## WHICH ##_selector>(       \
        std::move(v));                                                  \
}                                                                       \
                                                                        \
template<typename R = WHAT ##_selector>                                 \
inline std::shared_ptr<R> COMPLEMENT(                                   \
    const std::shared_ptr<WHAT ##_selector> &p)                         \
{                                                                       \
    return std::make_shared<Set_complement_## WHICH ##_selector>(p);    \
}

DEFINE_SELECTOR_SET_OPERATIONS(Face, face)
DEFINE_SELECTOR_SET_OPERATIONS(Vertex, vertex)
DEFINE_SELECTOR_SET_OPERATIONS(Edge, edge)

#undef DEFINE_SELECTOR_SET_OPERATIONS

//////////////////////
// Bounding volumes //
//////////////////////

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_PLANE(
    const FT &a, const FT &b, const FT &c, const FT &d)
{
    return std::make_shared<Bounding_halfspace>(
        Plane_3(a, b, c, d), Bounding_volume::BOUNDARY);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_HALFSPACE(
    const FT &a, const FT &b, const FT &c, const FT &d)
{
    return std::make_shared<Bounding_halfspace>(
        Plane_3(a, b, c, d), Bounding_volume::CLOSED);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_HALFSPACE_INTERIOR(
    const FT &a, const FT &b, const FT &c, const FT &d)
{
    return std::make_shared<Bounding_halfspace>(
        Plane_3(a, b, c, d), Bounding_volume::OPEN);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_BOX(const FT &a, const FT &b, const FT &c)
{
    return std::make_shared<Bounding_box>(a, b, c, Bounding_volume::CLOSED);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_BOX_BOUNDARY(
    const FT &a, const FT &b, const FT &c)
{
    return std::make_shared<Bounding_box>(a, b, c, Bounding_volume::BOUNDARY);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_BOX_INTERIOR(
    const FT &a, const FT &b, const FT &c)
{
    return std::make_shared<Bounding_box>(a, b, c, Bounding_volume::OPEN);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_SPHERE(const FT &r)
{
    return std::make_shared<Bounding_sphere>(r, Bounding_volume::CLOSED);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_SPHERE_INTERIOR(const FT &r)
{
    return std::make_shared<Bounding_sphere>(r, Bounding_volume::OPEN);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_SPHERE_BOUNDARY(const FT &r)
{
    return std::make_shared<Bounding_sphere>(r, Bounding_volume::BOUNDARY);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_CYLINDER(const FT &r, const FT &h)
{
    return std::make_shared<Bounding_cylinder>(r, h, Bounding_volume::CLOSED);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_CYLINDER_INTERIOR(const FT &r, const FT &h)
{
    return std::make_shared<Bounding_cylinder>(r, h, Bounding_volume::OPEN);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> BOUNDING_CYLINDER_BOUNDARY(const FT &r, const FT &h)
{
    return std::make_shared<Bounding_cylinder>(r, h, Bounding_volume::BOUNDARY);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> COMPLEMENT(
    const std::shared_ptr<Bounding_volume> &p)
{
    return std::make_shared<Bounding_volume_complement>(p);
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> JOIN(
    std::vector<std::shared_ptr<Bounding_volume>> &&v)
{
    return std::make_shared<Bounding_volume_union>(std::move(v));
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> INTERSECTION(
    std::vector<std::shared_ptr<Bounding_volume>> &&v)
{
    return std::make_shared<Bounding_volume_intersection>(std::move(v));
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> DIFFERENCE(
    std::vector<std::shared_ptr<Bounding_volume>> &&v)
{
    return std::make_shared<Bounding_volume_difference>(std::move(v));
}

template<typename R = Bounding_volume>
inline std::shared_ptr<R> TRANSFORM(
    const std::shared_ptr<Bounding_volume> &p,
    const Aff_transformation_3 &X)
{
    return p->transform(X);
}

////////////////
// Operations //
////////////////

template<typename R = Polygon_operation<Polygon_set>>
inline std::shared_ptr<R> POLYGON(std::vector<Point_2> &&v)
{
    return make_and_map<Simple_polygon_operation, R>(std::move(v));
}

template<typename R = Polygon_operation<Polygon_set>>
inline std::shared_ptr<R> REGULAR_POLYGON(const int n, const FT &r)
{
    return make_and_map<Regular_polygon_operation, R>(n, r);
}

template<typename R = Polygon_operation<Polygon_set>>
inline std::shared_ptr<R> ISOSCELES_TRIANGLE(const FT &a, const FT &b)
{
    FT a_2 = a / 2;
    std::vector<Point_2> v{
        Point_2(-a_2, 0),
        Point_2(a_2, 0),
        Point_2(0, b)};

    return POLYGON<R>(std::move(v));
}

template<typename R = Polygon_operation<Polygon_set>>
inline std::shared_ptr<R> RIGHT_TRIANGLE(const FT &a, const FT &b)
{
    std::vector<Point_2> v{
        Point_2(0, 0),
        Point_2(a, 0),
        Point_2(0, b)};

    if (a * b < 0) {
        std::swap(v[1], v[2]);
    }

    return POLYGON<R>(std::move(v));
}

template<typename R = Polygon_operation<Polygon_set>>
inline std::shared_ptr<R> RECTANGLE(const FT &a, const FT &b)
{
    FT w_2 = a / 2, h_2 = b / 2;
    std::vector<Point_2> v{
        Point_2(-w_2, -h_2),
        Point_2(w_2, -h_2),
        Point_2(w_2, h_2),
        Point_2(-w_2, h_2)};

    return POLYGON<R>(std::move(v));
}

template<typename R = Polygon_operation<Circle_polygon_set>>
inline std::shared_ptr<R> CIRCLE(const FT &r)
{
    return make_and_map<Circle_operation, R>(r);
}

template<typename R = Polygon_operation<Circle_polygon_set>>
inline std::shared_ptr<R> CIRCULAR_SEGMENT(const FT &c, const FT &h)
{
    return make_and_map<Circular_segment_operation, R>(c, h);
}

template<typename R = Polygon_operation<Circle_polygon_set>>
inline std::shared_ptr<R> CIRCULAR_SECTOR(const FT &r, const FT &a)
{
    return make_and_map<Circular_sector_operation, R>(r, a);
}

template<typename T, typename R = Polygon_operation<T>, typename U>
inline std::shared_ptr<R> CONVERT_TO(
    const std::shared_ptr<Polygon_operation<U>> &p)
{
    if constexpr (std::is_same_v<T, U>) {
        return p;
    } else {
        return make_and_map<Polygon_convert_operation<T, U>, R>(p);
    }
}

// Transformations that scale non-uniformly, turn circles into
// ellipses.  Circles must first be converted into conics, if they're
// to be thus transformed.  As a result, we must return a pointer to
// `Operation` when transforming circle polygons, the exact type to be
// determined at runtime, via dynamic casting.

template<typename T, typename R = std::conditional_t<
                         std::is_same_v<T, Circle_polygon_set>,
                         Operation, Polygon_operation<T>>>
inline std::shared_ptr<R> TRANSFORM(
    const std::shared_ptr<Polygon_operation<T>> &p,
    Aff_transformation_2 X)
{
    if constexpr (std::is_same_v<T, Circle_polygon_set>) {
        const Vector_2 u = X.transform(Vector_2(1, 0)),
            v = X.transform(Vector_2(0, 1));

        // To determine if we need to convert to a conic polygon, we
        // simply test whether the transformed basis vectors are still
        // orthogonal and of equal lengths.

        if (u * v != 0 || CGAL::squared_length(u) != CGAL::squared_length(v)) {
            return TRANSFORM<Conic_polygon_set, R>(
                CONVERT_TO<Conic_polygon_set>(p), X);
        }
    }

    return make_and_map<Polygon_transform_operation<T>, R>(p, X);
}

template<typename T, typename R = Polygon_operation<Polygon_set>>
inline std::shared_ptr<R> FLUSH(
    const std::shared_ptr<Polygon_operation<T>> &p,
    const FT &lambda, const FT &mu)
{
    return make_and_map<Polygon_flush_operation, R>(
        CONVERT_TO<Polygon_set>(p), lambda, mu);
}

template<typename R = Polygon_operation<Conic_polygon_set>>
inline std::shared_ptr<R> ELLIPSE(const FT &a, const FT &b)
{
    std::shared_ptr<Polygon_operation<Conic_polygon_set>> p =
        TRANSFORM(CONVERT_TO<Conic_polygon_set>(CIRCLE(1)), SCALING_2(a, b));

    if constexpr (std::is_same_v<R, decltype(p)>) {
        return p;
    } else {
        return std::static_pointer_cast<R>(p);
    }
}

template<typename R = Polygon_operation<Conic_polygon_set>>
inline std::shared_ptr<R> ELLIPTIC_SECTOR(const FT &a, const FT &b, const FT &c)
{
    std::shared_ptr<Polygon_operation<Conic_polygon_set>> p =
        TRANSFORM(CONVERT_TO<Conic_polygon_set>(CIRCULAR_SECTOR(1, c)),
                  SCALING_2(a, b));

    if constexpr (std::is_same_v<R, decltype(p)>) {
        return p;
    } else {
        return std::static_pointer_cast<R>(p);
    }
}

template<typename R = Polygon_hull_operation>
inline std::shared_ptr<R> POLYGON_HULL_OPEN()
{
    return make_operation<Polygon_hull_operation, R>();
}

template<typename R = Polygon_operation<Polygon_set>>
inline std::shared_ptr<R> POLYGON_HULL_CLOSE(
    std::shared_ptr<Polygon_hull_operation> &p)
{
    return std::static_pointer_cast<R>(map_operation(p));
}

template<typename T, typename R = Polygon_operation<Polygon_set>,
         typename U>
inline auto MINKOWSKI_SUM(
    const std::shared_ptr<Polygon_operation<T>> &p,
    const std::shared_ptr<Polygon_operation<U>> &q)
{
    return make_and_map<Polygon_minkowski_sum_operation, R>(
        CONVERT_TO<Polygon_set>(p),
        CONVERT_TO<Polygon_set>(q));
}

template<typename T, typename R = Polygon_operation<Polygon_set>>
inline std::shared_ptr<R> OFFSET(
    const std::shared_ptr<Polygon_operation<T>> &p, const FT &delta)
{
    return make_and_map<Polygon_offset_operation, R>(
        CONVERT_TO<Polygon_set>(p), delta);
}

template<typename T, typename U>
using Polygon_set_operation_result =
    Polygon_operation<typename std::conditional_t<
                          std::is_same_v<T, Polygon_set>
                          || std::is_same_v<T, U>
                          || std::is_abstract_v<
                              Polygon_convert_operation<T, U>>,
                          U, T>>;

#define DEFINE_POLYGON_SET_OPERATION(NAME, WHAT)                        \
template<typename T, typename R = Polygon_operation<T>>                 \
inline std::shared_ptr<R> NAME(                                         \
    const std::shared_ptr<Polygon_operation<T>> &p,                     \
    const std::shared_ptr<Polygon_operation<T>> &q)                     \
{                                                                       \
    return make_and_map<Polygon_## WHAT ##_operation<T>, R>(p, q);      \
}                                                                       \
                                                                        \
template<typename T, typename U,                                        \
         typename R = Polygon_set_operation_result<T, U>,               \
         typename = std::enable_if_t<!std::is_same_v<T, U>>>            \
inline std::shared_ptr<R> NAME(                                         \
    const std::shared_ptr<Polygon_operation<T>> &p,                     \
    const std::shared_ptr<Polygon_operation<U>> &q)                     \
{                                                                       \
    if constexpr (                                                      \
        std::is_same_v<T, Polygon_set>                                  \
        || std::is_abstract_v<Polygon_convert_operation<T, U>>) {       \
        return make_and_map<Polygon_## WHAT ##_operation<U>, R>(        \
            CONVERT_TO<U>(p), q);                                       \
    } else {                                                            \
        return make_and_map<Polygon_## WHAT ##_operation<T>, R>(        \
            p, CONVERT_TO<T>(q));                                       \
    }                                                                   \
}

DEFINE_POLYGON_SET_OPERATION(JOIN, join)
DEFINE_POLYGON_SET_OPERATION(DIFFERENCE, difference)
DEFINE_POLYGON_SET_OPERATION(INTERSECTION, intersection)
DEFINE_POLYGON_SET_OPERATION(SYMMETRIC_DIFFERENCE, symmetric_difference)

#undef DEFINE_POLYGON_SET_OPERATION

template<typename T, typename R = Polygon_operation<T>>
inline std::shared_ptr<R> COMPLEMENT(
    const std::shared_ptr<Polygon_operation<T>> &p)
{
    return make_and_map<Polygon_complement_operation<T>, R>(p);
}

template<typename R = Polyhedron_operation<Polyhedron>, typename T>
inline std::shared_ptr<R> EXTRUSION(
    const std::shared_ptr<Polygon_operation<T>> &p,
    std::vector<Aff_transformation_3> &&v)
{
    return make_and_map<Extrusion_operation, R>(
        CONVERT_TO<Polygon_set>(p),
        v.size() > 0
        ? std::move(v)
        : std::vector<Aff_transformation_3>({
                TRANSLATION_3(0, 0, 0)}));
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> TETRAHEDRON(const FT &a, const FT &b, const FT &c)
{
    return make_and_map<Tetrahedron_operation, R>(a, b, c);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> SQUARE_PYRAMID(const FT &a, const FT &b, const FT &c)
{
    return make_and_map<Square_pyramid_operation, R>(a, b, c);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> OCTAHEDRON(const FT &a, const FT &b, const FT &c)
{
    return make_and_map<Octahedron_operation, R>(a, b, c);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> OCTAHEDRON(
    const FT &a, const FT &b, const FT &c, const FT &d)
{
    return make_and_map<Octahedron_operation, R>(a, b, c, d);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> CUBOID(const FT &a, const FT &b, const FT &c)
{
    return make_and_map<Cuboid_operation, R>(a, b, c);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> ICOSAHEDRON(const FT &r)
{
    return make_and_map<Icosahedron_operation, R>(r);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> SPHERE(const FT &r)
{
    return make_and_map<Sphere_operation, R>(r);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> REGULAR_PYRAMID(const int n, const FT &r, const FT &h)
{
    return make_and_map<Regular_pyramid_operation, R>(n, r, h);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> REGULAR_BIPYRAMID(
    const int n, const FT &r, const FT &h)
{
    return make_and_map<Regular_bipyramid_operation, R>(n, r, h);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> REGULAR_BIPYRAMID(
    const int n, const FT &r, const FT &h_1, const FT &h_2)
{
    return make_and_map<
        Regular_bipyramid_operation, R>(n, r, h_1, h_2);
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> PRISM(const int n, const FT &r, const FT &h)
{
    if (h == 0) {
        return EXTRUSION<R>(REGULAR_POLYGON(n, r),
                            std::vector<Aff_transformation_3>({
                                    TRANSLATION_3(0, 0, 0)}));
    } else {
        return EXTRUSION<R>(REGULAR_POLYGON(n, r),
                            std::vector<Aff_transformation_3>({
                                    TRANSLATION_3(0, 0, h / -2),
                                    TRANSLATION_3(0, 0, h / 2)}));
    }
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> CYLINDER(const FT &r, const FT &h)
{
    const int n = std::ceil(
        std::acos(-1) / std::acos(1 - CGAL::to_double(Tolerances::curve / r)));

    if (h == 0) {
        return EXTRUSION<R>(REGULAR_POLYGON(n, r),
                            std::vector<Aff_transformation_3>({
                                    TRANSLATION_3(0, 0, 0)}));
    } else {
        return EXTRUSION<R>(REGULAR_POLYGON(n, r),
                            std::vector<Aff_transformation_3>({
                                    TRANSLATION_3(0, 0, h / -2),
                                    TRANSLATION_3(0, 0, h / 2)}));
    }
}

template<typename T, typename R = Polyhedron_operation<T>>
inline std::shared_ptr<R> TRANSFORM(
    const std::shared_ptr<Polyhedron_operation<T>> &p,
    const Aff_transformation_3 &X)
{
    return make_and_map<Polyhedron_transform_operation<T>, R>(p, X);
}

template<typename T, typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> TRANSFORM(
    const std::shared_ptr<Polygon_operation<T>> &p,
    const Aff_transformation_3 &X)
{
    return EXTRUSION(p, {X});
}

template<typename T, typename R = Polyhedron_operation<T>>
inline std::shared_ptr<R> FLUSH(
    const std::shared_ptr<Polyhedron_operation<T>> &p,
    const FT &lambda, const FT &mu, const FT &nu)
{
    return make_and_map<Polyhedron_flush_operation<T>, R>(p, lambda, mu, nu);
}

template<typename R = Polyhedron_hull_operation>
inline std::shared_ptr<R> POLYHEDRON_HULL_OPEN()
{
    return make_operation<Polyhedron_hull_operation, R>();
}

template<typename R = Polyhedron_operation<Polyhedron>>
inline std::shared_ptr<R> POLYHEDRON_HULL_CLOSE(
    std::shared_ptr<Polyhedron_hull_operation> &p)
{
    return std::static_pointer_cast<R>(map_operation(p));
}

template<typename T, typename R = Polyhedron_operation<T>>
inline std::shared_ptr<R> CLIP(
    const std::shared_ptr<Polyhedron_operation<T>> &p, const Plane_3 &Pi)
{
    return make_and_map<Polyhedron_clip_operation<T>, R>(p, Pi);
}

template<typename T, typename R = Polyhedron_operation<T>, typename U>
inline std::shared_ptr<R> CONVERT_TO(
    const std::shared_ptr<Polyhedron_operation<U>> &p)
{
    if constexpr (std::is_same_v<T, U>) {
        return p;
    } else {
        return make_and_map<Polyhedron_convert_operation<T, U>, R>(p);
    }
}

template<typename T, typename R = Polyhedron_operation<Nef_polyhedron>,
         typename U>
inline auto MINKOWSKI_SUM(
    const std::shared_ptr<Polyhedron_operation<T>> &p,
    const std::shared_ptr<Polyhedron_operation<U>> &q)
{
    return make_and_map<Polyhedron_minkowski_sum_operation, R>(
        CONVERT_TO<Nef_polyhedron>(p),
        CONVERT_TO<Nef_polyhedron>(q));
}

// Boolean set operations.  In case of mixed types, convert towards
// first operand.

#define DEFINE_POLYHEDRON_SET_OPERATION(NAME, WHAT)                     \
template<typename T, typename U, typename R = Polyhedron_operation<T>>  \
inline auto NAME(                                                       \
    const std::shared_ptr<Polyhedron_operation<T>> &p,                  \
    const std::shared_ptr<Polyhedron_operation<U>> &q)                  \
{                                                                       \
    return make_and_map<Polyhedron_## WHAT ##_operation<T>, R>(         \
        p, CONVERT_TO<T>(q));                                           \
}

DEFINE_POLYHEDRON_SET_OPERATION(JOIN, join)
DEFINE_POLYHEDRON_SET_OPERATION(DIFFERENCE, difference)
DEFINE_POLYHEDRON_SET_OPERATION(INTERSECTION, intersection)

template<typename T, typename R = Polyhedron_operation<Nef_polyhedron>,
         typename U>
inline auto SYMMETRIC_DIFFERENCE(
    const std::shared_ptr<Polyhedron_operation<T>> &p,
    const std::shared_ptr<Polyhedron_operation<U>> &q)
{
    return make_and_map<
        Polyhedron_symmetric_difference_operation, R>(
        CONVERT_TO<Nef_polyhedron>(p),
        CONVERT_TO<Nef_polyhedron>(q));
}

#undef DEFINE_POLYHEDRON_SET_OPERATION

template<typename T, typename R = Polyhedron_operation<T>>
inline std::shared_ptr<R> COMPLEMENT(
    const std::shared_ptr<Polyhedron_operation<T>> &p)
{
    return make_and_map<Polyhedron_complement_operation<T>, R>(p);
}

template<typename T, typename R = Polyhedron_operation<Nef_polyhedron>>
inline std::shared_ptr<R> BOUNDARY(
    const std::shared_ptr<Polyhedron_operation<T>> &p)
{
    return make_and_map<Polyhedron_boundary_operation, R>(
        CONVERT_TO<Nef_polyhedron>(p));
}

// Operations that work on Polyhedron and Surface_mesh but not on
// Nef_polyhedron.

#define DEFINE_MESH_OPERATION(NAME, ...)                                \
template<typename T, typename R = Polyhedron_operation<                 \
                         std::conditional_t<std::is_same_v<T, Nef_polyhedron>, \
                                            Polyhedron, T>>>            \
inline std::shared_ptr<R> NAME(                                         \
    const std::shared_ptr<Polyhedron_operation<T>> &p, __VA_ARGS__)

#define FOR(OP, ...)                                                    \
{                                                                       \
    if constexpr (std::is_same_v<T, Nef_polyhedron>) {                  \
        return make_and_map<OP<Polyhedron>, R>(                         \
            CONVERT_TO<Polyhedron>(p), __VA_ARGS__);                    \
    } else {                                                            \
        return make_and_map<OP<T>, R>(p, __VA_ARGS__);                  \
    }                                                                   \
}

// Subdivision

#define DEFINE_SUBDIVISION_OPERATION(NAME, WHAT)                        \
DEFINE_MESH_OPERATION(NAME, const unsigned int n)                       \
FOR(WHAT ##_subdivision_operation, n)

DEFINE_SUBDIVISION_OPERATION(LOOP, Loop)
DEFINE_SUBDIVISION_OPERATION(CATMULL_CLARK, Catmull_clark)
DEFINE_SUBDIVISION_OPERATION(DOO_SABIN, Doo_sabin)
DEFINE_SUBDIVISION_OPERATION(SQRT_3, Sqrt_3)

#undef DEFINE_SUBDIVISION_OPERATION

// Remesh

DEFINE_MESH_OPERATION(
    REMESH,
    const std::shared_ptr<Face_selector> &q,
    const std::shared_ptr<Edge_selector> &r,
    const FT &l, const int n)
FOR(Remesh_operation, q, r, l, n)

template<typename T, typename U,
         typename R = Polyhedron_operation<Surface_mesh>>
inline std::shared_ptr<R> COLOR_SELECTION(
    const std::shared_ptr<Polyhedron_operation<T>> &p,
    const std::shared_ptr<U> &q,
    const FT r, const FT g, const FT b, const FT a)
{
    return make_and_map<Color_selection_operation<U>, R>(
        CONVERT_TO<Surface_mesh>(p), q, r, g, b, a);
}

// Perturb

DEFINE_MESH_OPERATION(
    PERTURB, const std::shared_ptr<Vertex_selector> &q, const FT &m)
FOR(Perturb_operation, q, m)

// Refine

DEFINE_MESH_OPERATION(
    REFINE, const std::shared_ptr<Face_selector> &q, const FT &rho)
FOR(Refine_operation, q, rho)

// Fair

DEFINE_MESH_OPERATION(
    FAIR, const std::shared_ptr<Vertex_selector> &q, const int n)
FOR(Fair_operation, q, n)

// Smooth shape

DEFINE_MESH_OPERATION(
    SMOOTH_SHAPE,
    const std::shared_ptr<Face_selector> &q,
    const std::shared_ptr<Vertex_selector> &r,
    const FT &t, const int n)
FOR(Smooth_shape_operation, q, r, t, n)

// Deform

DEFINE_MESH_OPERATION(
    DEFORM,
    const std::shared_ptr<Vertex_selector> &q,
    std::vector<std::pair<std::shared_ptr<Vertex_selector>,
                          Aff_transformation_3>> &&v,
    const FT &tau, const unsigned int n)
FOR(Deform_operation, q, std::move(v), tau, n)

DEFINE_MESH_OPERATION(
    DEFORM,
    std::vector<std::pair<std::shared_ptr<Vertex_selector>,
                          Aff_transformation_3>> &&v,
    const FT &tau, const unsigned int n)
FOR(Deform_operation, std::move(v), tau, n)

// Deflate

DEFINE_MESH_OPERATION(
    DEFLATE,
    const std::shared_ptr<Vertex_selector> &q,
    const int n, const FT w_H, const FT w_M)
FOR(Deflate_operation, q, n, w_H, w_M)

DEFINE_MESH_OPERATION(
    DEFLATE,
    const int n, const FT w_H, const FT w_M)
FOR(Deflate_operation, n, w_H, w_M)

#undef DEFINE_MESH_OPERATION

template<typename T, typename U,
         typename R = Polyhedron_operation<
             std::conditional_t<!std::is_same_v<T, Nef_polyhedron>,
                                T, std::conditional_t<!std::is_same_v<
                                                          U, Nef_polyhedron>,
                                                      U, Polyhedron>>>>
inline auto COREFINE(
    const std::shared_ptr<Polyhedron_operation<T>> &p,
    const std::shared_ptr<Polyhedron_operation<U>> &q)
{
    if constexpr (!std::is_same_v<T, Nef_polyhedron>) {
        return make_and_map<Corefine_operation<T>, R>(
            p, CONVERT_TO<T>(q));
    } else if constexpr (!std::is_same_v<U, Nef_polyhedron>) {
        return make_and_map<Corefine_operation<U>, R>(
            CONVERT_TO<U>(p), q);
    } else {
        return make_and_map<Corefine_operation<Polyhedron>, R>(
            CONVERT_TO<Polyhedron>(p), CONVERT_TO<Polyhedron>(q));
    }
}

template<typename T, typename R = Polyhedron_operation<
                         std::conditional_t<!std::is_same_v<T, Nef_polyhedron>,
                                            T, Polyhedron>>>
inline auto COREFINE(
    const std::shared_ptr<Polyhedron_operation<T>> &p, const Plane_3 &pi)
{
    if constexpr (!std::is_same_v<T, Nef_polyhedron>) {
        return make_and_map<
            Corefine_with_plane_operation<T>, R>(p, pi);
    } else {
        return make_and_map<
            Corefine_with_plane_operation<Polyhedron>, R>(
            CONVERT_TO<Polyhedron>(p), pi);
    }
}

#define DEFINE_WRITE_OPERATION(WHAT)                                    \
template<typename R = Operation>                                        \
inline std::shared_ptr<R> WRITE_## WHAT(                                \
    const char *filename,                                               \
    std::vector<std::shared_ptr<Polyhedron_operation<Surface_mesh>>> &&v) \
{                                                                       \
    return make_and_map<Write_## WHAT ##_operation, R>(                 \
        filename, std::move(v));                                        \
}

DEFINE_WRITE_OPERATION(OFF)
DEFINE_WRITE_OPERATION(STL)
DEFINE_WRITE_OPERATION(WRL)

template<typename R = Operation>
inline std::shared_ptr<R> PIPE(
    const char *geom_name,
    std::vector<std::shared_ptr<Polyhedron_operation<Surface_mesh>>> &&v)
{
    return make_and_map<Pipe_to_geomview_operation, R>(geom_name, std::move(v));
}

#undef DEFINE_WRITE_OPERATION

#endif
