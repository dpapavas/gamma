#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>

#include "kernel.h"
#include "projection.h"
#include "transformation_types.h"
#include "transformations.h"
#include "circle_polygon_operations.h"

using Traits = Circle_segment_traits;

typedef Traits::Curve_2 Curve_2;
typedef Traits::X_monotone_curve_2 X_monotone_curve;

// Polygons consist of x-monotone curves so that it is, in general,
// necessary to subdivide curves into x-monotone parts.  When
// transforming polygons, previously x-monotone curves, are no longer
// necessarily so (unless the transformation is a simple translation),
// so that further subdivision is necessary.  In order to avoid
// unnecessary growth of polygon edges, we reassemble the subdivided
// curves before subdividing anew.

static void subdivide_curve(Curve_2 c, Circle_polygon &P)
{
    static auto make_monotone = Traits().make_x_monotone_2_object();
    std::vector<CGAL::Object> v;

    make_monotone(c, std::back_inserter(v));

    for (auto x: v) {
        P.push_back (CGAL::object_cast<X_monotone_curve>(x));
    }
}

static bool have_same_curve(const X_monotone_curve &a,
                            const X_monotone_curve &b)
{
    return ((a.is_linear() && b.is_linear() &&
             a.supporting_line() == b.supporting_line())
            || (a.is_circular() && b.is_circular() &&
                a.supporting_circle() == b.supporting_circle()));
}

static void reassemble_curves(const Circle_polygon &P, std::list<Curve_2> &l)
{
    Circle_polygon::Curve_const_iterator b = P.curves_begin();
    Circle_polygon::Curve_const_iterator e = P.curves_end();

    for (auto [c_0, T, c] =
             std::tuple(b, b->target(), std::next(b)); ;
         T = c->target(), c++) {

        // While the next curve is just an extension of the current,
        // update the target point.

        if (c != e
            && have_same_curve(*c, *c_0)
            && c->source() == T) {
            continue;
        }

        if (c_0->is_linear()) {
            l.push_back(Curve_2(c_0->supporting_line(), c_0->source(), T));
        } else {
            if (c == e) {
                if (c_0 == b) {
                    // Just one curve: a circle.

                    l.push_back(Curve_2(c_0->supporting_circle()));
                    return;
                } else if (have_same_curve(*c_0, *b) && T == b->source()) {
                    // We can merge with the beginning curve.

                    l.pop_front();
                    T = b->target();
                }
            }

            l.push_back(Curve_2(c_0->supporting_circle(), c_0->source(), T));
        }

        if (c == e) {
            break;
        }

        c_0 = c;
    }
}

void Circle_operation::evaluate()
{
    assert(!polygon);

    Circle_polygon P;

    subdivide_curve(
        Curve_2(Circle_2(Point_2(CGAL::ORIGIN), radius * radius)), P);

    polygon = std::make_shared<Circle_polygon_set>(P);
}

void Circular_segment_operation::evaluate()
{
    assert(!polygon);

    const Point_2 A(chord / -2, 0), B(chord / 2, 0), H(0, sagitta);
    Circle_polygon P;

    P.push_back(X_monotone_curve(A, B));
    subdivide_curve(Curve_2(B, H, A), P);

    polygon = std::make_shared<Circle_polygon_set>(P);
}

void Circular_sector_operation::evaluate()
{
    assert(!polygon);

    const Point_2 O(CGAL::ORIGIN), A(radius, 0);
    const Point_2 B = A.transform(basic_rotation(CGAL::to_double(angle)));

    Circle_polygon P;

    P.push_back(X_monotone_curve(B, O));
    P.push_back(X_monotone_curve(O, A));
    subdivide_curve(
        Curve_2(O, radius, CGAL::COUNTERCLOCKWISE,
                Traits::Point_2(A.x(), A.y()),
                Traits::Point_2(B.x(), B.y())), P);

    polygon = std::make_shared<Circle_polygon_set>(P);
}

////////////////////
// Transformation //
////////////////////

static X_monotone_curve::Point_2 transform_point(
    const Aff_transformation_2 &T, const X_monotone_curve::Point_2 &P)
{
    const auto x = P.x(), y = P.y();

    return X_monotone_curve::Point_2(T.m(0, 0) * x + T.m(0, 1) * y + T.m(0, 2),
                                     T.m(1, 0) * x + T.m(1, 1) * y + T.m(1, 2));
}

static Circle_polygon transform_curves(const Aff_transformation_2 &T,
                                       const Circle_polygon &P)
{
    Circle_polygon G;
    std::list<Curve_2> l;

    reassemble_curves(P, l);

    for (const Curve_2 &c: l) {
        if (c.is_linear()) {
            G.push_back(X_monotone_curve(T.transform(c.supporting_line()),
                                         transform_point(T, c.source()),
                                         transform_point(T, c.target())));
        } else if (c.is_full()) {
            subdivide_curve(c.supporting_circle().orthogonal_transform(T), G);
        } else {
            subdivide_curve(
                Curve_2(c.supporting_circle().orthogonal_transform(T),
                                         transform_point(T, c.source()),
                                         transform_point(T, c.target())), G);
        }
    }

    if (T.is_odd()) {
        G.reverse_orientation();
    }

    return G;
}

template<>
void Polygon_transform_operation<Circle_polygon_set>::evaluate()
{
    assert(!polygon);

    polygon = std::make_shared<Circle_polygon_set>();

    using namespace std::placeholders;
    auto f = std::bind(transform_curves, transformation, _1);

    transform_polygon_set(*operand->get_value(), *polygon, f);
}

/////////////////
// Conversions //
/////////////////

static Circle_polygon convert_polygon(const Polygon &P)
{
    Circle_polygon G;

    for (auto e = P.edges_begin(); e != P.edges_end(); e++) {
        G.push_back(X_monotone_curve(e->source(), e->target()));
    }

    return G;
}

void Polygon_convert_operation<Circle_polygon_set, Polygon_set>::evaluate()
{
    assert(!polygon);

    polygon = std::make_shared<Circle_polygon_set>();
    transform_polygon_set(*operand->get_value(), *polygon, convert_polygon);
}

// Piecewise-linear approximation

static FT convert_coord_nt(const Traits::Point_2::CoordNT &x)
{
    if (x.a1() == FT(0) || x.root() == FT(0)) {
        return x.a0();
    } else {
        return x.a0() + x.a1() * FT(std::sqrt(CGAL::to_double(x.root())));
    }
}

static Point_2 convert(const Traits::Point_2 &P)
{
    return Point_2(convert_coord_nt(P.x()), convert_coord_nt(P.y()));
}

static Polygon convert_circle_polygon(const double tau, const FT &sigma,
                                      const Circle_polygon &P)
{
    Polygon G;
    std::list<Curve_2> l;

    reassemble_curves(P, l);

    for (const Curve_2 &c: l) {
        if (c.is_linear()) {
            G.push_back(convert(c.target()));
        } else {
            const Circle_2 &C = c.supporting_circle();
            const Point_2 &O = C.center();
            const FT rho = rational_sqrt(C.squared_radius());

            // The apothem a of a regular n-sided polygon with
            // circumradius rho is:
            //
            //     a = rho cos(pi / n)
            //
            // Approximating a circle with such a polygon, would
            // result in a tolerance of rho - a.  So for a full circle
            // with our specified tolerance tau, we would need:
            //
            //     n = pi / acos(1 - tau / rho)
            //
            // Or, equavalently, the following per radian of arc:

            const double n_prime =
                1 / (2 * std::acos(1 - tau / CGAL::to_double(rho)));

            if (c.is_full()) {
                const double delta = C.orientation() * 2 * std::acos(-1);
                const double n = std::ceil(std::abs(delta) * n_prime);

                // A full circle.

                for (int i = 0; i < n; i++) {
                    const double theta = std::asin(1) + delta / n * i;

                    Vector_2 r(
                        CGAL::ORIGIN, project_to_circle(std::cos(theta),
                                                        std::sin(theta),
                                                        rho,
                                                        sigma));
                    G.push_back(O + r);
                };
            } else {
                const Point_2 S = convert(c.source());
                const Point_2 T = convert(c.target());

                const double theta_0 = std::atan2(
                    CGAL::to_double(S.y() - O.y()),
                    CGAL::to_double(S.x() - O.x()));

                double theta_1 = std::atan2(CGAL::to_double(T.y() - O.y()),
                                            CGAL::to_double(T.x() - O.x()));

                if (C.orientation() == CGAL::CLOCKWISE && theta_1 >= theta_0) {
                    theta_1 -= 2 * std::acos(-1);
                } else if (C.orientation() == CGAL::COUNTERCLOCKWISE
                           && theta_1 <= theta_0) {
                    theta_1 += 2 * std::acos(-1);
                }

                const double delta = theta_1 - theta_0;
                const double n = std::ceil(std::abs(delta) * n_prime);

                // Skip the starting point, which will have been added
                // as the target of the previous segment.

                for (int i = 1; i < n; i++) {
                    const double theta = theta_0 + delta / n * i;

                    Vector_2 r(
                        CGAL::ORIGIN, project_to_circle(std::cos(theta),
                                                        std::sin(theta),
                                                        rho,
                                                        sigma));

                    G.push_back(O + r);
                };

                G.push_back(T);
            }
        }
    }

    return G;
}

void convert_circle_polygon_set(const Circle_polygon_set &S, Polygon_set &T,
                                const double tau, const FT &sigma)
{
    using namespace std::placeholders;
    auto f = std::bind(convert_circle_polygon, tau, sigma, _1);

    transform_polygon_set(S, T, f);
}

void Polygon_convert_operation<Polygon_set, Circle_polygon_set>::evaluate()
{
    assert(!polygon);

    polygon = std::make_shared<Polygon_set>();
    convert_circle_polygon_set(*operand->get_value(), *polygon,
                               CGAL::to_double(tolerances[0]),
                               tolerances[1]);
}
