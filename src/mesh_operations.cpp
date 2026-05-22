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

#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/random_perturbation.h>
#include <CGAL/Polygon_mesh_processing/remesh.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>

#include "kernel.h"
#include "iterators.h"
#include "selection.h"
#include "polyhedron_operations.h"
#include "mesh_operations.h"

// Round number to multiples of epsilon
inline double round_epsilon(double value, double epsilon)
{
  return std::floor(value / epsilon);
}

/// Utility class for grid_simplify_point_set(): Hash_epsilon_points_3
/// defines a 3D point hash / 2 points are equal iff they belong to
/// the same cell of a grid of cell size = epsilon.
template <class Point_3, class PointMap>
struct Hash_epsilon_points_3
{
private:

    double m_epsilon;
    PointMap point_map;
    typedef typename boost::property_traits<PointMap>::value_type Point;
public:

    Hash_epsilon_points_3 (double epsilon, PointMap p_map)
        : m_epsilon (epsilon), point_map(p_map)
    {
        CGAL_precondition(epsilon > 0);
    }

  std::size_t operator() (const Point_3& a) const
  {
    const Point& pa = get(point_map,a);
    std::size_t result = boost::hash_value(round_epsilon(pa.x(), m_epsilon));
    boost::hash_combine(result, boost::hash_value(round_epsilon(pa.y(), m_epsilon)));
    boost::hash_combine(result, boost::hash_value(round_epsilon(pa.z(), m_epsilon)));
    return result;
  }

};

/// Utility class for grid_simplify_point_set(): Hash_epsilon_points_3
/// defines a 3D point equality / 2 points are equal iff they belong
/// to the same cell of a grid of cell size = epsilon.
template <class Point_3, class PointMap>
struct Equal_epsilon_points_3
{
private:

    const double m_epsilon;
    PointMap point_map;
    typedef typename boost::property_traits<PointMap>::value_type Point;
public:

    Equal_epsilon_points_3 (const double& epsilon, PointMap p_map)
        : m_epsilon (epsilon), point_map(p_map)
    {
        CGAL_precondition(epsilon > 0);
    }

    bool operator() (const Point_3& a, const Point_3& b) const
    {
      const Point& pa = get(point_map,a);
      const Point& pb = get(point_map,b);

      double ra = round_epsilon(pa.x(), m_epsilon);
      double rb = round_epsilon(pb.x(), m_epsilon);
      if (ra != rb)
        return false;
      ra = round_epsilon(pa.y(), m_epsilon);
      rb = round_epsilon(pb.y(), m_epsilon);
      if (ra != rb)
        return false;
      ra = round_epsilon(pa.z(), m_epsilon);
      rb = round_epsilon(pb.z(), m_epsilon);
      return ra == rb;
    }
};

// Document: program

// # Mesh Operations

// The following operation work on polygon or triangle meshes and
// mostly use functionality in CGAL's Polygon Mesh Processing package.

// ## Coloring Vertices and Faces

// Color can be selected either by a specific RGB triple, or via an
// index into the color map defined below.

const CGAL::IO::Color Color_operation::palette[13] = {
    CGAL::IO::Color(55, 55, 55),

    CGAL::IO::Color(231, 31, 36),
    CGAL::IO::Color(244, 230, 0),
    CGAL::IO::Color(38, 113, 179),
    CGAL::IO::Color(244, 142, 43),
    CGAL::IO::Color(0, 142, 93),
    CGAL::IO::Color(110, 57, 137),
    CGAL::IO::Color(237, 96, 39),
    CGAL::IO::Color(255, 198, 48),
    CGAL::IO::Color(139, 187, 55),
    CGAL::IO::Color(0, 151, 196),
    CGAL::IO::Color(67, 78, 151),
    CGAL::IO::Color(199, 0, 122)
};

// This utility function applies colors to selected elements (either
// faces or vertices of a mesh).

template<typename T>
static void apply_color(
    Surface_mesh &P, const T &elements, CGAL::IO::Color color)
{
    using U = typename T::iterator::value_type;

    // First, determine the name of the color map.

    const char *s;
    if constexpr (std::is_same_v<U, Surface_mesh::Face_index>) {
        s = "f:color";
    } else if constexpr (std::is_same_v<U, Surface_mesh::Edge_index>) {
        s = "e:color";
    } else {
        static_assert(std::is_same_v<U, Surface_mesh::Vertex_index>);
        s = "v:color";
    }

    // Look up or create the map if it doesn't exist.

    auto [map, p] = P.add_property_map<U, std::optional<CGAL::IO::Color>>(s);

    // For each element, we either assign the color if the element is
    // uncolored, or mix it with the current color.

    for (const auto &x: elements) {
        if (p || !map[x].has_value()) {
            map[x] = color;
        } else {
            const auto c = map[x].value();
            const double a = color.alpha() / 255.0, b = 1.0 - a;

            map[x] = CGAL::IO::Color(
                a * color.red() + b * c.red(),
                a * color.green() + b * c.green(),
                a * color.blue() + b * c.blue(),
                a * color.alpha() + b * c.alpha());
        }
    }
}

// The following operations then apply a color either to all vertices
// or faces, or a selection of them.

template<typename T>
void Color_selection_operation<T>::evaluate()
{
    assert(!polyhedron);
    polyhedron = std::make_shared<Surface_mesh>(*operand->get_value());

    const auto v = selector->apply(*polyhedron);

    apply_color(*polyhedron, v, color);

    annotations.insert({"selected", std::to_string(v.size())});
}

template void Color_selection_operation<Face_selector>::evaluate();
template void Color_selection_operation<Edge_selector>::evaluate();
template void Color_selection_operation<Vertex_selector>::evaluate();

void Color_vertices_operation::evaluate()
{
    assert(!polyhedron);
    polyhedron = std::make_shared<Surface_mesh>(*operand->get_value());

    apply_color(*polyhedron, polyhedron->vertices(), color);
}

void Color_faces_operation::evaluate()
{
    assert(!polyhedron);
    polyhedron = std::make_shared<Surface_mesh>(*operand->get_value());

    apply_color(*polyhedron, polyhedron->faces(), color);
}

// ## Perturbing Vertices

// This operation applies a random perturbation to the vertices of a
// mesh.

template<typename T>
void Perturb_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());

    if (magnitude <= 0) {
        return;
    }

    CGAL::Polygon_mesh_processing::triangulate_faces(
        CGAL::faces(*this->polyhedron), *this->polyhedron);

    if (selector) {
        const auto &v = selector->apply(*this->polyhedron);

        CGAL::Polygon_mesh_processing::random_perturbation(
            v, *this->polyhedron, CGAL::to_double(magnitude),
            CGAL::parameters::do_project(false));

        this->annotations.insert({"selected", std::to_string(v.size())});
    } else {
        CGAL::Polygon_mesh_processing::random_perturbation(
            CGAL::vertices(*this->polyhedron),
            *this->polyhedron, CGAL::to_double(magnitude),
            CGAL::parameters::do_project(false));
    }
}

template void Perturb_operation<Polyhedron>::evaluate();
template void Perturb_operation<Surface_mesh>::evaluate();

// ## Refining Faces

// This operation refines all or part of the mesh, amplifying the face
// density by the given factor.

template<typename T>
void Refine_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());


    CGAL::Polygon_mesh_processing::triangulate_faces(
        CGAL::faces(*this->polyhedron), *this->polyhedron);

    using null_vertex_iterator = null_iterator<
        typename boost::graph_traits<T>::vertex_descriptor>;

    using null_face_iterator = null_iterator<
        typename boost::graph_traits<T>::face_descriptor>;

    if (selector) {
        const auto &v = selector->apply(*this->polyhedron);

        CGAL::Polygon_mesh_processing::refine(
            *this->polyhedron, v,
            null_face_iterator(), null_vertex_iterator(),
            CGAL::parameters::density_control_factor(
                CGAL::to_double(density)));

        this->annotations.insert({"selected", std::to_string(v.size())});
    } else {
        CGAL::Polygon_mesh_processing::refine(
            *this->polyhedron, CGAL::faces(*this->polyhedron),
            null_face_iterator(), null_vertex_iterator(),
            CGAL::parameters::density_control_factor(
                CGAL::to_double(density)));
    }
}

template void Refine_operation<Polyhedron>::evaluate();
template void Refine_operation<Surface_mesh>::evaluate();

// ## Isotropic remeshing

// This operand remeshes all or part of a mesh uniformly, so that the
// edges of the resulting mesh do not exceed the specified target
// length.

#define DO_REMESH(FACES)                                                \
{                                                                       \
    const auto parameters = CGAL::parameters::number_of_iterations(     \
        iterations).collapse_constraints(false);                        \
                                                                        \
    if (edge_selector) {                                                \
        const auto v_ = edge_selector->apply(*this->polyhedron);        \
        std::unordered_set<                                             \
            typename boost::graph_traits<T>::edge_descriptor> set(      \
                v_.begin(), v_.end());                                  \
                                                                        \
        assert(v_.size() == set.size());                                \
                                                                        \
        this->annotations.insert(                                       \
            {"constrained", std::to_string(v_.size())});                \
                                                                        \
        CGAL::Polygon_mesh_processing::isotropic_remeshing(             \
            FACES, CGAL::to_double(target), *this->polyhedron,          \
            parameters.edge_is_constrained_map(                         \
                CGAL::Boolean_property_map(set)));                      \
    } else {                                                            \
        CGAL::Polygon_mesh_processing::isotropic_remeshing(             \
            FACES, CGAL::to_double(target), *this->polyhedron,          \
            parameters);                                                \
    }                                                                   \
}

template<typename T>
void Remesh_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());

    // Isotropic remeshing accepts a polygonal mesh, but the to be
    // remeshed faces, must be triangulated.

    if (face_selector) {
        CGAL::Polygon_mesh_processing::triangulate_faces(
            face_selector->apply(*this->polyhedron), *this->polyhedron);
    } else {
        CGAL::Polygon_mesh_processing::triangulate_faces(
            CGAL::faces(*this->polyhedron), *this->polyhedron);
    }

    // We must be careful to select constrained edges *after*
    // triangulation, otherwise some of the passed constrained edges,
    // may no longer belong to the mesh by the time remeshing takes
    // place.

    if (face_selector) {
        const auto v = face_selector->apply(*this->polyhedron);
        this->annotations.insert({"selected", std::to_string(v.size())});

        DO_REMESH(v);
    } else {
        DO_REMESH(CGAL::faces(*this->polyhedron));
    }
}

template void Remesh_operation<Polyhedron>::evaluate();
template void Remesh_operation<Surface_mesh>::evaluate();

#undef DO_REMESH

// ## Polyhedron Corefinement

// This operation introduces into a mesh the edges belonging to its
// intersection with another mesh.

template<typename T>
void Corefine_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->first->get_value());
    T B(*this->second->get_value());

    CGAL::Polygon_mesh_processing::triangulate_faces(
        CGAL::faces(*this->polyhedron), *this->polyhedron);
    CGAL::Polygon_mesh_processing::triangulate_faces(CGAL::faces(B), B);

    CGAL::Polygon_mesh_processing::corefine(*this->polyhedron, B);
}

template void Corefine_operation<Polyhedron>::evaluate();
template void Corefine_operation<Surface_mesh>::evaluate();

// ## Polyhedron-Plane Corefinement

// This is similar to `Corefine_operation`, but the added edges belong to the
// intersection of the mesh with the specified plane.

#include <CGAL/Polygon_mesh_processing/bbox.h>
#include <CGAL/Polygon_mesh_processing/stitch_borders.h>
#include <CGAL/convex_hull_3.h>

template<typename T>
void Corefine_with_plane_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    // We intersect the plane with the polyhedron's bounding box,
    // yielding a finite part of the plane that contains the
    // polyhedron.  We then corefine this geometry with the polyhedron
    // to get the desired result.

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());
    CGAL::Bbox_3 b = CGAL::Polygon_mesh_processing::bbox(*this->polyhedron);

    const auto r = CGAL::intersection(plane, b);

    if (!r) {
        return;
    }

    // We need to create a triangulated surface out of the
    // intersection, which can be either a single triangle, a polygon,
    // or a single point (which is of no interest).

    T B;

    if (const std::vector<Point_3> *p =
        std::get_if<std::vector<Point_3>>(&*r)) {
        const std::vector<Point_3> &v = *p;
        assert(v.size() >= 4);

        CGAL::convex_hull_3(v.begin(), v.end(), B);
    } else if (const Triangle_3 *t = std::get_if<Triangle_3>(&*r)) {
        CGAL::make_triangle(t->vertex(0), t->vertex(1), t->vertex(2), B);
    } else {
        return;
    }

    CGAL::Polygon_mesh_processing::triangulate_faces(
        CGAL::faces(*this->polyhedron), *this->polyhedron);

    CGAL::Polygon_mesh_processing::corefine(*this->polyhedron, B);

    // The output can have duplicated vertices; fix that.

    CGAL::Polygon_mesh_processing::stitch_borders(*this->polyhedron);
}

template void Corefine_with_plane_operation<Polyhedron>::evaluate();
template void Corefine_with_plane_operation<Surface_mesh>::evaluate();

#include <CGAL/Polygon_mesh_processing/connected_components.h>

// ## Connected Polyhedron Components

// The following operation extracts connected components from a
// polyhedron.  Connected components are closed surfaces, not volumes,
// so in a hollow sphere for instance, both outer and inner surfaces
// can be selected independently.  This allows selective removal of
// holes as well as extraction of holes as new meshes.

template<typename T>
void Polyhedron_components_operation<T>::evaluate()
{
    typedef typename boost::graph_traits<T>::face_descriptor face_descriptor;
    typedef typename boost::graph_traits<T>::faces_size_type faces_size_type;

    assert(!this->polyhedron);

    const T &M = *this->operand->get_value();

    // First, we identify connected components in the operand.

    std::unordered_map<face_descriptor, faces_size_type> map;
    const auto property_map = boost::associative_property_map<decltype(map)>(map);

    const std::size_t n =
        CGAL::Polygon_mesh_processing::connected_components(M, property_map);

    this->annotations.insert({"components", std::to_string(n)});

    // We sort the components by their bounding boxes, to ensure
    // stable numbering.

    const auto v = sort_face_patches(M, map, 0, n);
    auto u = components;
    for (auto &x: u) {
        x = v[x - 1];
    }

    // We can then copy the selected components.

    const auto F = CGAL::Face_filtered_graph<T>(M, u, property_map);

    std::shared_ptr<T> p = std::make_shared<T>(), q = std::make_shared<T>();

    CGAL::copy_face_graph(F, *p);

    // The results may need to be reorientied, if inwardly oriented
    // hole components have been selected without their outwardly
    // oriented bounding components.  Since CGAL's orientation
    // functions work on triangle meshes only, we make a copy of the
    // result, triangulate it and check its orientation.

    *q = *p;
    CGAL::Polygon_mesh_processing::triangulate_faces(CGAL::faces(*q), *q);
    if (CGAL::Polygon_mesh_processing::is_outward_oriented(*q)) {
        // If it is oriented properly, we assign the untriangulated
        // copy as the result, to avoid triangulating the initial
        // mesh.

        this->polyhedron = p;
        return;
    }

    // If not, we reorient the triangulated mesh and use that as the
    // result.

    CGAL::Polygon_mesh_processing::orient(*q);
    this->polyhedron = q;
}

template void Polyhedron_components_operation<Polyhedron>::evaluate();
template void Polyhedron_components_operation<Surface_mesh>::evaluate();
