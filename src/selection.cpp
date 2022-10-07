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

#include <CGAL/boost/graph/selection.h>

#include "kernel.h"
#include "iterators.h"
#include "transformation_types.h"
#include "polyhedron_types.h"
#include "selection.h"

///////////
// Faces //
///////////

#define PARTIAL_MEMBERSHIP_TEST(TYPE, RANGE_1, RANGE_2, TEST)           \
std::vector<TYPE> v;                                                    \
                                                                        \
for (const auto x: RANGE_1) {                                           \
    for (const auto &y: RANGE_2) {                                      \
        if (TEST) {                                                     \
            if (partial) {                                              \
                v.push_back(x);                                         \
                goto next;                                              \
            }                                                           \
        } else if (!partial) {                                          \
            goto next;                                                  \
        }                                                               \
    }                                                                   \
                                                                        \
    if (!partial) {                                                     \
        v.push_back(x);                                                 \
    }                                                                   \
                                                                        \
  next:;                                                                \
}                                                                       \
                                                                        \
v.shrink_to_fit();                                                      \
return v

// Bounded

template<typename T>
inline auto select_bounded_faces(
    T &mesh, const Bounding_volume &volume,
    const bool partial)
{
    const auto map = CGAL::get(CGAL::vertex_point, mesh);

    PARTIAL_MEMBERSHIP_TEST(
        typename boost::graph_traits<T>::face_descriptor,
        CGAL::faces(mesh),
        CGAL::vertices_around_face(CGAL::halfedge(x, mesh), mesh),
        volume.contains(boost::get(map, y)));
}

std::vector<Polyhedron::Facet_handle>
Bounded_face_selector::apply(Polyhedron &mesh) const
{
    return select_bounded_faces(mesh, *volume, partial);
}

std::vector<Surface_mesh::Face_index>
Bounded_face_selector::apply(const Surface_mesh &mesh) const
{
    return select_bounded_faces(mesh, *volume, partial);
}

// Relative

template<typename T, typename U>
inline auto expand_or_contract_selection(T &mesh, const U &selector, int steps)
{
    auto v = selector.apply(mesh);
    std::unordered_set set(v.cbegin(), v.cend());
    auto map = CGAL::make_boolean_property_map(set);

    if (steps >= 0) {
        if constexpr(std::is_same_v<U, Face_selector>) {
            CGAL::expand_face_selection(
                v, mesh, steps, map, std::back_inserter(v));
        } else if constexpr(std::is_same_v<U, Vertex_selector>) {
            CGAL::expand_vertex_selection(
                v, mesh, steps, map, std::back_inserter(v));
        } else {
            static_assert(std::is_same_v<U, Edge_selector>);
            CGAL::expand_edge_selection(
                v, mesh, steps, map, std::back_inserter(v));
        }
    } else {
        if constexpr(std::is_same_v<U, Face_selector>) {
            CGAL::reduce_face_selection(
                v, mesh, -steps, map,
                null_iterator<typename decltype(v)::value_type>());
        } else if constexpr(std::is_same_v<U, Vertex_selector>) {
            CGAL::reduce_vertex_selection(
                v, mesh, -steps, map,
                null_iterator<typename decltype(v)::value_type>());
        } else {
            static_assert(std::is_same_v<U, Edge_selector>);
            CGAL::reduce_edge_selection(
                v, mesh, -steps, map,
                null_iterator<typename decltype(v)::value_type>());
        }

        v.erase(
            std::partition(
                v.begin(), v.end(), [&set](const auto &x) {
                    return set.find(x) != set.end();
                }),
            v.end());
    }

    v.shrink_to_fit();
    return v;
}

std::vector<Polyhedron::Facet_handle>
Relative_face_selector::apply(Polyhedron &mesh) const
{
    return expand_or_contract_selection(mesh, *selector, steps);
}

std::vector<Surface_mesh::Face_index>
Relative_face_selector::apply(const Surface_mesh &mesh) const
{
    return expand_or_contract_selection(mesh, *selector, steps);
}

// Faces from vertices

template<typename T>
auto faces_from_vertices(
    T &mesh, const Vertex_selector &selector, const bool partial)
{
    using V = typename boost::graph_traits<T>::vertex_descriptor;
    std::unordered_set<V> vertices;

    {
        const auto &w = selector.apply(mesh);

        vertices.reserve(w.size());
        vertices.insert(w.cbegin(), w.cend());
    }

    PARTIAL_MEMBERSHIP_TEST(
        typename boost::graph_traits<T>::face_descriptor,
        CGAL::faces(mesh),
        CGAL::vertices_around_face(CGAL::halfedge(x, mesh), mesh),
        vertices.find(y) != vertices.end());
}

std::vector<Polyhedron::Facet_handle>
Vertex_to_face_selector::apply(Polyhedron &mesh) const
{
    return faces_from_vertices(mesh, *selector, partial);
}

std::vector<Surface_mesh::Face_index>
Vertex_to_face_selector::apply(const Surface_mesh &mesh) const
{
    return faces_from_vertices(mesh, *selector, partial);
}

// Faces from edges

template<typename T>
auto faces_from_edges(
    T &mesh, const Edge_selector &selector, const bool partial)
{
    using E = typename boost::graph_traits<T>::edge_descriptor;
    std::unordered_set<E> edges;

    {
        const auto &w = selector.apply(mesh);

        edges.reserve(w.size());
        edges.insert(w.cbegin(), w.cend());
    }

    PARTIAL_MEMBERSHIP_TEST(
        typename boost::graph_traits<T>::face_descriptor,
        CGAL::faces(mesh),
        CGAL::halfedges_around_face(CGAL::halfedge(x, mesh), mesh),
        edges.find(E(y)) != edges.end());
}

std::vector<Polyhedron::Facet_handle>
Edge_to_face_selector::apply(Polyhedron &mesh) const
{
    return faces_from_edges(mesh, *selector, partial);
}

std::vector<Surface_mesh::Face_index>
Edge_to_face_selector::apply(const Surface_mesh &mesh) const
{
    return faces_from_edges(mesh, *selector, partial);
}

//////////////
// Vertices //
//////////////

// Bounded

std::vector<Polyhedron::Vertex_handle>
Bounded_vertex_selector::apply(Polyhedron &mesh) const
{
    std::vector<Polyhedron::Vertex_handle> v;

    for (auto x = mesh.vertices_begin(); x != mesh.vertices_end(); x++) {
        if (volume->contains(x->point())) {
            v.push_back(x);
        }
    }

    v.shrink_to_fit();
    return v;
}

std::vector<Surface_mesh::Vertex_index>
Bounded_vertex_selector::apply(const Surface_mesh &mesh) const
{
    std::vector<Surface_mesh::Vertex_index> v;

    for (const auto &x: mesh.vertices()) {
        if (volume->contains(mesh.point(x))) {
            v.push_back(x);
        }
    }

    v.shrink_to_fit();
    return v;
}

// Relative

std::vector<Polyhedron::Vertex_handle>
Relative_vertex_selector::apply(Polyhedron &mesh) const
{
    return expand_or_contract_selection(mesh, *selector, steps);
}

std::vector<Surface_mesh::Vertex_index>
Relative_vertex_selector::apply(const Surface_mesh &mesh) const
{
    return expand_or_contract_selection(mesh, *selector, steps);
}

// Vertices from faces

template<typename T>
auto vertices_from_faces(T &mesh, const Face_selector &selector)
{
    using V = typename boost::graph_traits<T>::vertex_descriptor;

    std::unordered_set<V> vertices;

    for (const auto &f: selector.apply(mesh)) {
        for (const auto &v: CGAL::vertices_around_face(
                 CGAL::halfedge(f, mesh), mesh)) {
            vertices.insert(v);
        }
    }

    return std::vector<V>(vertices.cbegin(), vertices.cend());
}

std::vector<Polyhedron::Vertex_handle>
Face_to_vertex_selector::apply(Polyhedron &mesh) const
{
    return vertices_from_faces(mesh, *selector);
}

std::vector<Surface_mesh::Vertex_index>
Face_to_vertex_selector::apply(const Surface_mesh &mesh) const
{
    return vertices_from_faces(mesh, *selector);
}

// Vertices from edges

template<typename T>
auto vertices_from_edges(T &mesh, const Edge_selector &selector)
{
    using V = typename boost::graph_traits<T>::vertex_descriptor;

    std::unordered_set<V> vertices;

    for (const auto &e: selector.apply(mesh)) {
        vertices.insert(CGAL::source(e, mesh));
        vertices.insert(CGAL::target(e, mesh));
    }

    return std::vector<V>(vertices.cbegin(), vertices.cend());
}

std::vector<Polyhedron::Vertex_handle>
Edge_to_vertex_selector::apply(Polyhedron &mesh) const
{
    return vertices_from_edges(mesh, *selector);
}

std::vector<Surface_mesh::Vertex_index>
Edge_to_vertex_selector::apply(const Surface_mesh &mesh) const
{
    return vertices_from_edges(mesh, *selector);
}

///////////
// Edges //
///////////

// Bounded

template<typename T>
inline auto select_bounded_edges(
    T &mesh, const Bounding_volume &volume,
    const bool partial)
{
    const auto map = CGAL::get(CGAL::vertex_point, mesh);

    PARTIAL_MEMBERSHIP_TEST(
        typename boost::graph_traits<T>::edge_descriptor,
        CGAL::edges(mesh),
        std::initializer_list({CGAL::source(x, mesh), CGAL::target(x, mesh)}),
        volume.contains(boost::get(map, y)));
}

std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Bounded_edge_selector::apply(Polyhedron &mesh) const
{
    return select_bounded_edges(mesh, *volume, partial);
}

std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Bounded_edge_selector::apply(const Surface_mesh &mesh) const
{
    return select_bounded_edges(mesh, *volume, partial);
}

// Relative

std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Relative_edge_selector::apply(Polyhedron &mesh) const
{
    return expand_or_contract_selection(mesh, *selector, steps);
}

std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Relative_edge_selector::apply(const Surface_mesh &mesh) const
{
    return expand_or_contract_selection(mesh, *selector, steps);
}

// Edges from vertices

template<typename T, typename V>
auto edges_from_vertices(
    T &mesh, const std::unordered_set<V> &vertices, const bool partial)
{
    PARTIAL_MEMBERSHIP_TEST(
        typename boost::graph_traits<T>::edge_descriptor,
        CGAL::edges(mesh),
        std::initializer_list({CGAL::source(x, mesh), CGAL::target(x, mesh)}),
        vertices.find(y) != vertices.end());
}

template<typename T>
auto edges_from_vertices(
    T &mesh, const Vertex_selector &selector, const bool partial)
{
    using V = typename boost::graph_traits<T>::vertex_descriptor;
    std::unordered_set<V> vertices;

    {
        const auto &w = selector.apply(mesh);

        vertices.reserve(w.size());
        vertices.insert(w.cbegin(), w.cend());
    }

    return edges_from_vertices(mesh, vertices, partial);
}

std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Vertex_to_edge_selector::apply(Polyhedron &mesh) const
{
    return edges_from_vertices(mesh, *selector, partial);
}

std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Vertex_to_edge_selector::apply(const Surface_mesh &mesh) const
{
    return edges_from_vertices(mesh, *selector, partial);
}

// Edges from faces

template<typename T>
auto edges_from_faces(
    T &mesh, const Face_selector &selector, const bool partial)
{
    // Edges "partially in" a face, are those for which one of their
    // targets belongs to the face.

    if (partial) {
        using V = typename boost::graph_traits<T>::vertex_descriptor;

        std::unordered_set<V> vertices;

        for (const auto &f: selector.apply(mesh)) {
            for (const auto &v: CGAL::vertices_around_face(
                     CGAL::halfedge(f, mesh), mesh)) {
                vertices.insert(v);
            }
        }

        return edges_from_vertices(mesh, vertices, partial);
    } else {
        using E = typename boost::graph_traits<T>::edge_descriptor;
        std::unordered_set<E> edges;

        for (const auto &f: selector.apply(mesh)) {
            for (const auto &h: CGAL::halfedges_around_face(
                     CGAL::halfedge(f, mesh), mesh)) {
                edges.insert(E(h));
            }
        }

        return std::vector<E>(edges.cbegin(), edges.cend());
    }
}

std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Face_to_edge_selector::apply(Polyhedron &mesh) const
{
    return edges_from_faces(mesh, *selector, partial);
}

std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Face_to_edge_selector::apply(const Surface_mesh &mesh) const
{
    return edges_from_faces(mesh, *selector, partial);
}

////////////////////////////
// Boolean set operations //
////////////////////////////

#define DEFINE_SET_OPERATION(OP, WHAT, HANDLE, INDEX)                   \
template<typename T, typename U>                                        \
inline auto WHAT ##_selection_## OP(T &mesh, const std::vector<U> &selectors) \
{                                                                       \
    using V = typename boost::graph_traits<T>::WHAT ##_descriptor;      \
                                                                        \
    bool p = true;                                                      \
    std::vector<V> a, b, *q = &a, *r = &b;                              \
                                                                        \
    for (const U &x: selectors) {                                       \
        std::vector<V> v = x->apply(mesh);                              \
        std::sort(v.begin(), v.end());                                  \
                                                                        \
        if (p) {                                                        \
            q->reserve(v.size());                                       \
            q->insert(q->cbegin(), v.cbegin(), v.cend());               \
            p = false;                                                  \
                                                                        \
            continue;                                                   \
        }                                                               \
                                                                        \
        std::set_## OP(                                                 \
            q->cbegin(), q->cend(), v.cbegin(), v.cend(),               \
            std::back_inserter(*r));                                    \
        q->clear();                                                     \
        std::swap(q, r);                                                \
    }                                                                   \
                                                                        \
    return *q;                                                          \
}                                                                       \
                                                                        \
std::vector<HANDLE>                                                     \
Set_## OP ##_## WHAT ##_selector::apply(Polyhedron &mesh) const         \
{                                                                       \
    return WHAT ##_selection_## OP(mesh, selectors);                    \
}                                                                       \
                                                                        \
std::vector<INDEX>                                                      \
Set_## OP ##_## WHAT ##_selector::apply(const Surface_mesh &mesh) const \
{                                                                       \
    return WHAT ##_selection_## OP(mesh, selectors);                    \
}

DEFINE_SET_OPERATION(union, face,
                     Polyhedron::Facet_handle,
                     Surface_mesh::Face_index)
DEFINE_SET_OPERATION(difference, face,
                     Polyhedron::Facet_handle,
                     Surface_mesh::Face_index)
DEFINE_SET_OPERATION(intersection, face,
                     Polyhedron::Facet_handle,
                     Surface_mesh::Face_index)

DEFINE_SET_OPERATION(union,
                     vertex, Polyhedron::Vertex_handle, Surface_mesh::Vertex_index)
DEFINE_SET_OPERATION(difference, vertex,
                     Polyhedron::Vertex_handle, Surface_mesh::Vertex_index)
DEFINE_SET_OPERATION(intersection, vertex,
                     Polyhedron::Vertex_handle, Surface_mesh::Vertex_index)

DEFINE_SET_OPERATION(union, edge,
                     boost::graph_traits<Polyhedron>::edge_descriptor,
                     boost::graph_traits<Surface_mesh>::edge_descriptor)
DEFINE_SET_OPERATION(difference, edge,
                     boost::graph_traits<Polyhedron>::edge_descriptor,
                     boost::graph_traits<Surface_mesh>::edge_descriptor)
DEFINE_SET_OPERATION(intersection, edge,
                     boost::graph_traits<Polyhedron>::edge_descriptor,
                     boost::graph_traits<Surface_mesh>::edge_descriptor)

#undef DEFINE_SET_OPERATION

#define DEFINE_COMPLEMENT_OPERATION(WHAT, HANDLE, INDEX, RANGE)         \
template<typename T, typename U>                                        \
inline auto WHAT ##_selection_complement(T &mesh, const U &selector)    \
{                                                                       \
    using V = typename boost::graph_traits<T>::WHAT ##_descriptor;      \
                                                                        \
    std::vector<V> u;                                                   \
                                                                        \
    const auto &x = CGAL::RANGE(mesh);                                  \
    u.insert(u.begin(), x.begin(), x.end());                            \
                                                                        \
    std::vector<V> v = selector->apply(mesh);                           \
                                                                        \
    std::sort(u.begin(), u.end());                                      \
    std::sort(v.begin(), v.end());                                      \
                                                                        \
    std::vector<V> w;                                                   \
                                                                        \
    std::set_difference(                                                \
        u.cbegin(), u.cend(), v.cbegin(), v.cend(),                     \
        std::back_inserter(w));                                         \
                                                                        \
    return w;                                                           \
}                                                                       \
                                                                        \
std::vector<HANDLE>                                                     \
Set_complement_## WHAT ##_selector::apply(Polyhedron &mesh) const       \
{                                                                       \
    return WHAT ##_selection_complement(mesh, selector);                \
}                                                                       \
                                                                        \
std::vector<INDEX>                                                      \
Set_complement_## WHAT ##_selector::apply(const Surface_mesh &mesh) const \
{                                                                       \
    return WHAT ##_selection_complement(mesh, selector);                \
}

DEFINE_COMPLEMENT_OPERATION(face,
                            Polyhedron::Facet_handle,
                            Surface_mesh::Face_index,
                            faces)
DEFINE_COMPLEMENT_OPERATION(vertex,
                            Polyhedron::Vertex_handle,
                            Surface_mesh::Vertex_index,
                            vertices)
DEFINE_COMPLEMENT_OPERATION(edge,
                            boost::graph_traits<Polyhedron>::edge_descriptor,
                            boost::graph_traits<Surface_mesh>::edge_descriptor,
                            edges)

#undef DEFINE_COMPLEMENT_OPERATION
