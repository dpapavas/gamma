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

#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_vertex_base_with_info_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Polygon_mesh_processing/polygon_soup_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/orientation.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>

#include "kernel.h"
#include "extrusion_operation.h"

typedef CGAL::Triangulation_vertex_base_with_info_2<std::size_t, Kernel> Vertex_base;
typedef CGAL::Triangulation_face_base_with_info_2<int, Kernel> Face_base;
typedef CGAL::Constrained_triangulation_face_base_2<
    Kernel, Face_base> Constrained_face_base;
typedef CGAL::Constrained_Delaunay_triangulation_2<
    Kernel,
    CGAL::Triangulation_data_structure_2<Vertex_base, Constrained_face_base>,
    CGAL::No_constraint_intersection_tag> Constrained_Delaunay_triangulation;
typedef Constrained_Delaunay_triangulation::Face_handle Face_handle;
typedef Constrained_Delaunay_triangulation::Vertex_handle Vertex_handle;

static void mark_domains(Constrained_Delaunay_triangulation& T,
                         Face_handle face, int index)
{
    int i;

    face->info() = index;

    for (i = 0; i < 3; i++) {
        Constrained_Delaunay_triangulation::Edge e(face, i);
        Face_handle n = face->neighbor(i);

        if (n->info() == -1) {
            mark_domains(T, n, T.is_constrained(e) ? index + 1 : index);
        }
    }
}

inline static void extrude_polygon_with_holes(
    Polygon_with_holes &G,
    const std::vector<Aff_transformation_3> &transformations,
    std::vector<Point_3> &points,
    std::vector<std::vector<std::size_t>> &polygons)
{
    Constrained_Delaunay_triangulation T;
    const Polygon &B = G.outer_boundary();
    Polygon_with_holes::Hole_const_iterator H;

    // Triangulate the to-be-extruded polygon.

    T.insert_constraint(B.vertices_begin(), B.vertices_end(), true);

    for (H = G.holes_begin(); H != G.holes_end(); H++) {
        T.insert_constraint(H->vertices_begin(), H->vertices_end(), true);
    }

    for(Face_handle f: T.all_face_handles()) {
        f->info() = -1;
    }

    mark_domains(T, T.infinite_face(), 0);

    // Extrude the triangulation.

    const bool close = (transformations.size() > 1
                        && transformations.front() == transformations.back());
    const int steps = transformations.size();
    const std::size_t k = T.number_of_vertices();
    const std::size_t l = T.number_of_faces();
    const std::size_t m = T.constrained_edges().size();
    const std::size_t n = (steps - 1) * m;
    std::vector<Vertex_handle> v;

    v.reserve(k);
    points.reserve(points.size() + (steps - close) * k);
    polygons.reserve(polygons.size() + 2 * l + n);

    {
        std::size_t i = 0;
        for (Vertex_handle h: T.finite_vertex_handles()) {
            v[(h->info() = i++)] = h;
        }
    }

    for (int s = 0; s < steps - close; s++) {
        for (std::size_t i = 0; i < k; i++) {
            const Constrained_Delaunay_triangulation::Point &p = v[i]->point();

            points.push_back(
                transformations[s].transform(Point_3(p.x(), p.y(), FT(0))));
        }
    }

    for(Face_handle f : T.finite_face_handles()) {
        // Faces within the triangulation have an odd number of
        // crossings.

        if (f->info() % 2 == 0) {
            continue;
        }

        if (!close) {
            // Bottom

            {
                auto &v = polygons.emplace_back();
                v.reserve(3);

                for (int i = 2; i >= 0; i--) {
                    v.push_back(f->vertex(i)->info());
                }
            }

            if (transformations.size() > 1) {
                // Top

                auto &v = polygons.emplace_back();
                v.reserve(3);

                for (int i = 0; i < 3; i++) {
                    v.push_back(n + f->vertex(i)->info());
                }
            }
        } else {
            // Loopback

            for (int i = 0; i < 3; i++) {
                const Constrained_Delaunay_triangulation::Edge e(f, i);

                if (T.is_constrained(e)) {
                    const std::size_t a = f->vertex(f->ccw(i))->info(),
                        b = f->vertex(f->cw(i))->info();

                    polygons.emplace_back(
                        std::initializer_list({n - k + a, n - k + b, b, a}));
                }
            }
        }

        // Sides

        for (int s = 1, i_0 = k; s < steps - close; s++, i_0 += k) {
            for (int i = 0; i < 3; i++) {
                const Constrained_Delaunay_triangulation::Edge e(f, i);

                if (T.is_constrained(e)) {
                    const std::size_t a = f->vertex(f->ccw(i))->info(),
                        b = f->vertex(f->cw(i))->info();

                    polygons.emplace_back(
                        std::initializer_list({
                                i_0 - k + a, i_0 - k + b, i_0 + b, i_0 + a}));
                }
            }
        }
    }
}

inline static void extrude_polygon(
    Polygon &G,
    const std::vector<Aff_transformation_3> &transformations,
    std::vector<Point_3> &points,
    std::vector<std::vector<std::size_t>> &polygons)
{
    const bool close = (transformations.size() > 1
                        && transformations.front() == transformations.back());
    const int steps = transformations.size();
    const std::size_t n = G.size();
    const std::size_t m = (steps - 1) * n;

    points.reserve(points.size() + (steps - close) * n);
    polygons.reserve(polygons.size() + 2 + m);

    for (int s = 0; s < steps - close; s++) {
        for (std::size_t i = 0; i < n; i++) {
            const Point_2 &p = G.vertex(i);

            points.push_back(
                transformations[s].transform(Point_3(p.x(), p.y(), FT(0))));
        }
    }

    if (!close) {
        // Bottom

        {
            auto &v = polygons.emplace_back();
            v.reserve(n);

            for (std::size_t i = n; i > 0; i--) {
                v.push_back(i - 1);
            }
        }

        if (transformations.size() > 1) {
            // Top

            auto &v = polygons.emplace_back();
            v.reserve(n);

            for (std::size_t i = 0; i < n; i++) {
                v.push_back(m + i);
            }
        }
    } else {
        // Loopback

        for (std::size_t i = 0; i < n; i++) {
            polygons.emplace_back(
                std::initializer_list({
                        m - n + i, m - n + (i + 1) % n,
                        (i + 1) % n, i}));
        }
    }

    for (int s = 1, i_0 = n; s < steps - close; s++, i_0 += n) {
        // Sides

        for (std::size_t i = 0; i < n; i++) {
            polygons.emplace_back(
                std::initializer_list({
                        i_0 - n + i, i_0 - n + (i + 1) % n,
                        i_0 + (i + 1) % n, i_0 + i}));
        }
    }
}

void Extrusion_operation::evaluate()
{
    assert(!polyhedron);

    const std::shared_ptr<Polygon_set> &p = operand->get_value();
    const int n = p->number_of_polygons_with_holes();
    std::vector<Polygon_with_holes> v;

    v.reserve(n);
    polyhedron = std::make_shared<Polyhedron>();

    // Extrude each polygon in the set separately.

    p->polygons_with_holes(std::back_inserter(v));
    for (Polygon_with_holes &G: v) {
        std::vector<Point_3> points;
        std::vector<std::vector<std::size_t>> polygons;

        if (G.number_of_holes() > 0) {
            extrude_polygon_with_holes(G, transformations, points, polygons);
        } else {
            extrude_polygon(
                G.outer_boundary(), transformations, points, polygons);
        }

        Polyhedron P;

        if (transformations.size() > 1) {
            bool p = false;

            while (true) {
                try {
                    if (!CGAL::Polygon_mesh_processing::orient_polygon_soup(
                            points, polygons)) {
                        CGAL_error_msg("extrusion cannot be oriented");
                        break;
                    }

                    CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(
                        points, polygons, P);

                    CGAL::Polygon_mesh_processing::triangulate_faces(
                        P.facet_handles(), P);

                    CGAL::Polygon_mesh_processing::orient_to_bound_a_volume(P);
                    break;
                } catch(const CGAL::Failure_exception &e) {
                    if (p) {
                        throw;
                    }

                    std::ostringstream s;
                    std::string t;

                    s << "attempted to repair extrusion";
                    if (const std::string &m = e.message(); !m.empty()) {
                        s << " (" << m << ")";
                    }

                    t = s.str();
                    message(NOTE, t);

                    P.clear();
                    CGAL::Polygon_mesh_processing::repair_polygon_soup(
                        points, polygons);

                    p = true;
                }
            }
        } else if (polygons.size() > 1) {
            CGAL::Polygon_mesh_processing::polygon_soup_to_polygon_mesh(
                points, polygons, P);
        } else {
            const auto &v = polygons.front();
            assert(v.size() >= 3);
            auto h = P.make_triangle(points[v[0]],
                                     points[v[1]],
                                     points[v[2]])->opposite();

            for (std::size_t i = 3; i < v.size(); i++) {
                h = P.split_edge(h);
                h->vertex()->point() = points[v[i]];
            }
        }

        CGAL::copy_face_graph(P, *polyhedron);
    }
}
