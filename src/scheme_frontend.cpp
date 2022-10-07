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

#include <chibi/sexp.h>
#include <chibi/install.h>

#include "options.h"
#include "kernel.h"
#include "transformations.h"
#include "macros.h"
#include "boxed_operations.h"
#include "frontend.h"

template<typename T>
static sexp foreign_type;

void add_exception_source(sexp ctx, sexp e)
{
    sexp_gc_var1(s);
    sexp_gc_preserve1(ctx, s);

    s = sexp_get_stack_trace(ctx);

    for (int i = 0; sexp_pairp(s); s = sexp_cdr(s), i++) {
        sexp t = sexp_cdar(s);

        if (sexp_pairp(t)) {
            sexp_exception_source(e) = sexp_cdar(sexp_get_stack_trace(ctx));
            break;
        }
    }

    sexp_gc_release1(ctx);
}

static sexp make_exception(sexp ctx, sexp self, const char *msg)
{
    sexp_gc_var1(s);
    sexp_gc_preserve1(ctx, s);

    s = sexp_user_exception(ctx, self, msg, SEXP_NULL);
    add_exception_source(ctx, s);

    sexp_gc_release1(ctx);

    return s;
}

static sexp make_args_exception(sexp ctx, sexp self)
{
    sexp_gc_var1(s);
    sexp_gc_preserve1(ctx, s);

    s = sexp_user_exception(ctx, self, "insufficient arguments", SEXP_NULL);
    add_exception_source(ctx, s);

    sexp_gc_release1(ctx);

    return s;
}

static sexp make_simple_type_exception(
    sexp ctx, sexp self, sexp_uint_t type, sexp x)
{
    sexp_gc_var1(s);
    sexp_gc_preserve1(ctx, s);

    s = sexp_type_exception(ctx, self, type, x);
    add_exception_source(ctx, s);

    sexp_gc_release1(ctx);

    return s;
}

static sexp make_foreign_type_exception(sexp ctx, sexp self, sexp type, sexp x)
{
    std::ostringstream s;

    s << "invalid type, expected ";

    const sexp n = sexp_type_name(type);
    s.write(sexp_string_data(n), sexp_string_size(n));
    std::string t = s.str();

    sexp_gc_var1(r);
    sexp_gc_preserve1(ctx, r);

    r = sexp_xtype_exception(ctx, self, t.c_str(), x);
    add_exception_source(ctx, r);

    sexp_gc_release1(ctx);

    return r;
}

static sexp make_type_exception(
    sexp ctx, sexp self, const char *message, sexp x)
{
    sexp_gc_var1(s);
    sexp_gc_preserve1(ctx, s);

    s = sexp_xtype_exception(ctx, self, message, x);
    add_exception_source(ctx, s);

    sexp_gc_release1(ctx);

    return s;
}

#define ASSERT_FOREIGN_TYPE(T, X)                                       \
if (!sexp_isa(X, foreign_type<T>)) {                                    \
    return make_foreign_type_exception(ctx, self, foreign_type<T>, X);  \
}

#define ASSERT_TYPE(P, T, X)                            \
if (!P(X)) {                                            \
    return make_simple_type_exception(ctx, self, T, X); \
}

template<typename T>
static sexp finalize(sexp ctx, sexp self, sexp_sint_t n, sexp x)
{
    delete static_cast<T *>(sexp_cpointer_value(x));

    return SEXP_VOID;
}

template<typename T, typename... Args>
static sexp to_scheme(sexp ctx, Args &&... args)
{
    if constexpr(std::is_same_v<T, FT>) {
        const auto x = FT(args...).exact();
        const long int a = x.get_num().get_si();
        const long int b = x.get_den().get_si();

        sexp_gc_var2(s, t);
        sexp_gc_preserve2(ctx, s, t);

        s = sexp_make_fixnum(a);

        if (b != 1) {
            t = sexp_make_fixnum(b);
            s = sexp_make_ratio(ctx, s, t);
        }

        sexp_gc_release2(ctx);

        return s;
    } else {
        return sexp_make_cpointer(
            ctx, sexp_type_tag(foreign_type<T>),
            static_cast<void *>(new T(std::forward<Args>(args)...)),
            SEXP_FALSE, 1);
    }
}

static mpz_class to_mpz_class(sexp x)
{
    return mpz_class(static_cast<long int>(sexp_sint_value(x)));
}

template<typename T>
static T from_scheme(sexp x)
{
    if constexpr(std::is_same_v<T, FT>) {
        if (sexp_exact_integerp(x)) {
            return FT(to_mpz_class(x));
        } else if (sexp_ratiop(x)) {
            return FT(
                FT::ET(to_mpz_class(sexp_ratio_numerator(x)),
                       to_mpz_class(sexp_ratio_denominator(x))));
        } else {
            return FT(sexp_flonum_value(x));
        }
    } else {
        return *static_cast<T *>(sexp_cpointer_value(x));
    }
}

//////////////
// Settings //
//////////////

template<FT &T>
static FT set_tolerance(const FT &x)
{
    FT x_0 = T;
    T = x;

    return x_0;
}

////////////////
// Operations //
////////////////

template<typename T>
sexp pop_argument(sexp ctx, sexp self, sexp &s, T &x)
{
    if (sexp_nullp(s)) {
        return make_args_exception(ctx, self);
    }

    sexp t = sexp_car(s);

    if constexpr(std::is_same_v<T, sexp>) {
        x = t;
    } else if constexpr (std::is_integral_v<T>) {
        ASSERT_TYPE(sexp_exact_integerp, SEXP_FIXNUM, t);
        x = sexp_unbox_fixnum(t);
    } else if constexpr(std::is_same_v<T, FT>) {
        ASSERT_TYPE(sexp_realp, SEXP_NUMBER, t);
        x = from_scheme<FT>(t);
    } else if constexpr(std::is_same_v<T, std::string>) {
        ASSERT_TYPE(sexp_stringp, SEXP_STRING, t);
        x = std::string(sexp_string_data(t), sexp_string_size(t));
    } else {
        ASSERT_FOREIGN_TYPE(T, t);
        x = from_scheme<T>(t);
    }

    s = sexp_cdr(s);

    return SEXP_VOID;
}

#define POP_ARGUMENT(S, X)                              \
{                                                       \
    if (sexp _x = pop_argument(ctx, self, S, X);        \
        sexp_exceptionp(_x)) {                          \
        return _x;                                      \
    }                                                   \
}

#define POP_INVALID_ARGUMENT(S, ...)                                    \
{                                                                       \
    sexp s = SEXP_VOID;                                                 \
                                                                        \
    POP_ARGUMENT(args, s);                                              \
    return make_type_exception(ctx, self, "invalid type" __VA_ARGS__, s); \
}

template<typename T>
bool pop_optional(sexp &s, T &x)
{
    if (sexp_nullp(s)) {
        return false;
    } else {
        sexp t = sexp_car(s);

        if constexpr(std::is_same_v<T, sexp>) {
            x = t;
            s = sexp_cdr(s);
        } else if constexpr (std::is_integral_v<T>) {
            if (sexp_exact_integerp(t)) {
                x = sexp_unbox_fixnum(t);
                s = sexp_cdr(s);
            } else {
                return false;
            }
        } else if constexpr(std::is_same_v<T, FT>) {
            if (sexp_realp(t)) {
                x = from_scheme<FT>(t);
                s = sexp_cdr(s);
            } else {
                return false;
            }
        } else if constexpr(std::is_same_v<T, std::string>) {
            if (sexp_stringp(t)) {
                x = std::string(sexp_string_data(t), sexp_string_size(t));
                s = sexp_cdr(s);
            } else {
                return false;
            }
        } else {
            if (sexp_isa(t, foreign_type<T>)) {
                x = from_scheme<T>(t);
                s = sexp_cdr(s);
            } else {
                return false;
            }
        }
    }

    return true;
}

template<typename T>
bool pop_optional(sexp &s, T &x, const T &def)
{
    const bool p = pop_optional(s, x);

    if (!p) {
        x = def;
    }

    return p;
}

// Generic operation taking any number of FT arguments.

template<int I, typename A, typename T, typename... Types>
static sexp make_primitive_args(sexp ctx, sexp self, sexp args, A &t)
{
    POP_ARGUMENT(args, std::get<I>(t));

    if constexpr (sizeof...(Types) > 0) {
        return make_primitive_args<I + 1, A, Types...>(
            ctx, self, args, t);
    } else {
        return SEXP_VOID;
    }
}

template<auto F, typename R, typename... Types>
static sexp make_primitive(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    std::tuple<Types...> t;

    if (sexp s = make_primitive_args<0, decltype(t), Types...>(
            ctx, self, args, t);
        s != SEXP_VOID) {
        return s;
    }

    return to_scheme<R>(ctx, std::apply(F, t));
}

template <std::size_t>
using FT_alias = FT;

template<auto F, typename R, std::size_t... Is>
static inline sexp make_primitive_helper(
    sexp ctx, sexp self, sexp_sint_t n, sexp args, std::index_sequence<Is...>)
{
    return make_primitive<F, R, FT_alias<Is>...>(ctx, self, n, args);
}

template<auto F, typename R, std::size_t N>
static inline sexp make_primitive(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    return make_primitive_helper<F, R>(
        ctx, self, n, args, std::make_index_sequence<N>{});
}

static sexp output(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    std::string s;
    pop_optional(args, s);

    std::vector<Boxed_polyhedron> v;
    sexp t = SEXP_VOID;

    while (pop_optional(args, t)) {
        ASSERT_FOREIGN_TYPE(Boxed_polyhedron, t);
        v.push_back(from_scheme<Boxed_polyhedron>(t));
    }

    add_output_operations(s, v);

    return t;
}

static sexp define_option(sexp ctx, sexp self, sexp_sint_t n, sexp s, sexp t)
{
    if (sexp_env_ref(ctx, sexp_context_env(ctx), s, SEXP_UNDEF) == SEXP_UNDEF) {
        sexp_env_define(ctx, sexp_context_env(ctx), s, t);
    }

    return SEXP_VOID;
}

///////////////////////
// Geometric objects //
///////////////////////

static sexp point(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    FT x, y, z;

    POP_ARGUMENT(args, x);
    POP_ARGUMENT(args, y);

    if (pop_optional(args, z)) {
        return to_scheme<Point_3>(ctx, x, y, z);
    } else {
        return to_scheme<Point_2>(ctx, x, y);
    }
}

static Plane_3 plane(const FT &a, const FT &b, const FT &c, const FT &d)
{
    return Plane_3(a, b, c, d);
}

/////////////////////
// Transformations //
/////////////////////

static sexp transformation_apply(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    // This facilitates transformation application folding.

    if (sexp_nullp(sexp_cadr(args))) {
        return sexp_car(args);
    }

    if (Aff_transformation_2 T; pop_optional(args, T)) {
        if (Boxed_polygon x; pop_optional(args, x)) {
            return to_scheme<Boxed_polygon>(ctx, std::visit(
                [&T](auto &&y) {
                    return make_boxed_polygon(TRANSFORM(y, T));
                }, x));
        }

        if (Point_2 x; pop_optional(args, x)) {
            return to_scheme<Point_2>(ctx, T.transform(x));
        }

        if (Aff_transformation_2 x; pop_optional(args, x)) {
            return to_scheme<Aff_transformation_2>(ctx, T * x);
        }

        POP_INVALID_ARGUMENT(
            args, ", expected polygon, point-2d, or transformation-2d");
    }

    if (Aff_transformation_3 T; pop_optional(args, T)) {
        if (Boxed_polyhedron x; pop_optional(args, x)) {
            return std::visit(
                    [&ctx, &T](auto &&x) {
                        return to_scheme<Boxed_polyhedron>(
                            ctx, TRANSFORM(x, T));
                    }, x);
        }

        if (Point_3 x; pop_optional(args, x)) {
            return to_scheme<Point_3>(ctx, T.transform(x));
        }

        if (Plane_3 x; pop_optional(args, x)) {
            return to_scheme<Plane_3>(ctx, T.transform(x));
        }

        if (Aff_transformation_3 x; pop_optional(args, x)) {
            return to_scheme<Aff_transformation_3>(ctx, T * x);
        }

        if (std::shared_ptr<Bounding_volume> x; pop_optional(args, x)) {
            return to_scheme<std::shared_ptr<Bounding_volume>>(ctx, x->transform(T));
        }

        POP_INVALID_ARGUMENT(
            args,
            ", expected polyhedron, bounding-volume, point-3d, plane-3d, "
            "or transformation-3d");
    }

    POP_INVALID_ARGUMENT(args, ", expected transformation");
}

template<auto F_3, auto F_2>
static sexp transformation_2_3(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    FT x, y, z;

    POP_ARGUMENT(args, x);
    POP_ARGUMENT(args, y);

    if (pop_optional(args, z)) {
        return to_scheme<Aff_transformation_3>(ctx, F_3(x, y, z));
    } else {
        return to_scheme<Aff_transformation_2>(ctx, F_2(x, y));
    }
}

static sexp rotation(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    FT a;

    POP_ARGUMENT(args, a);
    const double theta = CGAL::to_double(a);

    FT b, c, d;

    if (pop_optional(args, b)) {
        if (pop_optional(args, c) && pop_optional(args, d)) {
            double v[3] = {
                CGAL::to_double(b),
                CGAL::to_double(c),
                CGAL::to_double(d)};

            return to_scheme<Aff_transformation_3>(
                ctx, axis_angle_rotation(theta, v));
        }

        int i;

        if (b == (i = 0) || b == (i = 1)|| b == (i = 2)) {
            return to_scheme<Aff_transformation_3>(ctx, basic_rotation(theta, i));
        }

        return make_exception(ctx, self, "expected 0, 1, or 2");
    }

    return to_scheme<Aff_transformation_2>(ctx, basic_rotation(theta));
}

static sexp flush(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    FT lambda, mu;

    if (std::shared_ptr<Bounding_volume> x; pop_optional(args, x)) {
        FT nu;

        POP_ARGUMENT(args, lambda);
        POP_ARGUMENT(args, mu);
        POP_ARGUMENT(args, nu);

        if (const auto p = x->flush(lambda, mu, nu)) {
            return to_scheme<std::shared_ptr<Bounding_volume>>(ctx, p);
        }

        return make_exception(ctx, self, "cannot flush this bounding volume");
    }

    if (Boxed_polygon x; pop_optional(args, x)) {
        POP_ARGUMENT(args, lambda);
        POP_ARGUMENT(args, mu);

        return std::visit(
                [&ctx, &lambda, &mu](auto &&y) {
                    return to_scheme<Boxed_polygon>(ctx, FLUSH(y, lambda, mu));
                }, x);
    }

    if (Boxed_polyhedron x; pop_optional(args, x)) {
        FT nu;

        POP_ARGUMENT(args, lambda);
        POP_ARGUMENT(args, mu);
        POP_ARGUMENT(args, nu);

        return std::visit(
                [&ctx, &lambda, &mu, &nu](auto &&y) {
                    return to_scheme<Boxed_polyhedron>(
                        ctx, FLUSH(y, lambda, mu, nu));
                }, x);
    }

    POP_INVALID_ARGUMENT(
        args, ", expected bounding volume, polygon, or polyhedron");
}

#define DEFINE_FLUSH_OPERATION(WHERE, LAMBDA, MU, NU)                   \
static sexp flush_## WHERE(sexp ctx, sexp self, sexp_sint_t n, sexp args) \
{                                                                       \
    sexp_cdr(args) = sexp_list3(ctx, LAMBDA, MU, NU);                   \
    return flush(ctx, self, 4, args);                                   \
}

DEFINE_FLUSH_OPERATION(west, SEXP_NEG_ONE, SEXP_ZERO, SEXP_ZERO)
DEFINE_FLUSH_OPERATION(east, SEXP_ONE, SEXP_ZERO, SEXP_ZERO)

DEFINE_FLUSH_OPERATION(south, SEXP_ZERO, SEXP_NEG_ONE, SEXP_ZERO)
DEFINE_FLUSH_OPERATION(north, SEXP_ZERO, SEXP_ONE, SEXP_ZERO)

DEFINE_FLUSH_OPERATION(bottom, SEXP_ZERO, SEXP_ZERO, SEXP_NEG_ONE)
DEFINE_FLUSH_OPERATION(top, SEXP_ZERO, SEXP_ZERO, SEXP_ONE)

#undef DEFINE_FLUSH_OPERATION

static Boxed_polygon offset(Boxed_polygon &p, const FT &delta)
{
    return std::visit(
        [&delta](auto &&x) {
            return OFFSET(x, delta);
        }, p);
}

static sexp extrusion(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    Boxed_polygon p;

    POP_ARGUMENT(args, p);

    std::vector<Aff_transformation_3> v;

    while (!sexp_nullp(args)) {
        Aff_transformation_3 T;
        POP_ARGUMENT(args, T);
        v.push_back(T);
    }

    return to_scheme<Boxed_polyhedron>(
        ctx, std::visit(
            [&v](auto &&x) {
                return EXTRUSION(x, std::move(v));
            }, p));
}

////////////////
// Selections //
////////////////

static sexp complement(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (std::shared_ptr<Bounding_volume> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Bounding_volume>>(ctx, COMPLEMENT(x));
    }

    if (std::shared_ptr<Vertex_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Vertex_selector>>(ctx, COMPLEMENT(x));
    }

    if (std::shared_ptr<Face_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(ctx, COMPLEMENT(x));
    }

    if (std::shared_ptr<Edge_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Edge_selector>>(ctx, COMPLEMENT(x));
    }

    if (Boxed_polygon x; pop_optional(args, x)) {
        return std::visit(
            [&ctx](auto &&y) {
                return to_scheme<Boxed_polygon>(ctx, COMPLEMENT(y));
            }, x);
    }

    if (Boxed_polyhedron x; pop_optional(args, x)) {
        return std::visit(
            [&ctx](auto &&y) {
                return to_scheme<Boxed_polyhedron>(ctx, COMPLEMENT(y));
            }, x);
    }

    POP_INVALID_ARGUMENT(args);
}

static sexp boundary(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    Boxed_polyhedron a;

    POP_ARGUMENT(args, a);

    return std::visit(
        [&ctx](auto &&x) {
            return to_scheme<Boxed_polyhedron>(ctx, BOUNDARY(x));
        }, a);
}

template<int I>
static sexp relative_selection(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (std::shared_ptr<Face_selector> x; pop_optional(args, x)) {
        int k;

        POP_ARGUMENT(args, k);
        return to_scheme<std::shared_ptr<Face_selector>>(
            ctx, RELATIVE_SELECTION(x, I * k));
    }

    if (std::shared_ptr<Vertex_selector> x; pop_optional(args, x)) {
        int k;

        POP_ARGUMENT(args, k);
        return to_scheme<std::shared_ptr<Vertex_selector>>(
            ctx, RELATIVE_SELECTION(x, I * k));
    }

    POP_INVALID_ARGUMENT(args, ", expected selector");
}

static sexp vertices_in(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (std::shared_ptr<Face_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Vertex_selector>>(ctx, VERTICES_IN(x));
    }

    if (std::shared_ptr<Edge_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Vertex_selector>>(ctx, VERTICES_IN(x));
    }

    if (std::shared_ptr<Bounding_volume> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Vertex_selector>>(ctx, VERTICES_IN(x));
    }

    POP_INVALID_ARGUMENT(args, ", expected selector or bounding volume");
}

static sexp faces_in(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (std::shared_ptr<Vertex_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(ctx, FACES_IN(x));
    }

    if (std::shared_ptr<Edge_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(ctx, FACES_IN(x));
    }

    if (std::shared_ptr<Bounding_volume> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(ctx, FACES_IN(x));
    }

    POP_INVALID_ARGUMENT(args, ", expected selector or bounding volume");
}

static sexp faces_partially_in(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (std::shared_ptr<Vertex_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(
            ctx, FACES_PARTIALLY_IN(x));
    }

    if (std::shared_ptr<Edge_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(
            ctx, FACES_PARTIALLY_IN(x));
    }

    if (std::shared_ptr<Bounding_volume> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(
            ctx, FACES_PARTIALLY_IN(x));
    }

    POP_INVALID_ARGUMENT(args, ", expected selector or bounding volume");
}

static sexp edges_in(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (std::shared_ptr<Vertex_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Edge_selector>>(ctx, EDGES_IN(x));
    }

    if (std::shared_ptr<Face_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Edge_selector>>(ctx, EDGES_IN(x));
    }

    if (std::shared_ptr<Bounding_volume> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Edge_selector>>(ctx, EDGES_IN(x));
    }

    POP_INVALID_ARGUMENT(args, ", expected selector or bounding volume");
}

static sexp edges_partially_in(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (std::shared_ptr<Vertex_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Edge_selector>>(
            ctx, EDGES_PARTIALLY_IN(x));
    }

    if (std::shared_ptr<Face_selector> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Edge_selector>>(
            ctx, EDGES_PARTIALLY_IN(x));
    }

    if (std::shared_ptr<Bounding_volume> x; pop_optional(args, x)) {
        return to_scheme<std::shared_ptr<Edge_selector>>(
            ctx, EDGES_PARTIALLY_IN(x));
    }

    POP_INVALID_ARGUMENT(args, ", expected selector or bounding volume");
}

//////////////
// Polygons //
//////////////

static sexp ngon(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (sexp_length_unboxed(args) < 3) {
        return make_args_exception(ctx, self);
    }

    std::vector<Point_2> v;

    while (v.size() < 3 || !sexp_nullp(args)) {
        Point_2 P;
        POP_ARGUMENT(args, P);
        v.push_back(P);
    }

    return to_scheme<Boxed_polygon>(ctx, POLYGON(std::move(v)));
}

////////////////////////
// Polygons/polyhedra //
////////////////////////

// The Nef option branch can't be incorporated in the macro, since
// it's made at run-time.

// Prefer converting away from Nef, to avoid propagating conversions
// to Nef polyhedra down the line, unless both operands are already
// Nef polyhedra.

#define HANDLE_SELECTION_TYPE(OP, T)                    \
if (std::shared_ptr<T> p; pop_optional(args, p)) {      \
    std::vector<std::shared_ptr<T>> v;                  \
    v.push_back(p);                                     \
                                                        \
    while(!sexp_nullp(args)) {                          \
        POP_ARGUMENT(args, p);                          \
        v.push_back(p);                                 \
    }                                                   \
                                                        \
    return to_scheme<std::shared_ptr<T>>(               \
        ctx, OP(std::move(v)));                         \
}

#define DEFINE_SET_OPERATION(NAME, OP)                                  \
static sexp NAME ##_2(sexp ctx, sexp self, sexp s, sexp t)              \
{                                                                       \
    if (sexp_nullp(t)) {                                                \
        return s;                                                       \
    }                                                                   \
                                                                        \
    if (sexp_isa(s, foreign_type<Boxed_polyhedron>)) {                  \
        ASSERT_FOREIGN_TYPE(Boxed_polyhedron, t);                       \
                                                                        \
        return to_scheme<Boxed_polyhedron>(                             \
            ctx, std::visit(                                            \
                PERFORM_POLYHEDRON_SET_OPERATION(OP),                   \
                from_scheme<Boxed_polyhedron>(s),                       \
                from_scheme<Boxed_polyhedron>(t)));                     \
    } else if (sexp_isa(s, foreign_type<Boxed_polygon>)) {              \
        if (sexp_nullp(t)) {                                            \
            return s;                                                   \
        }                                                               \
                                                                        \
        ASSERT_FOREIGN_TYPE(Boxed_polygon, t);                          \
                                                                        \
        return std::visit(                                              \
            [&ctx](auto &&x, auto &&y) {                                \
                return to_scheme<Boxed_polygon>(ctx, OP(x, y));         \
            },                                                          \
            from_scheme<Boxed_polygon>(s),                              \
            from_scheme<Boxed_polygon>(t));                             \
    } else {                                                            \
        return make_type_exception(                                     \
            ctx, self, "invalid type, expected polygon or polyhedron", s); \
    }                                                                   \
}                                                                       \
                                                                        \
static sexp NAME ##_many(sexp ctx, sexp self, sexp_sint_t n, sexp args) \
{                                                                       \
    sexp s = sexp_car(args);                                            \
    sexp t = sexp_cdr(args);                                            \
    sexp r = sexp_car(t) = NAME ##_2(ctx, self, s, sexp_car(t));        \
                                                                        \
    if (n == 2) {                                                       \
        return r;                                                       \
    }                                                                   \
                                                                        \
    return NAME ##_many(ctx, self, n - 1, t);                           \
}                                                                       \
                                                                        \
static sexp NAME ##_any(sexp ctx, sexp self, sexp_sint_t n, sexp args)  \
{                                                                       \
    int m = sexp_length_unboxed(args);                                  \
                                                                        \
    if (m == 1) {                                                       \
        return sexp_car(args);                                          \
    }                                                                   \
                                                                        \
    HANDLE_SELECTION_TYPE(OP, Bounding_volume);                         \
    HANDLE_SELECTION_TYPE(OP, Vertex_selector);                         \
    HANDLE_SELECTION_TYPE(OP, Face_selector);                           \
    HANDLE_SELECTION_TYPE(OP, Edge_selector);                           \
                                                                        \
    return NAME ##_many(ctx, self, m, args);                            \
}

DEFINE_SET_OPERATION(union, JOIN)
DEFINE_SET_OPERATION(difference, DIFFERENCE)
DEFINE_SET_OPERATION(intersection, INTERSECTION)

#undef HANDLE_SELECTION_TYPE
#undef DEFINE_SET_OPERATION

///////////////
// Polyhedra //
///////////////

static sexp octahedron(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    FT a, b, c, d;

    POP_ARGUMENT(args, a);
    POP_ARGUMENT(args, b);
    POP_ARGUMENT(args, c);

    if (pop_optional(args, d)) {
        return to_scheme<Boxed_polyhedron>(ctx, OCTAHEDRON(a, b, c, d));
    } else {
        return to_scheme<Boxed_polyhedron>(ctx, OCTAHEDRON(a, b, c));
    }
}

static sexp regular_bipyramid(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    FT a, b, c;
    int k;

    POP_ARGUMENT(args, k);
    POP_ARGUMENT(args, a);
    POP_ARGUMENT(args, b);

    if (pop_optional(args, c)) {
        return to_scheme<Boxed_polyhedron>(ctx, REGULAR_BIPYRAMID(k, a, b, c));
    } else {
        return to_scheme<Boxed_polyhedron>(ctx, REGULAR_BIPYRAMID(k, a, b));
    }
}

static sexp clip(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    Boxed_polyhedron a;
    Plane_3 Pi;

    POP_ARGUMENT(args, a);
    POP_ARGUMENT(args, Pi);

    return to_scheme<Boxed_polyhedron>(
        ctx, std::visit(PERFORM_POLYHEDRON_CLIP_OPERATION(Pi), a));
}

static sexp deflate(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    Boxed_polyhedron a;

    POP_ARGUMENT(args, a);

    std::shared_ptr<Vertex_selector> p;
    pop_optional(args, p, std::shared_ptr<Vertex_selector>(nullptr));

    int m;
    POP_ARGUMENT(args, m);

    FT w_H, w_M;

    pop_optional(args, w_H, FT(FT::ET(1, 10)));
    pop_optional(args, w_M, FT(0));

    return std::visit(
        [&ctx, &p, &m, &w_H, &w_M](auto &&x) {
            return to_scheme<Boxed_polyhedron>(ctx, DEFLATE(x, p, m, w_H, w_M));
        }, a);
}

static sexp color_selection(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    std::variant<std::shared_ptr<Face_selector>,
                 std::shared_ptr<Vertex_selector>> p;

    Boxed_polyhedron a;
    POP_ARGUMENT(args, a);

    if (std::shared_ptr<Face_selector> x; pop_optional(args, x)) {
        p = x;
    } else if (std::shared_ptr<Vertex_selector> x; pop_optional(args, x)) {
        p = x;
    } else {
        POP_INVALID_ARGUMENT(args, ", expected selector");
    }

    FT v[4] = {0, 0, 0, 1};

    if (int n; pop_optional(args, n)) {
        for (int i = 0; i < 3; i++) {
            v[i] = (n >> i) & 1;
        }
    }

    return std::visit(
        [&ctx, &v](auto &&x, auto &&y) {
            return to_scheme<Boxed_polyhedron>(
                ctx, COLOR_SELECTION(x, y, v[0], v[1], v[2], v[3]));
        }, a, p);
}

#define DEFINE_SUBDIVISION_OPERATION(SUFFIX, OP)                \
static sexp subdivide_ ##SUFFIX(                                \
    sexp ctx, sexp self, sexp_sint_t n, sexp args)              \
{                                                               \
    Boxed_polyhedron a;                                         \
    int m;                                                      \
                                                                \
    POP_ARGUMENT(args, a);                                      \
    POP_ARGUMENT(args, m);                                      \
                                                                \
    return std::visit(                                          \
        [&ctx, &m](auto &&x) {                                  \
            return to_scheme<Boxed_polyhedron>(ctx, OP(x, m));  \
        }, a);                                                  \
}

DEFINE_SUBDIVISION_OPERATION(loop, LOOP)
DEFINE_SUBDIVISION_OPERATION(catmull_clark, CATMULL_CLARK)
DEFINE_SUBDIVISION_OPERATION(doo_sabin, DOO_SABIN)
DEFINE_SUBDIVISION_OPERATION(sqrt_3, SQRT_3)

#undef DEFINE_SUBDIVISION_OPERATION

#define DEFINE_SIMPLE_MESH_OPERATION(NAME, OP, T)                       \
static sexp NAME(sexp ctx, sexp self, sexp_sint_t n, sexp args)         \
{                                                                       \
    Boxed_polyhedron a;                                                 \
    std::shared_ptr<T> p;                                               \
    FT l;                                                               \
                                                                        \
    POP_ARGUMENT(args, a);                                              \
    pop_optional(args, p);                                              \
    POP_ARGUMENT(args, l);                                              \
                                                                        \
    return std::visit(                                                  \
        [&ctx, &p, &l](auto &&x) {                                      \
            return to_scheme<Boxed_polyhedron>(ctx, OP(x, p, l));       \
        }, a);                                                          \
}

DEFINE_SIMPLE_MESH_OPERATION(perturb, PERTURB, Vertex_selector)
DEFINE_SIMPLE_MESH_OPERATION(refine, REFINE, Face_selector)

#undef DEFINE_SIMPLE_MESH_OPERATION

static sexp remesh(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    Boxed_polyhedron a;
    std::shared_ptr<Face_selector> p;
    std::shared_ptr<Edge_selector> q;
    FT l;
    int m;

    POP_ARGUMENT(args, a);
    pop_optional(args, p);
    pop_optional(args, q);
    POP_ARGUMENT(args, l);
    pop_optional(args, m, 1);

    return std::visit(
        [&ctx, &p, &q, &l, &m](auto &&x) {
            return to_scheme<Boxed_polyhedron>(ctx, REMESH(x, p, q, l, m));
        }, a);
}

static sexp fair(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    Boxed_polyhedron a;
    std::shared_ptr<Vertex_selector> p;

    POP_ARGUMENT(args, a);
    POP_ARGUMENT(args, p);

    int m;
    pop_optional(args, m, 1);

    return std::visit(
        [&ctx, &p, &m](auto &&x) {
            return to_scheme<Boxed_polyhedron>(ctx, FAIR(x, p, m));
        }, a);
}

static sexp smooth_shape(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    Boxed_polyhedron a;

    std::shared_ptr<Face_selector> p;
    std::shared_ptr<Vertex_selector> q;
    FT t;
    int m;

    POP_ARGUMENT(args, a);
    pop_optional(args, p);
    pop_optional(args, q);
    POP_ARGUMENT(args, t);
    pop_optional(args, m, 1);

    return std::visit(
        [&ctx, &p, &q, &t, &m](auto &&x) {
            return to_scheme<Boxed_polyhedron>(
                ctx, SMOOTH_SHAPE(x, p, q, t, m));
        }, a);
}

static sexp deform(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    Boxed_polyhedron a;
    std::shared_ptr<Vertex_selector> p, q;
    std::vector<std::pair<std::shared_ptr<Vertex_selector>,
                          Aff_transformation_3>> v;

    POP_ARGUMENT(args, a);
    pop_optional(args, p);

    while (true) {
        Aff_transformation_3 T;

        while (pop_optional(args, q)) {
            POP_ARGUMENT(args, T);
            v.push_back(std::pair(q, T));
        }

        // If we read in control selector-transform pairs above, then
        // the first selector (p) was the ROI and all is well,
        // otherwise it was the first control region selector and we
        // need to pop the rest.

        if (!v.empty()) {
            break;
        }

        POP_ARGUMENT(args, T);
        v.push_back(std::pair(p, T));

        p = nullptr;
    }

    FT tau;
    POP_ARGUMENT(args, tau);

    unsigned int m;
    pop_optional(args, m, (tau == 0) ? 10 : std::numeric_limits<unsigned int>::max());

    return std::visit(
        [&ctx, &p, &v, &tau, &m](auto &&x) {
            return to_scheme<Boxed_polyhedron>(
                ctx, DEFORM(x, p, std::move(v), tau, m));
        }, a);
}

static sexp corefine(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (sexp_nullp(sexp_cadr(args))) {
        return sexp_car(args);
    }

    Boxed_polyhedron p;
    POP_ARGUMENT(args, p);

    if (Boxed_polyhedron q; pop_optional(args, q)) {
        return std::visit(
            [&ctx](auto &&x, auto &&y) {
                return to_scheme<Boxed_polyhedron>(
                    ctx, COREFINE(x, y));
            }, p, q);
    }

    if (Plane_3 Pi; pop_optional(args, Pi)) {
        return std::visit(
            [&ctx, &Pi](auto &&x) {
                return to_scheme<Boxed_polyhedron>(ctx, COREFINE(x, Pi));
            }, p);
    }

    POP_INVALID_ARGUMENT(args, ", expected polyhedron, or plane-3d");
}

static sexp hull(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    std::shared_ptr<Polyhedron_hull_operation> p;
    std::shared_ptr<Polygon_hull_operation> q;

    while (true) {
        if (Boxed_polyhedron x; !q && pop_optional(args, x)) {
            if (!p) {
                p = std::make_shared<Polyhedron_hull_operation>();
            }

            std::visit(
                [&p](auto &&y) {
                    p->push_back(y);
                }, x);
        } else if (Point_3 x; !q && pop_optional(args, x)) {
            if (!p) {
                p = std::make_shared<Polyhedron_hull_operation>();
            }

            p->push_back(x);
        } else if (Boxed_polygon x; !p && pop_optional(args, x)) {
            if (!q) {
                 q = std::make_shared<Polygon_hull_operation>();
            }

            std::visit(
                [&q](auto &&y) {
                    q->push_back(CONVERT_TO<Polygon_set>(y));
                }, x);
        } else if (Point_2 x; !p && pop_optional(args, x)) {
            if (!q) {
                 q = std::make_shared<Polygon_hull_operation>();
            }

            q->push_back(x);
        } else if (sexp_nullp(args)) {
            return (p
                    ? to_scheme<Boxed_polyhedron>(ctx, HULL(p))
                    : to_scheme<Boxed_polygon>(ctx, HULL(q)));
        } else if (p) {
            POP_INVALID_ARGUMENT(args, ", expected polyhedron or point-3d");
        } else if (q) {
            POP_INVALID_ARGUMENT(args, ", expected polygon or point-2d");
        } else {
            POP_INVALID_ARGUMENT(args, ", expected polygon, polyhedron or point");
        }
    }
}

static sexp minkowski_sum(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    if (Boxed_polyhedron x; pop_optional(args, x)) {
        Boxed_polyhedron y;

        POP_ARGUMENT(args, y);

        return std::visit(
            [&ctx](auto &&z, auto &&w) {
                return to_scheme<Boxed_polyhedron>(ctx, MINKOWSKI_SUM(z, w));
            }, x, y);
    }

    if (Boxed_polygon x; pop_optional(args, x)) {
        Boxed_polygon y;

        POP_ARGUMENT(args, y);

        return std::visit(
            [&ctx](auto &&z, auto &&w) {
                return to_scheme<Boxed_polygon>(ctx, MINKOWSKI_SUM(z, w));
            }, x, y);
    }

    POP_INVALID_ARGUMENT(args, ", expected polygon or polyhedron");
}

///////////////

#define DEFINE_TYPE(NAME, T)                            \
{                                                       \
    sexp_gc_var1(s);                                    \
    sexp_gc_preserve1(ctx, s);                          \
    s = sexp_c_string(ctx, NAME, -1);                   \
    foreign_type<T> =                                   \
        sexp_register_c_type(ctx, s, finalize<T>);      \
    sexp_gc_release1(ctx);                              \
}

#define DEFINE_FOREIGN(NAME, ...)                                       \
{                                                                       \
    auto f = __VA_ARGS__;                                               \
    [[maybe_unused]] sexp s = sexp_define_foreign_proc_rest(            \
        ctx, env, NAME, 0, reinterpret_cast<void *>(f));                \
    assert(!sexp_exceptionp(s));                                        \
}

template <Operation::Message_level L>
static sexp print(sexp ctx, sexp self, sexp_sint_t n, sexp args)
{
    sexp_gc_var1(s);
    sexp_gc_preserve1(ctx, s);

    while (!sexp_nullp(args)) {
        sexp t;
        POP_ARGUMENT(args, t);

        s = sexp_write_to_string(ctx, t);
        print_message(L, sexp_string_data(s), sexp_string_size(s));
    }

    sexp_gc_release1(ctx);

    return SEXP_VOID;
}

static void print_location(sexp l)
{
    if (l && sexp_pairp(l)) {
        if (sexp_stringp(sexp_car(l))) {
            std::cerr << ANSI_COLOR(1, 31);
            std::cerr.write(
                sexp_string_data(sexp_car(l)),
                sexp_string_size(sexp_car(l)));
            std::cerr << ANSI_COLOR(0, ) << ':';
        }

        if (sexp_fixnump(sexp_cdr(l)) && (sexp_cdr(l) >= SEXP_ZERO)) {
            std::cerr << ANSI_COLOR(1, 37)
                      << sexp_unbox_fixnum(sexp_cdr(l))
                      << ANSI_COLOR(0, ) << ':';
        }

        std::cerr.put(' ');
    }
}

static void print_exception(sexp ctx, sexp e)
{
    sexp_gc_var2(a, b);

    assert(sexp_exceptionp(e));

    // Unwrap continuable exceptions.

    if ((sexp_exception_kind(e) == sexp_global(ctx, SEXP_G_CONTINUABLE_SYMBOL)
         && sexp_exceptionp(sexp_exception_irritants(e)))
        || sexp_exception_kind(e) == SEXP_UNCAUGHT) {
        return print_exception(ctx, sexp_exception_irritants(e));
    }

    sexp_gc_preserve2(ctx, a, b);

    a = sexp_exception_source(e);
    sexp c = sexp_exception_procedure(e);
    if ((!(a && sexp_pairp(a))) && c && sexp_procedurep(c)) {
        a = sexp_bytecode_source(sexp_procedure_code(c));
    }

    if (c) {
        if (sexp_procedurep(c)) {
            b = sexp_bytecode_name(sexp_procedure_code(c));

            if (b && sexp_symbolp(b)) {
                print_location(a);

                b = sexp_symbol_to_string(ctx, b);

                std::cerr << "in procedure: '";
                std::cerr.write(sexp_string_data(b), sexp_string_size(b));
                std::cerr << '\'' << std::endl;
            }
        } else if (sexp_opcodep(c)) {
            print_location(a);

            b = sexp_opcode_name(c);
            assert(sexp_stringp(b));

            std::cerr << "in opcode: '";
            std::cerr.write(sexp_string_data(b), sexp_string_size(b));
            std::cerr << '\'' << std::endl;
        }
    }

    print_location(a);
    std::cerr << ANSI_COLOR(1, 31) << "error" << ANSI_COLOR(0, 37) << ": ";

    b = sexp_exception_message(e);
    if (sexp_stringp(b)) {
        std::cerr.write(sexp_string_data(b), sexp_string_size(b));
    } else if (!sexp_nullp(b) && sexp_truep(b)) {
        b = sexp_write_to_string(ctx, b);
        std::cerr.write(sexp_string_data(b), sexp_string_size(b));
    } else {
        std::cerr << "unhandled exception";

        b = sexp_exception_kind(e);
        if (sexp_symbolp(b)) {
            b = sexp_symbol_to_string(ctx, b);

            std::cerr << " of kind '";
            std::cerr.write(sexp_string_data(b), sexp_string_size(b));
            std::cerr << "'";
        }
    }

    b = sexp_exception_irritants(e);
    if (b && sexp_pairp(b)) {
        std::cerr << ": ";

        while (true) {
            a = sexp_write_to_string(ctx, sexp_car(b));
            std::cerr.write(sexp_string_data(a), sexp_string_size(a));

            b = sexp_cdr(b);
            if (!sexp_nullp(b)) {
                std::cerr << ", ";
            } else {
                break;
            }
        };
    }

    std::cerr << std::endl;

    sexp_gc_release2(ctx);
}

static void print_stack_trace(sexp ctx, sexp trace)
{
    sexp s;

    std::cerr << std::endl;

    for (int i = 0; sexp_pairp(trace); trace = sexp_cdr(trace), i++) {
        s = sexp_cdar(trace);

        std::cerr << "#" << i << ' ';

        if (sexp t = sexp_bytecode_name(sexp_procedure_code(sexp_caar(trace)));
            sexp_symbolp(t)) {
            t = sexp_symbol_to_string(ctx, t);

            std::cerr << "in " << ANSI_COLOR(0, 33) << '\'';
            std::cerr.write(sexp_string_data(t), sexp_string_size(t));
            std::cerr << '\'' << ANSI_COLOR(0, );

            if (sexp_pairp(s)) {
                std::cerr << ", ";
            }
        }

        if (sexp_pairp(s)) {
            if (sexp t = sexp_car(s); sexp_stringp(t)) {
                std::cerr << ANSI_COLOR(1, 37);
                std::cerr.write(sexp_string_data(t), sexp_string_size(t));
                std::cerr << ANSI_COLOR(0, ) << ':';
            }

            if (sexp t = sexp_cdr(s); sexp_fixnump(t) && (t >= SEXP_ZERO)) {
                std::cerr << ANSI_COLOR(1, 37);
                std::cerr << sexp_unbox_fixnum(t);
                std::cerr << ANSI_COLOR(0, );
            }
        }

        std::cerr << std::endl;
    }
}

sexp init_base(
    sexp ctx, sexp self, sexp_sint_t n, sexp env,
    const char *version, const sexp_abi_identifier_t abi) {

    DEFINE_FOREIGN(
        "set-projection-tolerance!",
        make_primitive<set_tolerance<Tolerances::projection>, FT, 1>);
    DEFINE_FOREIGN(
        "set-curve-tolerance!",
        make_primitive<set_tolerance<Tolerances::curve>, FT, 1>);
    DEFINE_FOREIGN(
        "set-sine-tolerance!",
        make_primitive<set_tolerance<Tolerances::sine>, FT, 1>);

    sexp_define_foreign_proc_rest(
        ctx, env, "%define-option", 2, reinterpret_cast<void *>(define_option));
    DEFINE_FOREIGN("output", output);
    DEFINE_FOREIGN("point", point);
    DEFINE_FOREIGN("plane", make_primitive<plane, Plane_3, 4>);

    return SEXP_VOID;
}

sexp init_transformation(
    sexp ctx, sexp self, sexp_sint_t n, sexp env,
    const char *version, const sexp_abi_identifier_t abi) {
    DEFINE_FOREIGN("translation", transformation_2_3<TRANSLATION_3, TRANSLATION_2>);
    DEFINE_FOREIGN("scaling", transformation_2_3<SCALING_3, SCALING_2>);

    DEFINE_FOREIGN("rotation", rotation);
    DEFINE_FOREIGN("transformation-apply", transformation_apply);

    DEFINE_FOREIGN("flush", flush);
    DEFINE_FOREIGN("flush-south", flush_south);
    DEFINE_FOREIGN("flush-north", flush_north);
    DEFINE_FOREIGN("flush-east", flush_east);
    DEFINE_FOREIGN("flush-west", flush_west);
    DEFINE_FOREIGN("flush-bottom", flush_bottom);
    DEFINE_FOREIGN("flush-top", flush_top);

    return SEXP_VOID;
}

sexp init_volumes(
    sexp ctx, sexp self, sexp_sint_t n, sexp env,
    const char *version, const sexp_abi_identifier_t abi) {
    DEFINE_FOREIGN(
        "bounding-plane", make_primitive<BOUNDING_PLANE<>,
                                         std::shared_ptr<Bounding_volume>, 4>);
    DEFINE_FOREIGN(
        "bounding-halfspace", make_primitive<BOUNDING_HALFSPACE<>,
                                             std::shared_ptr<Bounding_volume>, 4>);

    DEFINE_FOREIGN(
        "bounding-halfspace-interior", make_primitive<BOUNDING_HALFSPACE_INTERIOR<>,
                                             std::shared_ptr<Bounding_volume>, 4>);

    DEFINE_FOREIGN(
        "bounding-box", make_primitive<BOUNDING_BOX<>,
                                       std::shared_ptr<Bounding_volume>, 3>);

    DEFINE_FOREIGN(
        "bounding-box-boundary", make_primitive<BOUNDING_BOX_BOUNDARY<>,
                                       std::shared_ptr<Bounding_volume>, 3>);

    DEFINE_FOREIGN(
        "bounding-box-interior", make_primitive<BOUNDING_BOX_INTERIOR<>,
                                       std::shared_ptr<Bounding_volume>, 3>);

    DEFINE_FOREIGN(
        "bounding-sphere", make_primitive<BOUNDING_SPHERE<>,
                                          std::shared_ptr<Bounding_volume>, 1>);

    DEFINE_FOREIGN(
        "bounding-sphere-boundary", make_primitive<BOUNDING_SPHERE_BOUNDARY<>,
                                          std::shared_ptr<Bounding_volume>, 1>);

    DEFINE_FOREIGN(
        "bounding-sphere-interior", make_primitive<BOUNDING_SPHERE_INTERIOR<>,
                                          std::shared_ptr<Bounding_volume>, 1>);

    DEFINE_FOREIGN(
        "bounding-cylinder",
        make_primitive<BOUNDING_CYLINDER<>,
                      std::shared_ptr<Bounding_volume>, 2>);

    DEFINE_FOREIGN(
        "bounding-cylinder-boundary",
        make_primitive<BOUNDING_CYLINDER_BOUNDARY<>,
                      std::shared_ptr<Bounding_volume>, 2>);

    DEFINE_FOREIGN(
        "bounding-cylinder-interior",
        make_primitive<BOUNDING_CYLINDER_INTERIOR<>,
                      std::shared_ptr<Bounding_volume>, 2>);

    return SEXP_VOID;
}

sexp init_selection(
    sexp ctx, sexp self, sexp_sint_t n, sexp env,
    const char *version, const sexp_abi_identifier_t abi) {
    DEFINE_FOREIGN("vertices-in", vertices_in);
    DEFINE_FOREIGN("faces-in", faces_in);
    DEFINE_FOREIGN("faces-partially-in", faces_partially_in);
    DEFINE_FOREIGN("edges-in", edges_in);
    DEFINE_FOREIGN("edges-partially-in", edges_partially_in);

    DEFINE_FOREIGN("expand-selection", relative_selection<1>);
    DEFINE_FOREIGN("contract-selection", relative_selection<-1>);

    return SEXP_VOID;
}

sexp init_polygons(
    sexp ctx, sexp self, sexp_sint_t n, sexp env,
    const char *version, const sexp_abi_identifier_t abi) {
    DEFINE_FOREIGN("simple-polygon", ngon);
    DEFINE_FOREIGN("regular-polygon",
                   make_primitive<REGULAR_POLYGON<>, Boxed_polygon, int, FT>);

    DEFINE_FOREIGN(
        "isosceles-triangle", make_primitive<ISOSCELES_TRIANGLE<>,
        Boxed_polygon, 2>);
    DEFINE_FOREIGN(
        "right-triangle", make_primitive<RIGHT_TRIANGLE<>,
        Boxed_polygon, 2>);
    DEFINE_FOREIGN("rectangle", make_primitive<RECTANGLE<>, Boxed_polygon, 2>);
    DEFINE_FOREIGN("circle", make_primitive<CIRCLE<>, Boxed_polygon, 1>);
    DEFINE_FOREIGN("circular-sector",
                   make_primitive<CIRCULAR_SECTOR<>, Boxed_polygon, 2>);
    DEFINE_FOREIGN("circular-segment",
                   make_primitive<CIRCULAR_SEGMENT<>, Boxed_polygon, 2>);
    DEFINE_FOREIGN("ellipse", make_primitive<ELLIPSE<>, Boxed_polygon, 2>);
    DEFINE_FOREIGN("elliptic-sector",
                   make_primitive<ELLIPTIC_SECTOR<>, Boxed_polygon, 3>);

    return SEXP_VOID;
}

sexp init_polyhedra(
    sexp ctx, sexp self, sexp_sint_t n, sexp env,
    const char *version, const sexp_abi_identifier_t abi) {
    DEFINE_FOREIGN("tetrahedron",
                   make_primitive<TETRAHEDRON<>, Boxed_polyhedron, 3>);
    DEFINE_FOREIGN("square-pyramid",
                   make_primitive<SQUARE_PYRAMID<>, Boxed_polyhedron, 3>);
    DEFINE_FOREIGN("octahedron", octahedron);
    DEFINE_FOREIGN(
        "regular-pyramid",
        make_primitive<REGULAR_PYRAMID<>, Boxed_polyhedron, int, FT, FT>);
    DEFINE_FOREIGN("regular-bipyramid", regular_bipyramid);
    DEFINE_FOREIGN("cuboid", make_primitive<CUBOID<>, Boxed_polyhedron, 3>);
    DEFINE_FOREIGN("icosahedron",
                   make_primitive<ICOSAHEDRON<>, Boxed_polyhedron, 1>);
    DEFINE_FOREIGN("sphere", make_primitive<SPHERE<>, Boxed_polyhedron, 1>);
    DEFINE_FOREIGN("cylinder", make_primitive<CYLINDER<>, Boxed_polyhedron, 2>);
    DEFINE_FOREIGN(
        "prism", make_primitive<PRISM<>, Boxed_polyhedron, int, FT, FT>);

    return SEXP_VOID;
}

sexp init_operations(
    sexp ctx, sexp self, sexp_sint_t n, sexp env,
    const char *version, const sexp_abi_identifier_t abi) {
    DEFINE_FOREIGN(
        "offset", make_primitive<offset, Boxed_polygon, Boxed_polygon, FT>);

    DEFINE_FOREIGN("extrusion", extrusion);
    DEFINE_FOREIGN("hull", hull);
    DEFINE_FOREIGN("minkowski-sum", minkowski_sum);

    DEFINE_FOREIGN("union", union_any);
    DEFINE_FOREIGN("difference", difference_any);
    DEFINE_FOREIGN("intersection", intersection_any);
    DEFINE_FOREIGN("complement", complement);
    DEFINE_FOREIGN("boundary", boundary);
    DEFINE_FOREIGN("clip", clip);
    DEFINE_FOREIGN("deflate", deflate);

    DEFINE_FOREIGN("color-selection", color_selection);
    DEFINE_FOREIGN("subdivide-catmull-clark", subdivide_catmull_clark);
    DEFINE_FOREIGN("subdivide-doo-sabin", subdivide_doo_sabin);
    DEFINE_FOREIGN("subdivide-loop", subdivide_loop);
    DEFINE_FOREIGN("subdivide-sqrt-3", subdivide_sqrt_3);
    DEFINE_FOREIGN("remesh", remesh);
    DEFINE_FOREIGN("perturb", perturb);
    DEFINE_FOREIGN("refine", refine);
    DEFINE_FOREIGN("fair", fair);
    DEFINE_FOREIGN("smooth-shape", smooth_shape);
    DEFINE_FOREIGN("deform", deform);
    DEFINE_FOREIGN("corefine", corefine);

    return SEXP_VOID;
}

sexp init_write(
    sexp ctx, sexp self, sexp_sint_t n, sexp env,
    const char *version, const sexp_abi_identifier_t abi) {
    DEFINE_FOREIGN("print-note", print<Operation::NOTE>);
    DEFINE_FOREIGN("print-warning", print<Operation::WARNING>);
    DEFINE_FOREIGN("print-error", print<Operation::ERROR>);

    return SEXP_VOID;
}

int run_scheme(const char *input, char **first, char **last)
{
    sexp ctx, env;

    sexp_scheme_init();
    ctx = sexp_make_eval_context(nullptr, nullptr, nullptr, 0, 0);
    assert(ctx);

    // Export libraries

    if (static bool initialized; !initialized) {
        initialized = 1;

        extern struct sexp_library_entry_t *sexp_static_libraries;
        const std::initializer_list<struct sexp_library_entry_t> entries = {
            {"gamma/base", init_base},
            {"gamma/transformation", init_transformation},
            {"gamma/volumes", init_volumes},
            {"gamma/selection", init_selection},
            {"gamma/polygons", init_polygons},
            {"gamma/polyhedra", init_polyhedra},
            {"gamma/operations", init_operations},
            {"gamma/write", init_write},
        };

        int n = 0;
        for(; sexp_static_libraries[n].name; n++);

        static std::unique_ptr<struct sexp_library_entry_t[]>
            static_libraries_array(
                new struct sexp_library_entry_t[n + entries.size() + 1]);

        int i = 0;
        for (; i < n ; i++) {
            static_libraries_array[i] = sexp_static_libraries[i];
        }

        assert(!sexp_static_libraries[i].name);

        for (const auto &x: entries) {
            static_libraries_array[i++] = x;
        }

        static_libraries_array[i] = {nullptr, nullptr};
        sexp_static_libraries = static_libraries_array.get();
    }

    // Create the environment.

    {
        sexp_gc_var2(e, f);
        sexp_gc_preserve2(ctx, e, f);

        // Append user-specified features.

        e = sexp_global(ctx, SEXP_G_FEATURES);
        assert(sexp_pairp(e));
        for (; sexp_pairp(sexp_cdr(e)); e = sexp_cdr(e));

        for (const auto &x: Options::scheme_features) {
            f = sexp_intern(ctx, x.c_str(), -1);
            e = sexp_cdr(e) = sexp_cons(ctx, f, SEXP_NULL);
        }

        // Configure the library search path.

        for (const auto &x: Options::include_directories) {
            sexp_add_module_directory(
                ctx, (e = sexp_c_string(ctx, x.c_str(), -1)), SEXP_FALSE);
        }

        sexp_load_standard_env(ctx, nullptr, SEXP_SEVEN);

        // Initialize the environment.

        e = sexp_eval_string(
            ctx,
            "(environment"
            " '(scheme base) '(scheme process-context) '(scheme load)"
            " '(gamma base))",
            -1, sexp_global(ctx, SEXP_G_META_ENV));

        if (sexp_exceptionp(e)) {
            print_exception(ctx, e);
            sexp_destroy_context(ctx);

            return -1;
        }

        sexp_load_standard_ports(ctx, e, stdin, stdout, stderr, 1);
        f = sexp_make_env(ctx);
        sexp_env_parent(f) = e;
        sexp_context_env(ctx) = env = f;

        e = sexp_intern(ctx, "repl-import", -1);
        f = sexp_env_ref(ctx, sexp_global(ctx, SEXP_G_META_ENV), e, SEXP_VOID);
        e = sexp_intern(ctx, "import", -1);
        sexp_env_define(ctx, env, e, f);

        sexp_set_parameter(
            ctx, sexp_global(ctx, SEXP_G_META_ENV),
            sexp_global(ctx, SEXP_G_INTERACTION_ENV_SYMBOL), env);

        sexp_gc_release2(ctx);
    }

    if (!Flags::eliminate_tail_calls) {
        sexp_global(ctx, SEXP_G_NO_TAIL_CALLS_P) = SEXP_TRUE;
    }

    // Types

    DEFINE_TYPE("point-2d", Point_2);
    DEFINE_TYPE("point-3d", Point_3);
    DEFINE_TYPE("plane-3d", Plane_3);

    DEFINE_TYPE("transformation-2d", Aff_transformation_2);
    DEFINE_TYPE("transformation-3d", Aff_transformation_3);

    DEFINE_TYPE("volumes", std::shared_ptr<Bounding_volume>);
    DEFINE_TYPE("face-selector", std::shared_ptr<Face_selector>);
    DEFINE_TYPE("vertex-selector", std::shared_ptr<Vertex_selector>);
    DEFINE_TYPE("edge-selector", std::shared_ptr<Edge_selector>);

    DEFINE_TYPE("polygon", Boxed_polygon);
    DEFINE_TYPE("polyhedron", Boxed_polyhedron);

    // Annotate operations with source location information.

    assert(Operation::hook == nullptr);
    Operation::hook = [&ctx](Operation &op) {
        sexp_gc_var1(s);
        sexp_gc_preserve1(ctx, s);

        s = sexp_get_stack_trace(ctx);

        for (int i = 0; sexp_pairp(s); s = sexp_cdr(s), i++) {
            sexp t = sexp_cdar(s);

            if (sexp_pairp(t)) {
                if (sexp u = sexp_car(t); sexp_stringp(u)) {
                    op.annotations.insert({
                            "file", std::string(sexp_string_data(u),
                                                sexp_string_size(u))});
                }

                if (sexp u = sexp_cdr(t); sexp_fixnump(u) && (u >= SEXP_ZERO)) {
                    op.annotations.insert({
                            "line", std::to_string(sexp_unbox_fixnum(u))});
                }

                break;
            }
        }

        sexp_gc_release1(ctx);
    };

    int result = 0;

    {
        sexp_gc_var2(s, t);
        sexp_gc_preserve2(ctx, s, t);

        // Define variables.

        for (const auto &x: Options::definitions) {
            if (!x.second.empty()) {
                s = sexp_intern(ctx, x.first.c_str(), -1);
                t = sexp_eval_string(ctx, x.second.c_str(), -1, NULL);

                // In case of error print the exception.

                if (sexp_exceptionp(t)) {
                    sexp_car(sexp_exception_source(t)) =
                        sexp_c_string(ctx, "<command line>", -1);
                    goto handle_error;
                }

                sexp_env_define(ctx, env, s, t);
            } else {
                s = sexp_intern(ctx, (x.first + '?').c_str(), -1);
                sexp_env_define(ctx, env, s, SEXP_TRUE);
            }
        }

        // Assemble command-line.

        t = SEXP_NULL;
        for (char **i = last - 1; i >= first; i--) {
            s = sexp_c_string(ctx, *i, -1);
            t = sexp_cons(ctx, s, t);
        }

        s = sexp_c_string(ctx, input, -1);
        t = sexp_cons(ctx, s, t);

        s = sexp_intern(ctx, "command-line", -1);
        sexp_set_parameter(ctx, env, s, t);

        // Load and evaluate the source.

        s = sexp_intern(ctx, "load", -1);
        t = sexp_env_ref(ctx, env, s, SEXP_FALSE);
        if (sexp_procedurep(t)) {
            if (!strcmp(input, "-")) {
                s = sexp_current_input_port(ctx);
            } else {
                s = sexp_c_string(ctx, input, -1);
            }

            s = sexp_list2(ctx, s, env);
            t = sexp_apply(ctx, t, s);
        }

        // Print the exception in case of error.

        if (sexp_exceptionp(t)) {
handle_error:
            print_exception(ctx, t);

            t = sexp_exception_stack_trace(t);
            if (sexp_pairp(t)) {
                print_stack_trace(ctx, t);
            }

            result = -1;
        }

        sexp_gc_release2(ctx);
    }

    sexp_destroy_context(ctx);
    Operation::hook = nullptr;

    return result;
}
