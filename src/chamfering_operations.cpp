// Copyright 2026 Dimitris Papavasiliou

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

#include <CGAL/boost/graph/generators.h>
#include <CGAL/boost/graph/convert_nef_polyhedron_to_polygon_mesh.h>
#include <CGAL/Polygon_mesh_processing/triangulate_faces.h>
#include <CGAL/Polygon_mesh_processing/corefinement.h>
#include <CGAL/Polygon_mesh_processing/polygon_mesh_to_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/repair_polygon_soup.h>
#include <CGAL/Polygon_mesh_processing/remesh_planar_patches.h>

#include "kernel.h"
#include "evaluation.h"
#include "chamfering_operations.h"

// Document: program

// # Chamfering and Filleting Operations

// Given a mesh and a selection of faces, these operations chamfer or
// fillet these edges.  They proceed by generating geometry, which
// when added (for inner chamfers or fillets), or subtracted (for
// outer chamfers or fillets) from the mesh, will produce the desired
// result.

// Since both chamfers and fillets are handled uniformly below, we use
// the term "chamfer" to refer to both unless otherwise stated.

// We'll need to calculate face normals. For triangles and quads,
// which are always convex, this is simple to do by taking a cross
// product.  Larger faces may be non-convex, so we use Newell's
// method, implemented below.

template<typename T>
static Vector_3 calculate_normal(
    const typename boost::graph_traits<T>::halfedge_descriptor &h,
    const T &mesh)
{
    const auto map = CGAL::get(CGAL::vertex_point, mesh);
    FT x = 0, y = 0, z = 0;

    for (auto g = h; ; ) {
        const auto p = boost::get(map, CGAL::source(g, mesh));
        const auto q = boost::get(map, CGAL::target(g, mesh));

        x += (p[1] - q[1]) * (p[2] + q[2]);
        y += (p[2] - q[2]) * (p[0] + q[0]);
        z += (p[0] - q[0]) * (p[1] + q[1]);

        if ((g = CGAL::next(g, mesh)) == h) {
            break;
        }
    }

    return Vector_3(x, y, z);
}

// ## Chamfer Segments

// Below is a diagram of the basic chamfer geometry for the halfedge
// $h$, here drawn incident to two faces at right angles^[Of course
// the faces can meet at any angle, as long as they are not coplanar],
// drawn with dashed lines.

// The geometry is formed by first calculating a set of vectors
// $\vec{t}$ and $\vec{u}$, tangent to the faces and oriented towards
// their interior (ref: Inner and Outer Chamfers).  Loosely speaking,
// the edge is translated along these vectors, not necessarily by the
// same distance, forming a wedge made up of up to 5 faces^[This
// depends on whether one or both ends are capped], shown below with
// their incident halfedges.

// ```geometry
// \tkzDefPoint(0,0){p}
// \tkzDefPoint(3,0){a}
// \tkzDefPoint(0,-2){c}

// \tkzDefPoint(5,-2.5){q}
// \tkzDefPointsBy[translation=from p to q](a,c){b,d}

// \foreach \x [count=\i] in {p,q,a,c,b,d} {
//   \foreach \y [count=\j] in {p,q,a,c,b,d} {
//     \ifnum\j>\i
//       \tkzDefMidPoint(\x,\y) \tkzGetPoint{\x\y};
//     \fi
//   }
// }

// \tkzDefShiftPoint[pq](1,0){t}
// \tkzDefShiftPoint[pq](0,-1){u}

// \tkzDrawPoints(p,a,c,q,b,d)
// \tkzLabelPoints[above](p,q)
// \tkzLabelPoints(a,c,b,d)

// \tkzDrawLine[dashed,gray,add=0.25 and 0.5](p,q)
// \tkzDrawLines[dashed,gray,add=0 and 0.5](p,a p,c q,b q,d)

// \tkzDrawSegments[-Triangle](pq,t pq,u)
// \tkzDrawSegments[red,thick,-Triangle](p,pq q,qb b,ab a,pa)
// \tkzDrawSegments[teal,thick,-Triangle](a,ab b,bd d,cd c,ac)
// \tkzDrawSegments[blue,thick,-Triangle](c,cd d,qd q,pq p,pc)
// \tkzDrawSegments[violet,thick,-Triangle](p,pa a,ac c,pc)
// \tkzDrawSegments[violet,thick,-Triangle](q,qd d,bd b,qb)

// \tkzLabelSegment[above](p,pq){$s$}
// \tkzLabelSegment[below](pq,t){$\vec{t}$}
// \tkzLabelSegment[left](pq,u){$\vec{u}$}
// \tkzLabelSegment[above](p,q){$h$}
// ```

template<typename T>
static typename boost::graph_traits<T>::halfedge_descriptor
make_chamfer_segment(
    const T &mesh,
    const std::tuple<
       typename boost::graph_traits<T>::halfedge_descriptor,
       Vector_3, Vector_3> &edge,
    const FT &l, const FT &m,

    T &chamfer)
{
    // The vectors `t` and `u` have already been calculated and we can
    // get `p` and `q` from the edge.  We need to calculate the rest.

    const auto [h, t, u] = edge;

    Point_3 p, q;

    {
        const auto map = CGAL::get(CGAL::vertex_point, mesh);

        p = boost::get(map, CGAL::source(h, mesh));
        q = boost::get(map, CGAL::target(h, mesh));
    }

    assert(CGAL::coplanar(q + t * l, p + t * l, p + u * l, q + u * l));

    // We start with one quad, lying on the incident face, marked with
    // red halfedges above, which implicitly introduces vertices `a`
    // and `b`.  The order of the vertices is chosen, so that `s`
    // below is the halfedge from $p$ to $q$.  Note that unlike `h`
    // this edge belongs to the chamfer mesh).

    const auto s = CGAL::make_quad(q, q + t * l, p + t * l, p, chamfer);
    const auto map = CGAL::get(CGAL::vertex_point, chamfer);

    // Next we turn to the oblique face $abdc$, drawn with green
    // halfedges above, adding the last two vertices, `c` and `d`.

    auto c = CGAL::add_vertex(chamfer), d = CGAL::add_vertex(chamfer);

    boost::put(map, c, p + u * m);
    boost::put(map, d, q + u * m);

    // We can now add the oblique face and the third face $cdqp$.

    CGAL::Euler::add_face(
        std::initializer_list{
            CGAL::source(CGAL::prev(s, chamfer), chamfer),
            CGAL::target(CGAL::next(s, chamfer), chamfer), d, c}, chamfer);

    CGAL::Euler::add_face(
        std::initializer_list{
            CGAL::target(s, chamfer), CGAL::source(s, chamfer), c, d}, chamfer);

#if 0
        {
            static std::size_t i;
            CGAL::IO::write_OFF(
                "chamfer_" + std::to_string(i++) + ".off", chamfer);
        }
#endif

    return s;
}

// ## Fillet Segments

// The basic fillet segment is derived from the symmetric chamfer
// segment, by applying recursive subdivision of its oblique face to
// form the faces that make up its rounded portion.

// The diagram below shows the fillet geometry, as seen from the side.

template<typename T>
static typename boost::graph_traits<T>::halfedge_descriptor
make_fillet_segment(
    const T &mesh,
    const std::tuple<
       typename boost::graph_traits<T>::halfedge_descriptor,
       Vector_3,
       Vector_3> &edge,
    const FT &r, const FT &tau,

    T &fillet)
{
    // ```geometry
    // \tkzDefPoint(0,0){O}
    // \tkzDefPoint(3,3){c}
    // \tkzDefPointBy[rotation=center O angle 75](c) \tkzGetPoint{a}

    // \tkzDefLine[tangent at=a](O) \tkzGetPoint{x}
    // \tkzDefLine[tangent at=c](O) \tkzGetPoint{y}
    // \tkzInterLL(x,a)(y,c) \tkzGetPoint{p}

    // \tkzDefPointWith[linear normed](p,c)\tkzGetPoint{t}
    // \tkzDefPointWith[linear normed](p,a)\tkzGetPoint{u}

    // \tkzDefMidPoint(a,c) \tkzGetPoint{m};
    // \tkzInterLC[near](m,p)(O,c) \tkzGetFirstPoint{m'}

    // \tkzDrawPoints(p,a,c,m,O)

    // \tkzDrawLines[dashed,gray,add=0 and 0.5](p,a p,c)
    // \tkzDrawLines[dashed,gray,add=0.1 and 0.1](O,p O,a O,c a,c)

    // \tkzDrawSegments[-Triangle](p,t p,u m,m')
    // \tkzDrawSegments(p,a p,c)

    // \tkzDrawArc(O,c)(a)

    // \tkzMarkRightAngles(p,a,O p,c,O)
    // \tkzMarkAngle[size=0.6](a,p,c)
    // \tkzMarkAngle[size=0.75](O,p,c)
    // \tkzMarkAngle[size=0.75](c,O,p)

    // \tkzLabelPoints[above](p,a,c)
    // \tkzLabelPoints[left](m)
    // \tkzLabelSegment[left](O,a){$r$}
    // \tkzLabelSegment[right](O,c){$r$}
    // \tkzLabelSegment[left](O,m){$a$}
    // \tkzLabelSegments[above](p,a p,c){$l$}
    // \tkzLabelSegment[above](p,t){$\vec{t}$}
    // \tkzLabelSegment[above](p,u){$\vec{u}$}
    // \tkzLabelSegment[right](m,m'){$\delta\cdot\vec{v}$}
    // \tkzLabelAngle[pos=0.3](a,p,c){$\theta$}
    // \tkzLabelAngle(O,p,c){$\phi$}
    // \tkzLabelAngle(c,O,p){$\psi$}
    // ```

    // We first need to find the appropriate side length for the
    // chamfer segment, so that the oblique edge will be rounded with
    // radius $r$.  We have:

    // ```displaymath
    // l = \frac{r}{\tan\phi} = r \cot\phi = r\cot\frac{\theta}{2} =
    // sgn(\sin\theta)\sqrt{\frac{1 + \cos\theta}{1 - \cos\theta}}
    // ```

    // We're looking for a posiitve length $l$ and $\cos\theta =
    // \vec{t}\cdot\vec{u}$, so:

    // ```displaymath
    // l = \sqrt{\frac{1 + \vec{t}\cdot\vec{u}}{1 - \vec{t}\cdot\vec{u}}}
    // ```

    const FT tu = std::get<1>(edge) * std::get<2>(edge);
    const FT l = sqrt(CGAL::to_double((1 + tu) / (1 - tu))) * r;
    const auto h = make_chamfer_segment(mesh, edge, l, l, fillet);

    const auto map = CGAL::get(CGAL::vertex_point, fillet);
    const auto p = boost::get(map, CGAL::source(h, fillet));
    const auto q = boost::get(map, CGAL::target(h, fillet));

    auto split = [&](const auto &x, const double cospsi, auto &&split) -> void {
        // We need to subdivide the oblique face, marked with teal
        // colored half edges in the diagram in ref: Chamfer Segments.
        // This function is initially given the halfedge `x` from $a$
        // to $b$ as input, along with the cosine of angle $\psi$.

        // We decide whether to subdivide further by comparing the
        // approximation error $\delta$ (see below) to the curve
        // tolerance.

        const double delta = CGAL::to_double(r) * (1 - cospsi);

        if (delta < tau) {
            return;
        }

        // Assuming we need to subdivide further, we get the halfedges
        // from $c$ to $a$, `ca` and from $b$ to $d$, `bd` and the
        // associated points.

        const auto ca = CGAL::prev(x, fillet), bd = CGAL::next(x, fillet);

        const auto c = boost::get(map, CGAL::source(ca, fillet));
        const auto a = boost::get(map, CGAL::target(ca, fillet));

        const auto b = boost::get(map, CGAL::source(bd, fillet));
        const auto d = boost::get(map, CGAL::target(bd, fillet));

        // We can now split the edges $ca$ and $bd$.

        const auto ca_prime = CGAL::Euler::split_edge(ca, fillet);
        const auto bd_prime = CGAL::Euler::split_edge(bd, fillet);

        // We want to move the newly introduced points to the midpoint
        // of the split segments, marked $m$ in the diagram, and
        // furthermore shift them at right angles to it, along the
        // vector $\vec{v}$, by a distance $\delta$ so that they lie
        // on the arc from $a$ to $c$.

        // We have $v = \vec{pq} \times \vec{ca}$, after
        // normalization, while $\delta$ is just the radius $r$ minus the
        // apothem $a = r\cos\psi$.

        auto v = CGAL::cross_product(q - p, a - c);
        v *= delta / sqrt(CGAL::to_double(v.squared_length()));

        boost::put(map, CGAL::target(ca_prime, fillet), CGAL::midpoint(c, a) + v);
        boost::put(map, CGAL::target(bd_prime, fillet), CGAL::midpoint(b, d) + v);

        // Splitting the edges above only introduced new vertices and
        // edges; we still need to introduce new faces.

        CGAL::Euler::split_face(ca_prime, bd_prime, fillet);

        // We now need to recurse on the two newly created faces, so
        // we'll need to pass the associated halfedges for each.  For
        // the first, this is just `x`; while for the other it's
        // `opposite(next(next(x)))`.

        // We'll also need to pass in $\cos\frac{\psi}{2} =
        // \sqrt{\frac{1 + \cos\psi}{2}}$

        assert(CGAL::next(x, fillet) == bd_prime);
        const double cospsi_2 = sqrt((1.0 + cospsi) / 2.0);

        const auto y = CGAL::opposite(CGAL::next(bd_prime, fillet), fillet);

        split(x, cospsi_2, split);
        split(y, cospsi_2, split);
    };

    split(
        CGAL::opposite(CGAL::next(CGAL::next(h, fillet), fillet), fillet),
        sqrt(CGAL::to_double((1 - tu) / 2)), split);

#if 0
        {
            static std::size_t i;
            CGAL::IO::write_OFF(
                "fillet_" + std::to_string(i++) + ".off", fillet);
        }
#endif

    return h;
}

// ## Forming Chamfer Strips and Loops

// We'll generally want to chamfer multiple consecutive edges, so
// we'll need to join the adjacent ends of consecutive segments.  The
// operation is easier explained with the figure below, where two
// consecutive chamfer segments are drawn meeting at right angles for
// simplicity.  In the general case, the segments may be fillets and
// they can meet at any angle, but the procedure is essentially the
// same.

// ```geometry
// \tkzDefPoint(0,0){p}
// \tkzDefPoint(3,0){a}
// \tkzDefPoint(0,-2){X}

// \tkzDefPoint(5,-2.5){q}
// \tkzDefPointsBy[translation=from p to q](a,X){b,Y}

// \tkzDefPointOnLine[pos=0.5](Y,X) \tkzGetPoint{c}
// \tkzDefPointOnLine[pos=2](Y,b) \tkzGetPoint{Z}
// \tkzDefPointsBy[translation=from Y to Z](q,c){r,d}

// \tkzInterLL(a,b)(c,d) \tkzGetPoint{z}

// \tkzDrawPoints(p,q,r,a,b,c,d,X,Y,Z)

// \tkzDrawPolygons[gray](p,a,X q,b,Y)
// \tkzDrawPolygons[gray,thick](q,Y,c r,d,Z)

// \tkzDrawSegment[-Triangle](a,b)
// \tkzDrawSegment[thick,-Triangle](c,d)
// \tkzDrawSegments[gray](X,Y)
// \tkzDrawSegments[gray,-Triangle](p,q)
// \tkzDrawSegments[gray,thick](Y,Z)
// \tkzDrawSegments[gray,thick,-Triangle](q,r)
// \tkzDrawSegments[dashed](q,z Y,z)

// \tkzDrawLines[dashed,gray,add=0.25 and 0.25](a,b c,d)
// \tkzDrawLines[dashed,gray,add=0.5 and 0.5](q,z)

// \tkzLabelPoints(q,a,b,c,d)
// \tkzLabelPoints[above](z)
// \tkzLabelPoints[left](p)
// \tkzLabelPoints[right](r)

// \tkzLabelSegment[pos=0.25](a,b){$s$}
// \tkzLabelSegment[pos=0.75](c,d){$t$}
// \tkzLabelSegment(p,q){$h$}
// \tkzLabelSegment(q,r){$g$}

// \tkzLabelLine[above,pos=-0.1](a,b){$l$}
// \tkzLabelLine[left,pos=1.1](c,d){$m$}
// \tkzLabelLine[left](q,z){$n$}
// ```

// In order to join the segments, we need to shift the targets of
// edges in the first segment and the source of edges in the second,
// to the intersection points of those edges.  In other words, we need
// to miter the segments.

// As we can see in the graph, we can skip the firs set of edges, `h`
// and `g`.  They're already lined up by definition, as they
// correspond to the consecutive edges of the polyhedron that are to
// be chamfered.

template<typename T>
static bool miter_segments(
    T &source, T &target,
    const typename boost::graph_traits<T>::halfedge_descriptor &h,
    const typename boost::graph_traits<T>::halfedge_descriptor &g)
{
    const auto map_s = CGAL::get(CGAL::vertex_point, source);
    const auto map_t = CGAL::get(CGAL::vertex_point, target);

    // We iterate, through all other pairs^[There are only two in a
    // chamfer segment, but there can be any number of them in a
    // fillet segment.] $(s,t)$ of matching edges from each segment
    // and for each pair, we shift the target of the first edge to the
    // point of intersection.

    // If we could be certain that we'd always be able to miter the
    // segments succesfully, we could apply the shifts immediately.
    // Regrettably though, depending on the geometry of the chamfererd
    // edges and the depth of the chamfer, we might not be able to
    // succesfully apply the simple process below and resort to
    // joining via corefinement.

    // We therefore defer application of the shifts, until we've
    // determined that all can be applied.

    std::vector<std::pair<Point_3 *, Point_3>> shifts;
    shifts.reserve(3);

    for (auto s = h, t = g;
         (s = CGAL::opposite(
             CGAL::next(CGAL::next(s, source), source), source)) != h &&
             (t = CGAL::opposite(
                 CGAL::next(CGAL::next(t, target), target), target)) != g; ) {
        const auto a = boost::get(map_s, CGAL::source(s, source));
        const auto b = boost::get(map_s, CGAL::target(s, source));
        const auto c = boost::get(map_t, CGAL::source(t, target));
        const auto d = boost::get(map_t, CGAL::target(t, target));

        // The endpoints may already be coincident, although this is
        // not very likely, even though in many cases we would expect
        // them be.  For instance, consider the case of chamfering the
        // top face of a cylinder.  All segments should naturally line
        // up on the side faces.  Nevertheless, the approximate
        // normalization of the displacement vectors `t` and `u` will
        // push them apart, with a distance on the order of the
        // machine epsilon.

        if (b == c) {
            continue;
        }

        // For each edge, we form the respective line and take their
        // intersection.

        const Line_3 l(a, b);
        const Line_3 m(c, d);

        // If the segements we're mitering are collinear, so will be
        // $l$ and $m$.  All edges should be lined up in such cases
        // though^[They *should* be in well-behaved cases, but won't
        // always be so.  For instance, the collinear edges may have
        // incident faces that are not coplanar.  This can come about,
        // for example, if the faces incident to the edges are not
        // consecutive faces,i.e. three or more faces fan out from the
        // shared endpoint of the edges.  Such cases, although not
        // necessarily rare --- they can easily come about when
        // remeshing geometry for instance --- won't yield good
        // results anyway, so we ignore them.], so we can leave point
        // `b` as is.  We still need to update `d` below as `b` will
        // be snapped to the quasi-coincident `c`.

        std::optional<std::variant<Point_3, Line_3>> x;

        if (CGAL::parallel(l, m)) {
            x = b;
        } else {
            x = CGAL::intersection(l, m);
        }

        // If the edges were the result of shifting the center edge
        // exactly by the chamfer length, the lines would always
        // intersect.  We're forced to shift by an approximate
        // distance though, so they will not always do so in practice.

        if (!x.has_value()) {
            // If they don't we:

            const auto p = boost::get(map_s, CGAL::source(h, source));
            const auto q = boost::get(map_t, CGAL::source(g, target));

            //   1. calculate the two planes of the oblique segment
            //   faces and take their intersection yielding a line,
            //   `n`, on which the intersection of `l` and `m` lies,
            //   then

            const auto y = CGAL::intersection(Plane_3(a, b, p), Plane_3(c, d, q));
            assert(y.has_value() && std::holds_alternative<Line_3>(y.value()));

            const auto n = std::get<Line_3>(y.value());

            //   2. intersect it with `l`, yielding an approximation
            //   to the exact point of intersection of `l` and `m`,
            //   that has the property of ensuring planarity of all
            //   faces in the segment `s`.

            x = CGAL::intersection(l, n);

            assert(x.has_value());
            assert(std::holds_alternative<Point_3>(x.value()));
        }

        // However we might have calculated the new point in `x`, we
        // can use it to update `b` provided it won't introduce
        // self-intersections in the resulting mesh.  This can happen
        // if the new point is to the left of $a$, or to the right of
        // $d$ in the diagram.

        const auto &b_prime = std::get<Point_3>(x.value());

        if ((b_prime - a) * (b - a) <= FT(0)
            || (b_prime - d) * (c - d) <= FT(0)) {
                return false;
        }

        shifts.push_back({&boost::get(map_s, CGAL::target(s, source)), b_prime});

        // When `join_loop` is called during joining, `c` will also be
        // shifted to `b_prime`, so we don't need to do it explicitly.
        // We do so anyway, because we may not be able to join in the
        // end (ref: chamfer loop edge case).

        shifts.push_back({&boost::get(map_t, CGAL::source(t, target)), b_prime});

        // As a consequence, if we leave `d` as is, we will lose
        // planarity in the faces of segment `t`.  Even worse, its
        // edges won't be parallel any more, which will lead to
        // problems when joining subsequent segments.

        shifts.push_back({
                &boost::get(map_t, CGAL::target(t, target)),
                d + (b_prime - m.projection(b_prime))});
    }

    // If we've come this far, we can proceed to apply all shifts.

    for (const auto &x: shifts) {
        *x.first = x.second;
    }

    return true;
}

// Successfully mitered segments can be joined and merged into a
// single mesh.

template<typename T>
static void join_segments(
    T &chamfer, const T &segment,
    const typename boost::graph_traits<T>::halfedge_descriptor &h,
    const typename boost::graph_traits<T>::halfedge_descriptor &g)
{
    CGAL::Euler::join_loop(
        CGAL::opposite(CGAL::next(h, chamfer), chamfer),
        CGAL::opposite(CGAL::prev(g, chamfer), chamfer), chamfer);
}

template<typename T>
static typename boost::graph_traits<T>::halfedge_descriptor merge_segments(
    T &chamfer, const T &segment,
    const typename boost::graph_traits<T>::halfedge_descriptor &h,
    const typename boost::graph_traits<T>::halfedge_descriptor &g)
{
    typename boost::graph_traits<T>::halfedge_descriptor g_prime;

    // We copy the faces of the segment into our chamfer mesh, noting
    // the new central halfedge corresponding to it.

    CGAL::copy_face_graph(
        segment, chamfer,
        CGAL::parameters::halfedge_to_halfedge_output_iterator(
            boost::make_function_output_iterator(
                [&g, &g_prime](const auto &p) {
                    if (g == p.first) {
                        g_prime = p.second;
                    }
                })));

    join_segments(chamfer, segment, h, g_prime);

    return g_prime;
}

// When we can't miter and join segments, or when forming open path
// strips, we need to cap their ends.

template<typename T>
static void cap_segments(
    T &source, T &target,
    const typename boost::graph_traits<T>::halfedge_descriptor &h,
    const typename boost::graph_traits<T>::halfedge_descriptor &g)
{
    CGAL::Euler::fill_hole(CGAL::opposite(CGAL::next(h, source), source), source);
    CGAL::Euler::fill_hole(CGAL::opposite(CGAL::prev(g, target), target), target);
}

// ## Inner and Outer Chamfers

// Depending on the relative orientation of the incident faces, a
// chamfer may be an "inner" chamfer, in which case the chamfer
// geometry needs to be added to the mesh, or "outer" where the
// geometry needs to be subtracted.

// Since we need to treat these cases slightly differently, we need to
// partition the given edges into sets of "inner" and "outer" edges.
// We'll need to compute normal and tangent vectors in the process
// which we'll need again in the following steps, as they are the same
// vectors as `t` and `u` in the preceding sections.  We therefore
// return maps from *halfedges* to the tangent vectors corresponding
// to their *incident* faces.

template<typename T, typename E>
static auto partition_edges(const T &mesh, const E &edges, CGAL::Sign orientation)
{
    std::unordered_map<
        typename boost::graph_traits<T>::halfedge_descriptor, Vector_3> mapped;

    for (const auto &e: edges) {
        const auto map = CGAL::get(CGAL::vertex_point, mesh);

        // Refer to the diagram in ref: Chamfer Segments.

        const auto h = CGAL::halfedge(e, mesh);
        const auto p = boost::get(map, CGAL::source(h, mesh));
        const auto q = boost::get(map, CGAL::target(h, mesh));

        const auto pq = q - p;

        // By convention, the halfedge vectors circulate counterclockwise
        // around the incident face.  The tangent vectors, given by $n
        // \times pq$ will therefore point along the face and inwards.

        Vector_3 t, u;

        // In the following, we'll need to distinguish between convex
        // and potentially non-convex faces.  We begin by counting the
        // vertices.

        {
            std::size_t k = 1;
            for (auto g = h; k < 5 && ((g = CGAL::next(g, mesh)) != h); k++);

            if (k < 5) {
                // The face is either a triangle or a quad, hence
                // necessarily convex.  We compute a normal vector $n$
                // by the cross product of two of its edges $pq$ and
                // $qr$.  The tangent vector $t$ is then the triple
                // vector product $(pq \times qr) \times pq = qr(pq
                // \cdot pq) - pq(qr \cdot pq)$.

                const auto qr = boost::get(
                    map, CGAL::target(CGAL::next(h, mesh), mesh)) - q;

                t = qr * (pq * pq) - pq * (qr * pq);
            } else {
                // If the face is of larger degree, it might be
                // non-convex, so we must calculate the normal by Newell's
                // method.

                t = CGAL::cross_product(calculate_normal(h, mesh), pq);
            }
        }

        // Now we do the same calculation for the opposite face.

        const auto h_prime = CGAL::opposite(h, mesh);

        {
            std::size_t k = 1;
            for (auto g = h_prime;
                 k < 5 && ((g = CGAL::next(g, mesh)) != h_prime);
                 k++);

            if (k < 5) {
                const auto pr = boost::get(
                    map, CGAL::target(
                        CGAL::next(h_prime, mesh), mesh)) - p;

                // The opposite halfedge points towards p now, but the
                // normal computation can still use `pq` since $(-pq
                // \times pr) \times -pq = (pq \times pr) \times pq$.

                u = pr * (pq * pq) - pq * (pr * pq);
            } else {
                // Here, we compute the normal with the correct edge $qp$,
                // so we have to compute the tangent vector as $n \times
                // qp = pq \times n$.

                u = CGAL::cross_product(pq, calculate_normal(h_prime, mesh));
            }
        }

        // Depending on the relative orientation of the edge and the
        // normal (or equivalently tangent) vectors of the incident
        // faces, we classify it as "inner", or "outer".

        const auto n = CGAL::orientation(pq, t, u);

        if (n != orientation) {
            continue;
        }

        // We need to normalize the tangent vectors so that we can
        // then scale them to the proper lengths.  Unfortunately this
        // cannot be done in rational arithmetic, at least not if we
        // want to retain the direction exactly, so we need to
        // approximate.  This will lead to all sorts of complications
        // in the following steps...

        t /= std::sqrt(CGAL::to_double(t.squared_length()));
        u /= std::sqrt(CGAL::to_double(u.squared_length()));

        // If this is an "inner" chamfer, we need to swap the tangent
        // vectors to produce a correctly oriented segment.

        if (n == CGAL::POSITIVE) {
            mapped.insert({h, u});
            mapped.insert({h_prime, t});
        } else {
            mapped.insert({h, t});
            mapped.insert({h_prime, u});
        }
    }

    return mapped;
}

// We can finally get to the implementation of the operations
// themselves.  Since most of the logic is common to both, chamfers
// and fillets are handled by different instantiations of the same
// template.

template<typename T, bool Fillet, bool Make_only>
void Chamfering_operation<T, Fillet, Make_only>::evaluate()
{
    using vertex_descriptor =
        typename boost::graph_traits<T>::vertex_descriptor;
    using halfedge_descriptor =
        typename boost::graph_traits<T>::halfedge_descriptor;

    const auto &mesh = *this->operand->get_value();

    // We start with partitioning the edges and selecting either the
    // inner or outer chamfers, as requested.

    std::unordered_map<halfedge_descriptor, Vector_3> edges;

    {
        const auto n =
            (mode == Chamfering_operation_mode::INNER)
            ? CGAL::POSITIVE
            : CGAL::NEGATIVE;

        if (edge_selector) {
            edges = partition_edges(
                mesh, edge_selector->apply(
                    const_cast<T&>(mesh), this->annotations), n);
        } else {
            edges = partition_edges(mesh, CGAL::edges(const_cast<T&>(mesh)), n);
        }
    }

    this->annotations.insert({"selected", std::to_string(edges.size())});

    assert(!this->polyhedron);
    this->polyhedron = std::make_shared<T>();

    if (edges.empty()) {
        if constexpr (!Make_only) {
            *this->polyhedron = mesh;
        }

        return;
    }

    // ## Assembling the Chamfer Geometry

    // We can only create chamfer geometry for open or closed strips
    // of edges, so we must decompose the selected edges into such
    // strips, or, in the parlance of graphs, into simple cycles and
    // paths.

    // ### Extracting Chamfer Edge Paths

    // We must be careful in how we go about it though: If we have two
    // (open) paths meet at a vertex of degree 2^[With respect to the
    // subgraph induced by the selected edges.], which means that only
    // those two open paths and no others will meet there, then we're
    // going to end up with a non-manifold and/or self-intersecting
    // result when we join them^[The chamfer segments will either meet
    // in a straight line, so that we have three coincident vertices
    // and edges and a coincident face, or they'll mee at an angle, so
    // that the result will have the end caps of the two segments
    // meeting at the single common vertex that was coincident with
    // the chamfered edge.].

    Task_worker worker;
    std::mutex mutex;
    std::condition_variable condition;

    std::list<T> parts;
    std::unordered_set<vertex_descriptor> extracted;
    std::size_t n = 0, m = 0;

    // We therefore proceed as follows, until all selected edges
    // have been chamfered:

    while (!edges.empty()) {
        // We first extract all vertex-disjoint^[Requiring that
        // the cycles be vertex-disjoint is not strictly
        // necessary, but seems to make a substantial difference
        // in the quality of the generated mesh and the chamfering
        // result.] cycles we can find using a simple recursive
        // DFS.

        std::unordered_set<vertex_descriptor> visited;
        std::list<
            std::tuple<halfedge_descriptor, Vector_3, Vector_3>> component;

        const auto null_vertex = boost::graph_traits<T>::null_vertex();

        auto visit = [&](
            const vertex_descriptor &u,
            const vertex_descriptor &v,
            auto &&visit) -> vertex_descriptor {

            // At each step, we arrive at vertex `v`, having come
            // from `u`.  We:

            //   1. mark it as visited,

            safely_assert(visited.insert(v).second);

            //   2. follow a selected edge to vertex `w` and,

            for (const auto &h: CGAL::halfedges_around_source(v, mesh)) {
                assert(CGAL::source(h, mesh) == v);

                if (edges.count(h) == 0) {
                    continue;
                }

                const auto w = CGAL::target(h, mesh);

                //   3. skipping over opposite half-edges and edges
                //   leading to extracted vertices,

                if (w == u || extracted.count(w) == 1) {
                    continue;
                }

                //   4. if we've arrived back at an already
                //   visited vertex (but not trivially following
                //   the opposite edge), we've found a cycle and
                //   start recording it returning the revisited
                //   vertex, so that we'll know when to stop on
                //   the way back,

#define PUSH(H, WHERE) {                                                \
                    auto n = edges.extract(H),                          \
                        m = edges.extract(CGAL::opposite(H, mesh));     \
                                                                        \
                    assert(!n.empty());                                 \
                    assert(!m.empty());                                 \
                    component.push_## WHERE({H, n.mapped(), m.mapped()}); \
                    extracted.insert(CGAL::source(H, mesh));            \
                }

                if (visited.count(w) == 1) {
                    PUSH(h, front);
                    return w;
                }

                //   5. otherwise we recurse and,

                const auto &r = visit(v, w, visit);

                //   6. if the recursion ended in finding a cycle,
                //   we accumulate it,

                if (r != null_vertex) {
                    PUSH(h, front);
                    return r == v ? null_vertex : r;
                }

                //   7. otherwise, we either haven't found one yet
                //   and the component vector is empty, or we've
                //   found one, so we can terminate the rest of
                //   the DFS.

                if (!component.empty()) {
                    break;
                }
            }

            return null_vertex;
        };

        // We can start anywhere, so we start at the first edge.
        // Note that it is not necessary to check that the
        // starting vertex hasn't already been extracted.  If it
        // has, no cycle will be able to close at it.

        visit(null_vertex, CGAL::source(edges.begin()->first, mesh), visit);

        // If we couldn't find a cycle, it means the connected
        // component to which our starting edge belonged doesn't
        // have any more cycles; it is a forest, i.e. a set of
        // disconnected trees of edges.  We can further decompose
        // these into paths by simply starting at any vertex and
        // following any two halfedges out of it.

        // Forming paths in this manner removes two degrees from
        // the selected vertex and all other vertices along the
        // path, except from the ends.  The formed paths will
        // therefore cross themselves at vertices of even degree
        // and only end at vertices from the same component that
        // are of odd degree, or at vertices that are part of a
        // cycle.  Since such cycle vertices started out with
        // degree 2 and got another 2 edges for any path that
        // crossed, but didn't end at them, a path ending there
        // will make make their degree odd.

        // We're therefore guaranteed that no path will end at a
        // vertex of degree 2.

        // We proceed, but only for our starting edge, as that's the
        // only that certainly belongs to the forest component.

        bool open = component.empty();

        if (open) {
            auto h = edges.begin()->first, g = h;
            PUSH(h, back);

            visited.clear();
            visited.insert(CGAL::source(h, mesh));
            visited.insert(CGAL::target(h, mesh));

            // We:

            //   1. follow the first available edge in the target
            //   direction and

          next:

            for (const auto &x:
                     CGAL::halfedges_around_source(
                         CGAL::target(h, mesh), mesh)) {
                if (x != CGAL::opposite(h, mesh)
                    && edges.count(x) == 1
                    && visited.insert(CGAL::target(x, mesh)).second) {
                    h = x;
                    PUSH(h, back);
                    goto next;
                }
            }

            //  2. the same for the source direction.

          prev:
            for (const auto &x:
                     CGAL::halfedges_around_target(
                         CGAL::source(g, mesh), mesh)) {
                if (x != CGAL::opposite(g, mesh)
                    && edges.count(x) == 1
                    && visited.insert(CGAL::source(x, mesh)).second) {
                    g = x;
                    PUSH(g, front);
                    goto prev;
                }
            }
        }

        // ### Generating Chamfer Geometry

        // We are now ready to form a strip out of the path or cycle
        // and add it to our mesh.  Mitering and joining segments is
        // cheap relative to assembly by corefinement, so we try to
        // make a single mesh out of every path.  Nevertheless, it's
        // not always possible to miter two segments (ref: Forming
        // Chamfer Strips and Loops), so we may need to break the path
        // up into multiple meshes.

        // In either case, we gather all meshes in `parts`.

        // Since each path can be processed independently of others,
        // we form each in a separate thread (if multi-threading is
        // enabled).

        worker.insert(
            [this, &mesh, &parts, component_ = std::move(component),
             open, &mutex, &condition]() {
                constexpr auto make =
                    Fillet ? make_fillet_segment<T> : make_chamfer_segment<T>;

                bool open_ = open;
                std::list<T> ends;

                assert(!component_.empty());

                // Since `edges` isn't empty, it contains at least one
                // edge.  We make a chamfer for it.

                // Below, `s` holds the edge we began with and `t`
                // holds the current (or, when we're done, the ending)
                // edge.

                halfedge_descriptor s, t;
                T &A = ends.emplace_front();

                s = t = make(
                    mesh, component_.front(), parameters[0], parameters[1], A);

                // Now for each of the following edges, we create a
                // segment and attempt to join it with the current end
                // of the strip.

                for (auto it = component_.cbegin(), p = true;
                     ++it != component_.cend(); ) {
                    // At each point, `ends` will contain:

                    //   1. the starting segment `A`, which we need to
                    //   hold on to, in order to cap off, or loop back
                    //   to at the end, as required and potentially,

                    //   2. the segment we're currently extending `B`,
                    //   which will initially be `A`, until we're
                    //   forced to cap it off, in which case `p` will
                    //   become false and finally, temporarily,

                    assert(ends.size() == (1 + static_cast<std::size_t>(!p)));

                    //   3. the segment we're adding for the current
                    //   edge `C`, which will either be merged into
                    //   `B`, or replace it.

                    auto it_B = std::prev(ends.cend());
                    T &B = ends.back(), &C = ends.emplace_back();
                    auto u = make(mesh, *it, parameters[0], parameters[1], C);

                    // Anchor: chamfer loop edge case

                    // There's one edge case: if we're making one long
                    // closed strip and discover at the end that we
                    // can't close the loop, because the ends don't
                    // miter, we're stuck with a self-intersecting
                    // mesh.

                    // Therefore, for closed loops we always miter the
                    // back to the front before merging the last
                    // segment `C` with `B`, but after mitering it, as
                    // this process will change `C` as well.

                    const bool q = miter_segments(B, C, t, u);

                    if (!open_
                        && std::next(it) == component_.cend()
                        && !miter_segments(C, A, u, s)) {
                        open_ = true;

                        // If they don't miter, we avoid merging
                        // if it would form one large strip.

                        if (p) {
                            goto cap;
                        }
                    }

                    // We use the flag `q` instead of just having the
                    // test in the `if` statement, because we want to
                    // miter `C` to `A` above unconditionally for the
                    // last segment.

                    if (q) {
                        t = merge_segments(B, C, t, u);
                        ends.pop_back();
                        continue;
                    }

                  cap:
                    cap_segments(B, C, t, u);

                    // If we got here, we can't extend `B` any more so
                    // we may as well ship it off, unless it's
                    // actually the first segment `A`.

                    if (ends.size() > 2) {
                        CGAL::Polygon_mesh_processing::triangulate_faces(B);

                        std::lock_guard<std::mutex> lock(mutex);
                        parts.splice(parts.cend(), ends, it_B, std::next(it_B));
                        condition.notify_all();
                    }

                    p = false;
                    t = u;
                }

                if (open_) {
                    // If we're forming an open path, we need to cap
                    // the open hole at each end.

                    cap_segments(ends.back(), A, t, s);
                } else {
                    // Otherwise, we need to join the target end of
                    // the final segment back to the source end of the
                    // first.  This join may leave the segment
                    // following `s` with non-planar faces^[See
                    // discussion about moving vertices `c` and `d` in
                    // `miter_segments`.], but it will be taken care
                    // of when the mesh is triangulated in
                    // post-processing.

                    if (ends.size() > 1) {
                        merge_segments(ends.back(), A, t, s);
                        ends.pop_front();
                    } else {
                        join_segments(ends.back(), A, t, s);
                    }
                }

                for (auto &X: ends) {
                    CGAL::Polygon_mesh_processing::triangulate_faces(X);
                }

                std::lock_guard<std::mutex> lock(mutex);
                parts.splice(parts.cend(), ends);
                condition.notify_all();
            });

        n += !open;
        m += open;

        // We can now proceed to extract the next path, starting with
        // the first remaining edge.  This might be in the same
        // connected component, where all cycles have been cleared, so
        // that looking for cycles is a waste of time, but in practice
        // this shouldn't matter much and it makes the algorithm
        // simpler.
    }
#undef PUSH

    this->annotations.insert({"cycles", std::to_string(n)});
    this->annotations.insert({"paths", std::to_string(m)});

    // ### Final Chamfer Geometry Assembly

    // We can now assembly the finale chamfer mesh by taking the union
    // of all parts via corefinement.  We merge every pair of meshes
    // in a separate thread if possible.

    // So we:

    while (true) {
        std::unique_lock<std::mutex> lock(mutex);

        //   1. wait for two parts to become available, if necessary,

        while (parts.size() < 2 && !worker.empty()) {
            condition.wait(lock);
        }

        if (parts.size() < 2) {
            break;
        }

        //   2. splice these off into a separate list to pass to the
        //   worker, which is necessary as the meshes aren't movalble
        //   and we don't want to copy them,

        decltype(parts) work;
        work.splice(
            work.cbegin(), parts,
            parts.cbegin(), std::next(std::next(parts.cbegin())));

        lock.unlock();

        //   3. then finally create a task to merge them and puth the
        //   result back on the parts list.

        worker.insert(
            [work_ = std::move(work), &parts, &mutex, &condition]() {
                auto work__ = std::move(work_);

#if 0
                {
                    static std::size_t i;
                    CGAL::IO::write_OFF(
                        "part_" + std::to_string(i++) + ".off", work__.front());
                    CGAL::IO::write_OFF(
                        "part_" + std::to_string(i++) + ".off", work__.back());
                }
#endif

                safely_assert(
                    CGAL::Polygon_mesh_processing::
                    corefine_and_compute_union(
                        work__.front(), work__.back(), work__.front()));

                work__.pop_back();

                std::lock_guard<std::mutex> lock(mutex);
                parts.splice(parts.cend(), work__);
                condition.notify_all();
            });
    }

    // The above will go on until there's only one part left: the
    // finaly result.

    assert(parts.size() == 1);

    // The result although correct, may contain many almost degenerate
    // triangles caused by exactly computing the boolean union of
    // intersecting segments which were produced, in part, with
    // inexact arithmetic.

    // These microscopic triangles can cause trouble down the
    // road^[For instance, near-coincident points might actually end
    // up coincident after serializing them to a file with
    // insufficient precision.  Even if we avoid that, we'll run into
    // similar issues after loading the geometry in single precision
    // in the Debugger.].

    // Attempting to fix the mesh at this point, either with
    // `remove_almost_degenerate_faces` or by converting it to a soup
    // and merging almost coincident vertices can cause problems when
    // we combine it with the operand below.  We therefore restrict
    // ourselves to remeshing planar patches with a miniscule fudge
    // factor at this point call `remove_almost_degenerate_faces` on
    // the final result below.

    CGAL::Polygon_mesh_processing::remesh_planar_patches(
        parts.front(), *this->polyhedron,
        CGAL::parameters::cosine_of_maximum_angle(1.0 - 1e-12));

    // We either export the chamfer geometry itself if `Make_only` is
    // set, or combine the chamfer geometry with the operand.

    if constexpr (!Make_only) {
        T P(mesh);
        CGAL::Polygon_mesh_processing::triangulate_faces(P);

        if (mode == Chamfering_operation_mode::INNER) {
            safely_assert(
                CGAL::Polygon_mesh_processing::corefine_and_compute_union(
                    P, *this->polyhedron, *this->polyhedron));
        } else {
            safely_assert(
                CGAL::Polygon_mesh_processing::corefine_and_compute_difference(
                    P, *this->polyhedron, *this->polyhedron));
        }
    }

    CGAL::Polygon_mesh_processing::remove_almost_degenerate_faces(
        *this->polyhedron,
        CGAL::parameters::cap_threshold(-1.0 + 1e-6).needle_threshold(1e6));
}

template void Chamfering_operation<Polyhedron, false, false>::evaluate();
template void Chamfering_operation<Surface_mesh, false, false>::evaluate();
template void Chamfering_operation<Polyhedron, false, true>::evaluate();
template void Chamfering_operation<Surface_mesh, false, true>::evaluate();

template void Chamfering_operation<Polyhedron, true, false>::evaluate();
template void Chamfering_operation<Surface_mesh, true, false>::evaluate();
template void Chamfering_operation<Polyhedron, true, true>::evaluate();
template void Chamfering_operation<Surface_mesh, true, true>::evaluate();
