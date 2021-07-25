#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>

#include "kernel.h"
#include "transformation_types.h"
#include "transformations.h"
#include "conic_polygon_operations.h"

using Traits = Conic_traits;

typedef Traits::Curve_2 Curve_2;
typedef Traits::X_monotone_curve_2 X_monotone_curve;

// For much of the below, see comments in respective operations for
// circle polygons.

static void subdivide_curve(Curve_2 c, Conic_polygon &P)
{
    static auto make_monotone = Traits().make_x_monotone_2_object();
    std::vector<CGAL::Object> v;

    make_monotone(c, std::back_inserter(v));

    for (auto x: v) {
        P.push_back (CGAL::object_cast<X_monotone_curve>(x));
    }
}

// Identical conics have the same coefficients, up to a multiplicative
// constant.

static bool have_same_conic(const X_monotone_curve &a,
                            const X_monotone_curve &b)
{
    decltype(a.r() / b.r()) q;

#define IF_CLAUSE(x)                                                    \
    (((CGAL::sign(a.x()) == CGAL::ZERO) && (CGAL::sign(b.x()) == CGAL::ZERO)) \
     || ((CGAL::sign(b.x()) != CGAL::ZERO) && (                         \
             (q == 0 && CGAL::sign((q = a.x() / b.x())) != CGAL::ZERO)  \
             || (q != 0 && a.x() / b.x() == q))))

    return (IF_CLAUSE(r)
            && IF_CLAUSE(s)
            && IF_CLAUSE(t)
            && IF_CLAUSE(u)
            && IF_CLAUSE(v)
            && IF_CLAUSE(w));
#undef IF_CLAUSE
}

static void reassemble_curves(const Conic_polygon &P, std::list<Curve_2> &l)
{
    Conic_polygon::Curve_const_iterator b = P.curves_begin();
    Conic_polygon::Curve_const_iterator e = P.curves_end();

    for (auto [c_0, T, c] =
             std::tuple(b, b->target(), std::next(b)); ;
         T = c->target(), c++) {

        if (c != e) {
            // Just extend the target point, as long as the curves
            // have are supported on the same conic.

            if (have_same_conic(*c_0, *c) && c->source() == T) {
                continue;
            }
        } else {
            if (c_0 == b) {
                // A full conic

                l.push_back(Curve_2(c_0->r(), c_0->s(), c_0->t(),
                                    c_0->u(), c_0->v(), c_0->w()));

                return;
            } else if (have_same_conic(*c_0, *b)
                       && T == b->source()) {
                // We can merge with the beginning curve.

                l.front().set_source(c_0->source());

                return;
            }
        }

        l.push_back(Curve_2(c_0->r(), c_0->s(), c_0->t(),
                            c_0->u(), c_0->v(), c_0->w(),
                            c_0->orientation(), c_0->source(), T));

        if (c == e) {
            break;
        }

        c_0 = c;
    }
}

////////////////////
// Transformation //
////////////////////

// The following assume that the "normal" kernel is based on mpq_class
// and that the rational and algebraic kernels are based on CORE.
// CORE's BigRat essentially wraps a mpq, as does mpq_class, so the
// conversion is straightforward and exact.

static inline Rat_kernel::FT to_rat(const FT &x)
{
    return Rat_kernel::FT(x.exact().get_mpq_t());
}

static inline Rat_kernel::Point_2 to_rat(const Point_2 &A)
{
    return Rat_kernel::Point_2(to_rat(A.x()), to_rat(A.y()));
}

// CORE's Expr must first be approximated to a BigRat, before
// conversion to Gmpq.

static inline Point_2 from_alg(const Alg_kernel::Point_2 &A)
{
    return Point_2(FT(FT::ET(A.x().BigRatValue().get_mp())),
                   FT(FT::ET(A.y().BigRatValue().get_mp())));
}

static inline Alg_kernel::FT to_alg(const Circle_segment_traits::CoordNT &x)
{
    const Rat_kernel::FT a_0 = to_rat(x.a0());
    const Rat_kernel::FT a_1 = to_rat(x.a1());
    const Rat_kernel::FT root = to_rat(x.root());

    return a_0 + a_1 * CGAL::sqrt(Alg_kernel::FT(root));
}

static inline Alg_kernel::Point_2 to_alg(const Circle_segment_traits::Point_2 &A)
{
    return Alg_kernel::Point_2(to_alg(A.x()), to_alg(A.y()));
}

static X_monotone_curve::Point_2 transform_point(
    const Aff_transformation_2 &T, const X_monotone_curve::Point_2 &P)
{
    const auto x = P.x(), y = P.y();
    const Rat_kernel::FT m_00 = to_rat(T.m(0, 0));
    const Rat_kernel::FT m_01 = to_rat(T.m(0, 1));
    const Rat_kernel::FT m_02 = to_rat(T.m(0, 2));
    const Rat_kernel::FT m_10 = to_rat(T.m(1, 0));
    const Rat_kernel::FT m_11 = to_rat(T.m(1, 1));
    const Rat_kernel::FT m_12 = to_rat(T.m(1, 2));

    return X_monotone_curve::Point_2(m_00 * x + m_01 * y + m_02,
                                     m_10 * x + m_11 * y + m_12);
}

static inline void multiply(const Rat_kernel::FT (&A)[3][3],
                            const Rat_kernel::FT (&B)[3][3],
                            Rat_kernel::FT (&C)[3][3],
                            bool transpose)
{
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            Rat_kernel::FT dot(0);

            for (int k = 0; k < 3; ++k) {
                dot += (transpose ? A[k][row] : A[row][k]) * B[k][col];
            }

            C[row][col] = dot;
        }
    }
}

static Conic_polygon transform_curves(CGAL::Orientation orientation,
                                      const Aff_transformation_2 &T,
                                      const Conic_polygon &P)
{
    Conic_polygon G;
    std::list<Curve_2> l;
    const Aff_transformation_2 T_1 = T.inverse();
    const Rat_kernel::FT M[3][3] = {
        {to_rat(T_1.m(0, 0)), to_rat(T_1.m(0, 1)), to_rat(T_1.m(0, 2))},
        {to_rat(T_1.m(1, 0)), to_rat(T_1.m(1, 1)), to_rat(T_1.m(1, 2))},
        {0, 0, 1}
    };
    bool flip = false;

    reassemble_curves(P, l);

    for (const Curve_2 &c: l) {
        // An arbitrary conic section can be written as:
        //
        //                   [ r   t/2  u/2] [x]
        // x^T C x = [x y 1] [t/2   s   v/2] [y]
        //                   [u/2  v/2   w ] [1]
        //
        // The image of this under any affine transformation M, that
        // transforms x to x' = M x is then:
        //
        //     (M^-1 x')^T C (M^-1 x') = x'^T (M^-T C M^-1) x'
        //
        // Therefore, the transformed conic coefficients are given by:
        //
        // C' = M^-T C M^-1

        const Rat_kernel::FT C[3][3] = {
            {c.r(), Rat_kernel::FT(c.t()) / 2, Rat_kernel::FT(c.u()) / 2},
            {Rat_kernel::FT(c.t()) / 2, c.s(), Rat_kernel::FT(c.v()) / 2},
            {Rat_kernel::FT(c.u()) / 2, Rat_kernel::FT(c.v()) / 2, c.w()}
        };
        Rat_kernel::FT D[3][3], E[3][3];

        multiply(M, C, D, true);
        multiply(D, M, E, false);

        assert(E[0][1] == E[1][0]);
        assert(E[0][2] == E[2][0]);
        assert(E[1][2] == E[2][1]);

        if (c.is_full_conic()) {
            assert(l.size() == 1);

            // CGAL only supports full conics in clockwise
            // orientation, so that we need to keep track of the
            // desired orientation and flip manually as required.

            const Curve_2 d(E[0][0], E[1][1], E[0][1] * 2,
                            E[0][2] * 2, E[1][2] * 2, E[2][2]);

            subdivide_curve(d, G);

            flip = G.orientation() != orientation;
        } else {
            subdivide_curve(
                Curve_2(E[0][0], E[1][1], E[0][1] * 2,
                        E[0][2] * 2, E[1][2] * 2, E[2][2],
                        c.orientation(),
                        transform_point(T, c.source()),
                        transform_point(T, c.target())), G);
        }
    }

    if (T.is_odd() ^ flip) {
        G.reverse_orientation();
    }

    return G;
}

template<>
void Polygon_transform_operation<Conic_polygon_set>::evaluate()
{
    assert(!polygon);

    polygon = std::make_shared<Conic_polygon_set>();

    using namespace std::placeholders;

    auto fb = std::bind(transform_curves,
                        CGAL::COUNTERCLOCKWISE, transformation, _1);
    auto fh = std::bind(transform_curves,
                        CGAL::CLOCKWISE, transformation, _1);

    transform_polygon_set(*operand->get_value(), *polygon, fb, fh);
}

/////////////////
// Conversions //
/////////////////

static Conic_polygon convert_polygon(const Polygon &P)
{
    Conic_polygon G;

    for (auto e = P.edges_begin(); e != P.edges_end(); e++) {
        subdivide_curve(
            Curve_2(
                Rat_kernel::Segment_2(
                    to_rat(e->source()), to_rat(e->target()))),
            G);
    }

    return G;
}

static Conic_polygon convert_circle_polygon(const Circle_polygon &P)
{
    Conic_polygon G;

    for (auto c = P.curves_begin(); c != P.curves_end(); c++) {
        if (c->is_linear()) {
            const Line_2 l = c->supporting_line();

            subdivide_curve(
                Curve_2(
                    0, 0, 0, to_rat(l.a()), to_rat(l.b()), to_rat(l.c()),
                    CGAL::COLLINEAR,
                    to_alg(c->source()), to_alg(c->target())),
                G);
        } else {
            const Circle_2 C = c->supporting_circle();

            subdivide_curve(
                Curve_2(
                    Rat_kernel::Circle_2(
                        to_rat(C.center()),
                        to_rat(C.squared_radius()),
                        C.orientation()),
                    c->orientation(),
                    to_alg(c->source()), to_alg(c->target())),
                G);
        }
    }

    return G;
}

void Polygon_convert_operation<Conic_polygon_set, Polygon_set>::evaluate()
{
    assert(!this->polygon);

    this->polygon = std::make_shared<Conic_polygon_set>();

    transform_polygon_set(
        *this->operand->get_value(), *this->polygon, convert_polygon);
}

void Polygon_convert_operation<Conic_polygon_set,
                               Circle_polygon_set>::evaluate()
{
    assert(!this->polygon);

    this->polygon = std::make_shared<Conic_polygon_set>();

    transform_polygon_set(
        *this->operand->get_value(), *this->polygon, convert_circle_polygon);
}

// Piecewise-linear approximation

static void sample_adaptively(const double tolerance,
                              const double a, const double b,
                              const double x_0, const double y_0,
                              const double costheta, const double sintheta,
                              const double t_0, const double t_1,
                              const Point_2 &P_0, const Point_2 &P_1,
                              Polygon &G)
{
    const double t = (t_0 + t_1) / 2;
    const Point_2 P(
        a * std::cos(t) * costheta - b * std::sin(t) * sintheta + x_0,
        a * std::cos(t) * sintheta + b * std::sin(t) * costheta + y_0);

    if (&P_0 == &P_1
        || CGAL::squared_distance(Line_2(P_0, P_1), P) > tolerance * tolerance) {
        sample_adaptively(tolerance, a, b, x_0, y_0, costheta, sintheta,
                          t_0, t, P_0, P, G);
        sample_adaptively(tolerance, a, b, x_0, y_0, costheta, sintheta,
                          t, t_1, P, P_1, G);
    } else {
        G.push_back(P_0);
    }
}

static Polygon convert_conic_polygon(CGAL::Orientation orientation,
                                     const double tolerance,
                                     const Conic_polygon &P)
{
    Polygon G;
    std::list<Curve_2> l;

    reassemble_curves(P, l);

    for (const Curve_2 &c: l) {
        if (c.orientation() == CGAL::COLLINEAR) {
            // A simple line segment.

            G.push_back(from_alg(c.source()));

            continue;
        }

        // Extract semi-axes, shift and rotation from the equation
        // coefficients. See:
        //
        // https://en.wikipedia.org/wiki/Ellipse#General_ellipse

        const double r = CGAL::to_double(c.r());
        const double s = CGAL::to_double(c.s());
        const double t = CGAL::to_double(c.t());
        const double u = CGAL::to_double(c.u());
        const double v = CGAL::to_double(c.v());
        const double w = CGAL::to_double(c.w());
        const double k = t * t - 4 * r * s;
        const double l = sqrt((r - s) * (r - s) + t * t);

#define semiaxis(op)                                                    \
        -std::sqrt(                                                     \
            2 * (r * v * v + s * u * u - t * u * v + k * w) * (r + s op l)) / k

        const double a = semiaxis(+);
        const double b = semiaxis(-);

#undef semiaxis

        const double x_0 = (2 * s * u - t * v) / k;
        const double y_0 = (2 * r * v - t * u) / k;

        double costheta, sintheta;

        if (t == 0) {
            if (r < s) {
                costheta = 1;
                sintheta = 0;
            } else {
                costheta = 0;
                sintheta = 1;
            }
        } else {
            const double tan = (s - r - l) / t;

            costheta = 1 / std::sqrt(1 + tan * tan);
            sintheta = tan * costheta;
        }

        // Sample the full conic or arc in their parametric forms.

        if (c.is_full_conic()) {
            const Point_2 A = Point_2(x_0 + a * costheta, y_0 + a * sintheta);

            sample_adaptively(
                tolerance,
                a, b, x_0, y_0, costheta, sintheta,
                0, orientation * 2 * std::acos(-1),
                A, A, G);
        } else {
            const X_monotone_curve::Point_2 &S = c.source();
            const X_monotone_curve::Point_2 &T = c.target();
            double t[2];

            // Find the parameters corresponding to each endpoint.

            for (int i = 0; i < 2; i++) {
                const X_monotone_curve::Point_2 &P = i == 0 ? S : T;
                const double dx = CGAL::to_double(P.x()) - x_0;
                const double dy = CGAL::to_double(P.y()) - y_0;

                t[i] = std::atan2((costheta * dy - sintheta * dx) / b,
                                  (sintheta * dy + costheta * dx) / a);
            }

            if (c.orientation() == CGAL::CLOCKWISE && t[1] >= t[0]) {
                t[1] -= 2 * std::acos(-1);
            } else if (c.orientation() == CGAL::COUNTERCLOCKWISE
                       && t[1] <= t[0]) {
                t[1] += 2 * std::acos(-1);
            }

            // Sample between them.

            sample_adaptively(tolerance,
                              a, b, x_0, y_0, costheta, sintheta,
                              t[0], t[1], from_alg(S), from_alg(T), G);
        }
    }

    return G;
}

void convert_conic_polygon_set(const Conic_polygon_set &S, Polygon_set &T,
                               const double tolerance)
{
    using namespace std::placeholders;

    auto fb = std::bind(convert_conic_polygon,
                        CGAL::COUNTERCLOCKWISE, tolerance, _1);
    auto fh = std::bind(convert_conic_polygon, CGAL::CLOCKWISE, tolerance, _1);

    transform_polygon_set(S, T, fb, fh);
}

void Polygon_convert_operation<Polygon_set, Conic_polygon_set>::evaluate()
{
    assert(!polygon);

    polygon = std::make_shared<Polygon_set>();

    convert_conic_polygon_set(*operand->get_value(), *polygon,
                              CGAL::to_double(tolerance));
}
