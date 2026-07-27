// Copyright 2022, 2026 Dimitris Papavasiliou

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

#include "assertions.h"
#include "compressed_stream.h"
#include "options.h"
#include "polygon_operations.h"
#include "circle_polygon_types.h"
#include "conic_polygon_types.h"

// Document: program

// # Generic Polygon Operations

// These operations apply to polygons of all types and are therfore
// implemented as class templates.

// ## Storing Polygons

// The function template below serializes a polygon or circle polygon
// to an output stream.  The geometry is stored using exact
// arithmetic, so that an exact copy can be restored using the
// `load_polygon` function below.

template<typename T>
static void store_polygon(std::ostream &s, const T &P)
{
    // We store the number of vertices and edges, followed by the
    // edges, one per line.

    s << P.size() << '\n';

    if constexpr (std::is_same_v<T, Polygon>) {
        for (auto v = P.vertices_begin(); v != P.vertices_end(); v++) {
            // For plain segment polygons we just store the vertices
            // in order.

            s << v->x().exact() << " " << v->y().exact() << '\n';
        }
    } else {
        for (auto c = P.curves_begin(); c != P.curves_end(); c++) {
            // For circle polygons, we store the edges as "source_x
            // source_y target_x target_y curve".

            if constexpr (std::is_same_v<T, Circle_polygon>) {
#define STORE_SQRT_EXTENSION(X) {                       \
                    const auto &x = X;                  \
                                                        \
                    s << x.a0().exact() << ' '          \
                      << x.a1().exact() << ' '          \
                      << x.root().exact() << ' ';       \
                }

                STORE_SQRT_EXTENSION(c->source().x());
                STORE_SQRT_EXTENSION(c->source().y());
                STORE_SQRT_EXTENSION(c->target().x());
                STORE_SQRT_EXTENSION(c->target().y());

#undef STORE_SQRT_EXTENSION

                // The curve is stored as a letter code (`L`, or `C`)
                // plus the supporting curve.

                if (c->is_linear()) {
                    const auto &L = c->supporting_line();

                    s << "L " << L.a().exact()
                      << ' ' << L.b().exact()
                      << ' ' << L.c().exact();
                } else {
                    const auto &C = c->supporting_circle();

                    s << "C " << C.center().x().exact()
                      << ' ' << C.center().y().exact()
                      << ' ' << C.squared_radius().exact()
                      << ' ' << static_cast<int>(C.orientation());
                }

                s << '\n';
            } else {
                assert_not_reached();
            }
        }
    }
}

// This is the `store` method, called when we determine that this
// polygon is actually worth storing to disk.

template<typename T>
bool Polygon_operation<T>::store() const
{
    // Conics aren't supported.  Serializing point coordinates, which
    // are expression trees of type `CORE::Expr`, exactly is not very
    // straightforward.

    if constexpr (std::is_same_v<T, Conic_polygon>) {
        return Operation::store();
    }

    assert(Flags::store_operations);
    assert(polygon);

    const T &S = *polygon;
    const int n = S.number_of_polygons_with_holes();
    std::vector<typename T::Polygon_with_holes_2> v;

    compressed_ofstream_wrapper f(Options::store_compression);
    f.open(store_path);

    if (!f.is_open()) {
        goto error;
    }

    // Store number of polygons in set.

    f << n << '\n';

    if (!f.good()) {
        goto error;
    }

    v.reserve(n);
    S.polygons_with_holes(std::back_inserter(v));

    // Store polygons in set.  Each polygon is stored as number of
    // holes, followed by outer boundary and hole polygons.

    for (const typename T::Polygon_with_holes_2 &H: v) {
        const int m = H.number_of_holes();

        f << m << '\n';

        if (!f.good()) {
            goto error;
        }

        store_polygon(f, H.outer_boundary());

        for (const typename T::Polygon_2 &P: H.holes()) {
            store_polygon(f, P);
        }
    }

    return f.good();

error:
    {
        std::ostringstream s;
        s << "Could not store polygon % to '" << store_path << "'";
        message(ERROR, s.str());
    }

    f.close();
    std::remove(store_path.c_str());

    return false;
}

template bool Polygon_operation<Polygon_set>::store() const;
template bool Polygon_operation<Circle_polygon_set>::store() const;
template bool Polygon_operation<Conic_polygon_set>::store() const;

// ## Loading Polygons

// This mirros the code in ref: Storing Polygons.

template<typename T>
static void load_polygon(std::istream &s, T &P)
{
    assert(!(std::is_same_v<T, Conic_polygon>));

    int n;
    s >> n;

    if (!s.good()) {
        return;
    }

    if constexpr (std::is_same_v<T, Polygon>) {
        for (int i = 0; i < n; i++) {
            FT x, y;

            s >> x;
            s >> y;

            if (!s.good()) {
                return;
            }

            P.push_back(typename T::Point_2(x, y));
        }
    } else {
        for (int i = 0; i < n; i++) {
            if constexpr (std::is_same_v<T, Circle_polygon>) {
                using C = Circle_segment_traits::Point_2::CoordNT;
                C x, y;

#define LOAD_SQRT_EXTENSION(X) {                \
                    C::NT a_0, a_1, r;          \
                                                \
                    s >> a_0 >> a_1 >> r;       \
                    X = C(a_0, a_1, r);         \
                }

                LOAD_SQRT_EXTENSION(x);
                LOAD_SQRT_EXTENSION(y);
                Circle_segment_traits::Point_2 A(x, y);

                LOAD_SQRT_EXTENSION(x);
                LOAD_SQRT_EXTENSION(y);
                Circle_segment_traits::Point_2 B(x, y);

#undef LOAD_SQRT_EXTENSION

                char c;
                s >> c;

                switch (c) {
                case 'L': {
                    FT a, b, c;

                    s >> a >> b >> c;

                    if (!s.good()) {
                        return;
                    }

                    P.push_back(
                        Circle_segment_traits::X_monotone_curve_2(
                            Line_2(a, b, c), A, B));

                    break;
                }

                case 'C': {
                    FT x, y, rr;
                    int j;

                    s >> x >> y >> rr >> j;

                    if (!s.good()) {
                        return;
                    }

                    const CGAL::Orientation o = static_cast<CGAL::Orientation>(j);

                    P.push_back(
                        Circle_segment_traits::X_monotone_curve_2(
                            Circle_2(Point_2(x, y), rr, o), A, B, o));

                    break;
                }

                default:
                    s.setstate(std::ios::failbit);
                    return;
                }
            } else {
                assert_not_reached();
            }
        }
    }
}

template<typename T>
bool Polygon_operation<T>::load()
{
    if constexpr (std::is_same_v<T, Conic_polygon>) {
        return Operation::load();
    }

    assert(Flags::load_operations);
    assert(!polygon);

    compressed_ifstream_wrapper f(Options::store_compression >= 0);
    f.open(store_path);

    if (!f.is_open()) {
        return false;
    }

    polygon = std::make_shared<T>();
    T &S = *polygon;
    int n;

    f >> n;

    if (!f.good()) {
        goto error;
    }

    for (int i = 0; i < n; i++) {
        int m;

        f >> m;

        if (!f.good()) {
            goto error;
        }

        typename T::Polygon_2 B;
        load_polygon(f, B);
        typename T::Polygon_with_holes_2 H(B);

        for (int j = 0; j < m; j++) {
            typename T::Polygon_2 P;
            load_polygon(f, P);
            H.add_hole(P);

            if (!f.good()) {
                goto error;
            }
        }

        S.insert(H);
    }

    return f.good();

error:
    {
        std::ostringstream s;
        s << "Could not load polygon % from '" << store_path << "'";
        message(ERROR, s.str());
    }

    return false;
}

template bool Polygon_operation<Polygon_set>::load();
template bool Polygon_operation<Circle_polygon_set>::load();
template bool Polygon_operation<Conic_polygon_set>::load();

// ## Polygon Complement

// This operation computes the complement of a polygon set.

#include <CGAL/Boolean_set_operations_2.h>

template<typename T>
void Polygon_complement_operation<T>::evaluate()
{
    assert(!this->polygon);

    this->polygon = std::make_shared<T>(*this->operand->get_value());
    this->polygon->complement();
}

template void Polygon_complement_operation<Polygon_set>::evaluate();
template void Polygon_complement_operation<Circle_polygon_set>::evaluate();
template void Polygon_complement_operation<Conic_polygon_set>::evaluate();

// ## Polygon Components

// Since polygons and their holes are ordered aribitrarily, we need to
// sort each polygon, to ensure a stable order when extracting
// components below.  We sort a set of polygons by their bounding
// boxes using the following function.

// As we'll need to sort entire polygons with holes, by their
// boundaries, as well as the holes within each one, we use the
// function `f` to extract the polygon from the to-be-sorted items.

template<typename T, typename U>
static void sort_polygon_components(std::vector<T> &v, U f)
{
    using V = std::remove_reference_t<decltype(f(std::declval<T &>()))>;

    // We first create a map from the polygons (their pointer to be
    // exact) to their bounding boxes.  Conic polygons need to be
    // handled separately when construcing bounding boxes, using a
    // functor, to which each of their arcs is fed in succession.

    std::unordered_map<V *, CGAL::Bbox_2> map;

    for (auto &x: v) {
        auto k = f(x);

        if constexpr (std::is_same_v<V, Conic_polygon_set::Polygon_2>) {
            const auto construct_bbox = Conic_traits().construct_bbox_2_object();
            CGAL::Bbox_2 b = construct_bbox(*(k.curves_begin()));

            for (auto it = ++k.curves_begin(); it != k.curves_end(); ++it) {
                b += construct_bbox(*it);
            }

            map[&k] = b;
        } else {
            map[&k] = k.bbox();
        }
    }

    // We now sort the given vector by looking up the map for each
    // item and comparing the resulting bounding boxes
    // lexicographically.

    std::sort(
        v.begin(), v.end(), [&map, &f](auto &x, auto &y) {
            auto k = f(x), l = f(y);
            const CGAL::Bbox_2 &a = map[&k], &b = map[&l];

            if (a.xmin() != b.xmin()) {
                return a.xmin() < b.xmin();
            }

            if (a.xmax() != b.xmax()) {
                return a.xmax() < b.xmax();
            }

            if (a.ymin() != b.ymin()) {
                return a.ymin() < b.ymin();
            }

            return a.ymax() < b.ymax();
        });
}

// This operation extracts components, which can be either boundaries
// or holes, from a polygon set.  This turns out to be more
// complicated to implement than one might expect.  Perhaps there's an
// easier way to do this.

template<typename T>
void Polygon_components_operation<T>::evaluate()
{
    assert(!this->polygon);

    auto &S = *this->operand->get_value();

    if (components.empty() || S.is_empty()) {
        return;
    }

    // We first extract the polygons with holes from the operand into
    // a vector of vectors, each having the boundary at index 0
    // followed by any holes.

    const int n = S.number_of_polygons_with_holes();
    std::vector<typename T::Polygon_with_holes_2> v;

    v.reserve(n);
    S.polygons_with_holes(std::back_inserter(v));

    std::vector<std::vector<typename T::Polygon_2>> u;
    u.reserve(n);

    for (const typename T::Polygon_with_holes_2 &P: v) {
        std::vector<typename T::Polygon_2> w;
        w.reserve(P.number_of_holes() + 1);
        w.push_back(P.outer_boundary());
        w.insert(w.end(), P.holes_begin(), P.holes_end());

        u.emplace_back(std::move(w));
        assert(w.empty());
    }

    // We now sort:

    //   1. the outer boundaries of each polygon with holes in the
    //   operand and

    sort_polygon_components(
        u, [](std::vector<typename T::Polygon_2> &x) {
            return x.front();
        });

    //   2. separately the holes.

    for (auto &w: u) {
        sort_polygon_components(
            w, [](typename T::Polygon_2 &x) {
                return x;
            });
    }

    // We're now ready to start extracting components.

    this->polygon = std::make_shared<T>();

    // This gets a bit tricky, as we need to juggle multiple vectors.
    // We have in turn:

    //   1. the index of the currently considered component as if the
    //   vector `u` were flattened,

    int i = 1;

    //   2. an iterator for the sorted polygons with holes,

    auto it_u = u.begin();

    //   3. an iterator for the selected components, which have
    //   already been sorted and finally

    auto it_c = components.begin();

    do {
        //   4. An iterator for the currently considered component as
        //   a polygon.

        auto it = it_u->begin();

        // This outer loop iterates through entire polygons with
        // holes.
        if (i++ == *it_c) {
            // If the polygon's outer boundary has been selected, we:

            //   1. make a new polygon with holes out of it,

            typename T::Polygon_with_holes_2 P(*it);

            if (++it_c == components.end()) {
                goto skip;
            }

            //   2.  go through its holes, adding any that are
            // selected and finally

            while (++it != it_u->end()) {
                if (i++ == *it_c) {
                    P.add_hole(*it);

                    if (++it_c == components.end()) {
                        goto skip;
                    }
                }
            }

            //   3. add it to the result.

          skip:
            this->polygon->insert(P);
        } else {
            // If the polygon's outer boundary hasn't been selected,
            // we just go through the holes, adding any that are
            // selected as separate polygons to the result, after
            // reorienting them.

            while (++it != it_u->end()) {
                if (i++ == *it_c) {
                    it->reverse_orientation();
                    this->polygon->insert(*it);

                    if (++it_c == components.end()) {
                        continue;
                    }
                }
            }
        }
    } while (++it_u != u.end() && it_c != components.end());
}

template void Polygon_components_operation<Polygon_set>::evaluate();
template void Polygon_components_operation<Circle_polygon_set>::evaluate();
template void Polygon_components_operation<Conic_polygon_set>::evaluate();
