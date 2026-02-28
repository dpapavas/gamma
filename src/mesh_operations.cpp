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

// Color selected vertices/faces

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

template<typename T>
static void apply_color(
    Surface_mesh &P, const T &elements, CGAL::IO::Color color)
{
    using U = typename T::iterator::value_type;

    const char *s;
    if constexpr (std::is_same_v<U, Surface_mesh::Face_index>) {
        s = "f:color";
    } else {
        static_assert(std::is_same_v<U, Surface_mesh::Vertex_index>);
        s = "v:color";
    }

    auto [map, p] = P.add_property_map<U, std::optional<CGAL::IO::Color>>(s);

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

// Perturb selected vertices

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

// Refine selected vertices

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

// Remesh operation

template<typename T>
void Remesh_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());

    std::unordered_set<
        typename boost::graph_traits<T>::edge_descriptor> constrained;

    if (edge_selector) {
        const auto v = edge_selector->apply(*this->polyhedron);
        constrained.insert(v.cbegin(), v.cend());
    }

    const auto is_constrained = CGAL::Boolean_property_map(constrained);

    // Isotropic remeshing accepts a polygonal mesh, but the selected
    // faces, must be triangulated.

    if (face_selector) {
        CGAL::Polygon_mesh_processing::triangulate_faces(
            face_selector->apply(*this->polyhedron), *this->polyhedron);

        const auto &v = face_selector->apply(*this->polyhedron);

        if (edge_selector) {
            CGAL::Polygon_mesh_processing::isotropic_remeshing(
                v, CGAL::to_double(target), *this->polyhedron,
                CGAL::parameters::edge_is_constrained_map(
                    is_constrained).number_of_iterations(
                    iterations));

            this->annotations.insert(
                {"constrained", std::to_string(constrained.size())});
        } else {
            CGAL::Polygon_mesh_processing::isotropic_remeshing(
                v, CGAL::to_double(target), *this->polyhedron,
                CGAL::parameters::number_of_iterations(
                    iterations));
        }

        this->annotations.insert({"selected", std::to_string(v.size())});
    } else {
        CGAL::Polygon_mesh_processing::triangulate_faces(
            CGAL::faces(*this->polyhedron), *this->polyhedron);

        if (edge_selector) {
            CGAL::Polygon_mesh_processing::isotropic_remeshing(
                CGAL::faces(*this->polyhedron),
                CGAL::to_double(target), *this->polyhedron,
                CGAL::parameters::edge_is_constrained_map(
                    is_constrained).number_of_iterations(
                        iterations));

            this->annotations.insert(
                {"constrained", std::to_string(constrained.size())});
        } else {
            CGAL::Polygon_mesh_processing::isotropic_remeshing(
                CGAL::faces(*this->polyhedron),
                CGAL::to_double(target), *this->polyhedron,
                CGAL::parameters::number_of_iterations(
                        iterations));
        }
    }
}

template void Remesh_operation<Polyhedron>::evaluate();
template void Remesh_operation<Surface_mesh>::evaluate();

// Corefine polyhedra

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

// Corefine polyhedron with plane

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

    // Create a triangulated surface out of the intersection, which
    // can either be a single triangle, a polygon, or a single point
    // (which is of no interest.

    T B;

    if (const std::vector<Point_3> *p =
        std::get_if<std::vector<Point_3>>(&*r)) {
        const std::vector<Point_3> &v = *p;
        assert(v.size() >= 4);

        CGAL::convex_hull_3(v.begin(), v.end(), B);
    } else if (const Kernel::Triangle_3 *t =
               std::get_if<Kernel::Triangle_3>(&*r)) {
        CGAL::make_triangle(t->vertex(0), t->vertex(1), t->vertex(2), B);
    } else {
        return;
    }

    CGAL::Polygon_mesh_processing::triangulate_faces(
        CGAL::faces(*this->polyhedron), *this->polyhedron);

    CGAL::Polygon_mesh_processing::corefine(*this->polyhedron, B);

    // The output can have duplicatd vertices; fix that.

    CGAL::Polygon_mesh_processing::stitch_borders(*this->polyhedron);
}

template void Corefine_with_plane_operation<Polyhedron>::evaluate();
template void Corefine_with_plane_operation<Surface_mesh>::evaluate();
