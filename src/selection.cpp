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
#include <CGAL/Polygon_mesh_processing/detect_features.h>

#include "kernel.h"
#include "iterators.h"
#include "transformation_types.h"
#include "polyhedron_types.h"
#include "selection.h"

// Document: program

// # Selections

// Selections are not operations in themselves.  They can be viewed as
// functions, or operators, that can be applied to meshes (which are
// typically the result of an operation) to return sets of elements
// from them (vertices, edges, or faces), selected in a prescribed
// way.

// They're typically created and passed to operations that need to
// operate on part of a mesh, such a `Color_selection_operation`, or
// which requireq a subset of the mesh as input (like the
// `Deform_operation`).

// The way in which the elements are selected falls into several broad
// categories.

// ## Selections Based on Bounding Volumes

// Here we select elements that fall within a volume, which can be of
// one of several shapes (sphere, box, etc.) or a boolean combination
// of such shapes.

// For vertices this operation is staightforward.

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
Bounded_vertex_selector::apply(Surface_mesh &mesh) const
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

// For edges and faces, we distinguish two forms of the selection
// test:

//   1. partial membership, when at least one of the element's
//   vertices is inside the volume, or

//   2. full membership, when all vertices are inside the volume.

// We perfrom the test with the following macro.

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
Bounded_edge_selector::apply(Surface_mesh &mesh) const
{
    return select_bounded_edges(mesh, *volume, partial);
}

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
Bounded_face_selector::apply(Surface_mesh &mesh) const
{
    return select_bounded_faces(mesh, *volume, partial);
}

// ## Relative Selections

// This category includes selections that are derived from the
// contraction or expansion of other selections.  Expansion augments a
// selection with elements that are adjacent to an element in the
// input selection.  Contraction performs the opposite operation.

template<typename T, typename U>
inline auto expand_or_contract_selection(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const U &selector, int steps)
{
    auto v = selector.apply(mesh, annotations);
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

std::vector<Polyhedron::Vertex_handle>
Relative_vertex_selector::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return expand_or_contract_selection(mesh, annotations, *selector, steps);
}

std::vector<Surface_mesh::Vertex_index>
Relative_vertex_selector::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return expand_or_contract_selection(mesh, annotations, *selector, steps);
}

std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Relative_edge_selector::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return expand_or_contract_selection(mesh, annotations, *selector, steps);
}

std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Relative_edge_selector::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return expand_or_contract_selection(mesh, annotations, *selector, steps);
}

std::vector<Polyhedron::Facet_handle>
Relative_face_selector::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return expand_or_contract_selection(mesh, annotations, *selector, steps);
}

std::vector<Surface_mesh::Face_index>
Relative_face_selector::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return expand_or_contract_selection(mesh, annotations, *selector, steps);
}

// ## Converting Selections

// These selections select elements based on a selection for different
// kinds of elements.  For instance we might want to select all
// vertices in a given face selection, or all faces whose vertices are
// fully or partially contained in a give vertex selection.

// As such, there's one kind of selection (two when we also consider
// partial membership) for each combination of elements:

//   1. Vertices from edges

template<typename T>
auto vertices_from_edges(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const Edge_selector &selector)
{
    using V = typename boost::graph_traits<T>::vertex_descriptor;

    std::unordered_set<V> vertices;

    for (const auto &e: selector.apply(mesh, annotations)) {
        vertices.insert(CGAL::source(e, mesh));
        vertices.insert(CGAL::target(e, mesh));
    }

    return std::vector<V>(vertices.cbegin(), vertices.cend());
}

std::vector<Polyhedron::Vertex_handle>
Edge_to_vertex_selector::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return vertices_from_edges(mesh, annotations, *selector);
}

std::vector<Surface_mesh::Vertex_index>
Edge_to_vertex_selector::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return vertices_from_edges(mesh, annotations, *selector);
}

//   2. Vertices from faces

template<typename T>
auto vertices_from_faces(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const Face_selector &selector)
{
    using V = typename boost::graph_traits<T>::vertex_descriptor;

    std::unordered_set<V> vertices;

    for (const auto &f: selector.apply(mesh, annotations)) {
        for (const auto &v: CGAL::vertices_around_face(
                 CGAL::halfedge(f, mesh), mesh)) {
            vertices.insert(v);
        }
    }

    return std::vector<V>(vertices.cbegin(), vertices.cend());
}

std::vector<Polyhedron::Vertex_handle>
Face_to_vertex_selector::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return vertices_from_faces(mesh, annotations, *selector);
}

std::vector<Surface_mesh::Vertex_index>
Face_to_vertex_selector::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return vertices_from_faces(mesh, annotations, *selector);
}

//   3. Edges from vertices

template<typename T, typename V>
auto edges_from_vertices(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const std::unordered_set<V> &vertices, const bool partial)
{
    PARTIAL_MEMBERSHIP_TEST(
        typename boost::graph_traits<T>::edge_descriptor,
        CGAL::edges(mesh),
        std::initializer_list({CGAL::source(x, mesh), CGAL::target(x, mesh)}),
        vertices.find(y) != vertices.end());
}

template<typename T>
auto edges_from_vertices(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const Vertex_selector &selector, const bool partial)
{
    using V = typename boost::graph_traits<T>::vertex_descriptor;
    std::unordered_set<V> vertices;

    {
        const auto &w = selector.apply(mesh, annotations);

        vertices.reserve(w.size());
        vertices.insert(w.cbegin(), w.cend());
    }

    return edges_from_vertices(mesh, annotations, vertices, partial);
}

std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Vertex_to_edge_selector::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return edges_from_vertices(mesh, annotations, *selector, partial);
}

std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Vertex_to_edge_selector::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return edges_from_vertices(mesh, annotations, *selector, partial);
}

//   4. Edges from faces

template<typename T>
auto edges_from_faces(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const Face_selector &selector, const bool partial)
{
    // Edges "partially in" a face, are those for which one of their
    // targets belongs to the face.

    if (partial) {
        using V = typename boost::graph_traits<T>::vertex_descriptor;

        std::unordered_set<V> vertices;

        for (const auto &f: selector.apply(mesh, annotations)) {
            for (const auto &v: CGAL::vertices_around_face(
                     CGAL::halfedge(f, mesh), mesh)) {
                vertices.insert(v);
            }
        }

        return edges_from_vertices(mesh, annotations, vertices, partial);
    } else {
        using E = typename boost::graph_traits<T>::edge_descriptor;
        std::unordered_set<E> edges;

        for (const auto &f: selector.apply(mesh, annotations)) {
            for (const auto &h: CGAL::halfedges_around_face(
                     CGAL::halfedge(f, mesh), mesh)) {
                edges.insert(E(h));
            }
        }

        return std::vector<E>(edges.cbegin(), edges.cend());
    }
}

std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Face_to_edge_selector::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return edges_from_faces(mesh, annotations, *selector, partial);
}

std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Face_to_edge_selector::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return edges_from_faces(mesh, annotations, *selector, partial);
}

//   5. Faces from vertices

template<typename T>
auto faces_from_vertices(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const Vertex_selector &selector, const bool partial)
{
    using V = typename boost::graph_traits<T>::vertex_descriptor;
    std::unordered_set<V> vertices;

    {
        const auto &w = selector.apply(mesh, annotations);

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
Vertex_to_face_selector::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return faces_from_vertices(mesh, annotations, *selector, partial);
}

std::vector<Surface_mesh::Face_index>
Vertex_to_face_selector::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return faces_from_vertices(mesh, annotations, *selector, partial);
}

//   6. Faces from edges

template<typename T>
auto faces_from_edges(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const Edge_selector &selector, const bool partial)
{
    using E = typename boost::graph_traits<T>::edge_descriptor;
    std::unordered_set<E> edges;

    {
        const auto &w = selector.apply(mesh, annotations);

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
Edge_to_face_selector::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return faces_from_edges(mesh, annotations, *selector, partial);
}

std::vector<Surface_mesh::Face_index>
Edge_to_face_selector::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return faces_from_edges(mesh, annotations, *selector, partial);
}

// ## Feature-Based Selections

// This selection returns edges whose incident faces have normals that
// form an angle equal to, or larger than the specified threshold.
// Parallel faces have zero angle, so specifying 0 will select all
// edges while specifying, for instance 90, will select edges between
// faces that are at right angles, or sharper still.

template<typename T>
static std::vector<typename boost::graph_traits<T>::edge_descriptor>
select_sharp_edges(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const FT &angle)
{
    using E = typename boost::graph_traits<T>::edge_descriptor;

    std::unordered_set<E> set;

    CGAL::Polygon_mesh_processing::detect_sharp_edges(
        mesh, angle, CGAL::Boolean_property_map(set));

    return std::vector(set.cbegin(), set.cend());
}

template<>
std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Sharp_edge_selector<FT>::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return select_sharp_edges(mesh, annotations, parameter);
}

template<>
std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Sharp_edge_selector<FT>::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return select_sharp_edges(mesh, annotations, parameter);
}

// Selecting sharp edges by angle can be cumbersome, as it requires
// calculation or trial and error to find the angle.  It would be more
// convenient to be able to select the angle based on prominent
// creases in the mesh geometry.  These translate to peaks in the
// histogram of dihedral angles in the mesh.

// The function below finds the nth largest such mode.

template<typename T>
static FT find_mode(
    T &mesh, std::unordered_map<std::string, std::string> &annotations,
    int mode)
{
    const auto map = CGAL::get(CGAL::vertex_point, mesh);

    // We start by calculating the dihedral angles of all edges,
    // storing them in `v`.

    const std::size_t n = CGAL::num_edges(mesh);
    std::vector<double> v;
    v.reserve(n);

    for (const auto &e: CGAL::edges(mesh)) {
        const auto h = CGAL::halfedge(e, mesh);

        v.push_back(
            180.0 - std::abs(
                CGAL::to_double(
                    CGAL::approximate_dihedral_angle(
                        boost::get(map, CGAL::source(h, mesh)),
                        boost::get(map, CGAL::target(h, mesh)),
                        boost::get(map, CGAL::target(CGAL::next(h, mesh), mesh)),
                        boost::get(map, CGAL::target(CGAL::next(CGAL::opposite(h, mesh), mesh), mesh))))));
    }

#if 0
    {
        std::ofstream f("angles");
        for (const auto &x: v) {
            f << x << std::endl;
        }
    }
#endif

    // Next we calculate a smoothed histogram of those angles, in `w`,
    // using Kernel Density Estimation with a Gaussian kernel.

    // Ideally, modes would be prominent and we wouldn't need much
    // resolution, but procedures such as isotropic remeshing for
    // instance, can introduce edges with relatively wide
    // distributions, which, for our current purposes, constitute
    // noise.

    // We therefore sample the histogram 16 times per degree, and set
    // the kernel's bandwidth accordingly^[Setting it equal to the
    // sampling interval should avoid aliasing.  Higher bandwidths may
    // be required for a smoother histogram.].

    const double h = 4.0 / 16.0;

    const int m = 180 * 16;
    std::vector<double> w;
    w.reserve(m);

    for (double x = 0.0; x < 180.0 ; x += 0.0625) {
        double f_x = 0.0;

        for (const auto x_i: v) {
            const double delta = x - x_i;

            // We truncate the Gaussian window a $5 \sigma$.

            if (std::fabs(delta) < 5.0 * h) {
                f_x += std::exp(-0.5 * delta * delta / h);
            }
        }

        w.push_back(f_x);
    }

#if 0
    {
        std::ofstream f("density");
        for (const auto &x: w) {
            f << x << std::endl;
        }
    }
#endif

    // The histogram will contain many local maxima, but we're only
    // interested in the most prominent ones.  We apply a technique
    // based on persistent homology.

    std::vector<std::ptrdiff_t> sorted(m);
    std::iota(sorted.begin(), sorted.end(), 0);

    std::sort(
        sorted.begin(), sorted.end(),
        [&](std::ptrdiff_t a, std::ptrdiff_t b) {
            return w[a] > w[b];
        });

    // We keep a list of peaks, recording for each the sample index
    // that corresponds to it `born`, it the furtherst peaks to the
    // left and right that have been merged into it so far, or the
    // sample of the saddle point at which it was merged to a taller
    // peak, `died`.

    struct peak {
        std::ptrdiff_t born, left, right, died;
    };

    std::vector<struct peak> peaks;
    std::vector<std::ptrdiff_t> peaks_map(m, -1);

    // We go through the samples in sorted order assigning each to a
    // peak.

    for (std::ptrdiff_t i: sorted) {
        // For each sample it may be the case that:

        std::ptrdiff_t l = i > 0 ? peaks_map[i - 1] : -1,
            r = i < m - 1 ? peaks_map[i + 1] : -1;

        //   1. no peak has been assigned either to its left, or
        //   right, in which case a new peak is born, or

        if (l == -1 && r == -1) {
            peaks.push_back({i, i, i, -1});
            peaks_map[i] = peaks.size() - 1;

            continue;
        }

        //   2. there's a taller peak on the left, to which this
        //   sample is merged, or

        if (l != -1 && r == -1) {
            peaks[l].right++;
            peaks_map[i] = l;

            continue;
        }

        //   3. the same, but on the right, or

        if (l == -1 && r != -1) {
            peaks[r].left--;
            peaks_map[i] = r;

            continue;
        }

        //   4. there are peaks on both ends.  In this case the higher
        // peak absorbs the lower one, which "dies".  The surviving
        // peak is updated accordingly.

        if (w[peaks[l].born] > w[peaks[r].born]) {
            peaks[r].died = i;
            peaks[l].right = peaks[r].right;
            peaks_map[peaks[l].right] = l;
            peaks_map[i] = l;
        } else {
            peaks[l].died = i;
            peaks[r].left = peaks[l].left;
            peaks_map[peaks[r].left] = r;
            peaks_map[i] = r;
        }
    }

    // The persistence of a peak is then the difference in height of
    // the peak and the saddle point where it "died".

    auto score = [&w](const struct peak &a) {
        return w[a.born] - (a.died == -1 ? 0 : w[a.died]);
    };

    // We now sort the peaks by descending persistence and keep only
    // those with a score above a given threshold.  This may require
    // some tweaking.

    std::sort(
        peaks.begin(), peaks.end(),
          [&](const struct peak &a, const struct peak &b) {
              return score(a) > score(b);
          });

    {
        auto it = peaks.begin();
        for (double s = score(*it++) / 100.0;
             it != peaks.end() && score(*it) > s;
             ++it);

        peaks.erase(it, peaks.end());
    }

    assert(!peaks.empty());

    // We want to select modes by descending angle, so we sort again
    // before we select the specified mode.

    std::sort(
        peaks.begin(), peaks.end(),
          [&](const struct peak &a, const struct peak &b) {
              return a.born > b.born;
          });

    for (std::size_t i = 0; i < peaks.size() ; i++) {
        std::ostringstream s;
        s << peaks[i].born / 16.0;
        annotations.insert({"mode-" + std::to_string(i + 1), s.str()});
    }

    const auto &p = peaks[
        std::clamp(
            mode - 1,
            0, static_cast<int>(peaks.size()) - 1)];

    // We can return the angle corresponding to the peak itself; we
    // would select half its edges if the peak has some width.

    std::ptrdiff_t i = 0;

    if (p.died != -1 && p.died < p.born) {
        // In most cases, we would expect the peaks to descend in
        // height as the angles grow larger^[A given mesh is likely to
        // have more edges in flatter regions than sharp creases.], so
        // that each peak "dies" to a taller peak at a lower angle.
        // We use the saddle point of its death in this case.  This
        // should result in selecting the edges of this peak and any
        // lower nearby peaks that were absorbed into it.

        i = p.died;
    } else {
        // If the peak was merged on its right, we find the closest
        // saddle point and use that.

        for (const auto &q: peaks) {
            if (q.died > i && q.died < p.born) {
                i = q.died;
            }
        }
    }

    {
        std::ostringstream s;
        s << i / 16.0;
        annotations.insert({"angle", s.str()});
    }

    return FT::ET(i, 16);
}

template<>
std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Sharp_edge_selector<int>::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return select_sharp_edges(
        mesh, annotations, find_mode(mesh, annotations, parameter));
}

template<>
std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Sharp_edge_selector<int>::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return select_sharp_edges(
        mesh, annotations, find_mode(mesh, annotations, parameter));
}

// Face patch indexes assigned by operations like
// `sharp_edges_segmentation`, or `connected_components` seem to be
// assigned aribtrarily, so that even "small" changes to the mesh
// (e.g. changing the curve tolerance, or the order of operations used
// to produce it) can change the nubmer of a given patch.

// This is inconvenient, so we reorder the numbering, in order to make
// it more stable.  The approach used below finnds the bounding box of
// the faces contained in a given patch and reorders the patches
// accordingly.  This should result in patch numbering that don't
// change, as long as the shape of the mesh doesn't change
// substantially.

template<typename T, typename F, typename S>
std::vector<S> sort_face_patches(
    const T &mesh, std::unordered_map<F, S> &map, std::size_t i_0, std::size_t n)
{
    std::vector<std::pair<std::array<FT, 6>, std::size_t>> u(n, {{}, i_0 + n});

    const auto point_map = CGAL::get(CGAL::vertex_point, mesh);

    for (const auto &[f, i]: map) {
        assert (i >= i_0 && i < i_0 + n);

        auto &t = u[i - i_0];
        auto &b = t.first;

        for (auto x: CGAL::vertices_around_face(CGAL::halfedge(f, mesh), mesh)) {
            const auto p = boost::get(point_map, x);

            if (t.second == i_0 + n) {
                t.second = i;

                b[0] = b[3] = p[0];
                b[1] = b[4] = p[1];
                b[2] = b[5] = p[2];

                continue;
            }

            for (std::size_t j = 0; j < 3; j++) {
                if (b[j] > p[j]) {
                    b[j] = p[j];
                }

                const size_t k = j + 3;
                if (b[k] < p[j]) {
                    b[k] = p[j];
                }
            }
        }
    }

    std::sort(u.begin(), u.end());

    std::vector<S> v;
    v.reserve(u.size());

    for (auto it = u.cbegin(); it != u.cend(); ++it) {
        v.push_back(it->second);
    }

    return v;
}

template std::vector<boost::graph_traits<Polyhedron>::faces_size_type>
sort_face_patches(
    const Polyhedron &mesh,
    std::unordered_map<boost::graph_traits<Polyhedron>::face_descriptor,
                       boost::graph_traits<Polyhedron>::faces_size_type> &map,
    std::size_t i_0, std::size_t n);

template std::vector<boost::graph_traits<Surface_mesh>::faces_size_type>
sort_face_patches(
    const Surface_mesh &mesh,
    std::unordered_map<boost::graph_traits<Surface_mesh>::face_descriptor,
                       boost::graph_traits<Surface_mesh>::faces_size_type> &map,
    std::size_t i_0, std::size_t n);

// The set of "sharp" edges described above defines a segmentation of
// the mesh into patches of faces lying between "sharp" edges.  This
// selection returns faces contained in one or more such patches.  The
// specified patches, must be positive integers.

template<typename T>
static std::vector<typename boost::graph_traits<T>::face_descriptor>
select_sharp_patch_faces(
    const T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const FT &angle, const std::vector<int> &patches)
{
    typedef typename boost::graph_traits<T>::edge_descriptor edge_descriptor;
    typedef typename boost::graph_traits<T>::face_descriptor face_descriptor;

    // First we run the sharp edge segmentation, noting the total
    // number of returned patches `n`.

    std::vector<face_descriptor> v;
    std::unordered_set<edge_descriptor> set;
    std::unordered_map<face_descriptor, std::size_t> map;

    std::size_t n =
        CGAL::Polygon_mesh_processing::sharp_edges_segmentation(
            mesh, angle,
            CGAL::Boolean_property_map(set),
            boost::associative_property_map<decltype(map)>(map));

    annotations.insert({"patches", std::to_string(n)});

    // We can now extract and return the faces of the selected
    // patches.

    const auto u = sort_face_patches(mesh, map, 1, n);
    for (const auto &[f, i]: map) {
        assert(i - 1 < n);

        if (std::find(patches.begin(), patches.end(), u[i - 1])
            != patches.end()) {
            v.push_back(f);
        }
    }

    v.shrink_to_fit();
    return v;
}

// This form of the sharp patch face selector determines the patches
// to be returned from the faces returned by a given selector.

template<typename T>
static std::vector<typename boost::graph_traits<T>::face_descriptor>
select_sharp_patch_faces(
    const T &mesh, std::unordered_map<std::string, std::string> &annotations,
    const FT &angle, const Face_selector &selector)
{
    typedef typename boost::graph_traits<T>::edge_descriptor edge_descriptor;
    typedef typename boost::graph_traits<T>::face_descriptor face_descriptor;

    // First we run the sharp edge segmentation, as before.

    std::vector<face_descriptor> v;
    std::unordered_set<edge_descriptor> set;
    std::unordered_map<face_descriptor, std::size_t> map;

    CGAL::Polygon_mesh_processing::sharp_edges_segmentation(
        mesh, angle,
        CGAL::Boolean_property_map(set),
        boost::associative_property_map<decltype(map)>(map));

    std::unordered_set<int> patches;

    // We then run the seeding selector and collect the set of all
    // patches its faces belong to.

    for (const auto &x: selector.apply(const_cast<T &>(mesh), annotations)) {
        patches.insert(map[x]);
    }

    // Finally, we extract and return the faces of the collected
    // patches.

    for (const auto &[f, i]: map) {
        if (patches.count(i) > 0) {
            v.push_back(f);
        }
    }

    v.shrink_to_fit();
    return v;
}

template<>
std::vector<Polyhedron::Facet_handle>
Sharp_patch_face_selector<FT>::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return (
        selector
        ? select_sharp_patch_faces(mesh, annotations, parameter, *selector)
        : select_sharp_patch_faces(mesh, annotations, parameter, patches));
}

template<>
std::vector<Surface_mesh::Face_index>
Sharp_patch_face_selector<FT>::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    return (
        selector
        ? select_sharp_patch_faces(mesh, annotations, parameter, *selector)
        : select_sharp_patch_faces(mesh, annotations, parameter, patches));
}

template<>
std::vector<Polyhedron::Facet_handle>
Sharp_patch_face_selector<int>::apply(
    Polyhedron &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    const auto theta = find_mode(mesh, annotations, parameter);

    return (
        selector
        ? select_sharp_patch_faces(mesh, annotations, theta, *selector)
        : select_sharp_patch_faces(mesh, annotations, theta, patches));
}

template<>
std::vector<Surface_mesh::Face_index>
Sharp_patch_face_selector<int>::apply(
    Surface_mesh &mesh,
    std::unordered_map<std::string, std::string> &annotations) const
{
    const auto theta = find_mode(mesh, annotations, parameter);

    return (
        selector
        ? select_sharp_patch_faces(mesh, annotations, theta, *selector)
        : select_sharp_patch_faces(mesh, annotations, theta, patches));
}

// ## Selections by Intersection

// Here we select all edges or faces that intersect a given geometric
// entity, which can be a segment, a ray, a line, or a plane.  This is
// different to selecting edges that are partially contained in a
// bounding plane for instance, as in the latter case and edge will
// not be selected if the plane intersects it, but none of its
// vertices lie on the plane.^[It would have been more convenient to
// implement partial bounding volume tests as selecting all elements
// that intersect the given volume, but this would have been much
// harder to do.]

#include <CGAL/AABB_tree.h>
#include <CGAL/AABB_traits_3.h>
#include <CGAL/AABB_halfedge_graph_segment_primitive.h>
#include <CGAL/Polygon_mesh_processing/triangulate_hole.h>

// Face selection presents a complication.  Since CGAL's `AABB_tree`
// functionality performs intersections with triangles only, we'd need
// to triangulate the mesh.  This is disagreeable anyway, but in this
// case having a selector have the side-effect of triangulating a
// mesh, could introduce all sorts of complications.^[Consider for
// instance an operation that needs to make multiple selections.  If
// it first selects faces bounded by a volume, then faces intersected
// by a plane, the latter will triangulate the mesh potentially
// removing polygonal faces already selected by the bounded
// selection.]

template<typename T, typename Q>
std::vector<typename boost::graph_traits<T>::face_descriptor>
select_intersecting_faces(const T &mesh, const Q &query)
{
    using face_descriptor = typename boost::graph_traits<T>::face_descriptor;

    const auto map = CGAL::get(CGAL::vertex_point, mesh);

    // We therefore manually triangulate each mesh face, creating a
    // mapping from the (one or more) `Triangle_3` extracted from a
    // given face to its descriptor.

    std::vector<std::pair<Triangle_3, face_descriptor>> p;

    for (const auto &x: CGAL::faces(mesh)) {
        std::vector<Point_3> u;

        for (const auto &y: CGAL::vertices_around_face(
                 CGAL::halfedge(x, mesh), mesh)) {
            u.push_back(boost::get(map, y));
        }

        assert(u.size() >= 3);

        std::vector<CGAL::Triple<int, int, int>> w;
        CGAL::Polygon_mesh_processing::triangulate_hole_polyline(
            u, std::back_inserter(w));

        for (const auto& y: w) {
            p.emplace_back(
                Triangle_3(u[y.first], u[y.second], u[y.third]), x);
        }
    }

    // We then define a custom primitive that will allow `AABB_tree`
    // to perform intersection tests on these triangles and return the
    // descriptors of the intersected mesh faces.

    struct Primitive {
        typedef Kernel::Point_3 Point;
        typedef Triangle_3 Datum;
        typedef face_descriptor Id;

    private:
        Triangle_3 triangle;
        face_descriptor descriptor;

    public:
        Primitive() = default;
        Primitive(typename decltype(p)::const_iterator it):
        triangle(it->first), descriptor(it->second) {}

        const Id &id() const {
            return descriptor;
        }

        Datum datum() const {
            return triangle;
        }

        Point reference_point() const {
            return triangle.vertex(0);
        }
    };

    // We can now create the tree and perform the intersection test
    // collecting all selected face descriptors in `v`.

    std::vector<face_descriptor> v;

    CGAL::AABB_tree<CGAL::AABB_traits_3<Kernel, Primitive>>
        tree(p.begin(), p.end());

    std::visit([&tree, &v](auto &&x) {
        tree.all_intersected_primitives(x, std::back_inserter(v));
    }, query);

    // Since more than one triangle can select the same mesh face,
    // some faces may have been returned more than one times, so we
    // need to deduplicate `v`.

    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    v.shrink_to_fit();

    return v;
}

// Edges are thankfully straightforward, as intersection is performed
// on the edge segments, so polygonal faces make no difference.

template<typename T, typename Q>
std::vector<typename boost::graph_traits<T>::edge_descriptor>
select_intersecting_edges(const T &mesh, const Q &query)
{
    const auto edges = CGAL::edges(mesh);
    CGAL::AABB_tree<
        CGAL::AABB_traits_3<
            Kernel,
            CGAL::AABB_halfedge_graph_segment_primitive<T>>> tree(
                edges.first, edges.second, mesh);

    std::vector<typename boost::graph_traits<T>::edge_descriptor> v;

    std::visit([&tree, &v](auto &&x) {
        tree.all_intersected_primitives(x, std::back_inserter(v));
    }, query);

    v.shrink_to_fit();
    return v;
}

std::vector<Polyhedron::Facet_handle>
Intersecting_face_selector::apply(Polyhedron &mesh) const
{
    return select_intersecting_faces(mesh, query);
}

std::vector<Surface_mesh::Face_index>
Intersecting_face_selector::apply(Surface_mesh &mesh) const
{
    return select_intersecting_faces(mesh, query);
}

std::vector<boost::graph_traits<Polyhedron>::edge_descriptor>
Intersecting_edge_selector::apply(Polyhedron &mesh) const
{
    return select_intersecting_edges(mesh, query);
}

std::vector<boost::graph_traits<Surface_mesh>::edge_descriptor>
Intersecting_edge_selector::apply(Surface_mesh &mesh) const
{
    return select_intersecting_edges(mesh, query);
}

// ## Boolean Set Operations

// Selections can also be produced from boolean operations of one or
// more existing selections (of the same element type).  The
// implementation consists in applying the respective set operation
// (via `std::set_*`, after sortng) on the sets of elements produced
// by the input selections.

#define DEFINE_SET_OPERATION(OP, WHAT, HANDLE, INDEX)                   \
template<typename T, typename U>                                        \
inline auto WHAT ##_selection_## OP(                                    \
    T &mesh, std::unordered_map<std::string, std::string> &annotations, \
    const std::vector<U> &selectors)                                    \
{                                                                       \
    using V = typename boost::graph_traits<T>::WHAT ##_descriptor;      \
                                                                        \
    bool p = true;                                                      \
    std::vector<V> a, b, *q = &a, *r = &b;                              \
                                                                        \
    for (const U &x: selectors) {                                       \
        std::vector<V> v = x->apply(mesh, annotations);                 \
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
Set_## OP ##_## WHAT ##_selector::apply(                                \
    Polyhedron &mesh,                                                   \
    std::unordered_map<std::string, std::string> &annotations) const    \
{                                                                       \
    return WHAT ##_selection_## OP(mesh, annotations, selectors);       \
}                                                                       \
                                                                        \
std::vector<INDEX>                                                      \
Set_## OP ##_## WHAT ##_selector::apply(                                \
    Surface_mesh &mesh,                                                 \
    std::unordered_map<std::string, std::string> &annotations) const    \
{                                                                       \
    return WHAT ##_selection_## OP(mesh, annotations, selectors);       \
}

DEFINE_SET_OPERATION(union, face,
                     Polyhedron::Facet_handle, Surface_mesh::Face_index)
DEFINE_SET_OPERATION(difference, face,
                     Polyhedron::Facet_handle, Surface_mesh::Face_index)
DEFINE_SET_OPERATION(intersection, face,
                     Polyhedron::Facet_handle, Surface_mesh::Face_index)

DEFINE_SET_OPERATION(union, vertex,
                     Polyhedron::Vertex_handle, Surface_mesh::Vertex_index)
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

// The complement of a selection returns all elements in the mesh
// except those in the input selection.

#define DEFINE_COMPLEMENT_OPERATION(WHAT, HANDLE, INDEX, RANGE)         \
template<typename T, typename U>                                        \
inline auto WHAT ##_selection_complement(                               \
    T &mesh, std::unordered_map<std::string, std::string> &annotations, \
    const U &selector)                                                  \
{                                                                       \
    using V = typename boost::graph_traits<T>::WHAT ##_descriptor;      \
                                                                        \
    std::vector<V> u;                                                   \
                                                                        \
    const auto &x = CGAL::RANGE(mesh);                                  \
    u.insert(u.begin(), x.begin(), x.end());                            \
                                                                        \
    std::vector<V> v = selector->apply(mesh, annotations);              \
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
Set_complement_## WHAT ##_selector::apply(                              \
    Polyhedron &mesh,                                                   \
    std::unordered_map<std::string, std::string> &annotations) const    \
{                                                                       \
    return WHAT ##_selection_complement(mesh, annotations, selector);   \
}                                                                       \
                                                                        \
std::vector<INDEX>                                                      \
Set_complement_## WHAT ##_selector::apply(                              \
    Surface_mesh &mesh,                                                 \
    std::unordered_map<std::string, std::string> &annotations) const    \
{                                                                       \
    return WHAT ##_selection_complement(mesh, annotations, selector);   \
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
