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

#include "assertions.h"
#include "compressed_stream.h"
#include "options.h"
#include "polygon_operations.h"
#include "circle_polygon_types.h"
#include "conic_polygon_types.h"

// Store

template<typename T>
static void store_polygon(std::ostream &s, const T &P)
{
    // Store number of vertices/edges.

    s << P.size() << '\n';

    if constexpr (std::is_same_v<T, Polygon>) {
        // Plain segment; store vertices.

        for (auto v = P.vertices_begin(); v != P.vertices_end(); v++) {
            s << v->x().exact() << " " << v->y().exact() << '\n';
        }
    } else {
        // Circle-segment; store edges as (source, target, curve).

        for (auto c = P.curves_begin(); c != P.curves_end(); c++) {
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

template<typename T>
bool Polygon_operation<T>::store()
{
    // Conics aren't supported.  Storing point coordinates exactly is
    // not very straightforward.

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

template bool Polygon_operation<Polygon_set>::store();
template bool Polygon_operation<Circle_polygon_set>::store();
template bool Polygon_operation<Conic_polygon_set>::store();

// Load; see storing functions for commentary.

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

// Complement

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
