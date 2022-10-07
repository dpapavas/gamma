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

#include "kernel.h"
#include "selection.h"
#include "polyhedron_operations.h"
#include "deform_operations.h"

#include <CGAL/Polygon_mesh_processing/fair.h>

// Fair operation

template<typename T>
void Fair_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    this->polyhedron = std::make_shared<T>(*this->operand->get_value());

    // Ensure mesh is triangulated.

    CGAL::Polygon_mesh_processing::triangulate_faces(
        CGAL::faces(*this->polyhedron), *this->polyhedron);

    // Attempt to fair selected portion.

    const auto &v = selector->apply(*this->polyhedron);

    if(CGAL::Polygon_mesh_processing::fair(
           *this->polyhedron, v,
           CGAL::Polygon_mesh_processing::parameters::fairing_continuity(
               continuity))) {
        this->annotations.insert({"selected", std::to_string(v.size())});
    } else {
        CGAL_error_msg("mesh fairing failed");
    }
}

template void Fair_operation<Polyhedron>::evaluate();
template void Fair_operation<Surface_mesh>::evaluate();

// Deform operation

#include <CGAL/Simple_cartesian.h>
#include <CGAL/boost/graph/copy_face_graph.h>
#include <CGAL/Polyhedron_items_with_id_3.h>
#include <CGAL/Surface_mesh_deformation.h>

template<typename P, typename V, typename O>
static void match_selection(
    std::vector<P> pairs, std::vector<V> v, O it)
{
    std::sort(
        pairs.begin(), pairs.end(),
        [](const auto &a, const auto &b) {
            return a.first < b.first;
        });
    std::sort(v.begin(), v.end());

    // Intersect the sorted vectors and ouput matching
    // descriptors.  (This is equivalent to std::set_intersection,
    // but without the need for defining comparators and
    // allocating extra vectors to store matching pairs).

    for (auto [pit, vit] = std::pair(pairs.begin(), v.begin());
         pit != pairs.end() && vit != v.end(); ) {

        if (pit->first < *vit) {
            ++pit;
        } else  {
            if (!(*vit < pit->first)) {
                *it = (pit++)->second;
            }
            ++vit;
        }
    }
}

typedef CGAL::Simple_cartesian<double> Double_kernel;
typedef CGAL::Polyhedron_3<Double_kernel,
                           CGAL::Polyhedron_items_with_id_3> Double_polyhedron;

template<typename T>
void Deform_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    T &O = *this->operand->get_value();
    Double_polyhedron P;
    std::vector<
        std::pair<typename boost::graph_traits<T>::vertex_descriptor,
                  Double_polyhedron::Vertex_handle>> pairs;

    CGAL::copy_face_graph(
        O, P, CGAL::parameters::vertex_to_vertex_output_iterator(
            std::back_inserter(pairs)));

    CGAL::Polygon_mesh_processing::triangulate_faces(P.facet_handles(), P);
    CGAL::set_halfedgeds_items_id(P);

    CGAL::Surface_mesh_deformation deform(P);

    if (selector) {
        std::vector<Double_polyhedron::Vertex_handle> v;

        match_selection(pairs, selector->apply(O), std::back_inserter(v));

        deform.insert_roi_vertices(v.begin(), v.end());
        this->annotations.insert({"selected", std::to_string(v.size())});
    } else {
        const auto v = CGAL::vertices(P);
        deform.insert_roi_vertices(v.begin(), v.end());
    }

    for (std::size_t i = 0; i < controls.size(); i++) {
        std::vector<Double_polyhedron::Vertex_handle> v;

        match_selection(
            pairs, controls[i].first->apply(O), std::back_inserter(v));

        const Aff_transformation_3 &X = controls[i].second;
        const Double_kernel::Aff_transformation_3 X_prime(
            CGAL::to_double(X.m(0,0)), CGAL::to_double(X.m(0,1)),
            CGAL::to_double(X.m(0,2)), CGAL::to_double(X.m(0,3)),
            CGAL::to_double(X.m(1,0)), CGAL::to_double(X.m(1,1)),
            CGAL::to_double(X.m(1,2)), CGAL::to_double(X.m(1,3)),
            CGAL::to_double(X.m(2,0)), CGAL::to_double(X.m(2,1)),
            CGAL::to_double(X.m(2,2)), CGAL::to_double(X.m(2,3)),
            CGAL::to_double(X.m(3,3)));

        this->annotations.insert({
                "control-" + std::to_string(i), std::to_string(v.size())});

        deform.insert_control_vertices(v.begin(), v.end());

        for (const auto &x: v) {
            deform.set_target_position(x, x->point().transform(X_prime));
        }
    }

    if (!deform.preprocess()) {
        CGAL_error_msg("preprocessing failed");
    }

    deform.deform(iterations, CGAL::to_double(tolerance));

    this->polyhedron = std::make_shared<T>();
    CGAL::copy_face_graph(P, *this->polyhedron);
}

template void Deform_operation<Polyhedron>::evaluate();
template void Deform_operation<Surface_mesh>::evaluate();

// Contract polyhedron via mean curvature flow.

#include <CGAL/Mean_curvature_flow_skeletonization.h>

typedef CGAL::Surface_mesh<Double_kernel::Point_3> Double_mesh;

template<typename T>
void Deflate_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    Double_mesh M;
    T &P = *this->operand->get_value();
    std::unordered_set<Double_mesh::Vertex_index> constrained;

    if (selector) {
        std::vector<
            std::pair<typename boost::graph_traits<T>::vertex_descriptor,
                      Double_mesh::Vertex_index>> pairs;

        CGAL::copy_face_graph(
            P, M, CGAL::parameters::vertex_to_vertex_output_iterator(
                std::back_inserter(pairs)));

        match_selection(
            pairs, selector->apply(P),
            std::inserter(constrained, constrained.end()));
    } else {
        CGAL::copy_face_graph(P, M);
    }

    CGAL::Polygon_mesh_processing::triangulate_faces(M.faces(), M);
    CGAL::Mean_curvature_flow_skeletonization<Double_mesh> skeletonization(M);

    if (selector) {
        skeletonization.set_fixed_vertices(
            constrained.cbegin(), constrained.cend());
    }

    skeletonization.set_quality_speed_tradeoff(
        CGAL::to_double(parameters[0]));

    if (parameters[1] > 0) {
        skeletonization.set_is_medially_centered(true);
        skeletonization.set_medially_centered_speed_tradeoff(
            CGAL::to_double(parameters[1]));
    }

    for (int i = 0; i < steps; i++) {
        skeletonization.contract();
    }

    this->polyhedron = std::make_shared<T>();
    CGAL::copy_face_graph(skeletonization.meso_skeleton(), *this->polyhedron);
}

template void Deflate_operation<Polyhedron>::evaluate();
template void Deflate_operation<Surface_mesh>::evaluate();

// Smooth shape

#include <CGAL/Polygon_mesh_processing/smooth_shape.h>

template<typename T>
void Smooth_shape_operation<T>::evaluate()
{
    assert(!this->polyhedron);

    Double_mesh M;
    T &P = *this->operand->get_value();
    std::unordered_set<Double_mesh::Vertex_index> constrained;
    std::vector<Double_mesh::Face_index> faces;

    // Copy to a double-based mesh and match any selections.

    if (vertex_selector && face_selector) {
        CGAL::Polygon_mesh_processing::triangulate_faces(CGAL::faces(P), P);

        std::vector<
            std::pair<typename boost::graph_traits<T>::vertex_descriptor,
                      Double_mesh::Vertex_index>> vertex_pairs;

        std::vector<
            std::pair<typename boost::graph_traits<T>::face_descriptor,
                      Double_mesh::Face_index>> face_pairs;

        CGAL::copy_face_graph(
            P, M,
            CGAL::parameters::vertex_to_vertex_output_iterator(
                std::back_inserter(vertex_pairs)).face_to_face_output_iterator(
                std::back_inserter(face_pairs)));

        match_selection(
            vertex_pairs, vertex_selector->apply(P),
            std::inserter(constrained, constrained.end()));

        match_selection(
            face_pairs, face_selector->apply(P), std::back_inserter(faces));
    } else if (face_selector) {
        CGAL::Polygon_mesh_processing::triangulate_faces(CGAL::faces(P), P);

        std::vector<
            std::pair<typename boost::graph_traits<T>::face_descriptor,
                      Double_mesh::Face_index>> face_pairs;

        CGAL::copy_face_graph(
            P, M,
            CGAL::parameters::face_to_face_output_iterator(
                std::back_inserter(face_pairs)));

        match_selection(
            face_pairs, face_selector->apply(P), std::back_inserter(faces));
    } else if (vertex_selector) {
        std::vector<
            std::pair<typename boost::graph_traits<T>::vertex_descriptor,
                      Double_mesh::Vertex_index>> vertex_pairs;

        CGAL::copy_face_graph(
            P, M,
            CGAL::parameters::vertex_to_vertex_output_iterator(
                std::back_inserter(vertex_pairs)));

        match_selection(
            vertex_pairs, vertex_selector->apply(P),
            std::inserter(constrained, constrained.end()));

        CGAL::Polygon_mesh_processing::triangulate_faces(M.faces(), M);
    } else {
        CGAL::copy_face_graph(P, M);
        CGAL::Polygon_mesh_processing::triangulate_faces(M.faces(), M);
    }

    // Smooth the mesh.

    if (face_selector) {
        if (vertex_selector) {
            const auto is_constrained = CGAL::Boolean_property_map(constrained);

            CGAL::Polygon_mesh_processing::smooth_shape(
                faces, M, CGAL::to_double(time),
                CGAL::Polygon_mesh_processing::parameters::vertex_is_constrained_map(
                is_constrained).number_of_iterations(
                    iterations));

            this->annotations.insert(
                {"constrained", std::to_string(constrained.size())});
        } else {
            CGAL::Polygon_mesh_processing::smooth_shape(
                faces, M, CGAL::to_double(time),
                CGAL::Polygon_mesh_processing::parameters::number_of_iterations(
                    iterations));
        }

        this->annotations.insert({"selected", std::to_string(faces.size())});
    } else {
        if (vertex_selector) {
            const auto is_constrained = CGAL::Boolean_property_map(constrained);

            CGAL::Polygon_mesh_processing::smooth_shape(
                M, CGAL::to_double(time),
                CGAL::Polygon_mesh_processing::parameters::vertex_is_constrained_map(
                is_constrained).number_of_iterations(
                    iterations));

            this->annotations.insert(
                {"constrained", std::to_string(constrained.size())});
        } else {
            CGAL::Polygon_mesh_processing::smooth_shape(
                M, CGAL::to_double(time),
                CGAL::Polygon_mesh_processing::parameters::number_of_iterations(
                    iterations));
        }
    }

    // Convert to the normal kernel.

    this->polyhedron = std::make_shared<T>();
    CGAL::copy_face_graph(M, *this->polyhedron);
}

template void Smooth_shape_operation<Polyhedron>::evaluate();
template void Smooth_shape_operation<Surface_mesh>::evaluate();
