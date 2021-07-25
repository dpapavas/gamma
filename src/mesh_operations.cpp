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

template<typename T>
void Color_selection_operation<T>::evaluate()
{
    assert(!polyhedron);

    polyhedron = std::make_shared<Surface_mesh>(*operand->get_value());

    using U = std::conditional_t<std::is_same_v<T, Face_selector>,
                                 Surface_mesh::Face_index,
                                 Surface_mesh::Vertex_index>;

    const char *s;
    if constexpr (std::is_same_v<T, Face_selector>) {
        s = "f:color";
    } else {
        s = "v:color";
    }

    auto [map, p] = polyhedron->add_property_map<U, CGAL::IO::Color>(
        s, CGAL::IO::Color(165, 165, 165, 255));

    const auto v = selector->apply(*polyhedron);
    for (const auto &x: v) {
        if (p) {
            map[x] = color;
        } else {
            const auto c = map[x];
            map[x] = CGAL::IO::Color(
                (c.red() + color.red()) / 2,
                (c.green() + color.green()) / 2,
                (c.blue() + color.blue()) / 2,
                (c.alpha() + color.alpha()) / 2);
        }
    }

    this->annotations.insert({"selected", std::to_string(v.size())});
}

template void Color_selection_operation<Face_selector>::evaluate();
template void Color_selection_operation<Vertex_selector>::evaluate();

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
            CGAL::Polygon_mesh_processing::parameters::do_project(false));

        this->annotations.insert({"selected", std::to_string(v.size())});
    } else {
        CGAL::Polygon_mesh_processing::random_perturbation(
            CGAL::vertices(*this->polyhedron),
            *this->polyhedron, CGAL::to_double(magnitude),
            CGAL::Polygon_mesh_processing::parameters::do_project(false));
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
            CGAL::Polygon_mesh_processing::parameters::density_control_factor(
                CGAL::to_double(density)));

        this->annotations.insert({"selected", std::to_string(v.size())});
    } else {
        CGAL::Polygon_mesh_processing::refine(
            *this->polyhedron, CGAL::faces(*this->polyhedron),
            null_face_iterator(), null_vertex_iterator(),
            CGAL::Polygon_mesh_processing::parameters::density_control_factor(
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
                v, CGAL::to_double(this->target), *this->polyhedron,
                CGAL::Polygon_mesh_processing::parameters::edge_is_constrained_map(
                    is_constrained).number_of_iterations(
                    iterations));
        } else {
            CGAL::Polygon_mesh_processing::isotropic_remeshing(
                v, CGAL::to_double(this->target), *this->polyhedron,
                CGAL::Polygon_mesh_processing::parameters::number_of_iterations(
                    iterations));
        }

        this->annotations.insert({"selected", std::to_string(v.size())});
    } else {
        CGAL::Polygon_mesh_processing::triangulate_faces(
            CGAL::faces(*this->polyhedron), *this->polyhedron);

        if (edge_selector) {
            CGAL::Polygon_mesh_processing::isotropic_remeshing(
                CGAL::faces(*this->polyhedron),
                CGAL::to_double(this->target), *this->polyhedron,
                CGAL::Polygon_mesh_processing::parameters::edge_is_constrained_map(
                    is_constrained).number_of_iterations(
                        iterations));
        } else {
            CGAL::Polygon_mesh_processing::isotropic_remeshing(
                CGAL::faces(*this->polyhedron),
                CGAL::to_double(this->target), *this->polyhedron,
                CGAL::Polygon_mesh_processing::parameters::number_of_iterations(
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

template<typename T>
void Corefine_with_plane_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());
    CGAL::Bbox_3 b = CGAL::Polygon_mesh_processing::bbox(*this->polyhedron);
    b.dilate(1);

    const auto r = CGAL::intersection(plane, b);

    if (!r) {
        return;
    }

    T B;

    if (const std::vector<Point_3> *p = boost::get<std::vector<Point_3>>(&*r)) {
        const std::vector<Point_3> &v = *p;
        assert(v.size() >= 4);

        auto map = CGAL::get(CGAL::vertex_point, B);
        auto h = CGAL::opposite(CGAL::make_triangle(v[0], v[1], v[2], B), B);

        for (std::size_t i = 3; i < v.size(); i++) {
            h = CGAL::Euler::split_edge(h, B);
            boost::get(map, CGAL::target(h, B)) = v[i];
        }

        CGAL::Polygon_mesh_processing::triangulate_faces(CGAL::faces(B), B);
    } else if (const Kernel::Triangle_3 *t =
               boost::get<Kernel::Triangle_3>(&*r)) {
        CGAL::make_triangle(t->vertex(0), t->vertex(1), t->vertex(2), B);
    } else {
        return;
    }

    CGAL::Polygon_mesh_processing::triangulate_faces(
        CGAL::faces(*this->polyhedron), *this->polyhedron);

    CGAL::Polygon_mesh_processing::corefine(*this->polyhedron, B);
}

template void Corefine_with_plane_operation<Polyhedron>::evaluate();
template void Corefine_with_plane_operation<Surface_mesh>::evaluate();
