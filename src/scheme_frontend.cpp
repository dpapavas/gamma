// Copyright 2025 Dimitris Papavasiliou

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

#include <libguile.h>
#include <sanitizer/lsan_interface.h>

#include "options.h"
#include "kernel.h"
#include "transformations.h"
#include "macros.h"

#include "polygon_operations.h"
#include "polyhedron_operations.h"
#include "boxed_operations.h"
#include "frontend.h"

// Document: program

// # The Scheme Front End

// The front end for the Scheme language uses GNU Guile, an
// implementation of the Scheme language that is meant to be emebedded
// in other applications.  Even so, emebedding Guile presents a number
// of challenges.  We'll devote a subsection to each one.

// ## Passing Values between C++ and Scheme

// We use "foreign objects" to expose C++ data to Scheme, and for each
// type, we need and `SCM` value to represent it.  This is handled
// with the variable template below.

template<typename T>
static SCM foreign_type;

// Guile uses the Boehm-Demers-Weiser garbage collector, to handle its
// objects.  This can be convenient, but presents us with a couple of
// problems.

//   1.  We can't really depend on the collector to collect all
//   unreachable objects at the end of program execution. ^[See the
//   question labeled "I want to ensure that all my objects are
//   finalized and reclaimed before process exit. How can I do that?"
//   in the collector implementation's FAQ at
//   "https://www.hboehm.info/gc/faq.html".]  Since we depend upon
//   destruction of operation objects (ref: `Operation` destructor) to
//   unlink them from the graph, we need to ensure that they do get
//   destroyed.

//   We use the following sets to keep track of created polygon and
//   polyhedron operations.

static std::unordered_set<Boxed_polygon *> boxed_polygons;
static std::unordered_set<Boxed_polyhedron *> boxed_polyhedra;

//   2. The garbage collector runs in a different thread, so we need
//   to protect access to these sets.  We use a mutex.

static std::mutex boxed_mutex;

// The following utility accepts a C++ value and turns it into a
// Scheme value. C++ values that need to be exposed to Scheme fall in
// one of three categories:

template<typename T, typename... Args>
static SCM to_scheme(Args &&... args)
{
    if constexpr(std::is_same_v<T, FT>) {
        //   1. Numbers, which we convert to Scheme rationals,

        auto x = FT(args...).exact();

        return scm_divide(
            scm_from_mpz(x.get_num().get_mpz_t()),
            scm_from_mpz(x.get_den().get_mpz_t()));
    } else {
        //   2. CGAL values, such as points, planes, transformations,
        //   etc. which are allocated with `new` and wrapped inside a
        //   foreign object, and

        //   3. Operations and other associated objects, like
        //   selectors and bounding volumes, which are created as
        //   shared pointers, again wrapped inside a foreign object.

        // LSan doesn't seem to be able to detect that these
        // allocations remain reachable at program exit (via pointers
        // managed by the garbage collector) and reports them as
        // leaks.

#ifdef __SANITIZE_ADDRESS__
        __lsan_disable();
#endif

        // We handle these uniformly.  The only difference is that for
        // the former case, `T` will be `Point_3`, `Plane_3`, etc.,
        // while for the latter it be `Boxed_polygon`,
        // `Boxed_polyhedron`, or `std::shared_ptr<...>`.

        auto p = new T(std::forward<Args>(args)...);
        SCM s = scm_make_foreign_object_1(
            foreign_type<T>, static_cast<void *>(p));

        // We also update our bookeeping as necessary.

        const std::lock_guard<std::mutex> lock(boxed_mutex);

        if constexpr (std::is_same_v<T, Boxed_polygon>) {
            boxed_polygons.insert(p);
        }

        if constexpr (std::is_same_v<T, Boxed_polyhedron>) {
            boxed_polyhedra.insert(p);
        }

#ifdef __SANITIZE_ADDRESS__
        __lsan_enable();
#endif

        return s;
    }
}

// We also have a utitly for the revese operation.  It takes a Scheme
// value and converts it back into the corresponding C++ value.  The
// situation is more or less symmetric.

template<typename T>
static const T from_scheme(SCM x)
{
    if constexpr(std::is_same_v<T, FT>) {
        // Technically, all numbers apart from complex numbers and
        // infinities, NaNs and the like, are rational, not just
        // numbers represented with the rational type in Scheme.  This
        // includes inexact, i.e. floating point numbers, which are of
        // bounded precision and can be represented as rationals.

        // Below `scm_is_rational` accounts for all that, so we only
        // need to deal with this case.

        if (scm_is_rational(x)) {
            // Inexact values like `0.25` will lead to inexact results
            // from `scm_numerator` and `scm_denominator` (`1.0` and
            // `4.0` respectively).  These won't work with
            // `scm_to_mpz` which expect exact integers, so we need to
            // convert to exact first.

            const SCM q = scm_inexact_to_exact(x);
            mpz_t z;

            mpz_init(z);
            scm_to_mpz(scm_numerator(q), z);

            FT r = FT::ET(mpz_class(z));

            if (!scm_is_exact_integer(q)) {
                scm_to_mpz(scm_denominator(q), z);
                r /= FT::ET(mpz_class(z));
            }

            mpz_clear(z);

            return r;
        }

        // All remaining Scheme numbers are either complex, infinite,
        // or NaN.  We don't expect those here.  (Inexact
        // i.e. floating point numbers are of bounded precision and
        // can be represented as rationals.  They have already been
        // handled above.)

        assert_not_reached();
    } else {
        // For more complex types, we return the object or (possibly
        // boxed) shared pointer from the foreign object.

        return *static_cast<T *>(scm_foreign_object_ref(x, 0));
    }
}

// When Guile's garbage collector determines that a value is no longer
// reachable, it marks it for collection.  For foreign types it also
// calls the "finalizer", a custom function we registered with the
// type.  There is one finalizer per type, but in each case
// finalization essentially boils down to deleting the C++ object or
// (possibly boxed) shared pointer allocted during construction.

// C++ objects like `Plane_3`, `Aff_transformation_3` etc., are
// destroyed as soon as the finalizer deletes them; since Scheme can
// no longer see them, they aren't useful any more.

// Operations, bounding volumes, selectors, etc. on the other hand,
// will need to be evaluated later, so only the shared pointer
// reference held by Scheme is destroyed on collection.  This may
// trigger the object's destruction, if it's not referenced elsewhere,
// or the object may be destroyed later, during evaluation.  Ref:
// `Operation` destructor.

// We use the function template below to instantiate finalizers during
// registration.

template<typename T>
static void finalize(SCM s)
{
    auto p = scm_foreign_object_ref(s, 0);

    const std::lock_guard<std::mutex> lock(boxed_mutex);

    if constexpr (std::is_same_v<T, Boxed_polygon>) {
        boxed_polygons.erase(static_cast<T *>(p));
    }

    if constexpr (std::is_same_v<T, Boxed_polyhedron>) {
        boxed_polyhedra.erase(static_cast<T *>(p));
    }

    delete static_cast<T *>(p);
}

// ## Handling Scheme Function Arguments

// When implementing Scheme functions we typically need to get the
// argumments passed to it, validate them and convert them to the
// corresponding C++ values.  We then pass these to the relevant C++
// constructors, or otherwise use them on the C++ side to produce the
// needed result and convert this into a Scheme value before returning
// it.

// When an agument doesn't pass validation, we need to signal an
// error.  The way this is handled in Guile, say when using the
// `scm_wrong_type_arg_msg` function is to do a `longjmp` out of the
// C++ code implementing the function with the invalid argument, and
// (eventually) into the installed *Scheme* exception handler.

// This is all fine for C, but alas, we're dealing with C++ and the
// compiler depends on the stack actually getting unwound the usual
// way, i.e. by exiting the function (and whatever nested scope the
// validation code ran in), in order to destroy any objects allocated
// on the stack.  When we `longjmp`, all such allocations remain in
// limbo, as leaks.

// In order to avoid this, we create custom C++ exceptions and open a
// `try` block as early as possible in the code implementing a Scheme
// function.  When we want to signal an error we throw the
// corresponding C++ exception, which unwinds most if not all of the
// stack and then call the `longjmp`ing Guile function from there.

class wrong_type_exception: public std::exception {
public:
    int argnum;
    SCM bad_value;
    const char *expected;

    wrong_type_exception(int argnum, SCM bad_value, const char *expected)
        : argnum(argnum), bad_value(bad_value), expected(expected) {}

    const char* what() const noexcept override {
        return "wrong argument type";
    }
};

class wrong_num_args_exception: public std::exception {
public:
    wrong_num_args_exception() {}

    const char* what() const noexcept override {
        return "wrong number of arguments";
    }
};

// Now follow some utility functions that take care of reading Scheme
// values and converting them to C++.  Variations include popping
// arguments from a list (the "rest" list part of variadic Scheme
// functions), or from a discrete Scheme value, handling optional
// arguments, etc.

// Below, we take a Scheme value from `s`, convert it to a C++ value
// and assign it to `x`.  Here, `s` can either be a list, in which
// case the car is converted and `s` (which we have by reference) is
// updated to the cdr, so that we actually "pop" the converted value
// from the list, or it can just be a single value to convert.

// Depending on whether we're converting a required or optional
// variable, `E` and `F` either throw an error and we never return,
// jumping to the handler instead, or they're no-ops and we return a
// boolean signifying whether a variable was converted.

template<typename T, auto &E, auto &F>
bool pop_argument_impl(int i, SCM &s, T &x)
{
    if (scm_is_eq(s, SCM_UNDEFINED) || scm_is_null(s)) {
        E();
        return false;
    }

    const SCM t = scm_is_pair(s) ? scm_car(s) : s;

    if constexpr(std::is_same_v<T, SCM>) {
        x = t;
    } else if constexpr (std::is_integral_v<T>) {
        // When a Scheme function expects an integral type, it is
        // generally a small integer like the number of iterations.
        // We could accept inexact integers here, like `1.0`, or
        // rationals like `4/2`, but this would only serve to mask
        // errors when the user has mixed up the order of arguments
        // for instance.

        // We expect an exact integer and convert it to the integral
        // type provided (the type of `x`).

        if (!scm_is_exact_integer(t)) {
            F(i, t, "exact integer");
            return false;
        }

        x = scm_to_intmax(t);
    } else if constexpr(std::is_same_v<T, FT>) {
        // We only expect real numbers.  This includes integer,
        // rational, or inexact real numbers, but not complex numbers,
        // infinities or NaNs.  This is the same as saying we only
        // expect rational numbers, since they only real numbers that
        // are not rational are irrational numbers, but these can only
        // be represented as approximations of bounded precision,
        // which *are* rational.

        if (!scm_is_rational(t)) {
            F(i, t, "rational number");
            return false;
        }

        x = from_scheme<FT>(t);
    } else if constexpr(std::is_same_v<T, std::string>) {
        if (!scm_is_string(t)) {
            F(i, t, "string");
            return false;
        }

        char *u = scm_to_locale_string(t);
        x = std::string(u);
        free(u);
    } else {
        if (!SCM_IS_A_P(t, foreign_type<T>)) {
            // Here the call to the handler `F` can potentially
            // `longjmp` (if we got here through `pop_argument` and a
            // `wrong_num_args_exception` is thrown, eventually
            // calling `scm_wrong_type_arg_msg`).  In this case, we
            // won't have a chance to `free` the pointer returned by
            // `scm_to_locale_string` ourselves and attempting to let
            // the compiler do it for us, say by passing an
            // `std::string`, instead of a raw pointer won't work
            // either, as the longjmp will not allow its destructor to
            // be called.

            // The method below should work, as per Guile manual,
            // although Valgrind still shows a possible loss.  In
            // either case, the loss concerns a few bytes only and the
            // program will soon exit anyway.

            scm_dynwind_begin(static_cast<scm_t_dynwind_flags>(0));

            char *u = scm_to_locale_string(
                scm_symbol_to_string(
                    scm_class_name(foreign_type<T>)));

            scm_dynwind_free(u);

            F(i, t, u);

            scm_dynwind_end();

            return false;
        }

        x = from_scheme<T>(t);
    }

    // Now we can pop the converted value, if `s` is a list.

    if (scm_is_pair(s)) {
        s = scm_cdr(s);
    }

    return true;
}

// This is the utility function we call when we want to pop a required
// argument.

template<typename T>
void pop_argument(int i, SCM &s, T &x)
{
    constexpr static auto E = [] () {
        throw wrong_num_args_exception();
    };

    constexpr static auto F =
        [] (int i, const SCM s, const char *msg) {
            throw wrong_type_exception(i, s, msg);
        };

    pop_argument_impl<T, E, F>(i, s, x);
}

// This variation converts one or more required arguments of the same
// type from the list at `s` and emplaces them into the vector `v`.
// The number of arguments converted depend on the size of `s`, but
// any invalid types in `s` signify an error.

template<typename T>
void pop_arguments(int i, SCM &s, std::vector<T> &v)
{
    T x;

    while (!scm_is_null(s)) {
        pop_argument(i++, s, x);
        v.emplace_back(std::move(x));
    }

    assert(scm_is_null(s));
}

// Here we convert one or more required arguments from the discrete
// values contained in `ss`, again emplacing them into the vector `v`.

template<typename T>
void pop_arguments(int i, std::initializer_list<SCM> ss, std::vector<T> &v)
{
    for (SCM s: ss) {
        T x;
        pop_argument(i++, s, x);
        v.emplace_back(std::move(x));
    }
}

// We again convert one or more required arguments from the discete
// values contained in `ss`, but now assign them to the discrete
// references contained in the tuple `t`, so the variables may be of
// different types.

template<typename... Ts>
void pop_arguments(int i, std::initializer_list<SCM> ss, std::tuple<Ts&...> t)
{
    const SCM *it = ss.begin();
    SCM s;

    std::apply(
        [&](Ts&... args) {
            ((pop_argument(i++, (s = *(it++)), args)), ...);
        }, t);
}

// This is the same as above, but the values are now read from the
// list `s`, again assigned to discrete references.

template<typename... Ts>
void pop_arguments(int i, SCM &s, std::tuple<Ts&...> t)
{
    std::apply(
        [&](Ts&... args) {
            ((pop_argument(i++, s, args)), ...);
        }, t);
}

// When an optional argument is the last argument of a function, we
// expect either a valid argument or none at all.  We therefore behave
// as if we're dealing with a required argument if one is provided,
// but just return `false` if that is not the case.

template<typename T>
bool pop_optional(int i, SCM &s, T &x)
{
    if (scm_is_null(s) || scm_is_eq(s, SCM_UNDEFINED)) {
        return false;
    }

    pop_argument<T>(i, s, x);
    return true;
}

// Some functions accept optional arguments in the middle of their
// argument list.  In that case, an argument of invalid type does not
// imply an error; it might mean the user just didn't provide the
// optional argument.

// Other functions may accept different kinds of arguments.  Consider
// for example a function applying a transformation to either polygons
// or polyhedra.  In that case, we want to test whether the provided
// argument is a polyhedron and convert it if so, but just return
// `false` otherwise, so that the function can then look for a
// polyhedron instead.

template<typename T>
bool try_pop_argument(int i, SCM &s, T &x)
{
    constexpr static auto E = [] () {};

    constexpr static auto F =
        [] (int i, const SCM s, const char *msg) {};

    return pop_argument_impl<T, E, F>(i, s, x);
}

// Just another variation of the above.  We convert as many values of
// the given type as are available and place them in the vector.

template<typename T>
void try_pop_arguments(int i, SCM &s, std::vector<T> &v)
{
    T x;
    while (try_pop_argument(i++, s, x)) {
        v.emplace_back(std::move(x));
    }
}

// The function template `make_primitive` conveniently instantiates a
// function accepting any number of Scheme arguments of arbitrary
// types and validates them, before passing them to C++ function `F`,
// returning type `R` which is returned as a Scheme value.

// The result can be used with the appropriate call to
// `scm_c_define_gsubr` to export this function to Scheme.

template<int I, typename A, typename T, typename... Types>
static void make_primitive_args(SCM s, A &t)
{
    pop_argument(I + 1, s, std::get<I>(t));

    if constexpr (sizeof...(Types) > 0) {
        make_primitive_args<I + 1, A, Types...>(s, t);
    } else {
        if (!scm_is_null(s)) {
            throw wrong_num_args_exception();
        }
    }
}

template<auto F, typename R, typename... Types>
static SCM make_primitive(SCM args)
{
    std::tuple<Types...> t;

    make_primitive_args<0, decltype(t), Types...>(args, t);
    return to_scheme<R>(std::apply(F, t));
}

// Many such functions only expect numeric (`FT`) arguments, so this
// variation allows us to just specify, e.g. `3` instead of `FT, FT,
// FT`.

template <std::size_t>
using FT_alias = FT;

template<auto F, typename R, std::size_t... Is>
static inline SCM make_primitive_helper(SCM args, std::index_sequence<Is...>)
{
    return make_primitive<F, R, FT_alias<Is>...>(args);
}

template<auto F, typename R, std::size_t M>
static inline SCM make_primitive(SCM args)
{
    return make_primitive_helper<F, R>(args, std::make_index_sequence<M>{});
}

// ## Scheme Function Implementations

// We're now ready to define the functions we need to export to
// Scheme.  Many of the functions, those that don't need to handle
// optional arguments, or arguments of multiple types through special
// logic, are handled via the `make_primitive` templates above.  Below
// we only need to provide implementations for the rest.

// ### Functions in the `base` Library

// This returns and optionally sets one of the tolerance parameters.

template<FT &T>
static SCM set_tolerance(SCM s)
{
    FT x_0 = T;
    FT x;

    if (pop_optional(1, s, x)) {
        T = x;
    }

    return to_scheme<FT>(x_0);
}

// Here we provide the implementation of the `define-option` macro.
// The invocation `(define-option foo ...)` defines variable `foo`,
// but only if it hasn't been already defined with `-Dfoo=...` on the
// command line.

static SCM define_option(SCM s, SCM t)
{
    if (scm_is_false(scm_module_variable(scm_current_module(), s))) {
        scm_define(s, t);
    }

    return SCM_UNSPECIFIED;
}

// The `output` function, usually used via the `define-output` macro,
// defines its argument as a named or unnamed output.  Most of the
// actual work, is done elsewhere; ref: Outputs.

static SCM output(SCM args)
{
    std::string s;
    int n = -1;

    // The first argument is the name of the output which can be
    // either a string or a positive integer.  It can also be omitted
    // altogether in which case the output is unnamed.

    int i = try_pop_argument(1, args, s) || try_pop_argument(1, args, n);
    const SCM t = scm_car(args);

    // Numeric outputs are converted to strings and are typically used
    // to target unnamed viewports in the debugger.

    if (i && s.empty()) {
        if (n > 0) {
            s = std::to_string(n);
        } else {
            scm_misc_error(
                "output",
                "wrong argument in position ~A (expecting positive integer "
                "or string): ~S",
                scm_list_2(scm_from_int(1), scm_from_int(n)));
        }
    }

    // Anchor: Scheme output argument handling

    // We generally only expect to get a single value to output;
    // either a polyhedron, or a polygon.  Nevertheless we accept a
    // vector for cases where a combination of items will be output.
    // Again, the expectation is that these will either be all
    // polyhedra, or all polygons, but it's easier to support a series
    // of zero or more polygons, potentially followed by zero or more
    // polyhedra, so we do that.

    {
        std::vector<Boxed_polygon> v;
        try_pop_arguments(1 + i, args, v);

        if (!v.empty()) {
            insert_output_operations(s, v);
        }
    }

    {
        std::vector<Boxed_polyhedron> v;
        pop_arguments(1 + i, args, v);

        if (!v.empty()) {
            insert_output_operations(s, v);
        }
    }

    return t;
}

// Finally, we provide implementations for some basic geometric
// values.  We only need the implementation for `plane` because we
// can't take the address of `Plane_3`, which is a constructor.

static SCM point(SCM s, SCM t, SCM u)
{
    FT x, y, z;

    pop_arguments(1, {s, t}, std::tie(x, y));

    if (pop_optional(3, u, z)) {
        return to_scheme<Point_3>(x, y, z);
    } else {
        return to_scheme<Point_2>(x, y);
    }
}

static Plane_3 plane(const FT &a, const FT &b, const FT &c, const FT &d)
{
    return Plane_3(a, b, c, d);
}

// ### Functions in the `write` Library

// This template prints one or more messages of a given level, or at
// least calls `print_message` to do so (ref: Program Messages).

template<Operation::Message_level LEVEL>
static SCM print_message(SCM args)
{
    std::vector<std::string> v;

    pop_arguments(1, args, v);

    for (std::string &s: v) {
        print_message(LEVEL, s.c_str(), s.size());
    }

    return SCM_UNSPECIFIED;
}

// ### Functions in the `transformation` Library

// We start with functions creating simple transformations in 2 or 3
// dimensions.

template<auto F_3, auto F_2>
static SCM translation_2_3(SCM s, SCM t, SCM u)
{
    FT x, y, z;

    pop_arguments(1, {s, t}, std::tie(x, y));

    if (pop_optional(3, u, z)) {
        return to_scheme<Aff_transformation_3>(F_3(x, y, z));
    } else {
        return to_scheme<Aff_transformation_2>(F_2(x, y));
    }
}

template<auto F_3, auto F_2>
static SCM scaling_2_3(SCM s, SCM t, SCM u)
{
    FT x, y, z;

    pop_arguments(1, {s, t}, std::tie(x, y));

    if (pop_optional(3, u, z)) {
        return to_scheme<Aff_transformation_3>(F_3(x, y, z));
    } else {
        return to_scheme<Aff_transformation_2>(F_2(x, y));
    }
}

// Rotations come in three flavors:

static SCM rotation(SCM s, SCM rest)
{
    FT a;

    //   1. 2D rotations, accepting a single angle,

    pop_argument(1, s, a);
    const double theta = CGAL::to_double(a);

    if (scm_is_null(rest)) {
        return to_scheme<Aff_transformation_2>(basic_rotation(theta));
    }

    //   2. 3D rotations around one of the axes of the reference frame
    //   and

    FT b;
    pop_argument(2, rest, b);

    if (scm_is_null(rest)) {
        int i;

        if (b == (i = 0) || b == (i = 1)|| b == (i = 2)) {
            return to_scheme<Aff_transformation_3>(basic_rotation(theta, i));
        }

        scm_misc_error(
            "rotation",
            "wrong argument in position ~A (expecting 0, 1, or 2): ~S",
            scm_list_2(scm_from_int(2), scm_car(rest)));
    }

    //   3. 3D rotations around an arbitrary axis.

    FT c, d;

    pop_arguments(3, rest, std::tie(c, d));

    double v[3] = {
        CGAL::to_double(b),
        CGAL::to_double(c),
        CGAL::to_double(d)};

    return to_scheme<Aff_transformation_3>(axis_angle_rotation(theta, v));
}

// Here we apply 2D or 3D transfromation to compatible geometry.

static SCM transformation_apply(SCM s, SCM t)
{
    // As a special case we accept an empty list as a second
    // parameter.  This facilitates transformation concatenation via
    // `fold`/`fold-right`, as is done in `transformation-append`.

    if (scm_is_eq(t, SCM_EOL)) {
        return s;
    }

    if (Aff_transformation_2 T;
        try_pop_argument(1, s, T)) {

        // A 2D transformation, can be applied to:

        //   1. a polygon,

        if (Boxed_polygon x;
            try_pop_argument(2, t, x)) {
            return std::visit(
                [&T](auto &&y) {
                    return to_scheme<Boxed_polygon>(
                        make_boxed_transformed_polygon(TRANSFORM(y, T)));
                }, x);
        }

        //   2. a 2D point, or

        if (Point_2 x;
            try_pop_argument(2, t, x)) {
            return to_scheme<Point_2>(T.transform(x));
        }

        //   3. another 2D transformation (yielding the composite
        //   transformation).

        if (Aff_transformation_2 x;
            try_pop_argument(2, t, x)) {
            return to_scheme<Aff_transformation_2>(T * x);
        }

        throw wrong_type_exception(
            2, t, "polygon, point-2d, or transformation-2d");
    }

    if (Aff_transformation_3 T;
        try_pop_argument(1, s, T)) {
        // There are more potential targets for a 3D transformation.
        // It can be applied to:

        //   1. a polyhedron,

        if (Boxed_polyhedron x;
            try_pop_argument(2, t, x)) {
            return std::visit(
                    [&T](auto &&x) {
                        return to_scheme<Boxed_polyhedron>(TRANSFORM(x, T));
                    }, x);
        }

        //   2. a 3D point,

        if (Point_3 x;
            try_pop_argument(2, t, x)) {
            return to_scheme<Point_3>(T.transform(x));
        }

        //   3. a 3D plane,

        if (Plane_3 x;
            try_pop_argument(2, t, x)) {
            return to_scheme<Plane_3>(T.transform(x));
        }

        //   4. another 3D transformation, or

        if (Aff_transformation_3 x;
            try_pop_argument(2, t, x)) {
            return to_scheme<Aff_transformation_3>(T * x);
        }

        //   5. a bounding volume.

        if (std::shared_ptr<Bounding_volume> x;
            try_pop_argument(2, t, x)) {
            return to_scheme<std::shared_ptr<Bounding_volume>>(x->transform(T));
        }

        throw wrong_type_exception(
            2, t, ("polyhedron, bounding-volume, point-3d, plane-3d, "
                   "or transformation-3d"));
    }

    throw wrong_type_exception(1, s, "transformation");
}

// Finally, we have flush transformations, which are operations in
// their own right.  They can be applied to:

static SCM flush(SCM s, SCM t, SCM u, SCM v)
{
    FT lambda, mu;

    //   1. bounding volumes, whose extents can be evaluated exactly,

    if (std::shared_ptr<Bounding_volume> x;
        try_pop_argument(1, s, x)) {
        FT nu;

        pop_arguments(2, {t, u, v}, std::tie(lambda, mu, nu));

        if (const auto p = x->flush(lambda, mu, nu)) {
            return to_scheme<std::shared_ptr<Bounding_volume>>(p);
        }

        scm_misc_error("flush", "cannot flush this bounding volume", SCM_EOL);
    }

    //   2. polygons and

    if (Boxed_polygon x; try_pop_argument(1, s, x)) {
        pop_arguments(2, {t, u}, std::tie(lambda, mu));

        return std::visit(
                [&lambda, &mu](auto &&y) {
                    return to_scheme<Boxed_polygon>(FLUSH(y, lambda, mu));
                }, x);
    }

    //   3. polyhedra.

    if (Boxed_polyhedron x; try_pop_argument(1, s, x)) {
        FT nu;

        pop_arguments(2, {t, u, v}, std::tie(lambda, mu, nu));

        return std::visit(
                [&lambda, &mu, &nu](auto &&y) {
                    return to_scheme<Boxed_polyhedron>(
                        FLUSH(y, lambda, mu, nu));
                }, x);
    }

    throw wrong_type_exception(1, s, "bounding volume, polygon, or polyhedron");
}

// ### Functions in the `selections` Library

// Selections can either be based on a bounding volume or they may be
// converted from different kinds of selections.  We have to use a
// macro here, because the parameter `F` below is a function template
// (e.g. `VERTICES_IN`), which we can't pass as a template template
// parameter.

#define DEFINE_SELECTOR(FUNC, S, T, U, F)                               \
static SCM FUNC(SCM s)                                                  \
{                                                                       \
    if (std::shared_ptr<S> x;                                           \
        try_pop_argument(1, s, x)) {                                    \
        return to_scheme<std::shared_ptr<U>>(F(x));                     \
    }                                                                   \
                                                                        \
    if (std::shared_ptr<T> x;                                           \
        try_pop_argument(1, s, x)) {                                    \
        return to_scheme<std::shared_ptr<U>>(F(x));                     \
    }                                                                   \
                                                                        \
    if (std::shared_ptr<Bounding_volume> x;                             \
        try_pop_argument(1, s, x)) {                                    \
        return to_scheme<std::shared_ptr<U>>(F(x));                     \
    }                                                                   \
                                                                        \
    throw wrong_type_exception(1, s, "selector or bounding volume");    \
}

DEFINE_SELECTOR(
    vertices_in, Face_selector, Edge_selector, Vertex_selector,
    VERTICES_IN)
DEFINE_SELECTOR(
    faces_in, Vertex_selector, Edge_selector, Face_selector,
    FACES_IN)
DEFINE_SELECTOR(
    faces_partially_in, Vertex_selector, Edge_selector, Face_selector,
    FACES_PARTIALLY_IN)
DEFINE_SELECTOR(
    edges_in, Vertex_selector, Face_selector, Edge_selector,
    EDGES_IN)
DEFINE_SELECTOR(
    edges_partially_in, Vertex_selector, Face_selector, Edge_selector,
    EDGES_PARTIALLY_IN)

#undef DEFINE_SELECTOR

// Another useful category is feature-based selections.

static SCM faces_by_sharpness_angle(SCM s, SCM rest)
{
    FT theta;
    pop_argument(1, s, theta);

    if (std::shared_ptr<Face_selector> x;
        scm_is_null(scm_cdr(rest)) && try_pop_argument(2, rest, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(
            FACES_BY_SHARPNESS_ANGLE(theta, x));
    }

    std::vector<int> v;
    pop_arguments(2, rest, v);

    return to_scheme<std::shared_ptr<Face_selector>>(
        FACES_BY_SHARPNESS_ANGLE(theta, v));
}

static SCM faces_by_sharpness_mode(SCM s, SCM rest)
{
    int n;
    pop_argument(1, s, n);

    if (std::shared_ptr<Face_selector> x;
        scm_is_null(scm_cdr(rest)) && try_pop_argument(2, rest, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(
            FACES_BY_SHARPNESS_MODE(n, x));
    }

    std::vector<int> v;
    pop_arguments(2, rest, v);

    return to_scheme<std::shared_ptr<Face_selector>>(
        FACES_BY_SHARPNESS_MODE(n, v));
}

// As are selections based on intersection with primitives such as
// planes and lines.

#define DEFINE_SELECTOR(NAME, T, SEL, FUNC)                     \
static SCM NAME(SCM s, SCM t)                                   \
{                                                               \
    Point_3 a, b;                                               \
                                                                \
    pop_argument(1, s, a);                                      \
    pop_argument(2, t, b);                                      \
                                                                \
    return to_scheme<std::shared_ptr<SEL>>(FUNC(T(a, b)));      \
}

DEFINE_SELECTOR(faces_through_segment, Segment_3, Face_selector, FACES_THROUGH)
DEFINE_SELECTOR(faces_through_ray, Ray_3, Face_selector, FACES_THROUGH)
DEFINE_SELECTOR(faces_through_line, Line_3, Face_selector, FACES_THROUGH)

DEFINE_SELECTOR(edges_through_segment, Segment_3, Edge_selector, EDGES_THROUGH)
DEFINE_SELECTOR(edges_through_ray, Ray_3, Edge_selector, EDGES_THROUGH)
DEFINE_SELECTOR(edges_through_line, Line_3, Edge_selector, EDGES_THROUGH)

#undef DEFINE_SELECTOR

#define DEFINE_SELECTOR(NAME, T, SEL, FUNC)                     \
static SCM NAME(SCM s)                                          \
{                                                               \
    T x;                                                        \
    pop_argument(1, s, x);                                      \
    return to_scheme<std::shared_ptr<SEL>>(FUNC(x));            \
}

DEFINE_SELECTOR(faces_through_plane, Plane_3, Face_selector, FACES_THROUGH)
DEFINE_SELECTOR(edges_through_plane, Plane_3, Edge_selector, EDGES_THROUGH)

#undef DEFINE_SELECTOR

// Selections can also be derived by expanding or contracting.

template<int SIGN>
static SCM relative_selection(SCM s, SCM t)
{
    if (std::shared_ptr<Face_selector> x;
        try_pop_argument(1, s, x)) {
        int k;

        pop_argument(2, t, k);
        return to_scheme<std::shared_ptr<Face_selector>>(
            RELATIVE_SELECTION(x, SIGN * k));
    }

    if (std::shared_ptr<Vertex_selector> x;
        try_pop_argument(1, s, x)) {
        int k;

        pop_argument(2, t, k);
        return to_scheme<std::shared_ptr<Vertex_selector>>(
            RELATIVE_SELECTION(x, SIGN * k));
    }

    throw wrong_type_exception(1, s, "selector");
}

// This function is defined for many kinds of set-like values, but it
// is mostly of use for volumes and selections.  It is also defined
// for polygons and (Nef) polyehdra.

// Although we export this primarily through the `selection` library,
// we also make it available through the `operations` and `volumes`
// libararies.  This seems to be supported behavior in R6RS, which
// states (in section 7.1):

//   > An identifier can be imported with the same local name from two
//   > or more libraries or for two levels from the same library only
//   > if the binding exported by each library is the same ...

// For R7RS the situation is a bit more gray.  Section 5.2 of R7RS
// small says:

//   > In a program or library declaration, it is an error to import
//   > the same identifer more than once with different bindings, ...

// Since it's forbidden to import the same identifier *with different
// bindings*, we may assume it is allowed to do so if it has the same
// bindings.

static SCM complement(SCM s)
{
    if (std::shared_ptr<Bounding_volume> x;
        try_pop_argument(1, s, x)) {
        return to_scheme<std::shared_ptr<Bounding_volume>>(COMPLEMENT(x));
    }

    if (std::shared_ptr<Vertex_selector> x;
        try_pop_argument(1, s, x)) {
        return to_scheme<std::shared_ptr<Vertex_selector>>(COMPLEMENT(x));
    }

    if (std::shared_ptr<Face_selector> x;
        try_pop_argument(1, s, x)) {
        return to_scheme<std::shared_ptr<Face_selector>>(COMPLEMENT(x));
    }

    if (std::shared_ptr<Edge_selector> x;
        try_pop_argument(1, s, x)) {
        return to_scheme<std::shared_ptr<Edge_selector>>(COMPLEMENT(x));
    }

    if (Boxed_polygon x; try_pop_argument(1, s, x)) {
        return std::visit(
            [](auto &&y) {
                return to_scheme<Boxed_polygon>(COMPLEMENT(y));
            }, x);
    }

    if (Boxed_polyhedron x; try_pop_argument(1, s, x)) {
        return std::visit(
            [](auto &&y) {
                return to_scheme<Boxed_polyhedron>(COMPLEMENT(y));
            }, x);
    }

    throw wrong_type_exception(
        1, s, "bounding volume, selector, polygon, or polyhedron");
}

// These are similar, albeit only defined on volumes and polyhedra.

#define DEFINE_BOUNDARY_OR_INTERIOR_OPERATION(FUNC, OP)                 \
static SCM FUNC(SCM s)                                                  \
{                                                                       \
    if (std::shared_ptr<Bounding_volume> x;                             \
        try_pop_argument(1, s, x)) {                                    \
        if (const auto p = OP(x)) {                                     \
            return to_scheme<std::shared_ptr<Bounding_volume>>(p);      \
        }                                                               \
                                                                        \
        scm_misc_error(                                                 \
            #FUNC, "cannot take " #FUNC " of this bounding volume",     \
            SCM_EOL);                                                   \
    }                                                                   \
                                                                        \
    if (Boxed_polyhedron p; try_pop_argument(1, s, p)) {                \
        return std::visit(                                              \
            [](auto &&x) {                                              \
                return to_scheme<Boxed_polyhedron>(OP(x));              \
            }, p);                                                      \
    }                                                                   \
                                                                        \
    throw wrong_type_exception(                                         \
        1, s, "bounding volume or polyhedron");                         \
}

DEFINE_BOUNDARY_OR_INTERIOR_OPERATION(boundary, BOUNDARY)
DEFINE_BOUNDARY_OR_INTERIOR_OPERATION(interior, INTERIOR)

#undef DEFINE_BOUNDARY_OR_INTERIOR_OPERATION

// ### Functions in the `polygons` Library

// A simple polygon is created from a sequence of at least 3 poionts.

static SCM simple_polygon(SCM s, SCM t, SCM u, SCM rest)
{
    std::vector<Point_2> v;

    pop_arguments(1, {s, t, u}, v);
    pop_arguments(4, rest, v);

    return to_scheme<Boxed_polygon>(POLYGON(std::move(v)));
}

// ### Functions in the `polyhedra` Library

// Octahedra and regular bipyramids can accept a single height
// parameter, in which case they're symmetric wrt. the XY plane, or
// different heights may be specified.

static SCM octahedron(SCM s, SCM t, SCM u, SCM v)
{
    FT a, b, c, d;

    pop_arguments(1, {s, t, u}, std::tie(a, b, c));

    if (pop_optional(4, v, d)) {
        return to_scheme<Boxed_polyhedron>(OCTAHEDRON(a, b, c, d));
    } else {
        return to_scheme<Boxed_polyhedron>(OCTAHEDRON(a, b, c));
    }
}

static SCM regular_bipyramid(SCM s, SCM t, SCM u, SCM v)
{
    FT a, b, c;
    int k;

    pop_arguments(1, {s, t, u}, std::tie(k, a, b));

    if (pop_optional(4, v, c)) {
        return to_scheme<Boxed_polyhedron>(REGULAR_BIPYRAMID(k, a, b, c));
    } else {
        return to_scheme<Boxed_polyhedron>(REGULAR_BIPYRAMID(k, a, b));
    }
}

// ### Functions in the `operations` Library

// We start with some trivial functions, which are mostly necessary
// because they operate on boxed pointers and we have to call
// `std::visit` on them.

static Boxed_polygon offset(Boxed_polygon &p, const FT &delta)
{
    return std::visit(
        [&delta](auto &&x) {
            return OFFSET(x, delta);
        }, p);
}

// Extrusions operate on a polygon and an arbitrary number of
// transformations.  (If no transformations are specified,
// `EXTRUSIONS` implicitly inserts a null translation.)

static SCM extrusion(SCM s, SCM rest)
{
    Boxed_polygon p;

    pop_argument(1, s, p);

    std::vector<Aff_transformation_3> v;

    pop_arguments(2, rest, v);

    return to_scheme<Boxed_polyhedron>(
        std::visit(
            [&v](auto &&x) {
                return EXTRUSION(x, std::move(v));
            }, p));
}

// Convex hulls are defined in 2D or 3D and in each case, they
// ultimately operate on points.  Nevertheless, it is usually
// conventient to define them on polygons and polyhedra as well,
// meaning that you take the hull of the points they're made of.  As a
// result we either expect a mixture of 2D ponints and polygons, or 3D
// points and polyhedra.

static SCM hull(SCM args)
{
    std::shared_ptr<Polyhedron_hull_operation> p;
    std::shared_ptr<Polygon_hull_operation> q;

    int i = 1;

    while (true) {
        // We open a polygon or polyhedron hull operation, depending
        // on the first argument and push back all arguments.

        if (Boxed_polyhedron x;
            !q && try_pop_argument(i, args, x)) {
            if (!p) {
                p = POLYHEDRON_HULL_OPEN();
            }

            std::visit(
                [&p](auto &&y) {
                    p->push_back(y);
                }, x);

            i++;
        } else if (Point_3 x;
                   !q && try_pop_argument(i, args, x)) {
            if (!p) {
                p = POLYHEDRON_HULL_OPEN();
            }

            p->push_back(x);
            i++;
        } else if (Boxed_polygon x;
                   !p && try_pop_argument(i, args, x)) {
            if (!q) {
                q = POLYGON_HULL_OPEN();
            }

            std::visit(
                [&q](auto &&y) {
                    q->push_back(CONVERT_TO<Polygon_set>(y));
                }, x);

            i++;
        } else if (Point_2 x;
                   !p && try_pop_argument(i, args, x)) {
            if (!q) {
                q = POLYGON_HULL_OPEN();
            }

            q->push_back(x);
            i++;
        } else if (scm_is_null(args)) {
            // When all arguments have been consumed, we close and
            // return the hull operation.

            if (p) {
                return to_scheme<Boxed_polyhedron>(POLYHEDRON_HULL_CLOSE(p));
            } else if (q) {
                return to_scheme<Boxed_polygon>(POLYGON_HULL_CLOSE(q));
            } else {
                throw wrong_num_args_exception();
            }
        } else if (p) {
            throw wrong_type_exception(
                i, scm_car(args), "polyhedron or point-3d");
        } else if (q) {
            throw wrong_type_exception(
                i, scm_car(args), "polygon or point-2d");
        } else {
            throw wrong_type_exception(
                i, scm_car(args), "polygon, polyhedron or point");
        }
    }
}

// Minkowski sums are defined:

static SCM minkowski_sum(SCM s, SCM t)
{
    //   1. on polyhedra and

    if (Boxed_polyhedron a;
        try_pop_argument(1, s, a)) {
        Boxed_polyhedron b;

        pop_argument(2, t, b);

        return std::visit(
            [](auto &&x, auto &&y) {
                return to_scheme<Boxed_polyhedron>(MINKOWSKI_SUM(x, y));
            }, a, b);
    }

    //   2. on polygons.

    if (Boxed_polygon a;
        try_pop_argument(1, s, a)) {
        Boxed_polygon b;

        pop_argument(2, t, b);

        return std::visit(
            [](auto &&x, auto &&y) {
                return to_scheme<Boxed_polygon>(MINKOWSKI_SUM(x, y));
            }, a, b);
    }

    throw wrong_type_exception(1, s, "polygon or polyhedron");
}

// Boolean set operations are defined on different types of values,
// including volumes, selections, polygons and polyhedra.  The Scheme
// functions need to accept any number of arguments (of the same
// type), but the operations for polygons and polyhedra are binary
// operations.

// We handle volumes and selections separately and for geometry, we
// create left-associative chains of binary boolean operations.
// (Associativity is important only in the case of difference
// operations of course.)

#define HANDLE_SELECTION_TYPE(OP, T)                                    \
if (std::shared_ptr<T> p; try_pop_argument(1, s, p)) {                  \
    std::vector<std::shared_ptr<T>> v;                                  \
    v.push_back(p);                                                     \
    pop_arguments(2, rest, v);                                          \
                                                                        \
    return to_scheme<std::shared_ptr<T>>(OP(std::move(v)));             \
}

#define DEFINE_FOLDED_OPERATION(NAME, ...)      \
                                                \
static SCM NAME ##_many(SCM s, SCM rest)        \
{                                               \
    const SCM t = NAME ##_2(s, scm_car(rest));  \
    const SCM u = scm_cdr(rest);                \
                                                \
    if (scm_is_null(u)) {                       \
        return t;                               \
    }                                           \
                                                \
    return NAME ##_many(t, u);                  \
}                                               \
                                                \
static SCM NAME ##_any(SCM s, SCM rest)         \
{                                               \
    if (scm_is_null(rest)) {                    \
        return s;                               \
    }                                           \
                                                \
    __VA_ARGS__                                 \
                                                \
    return NAME ##_many(s, rest);               \
}

#define DEFINE_SET_OPERATION(NAME, OP)                                  \
static SCM NAME ##_2(SCM s, SCM t)                                      \
{                                                                       \
    if (scm_is_null(t)) {                                               \
        return s;                                                       \
    }                                                                   \
                                                                        \
    if (Boxed_polyhedron a;                                             \
        try_pop_argument(1, s, a)) {                                    \
        Boxed_polyhedron b;                                             \
        pop_argument(2, t, b);                                          \
                                                                        \
        return to_scheme<Boxed_polyhedron>(                             \
            std::visit(                                                 \
                make_polyhedron_boolean_visitor(OP), a, b));            \
    } else if (Boxed_polygon a;                                         \
               try_pop_argument(1, s, a)) {                             \
        Boxed_polygon b;                                                \
        pop_argument(2, t, b);                                          \
                                                                        \
        return std::visit(                                              \
            [](auto &&x, auto &&y) {                                    \
                return to_scheme<Boxed_polygon>(OP(x, y));              \
            }, a, b);                                                   \
    } else {                                                            \
        throw wrong_type_exception(1, s, "polygon or polyhedron");      \
    }                                                                   \
}                                                                       \
                                                                        \
DEFINE_FOLDED_OPERATION(                                                \
    NAME, {                                                             \
        HANDLE_SELECTION_TYPE(OP, Bounding_volume);                     \
        HANDLE_SELECTION_TYPE(OP, Vertex_selector);                     \
        HANDLE_SELECTION_TYPE(OP, Face_selector);                       \
        HANDLE_SELECTION_TYPE(OP, Edge_selector);                       \
    })

DEFINE_SET_OPERATION(union, JOIN)
DEFINE_SET_OPERATION(difference, DIFFERENCE)
DEFINE_SET_OPERATION(intersection, INTERSECTION)

#undef HANDLE_SELECTION_TYPE
#undef DEFINE_SET_OPERATION

// A polyhedron can be corefined with:

static SCM corefine_2(SCM s, SCM t)
{
    if (scm_is_eq(t, SCM_UNDEFINED)) {
        return s;
    }

    Boxed_polyhedron p;
    pop_argument(1, s, p);

    //   1. another polyhedron, or

    if (Boxed_polyhedron q;
        try_pop_argument(2, t, q)) {
        return std::visit(
            [](auto &&x, auto &&y) {
                return to_scheme<Boxed_polyhedron>(COREFINE(x, y));
            }, p, q);
    }

    //   2. a plane.

    if (Plane_3 pi;
        try_pop_argument(2, t, pi)) {
        return std::visit(
            [&pi](auto &&x) {
                return to_scheme<Boxed_polyhedron>(COREFINE(x, pi));
            }, p);
    }

    throw wrong_type_exception(1, s, "polyhedron, or plane-3d");
}

DEFINE_FOLDED_OPERATION(corefine)

// Below `make_polyhedron_clip_visitor` provides a visitor that
// will perform the clip via Nef or corefinement operations.  Ref:
// Selecting Between Nef and Corefinement Operations.

static SCM clip_2(SCM s, SCM t)
{
    Boxed_polyhedron a;
    Plane_3 pi;

    pop_argument(1, s, a);
    pop_argument(2, t, pi);

    return to_scheme<Boxed_polyhedron>(
        std::visit(make_polyhedron_clip_visitor(pi), a));
}

DEFINE_FOLDED_OPERATION(clip)

#undef DEFINE_FOLDED_OPERATION

// Subdivision operations take a single polyhedron and an integer
// number of iterations.

#define DEFINE_SUBDIVISION_OPERATION(NAME_SUFFIX, SUFFIX, OP)   \
static SCM subdivide_ ##SUFFIX(SCM s, SCM t)                    \
{                                                               \
    Boxed_polyhedron a;                                         \
    int n;                                                      \
                                                                \
    pop_argument(1, s, a);                                      \
    pop_argument(2, t, n);                                      \
                                                                \
    return std::visit(                                          \
        [&n](auto &&x) {                                        \
            return to_scheme<Boxed_polyhedron>(OP(x, n));       \
        }, a);                                                  \
}

DEFINE_SUBDIVISION_OPERATION("loop", loop, LOOP)
DEFINE_SUBDIVISION_OPERATION("catmull-clark", catmull_clark, CATMULL_CLARK)
DEFINE_SUBDIVISION_OPERATION("doo-sabin", doo_sabin, DOO_SABIN)
DEFINE_SUBDIVISION_OPERATION("sqrt-3", sqrt_3, SQRT_3)

#undef DEFINE_SUBDIVISION_OPERATION


// All mesh opeations work on polyhedra.  Some of them take an
// optional selector and a number:

#define DEFINE_MESH_OPERATION(NAME, OP, T)                              \
static SCM NAME(SCM s, SCM rest)                                        \
{                                                                       \
    Boxed_polyhedron a;                                                 \
    std::shared_ptr<T> p;                                               \
    FT l;                                                               \
                                                                        \
    pop_argument(1, s, a);                                              \
    int i = 2;                                                          \
    i += try_pop_argument(i, rest, p);                                  \
    pop_argument(i, rest, l);                                           \
                                                                        \
    return std::visit(                                                  \
        [&p, &l](auto &&x) {                                            \
            return to_scheme<Boxed_polyhedron>(OP(x, p, l));            \
        }, a);                                                          \
}

DEFINE_MESH_OPERATION(perturb, PERTURB, Vertex_selector)
DEFINE_MESH_OPERATION(refine, REFINE, Face_selector)

#undef DEFINE_MESH_OPERATION

// Others take a couple of optional selectors, a number and an
// integer:

#define DEFINE_MESH_OPERATION(FUNC, OP, T)                              \
static SCM FUNC(SCM s, SCM rest)                                        \
{                                                                       \
    Boxed_polyhedron a;                                                 \
    std::shared_ptr<Face_selector> p;                                   \
    std::shared_ptr<T> q;                                               \
    FT l;                                                               \
    int n = 1;                                                          \
                                                                        \
    pop_argument(1, s, a);                                              \
                                                                        \
    int i = 2;                                                          \
    i += try_pop_argument(i, rest, p);                                  \
    i += try_pop_argument(i, rest, q);                                  \
    pop_argument(i, rest, l);                                           \
    pop_optional(i + 1, rest, n);                                       \
                                                                        \
    return std::visit(                                                  \
        [&p, &q, &l, &n](auto &&x) {                                    \
            return to_scheme<Boxed_polyhedron>(OP(x, p, q, l, n));      \
        }, a);                                                          \
}

DEFINE_MESH_OPERATION(remesh, REMESH, Edge_selector)
DEFINE_MESH_OPERATION(smooth_shape, SMOOTH_SHAPE, Vertex_selector)

#undef DEFINE_MESH_OPERATION

// And some don't follow any of the preceding patterns.  Fairing
// requires a selector and takes an optional integer.

static SCM fair(SCM s, SCM t, SCM u)
{
    Boxed_polyhedron a;
    std::shared_ptr<Vertex_selector> p;

    pop_argument(1, s, a);
    pop_argument(2, t, p);

    int m = 1;
    pop_optional(3, u, m);

    return std::visit(
        [&p, &m](auto &&x) {
            return to_scheme<Boxed_polyhedron>(FAIR(x, p, m));
        }, a);
}

// Deflations work on polyhedra.  They also take:

static SCM deflate(SCM s, SCM rest)
{
    Boxed_polyhedron a;
    std::shared_ptr<Vertex_selector> p;

    pop_argument(1, s, a);

    //   1. an optional constraining selector,

    int i = 2;
    i += try_pop_argument(i, rest, p);

    //   2. an integer number of steps and

    int m;
    pop_argument(i++, rest, m);

    //   3. a couple of optional parameters.

    FT w_H = FT::ET(1, 10), w_M;

    i += pop_optional(i, rest, w_H);
    pop_optional(i, rest, w_M);

    return std::visit(
        [&p, &m, &w_H, &w_M](auto &&x) {
            return to_scheme<Boxed_polyhedron>(DEFLATE(x, p, m, w_H, w_M));
        }, a);
}

// Deformations are more complex.  They accept an optional selector
// for the ROI (defaulting to the whole mesh if omitted) and a number
// of (select, transformation) pairs.

static SCM deform(SCM s, SCM rest)
{
    Boxed_polyhedron a;
    std::shared_ptr<Vertex_selector> p, q;
    std::vector<std::pair<std::shared_ptr<Vertex_selector>,
                          Aff_transformation_3>> v;

    pop_argument(1, s, a);

    // We tentatively pop a selector.  If the next argument is a
    // transformation, it part of a control pair, otherwise it was the
    // ROI.

    int i = 2;
    i += try_pop_argument(i, rest, p);

    while (true) {
        Aff_transformation_3 T;

        // Now we try to pop control pairs.

        while (try_pop_argument(i, rest, q)) {
            pop_argument(i + 1, rest, T);
            v.push_back(std::pair(q, T));
            i += 2;
        }

        // If we read in any pairs above, then the first selector
        // (`p`) was the ROI and all is well, otherwise it was the
        // first control region selector and we need to pop the rest.

        if (!v.empty()) {
            break;
        }

        // Since the first selector (`p`) was a control selector, we
        // expect a transformation at this point.

        pop_argument(i++, rest, T);
        v.push_back(std::pair(p, T));

        p = nullptr;
    }

    // We also have to deal with a couple of numberic parameters.

    FT tau;
    pop_argument(i++, rest, tau);

    unsigned int m = (tau == 0) ? 10 : std::numeric_limits<unsigned int>::max();
    pop_optional(i, rest, m);

    return std::visit(
        [&p, &v, &tau, &m](auto &&x) {
            return to_scheme<Boxed_polyhedron>(
                DEFORM(x, p, std::move(v), tau, m));
        }, a);
}

// This operation extracts connected components (i.e. "separate
// parts") from polygon sets and polyhedra.

static SCM components(SCM s, SCM rest)
{
    std::vector<int> v;
    pop_arguments(2, rest, v);

    if (Boxed_polygon x; try_pop_argument(1, s, x)) {
        return std::visit(
                [&v](auto &&y) {
                    return to_scheme<Boxed_polygon>(COMPONENTS(y, v));
                }, x);
    }

    if (Boxed_polyhedron x; try_pop_argument(1, s, x)) {
        return std::visit(
                [&v](auto &&y) {
                    return to_scheme<Boxed_polyhedron>(
                        COMPONENTS(y, v));
                }, x);
    }

    throw wrong_type_exception(1, s, "polygon or polyhedron");
}

// Chamfer and fillet operations share much of their machinery and are
// handled uniformly.  They take an optional edge selector and one or
// more numbers.

// Actually, chamfer operations normally take 2 numbers, but omitting
// the second implies a symmetric chamfer.

#define DEFINE_CHAMFERING_OPERATION(NAME, OP, MODE)                     \
static SCM NAME(SCM s, SCM rest)                                        \
{                                                                       \
    Boxed_polyhedron a;                                                 \
    std::shared_ptr<Edge_selector> p;                                   \
    FT l, m;                                                            \
                                                                        \
    pop_argument(1, s, a);                                              \
    int i = 2;                                                          \
    i += try_pop_argument(i, rest, p);                                  \
    pop_argument(i++, rest, l);                                         \
    m = l;                                                              \
    try_pop_argument(i, rest, m);                                       \
                                                                        \
    return to_scheme<Boxed_polyhedron>(                                 \
        std::visit(make_polyhedron_chamfer_visitor(OP, p, l, m, MODE), a)); \
}

DEFINE_CHAMFERING_OPERATION(
    chamfer_inner, CHAMFER, Chamfering_operation_mode::INNER)
DEFINE_CHAMFERING_OPERATION(
    chamfer_outer, CHAMFER, Chamfering_operation_mode::OUTER)

DEFINE_CHAMFERING_OPERATION(
    make_inner_chamfer, MAKE_CHAMFER, Chamfering_operation_mode::INNER)
DEFINE_CHAMFERING_OPERATION(
    make_outer_chamfer, MAKE_CHAMFER, Chamfering_operation_mode::OUTER)

#undef DEFINE_CHAMFERING_OPERATION

// Fillets always take one number, the radius.

#define DEFINE_FILLETING_OPERATION(NAME, OP, MODE)                      \
static SCM NAME(SCM s, SCM rest)                                        \
{                                                                       \
    Boxed_polyhedron a;                                                 \
    std::shared_ptr<Edge_selector> p;                                   \
    FT r;                                                               \
                                                                        \
    pop_argument(1, s, a);                                              \
    int i = 2;                                                          \
    i += try_pop_argument(i, rest, p);                                  \
    pop_argument(i++, rest, r);                                         \
                                                                        \
    return to_scheme<Boxed_polyhedron>(                                 \
        std::visit(make_polyhedron_chamfer_visitor(OP, p, r, MODE), a)); \
}

DEFINE_FILLETING_OPERATION(
    fillet_inner, FILLET, Chamfering_operation_mode::INNER)
DEFINE_FILLETING_OPERATION(
    fillet_outer, FILLET, Chamfering_operation_mode::OUTER)

DEFINE_FILLETING_OPERATION(
    make_inner_fillet, MAKE_FILLET, Chamfering_operation_mode::INNER)
DEFINE_FILLETING_OPERATION(
    make_outer_fillet, MAKE_FILLET, Chamfering_operation_mode::OUTER)

#undef DEFINE_FILLETING_OPERATION

// Color operations apply color to specific elements of a mesh
// (vertices, edges, or faces), either uniformly, or on specific
// selections.

#define DEFINE_COLOR_OPERATION(FUNC, OP)                                \
static SCM FUNC(SCM s, SCM rest)                                        \
{                                                                       \
    if (scm_is_null(rest) || scm_is_null(scm_cdr(rest))) {              \
        int i = 0;                                                      \
        try_pop_argument(2, rest, i);                                   \
                                                                        \
        if (Boxed_polygon p; try_pop_argument(1, s, p)) {               \
            return std::visit(                                          \
                [&i](auto &&x) {                                        \
                    return to_scheme<Boxed_polyhedron>(OP(x, i));       \
                }, p);                                                  \
        } else if (Boxed_polyhedron p; try_pop_argument(1, s, p)) {     \
            return std::visit(                                          \
                [&i](auto &&x) {                                        \
                    return to_scheme<Boxed_polyhedron>(OP(x, i));       \
                }, p);                                                  \
        }                                                               \
    } else if (FT v[] = {0, 0, 0, 1};                                   \
               try_pop_argument(2, rest, v[0])) {                       \
        pop_argument(3, rest, v[1]);                                    \
        pop_argument(4, rest, v[2]);                                    \
        pop_optional(5, rest, v[3]);                                    \
                                                                        \
        if (Boxed_polygon p; try_pop_argument(1, s, p)) {               \
            return std::visit(                                          \
                [&v](auto &&x) {                                        \
                    return to_scheme<Boxed_polyhedron>(                 \
                        OP(x, v[0], v[1], v[2], v[3]));                 \
                }, p);                                                  \
        } else if (Boxed_polyhedron p; try_pop_argument(1, s, p)) {     \
            return std::visit(                                          \
                [&v](auto &&x) {                                        \
                    return to_scheme<Boxed_polyhedron>(                 \
                        OP(x, v[0], v[1], v[2], v[3]));                 \
                }, p);                                                  \
        }                                                               \
    } else {                                                            \
        throw wrong_type_exception(                                     \
            2, scm_car(rest), "rational number or integer");            \
    }                                                                   \
                                                                        \
    throw wrong_type_exception(1, s, "polygon or polyhedron");          \
}

DEFINE_COLOR_OPERATION(color_vertices, COLOR_VERTICES)
DEFINE_COLOR_OPERATION(color_faces, COLOR_FACES)

#undef DEFINE_COLOR_OPERATION

static SCM color_selection(SCM s, SCM t, SCM rest)
{
    std::variant<std::shared_ptr<Face_selector>,
                 std::shared_ptr<Vertex_selector>,
                 std::shared_ptr<Edge_selector>> q;

    if (std::shared_ptr<Face_selector> x;
        try_pop_argument(2, t, x)) {
        q = x;
    } else if (std::shared_ptr<Vertex_selector> x;
               try_pop_argument(2, t, x)) {
        q = x;
    } else if (std::shared_ptr<Edge_selector> x;
               try_pop_argument(2, t, x)) {
        q = x;
    } else {
        throw wrong_type_exception(2, t, "selector");
    }

    if (scm_is_null(rest) || scm_is_null(scm_cdr(rest))) {
        int i = 0;
        try_pop_argument(3, rest, i);

        if (Boxed_polygon p; try_pop_argument(1, s, p)) {
            return std::visit(
                [&i](auto &&x, auto &&y) {
                    return to_scheme<Boxed_polyhedron>(
                        COLOR_SELECTION(x, y, i));
                }, p, q);
        } else if (Boxed_polyhedron p; try_pop_argument(1, s, p)) {
            return std::visit(
                [&i](auto &&x, auto &&y) {
                    return to_scheme<Boxed_polyhedron>(
                        COLOR_SELECTION(x, y, i));
                }, p, q);
        }
    } else if (FT v[] = {0, 0, 0, 1};
               try_pop_argument(3, rest, v[0])) {
        pop_argument(4, rest, v[1]);
        pop_argument(5, rest, v[2]);
        pop_optional(6, rest, v[3]);

        if (Boxed_polygon p; try_pop_argument(1, s, p)) {
            return std::visit(
                [&v](auto &&x, auto &&y) {
                    return to_scheme<Boxed_polyhedron>(
                        COLOR_SELECTION(x, y, v[0], v[1], v[2], v[3]));
                }, p, q);
        } else if (Boxed_polyhedron p; try_pop_argument(1, s, p)) {
            return std::visit(
                [&v](auto &&x, auto &&y) {
                    return to_scheme<Boxed_polyhedron>(
                        COLOR_SELECTION(x, y, v[0], v[1], v[2], v[3]));
                }, p, q);
        }
    } else {
        throw wrong_type_exception(
            2, scm_car(rest), "rational number or integer");
    }

    throw wrong_type_exception(1, s, "polygon or polyhedron");
}

// ## Scheme Library Definitions

// We're now ready to define our Scheme libraries.  This is
// straightforard enough, except that we need to wrap all functions in
// a try-catch block as discussed in ref: Handling Scheme Function
// Arguments.  We use the following template for this purpose.

template <typename T>
struct function_wrapper;

template <typename R, typename... Args>
struct function_wrapper<R(* const)(Args...)> {
    using return_type = R;
    using argument_types = std::tuple<Args...>;

    template <const char *N, auto F>
    struct wrapper {
        static R call(Args... args) {
            try {
                return F(std::forward<Args>(args)...);
            } catch (const wrong_type_exception &e) {
                scm_wrong_type_arg_msg(
                    N, e.argnum, e.bad_value, e.expected);
            } catch (const wrong_num_args_exception &e) {
                scm_error_num_args_subr(N);
            }

            assert_not_reached();
        }
    };
};

// Next we define some macros exporting Scheme types and subroutines.

#define DEFINE_FOREIGN_TYPE(NAME, T)                                    \
    foreign_type<T> = scm_make_foreign_object_type(                     \
        scm_from_latin1_symbol(NAME),                                   \
        scm_list_1(scm_from_latin1_symbol("data")),                     \
        finalize<T>);

#define DEFINE_FOREIGN_PROC(NAME, N, M, L, ...)                         \
    {                                                                   \
        static constexpr const char s[] = NAME;                         \
        constexpr auto f = __VA_ARGS__;                                 \
        scm_c_define_gsubr(                                             \
            NAME, N, M, L, reinterpret_cast<scm_t_subr>(                \
                function_wrapper<decltype(f)>::wrapper<s, f>::call));   \
        scm_c_export(NAME, NULL);                                       \
    }

#define DEFINE_FOREIGN_PRIMITIVE(NAME, ...)                             \
    {                                                                   \
        static constexpr const char s[] = NAME;                         \
        constexpr auto f = make_primitive<__VA_ARGS__>;                 \
                                                                        \
        scm_c_define_gsubr(                                             \
            s, 0, 0, 1,                                                 \
            reinterpret_cast<scm_t_subr>(                               \
                function_wrapper<decltype(f)>::wrapper<s, f>::call));   \
        scm_c_export(s, NULL);                                          \
    }

// Finally we define functions exporting the core symbols for each
// library.  These are then further augmented in Scheme code located
// in `scheme/gamma/*.sld` files.

static void define_base(void *)
{
    DEFINE_FOREIGN_PROC(
        "set-projection-tolerance!", 0, 1, 0,
        set_tolerance<Tolerances::projection>);
    DEFINE_FOREIGN_PROC(
        "set-curve-tolerance!", 0, 1, 0,
        set_tolerance<Tolerances::curve>);
    DEFINE_FOREIGN_PROC(
        "set-sine-tolerance!", 0, 1, 0,
        set_tolerance<Tolerances::sine>);

    DEFINE_FOREIGN_PROC("%define-option", 2, 0, 0, define_option);
    DEFINE_FOREIGN_PROC("output", 0, 0, 1, output);
    DEFINE_FOREIGN_PROC("point", 2, 1, 0, point);
    DEFINE_FOREIGN_PRIMITIVE("plane", plane, Plane_3, 4);

    DEFINE_FOREIGN_TYPE("point-2d", Point_2);
    DEFINE_FOREIGN_TYPE("point-3d", Point_3);
    DEFINE_FOREIGN_TYPE("plane-3d", Plane_3);

    DEFINE_FOREIGN_TYPE("transformation-2d", Aff_transformation_2);
    DEFINE_FOREIGN_TYPE("transformation-3d", Aff_transformation_3);

    DEFINE_FOREIGN_TYPE("bounding-volume", std::shared_ptr<Bounding_volume>);

    DEFINE_FOREIGN_TYPE("face-selector", std::shared_ptr<Face_selector>);
    DEFINE_FOREIGN_TYPE("vertex-selector", std::shared_ptr<Vertex_selector>);
    DEFINE_FOREIGN_TYPE("edge-selector", std::shared_ptr<Edge_selector>);

    DEFINE_FOREIGN_TYPE("polygon", Boxed_polygon);
    DEFINE_FOREIGN_TYPE("polyhedron", Boxed_polyhedron);
}

static void define_write(void *)
{
    DEFINE_FOREIGN_PROC(
        "print-note", 0, 0, 1, print_message<Operation::NOTE>);
    DEFINE_FOREIGN_PROC(
        "print-warning", 0, 0, 1, print_message<Operation::WARNING>);
    DEFINE_FOREIGN_PROC(
        "print-error", 0, 0, 1, print_message<Operation::ERROR>);
}

static void define_transformation(void *)
{
    DEFINE_FOREIGN_PROC(
        "translation", 2, 1, 0,
        translation_2_3<TRANSLATION_3, TRANSLATION_2>);
    DEFINE_FOREIGN_PROC(
        "scaling", 2, 1, 0,
        scaling_2_3<SCALING_3, SCALING_2>);
    DEFINE_FOREIGN_PROC("rotation", 1, 0, 1, rotation);

    DEFINE_FOREIGN_PROC("transformation-apply", 1, 1, 0, transformation_apply);

    DEFINE_FOREIGN_PROC("flush", 3, 1, 0, flush);
}

static void define_volumes(void *)
{
    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-plane", BOUNDING_PLANE<>,
        std::shared_ptr<Bounding_volume>, 4);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-halfspace", BOUNDING_HALFSPACE<>,
        std::shared_ptr<Bounding_volume>, 4);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-halfspace-interior", BOUNDING_HALFSPACE_INTERIOR<>,
        std::shared_ptr<Bounding_volume>, 4);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-box", BOUNDING_BOX<>,
        std::shared_ptr<Bounding_volume>, 3);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-box-boundary", BOUNDING_BOX_BOUNDARY<>,
        std::shared_ptr<Bounding_volume>, 3);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-box-interior", BOUNDING_BOX_INTERIOR<>,
        std::shared_ptr<Bounding_volume>, 3);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-sphere", BOUNDING_SPHERE<>,
        std::shared_ptr<Bounding_volume>, 1);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-sphere-boundary", BOUNDING_SPHERE_BOUNDARY<>,
        std::shared_ptr<Bounding_volume>, 1);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-sphere-interior", BOUNDING_SPHERE_INTERIOR<>,
        std::shared_ptr<Bounding_volume>, 1);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-cylinder",
        BOUNDING_CYLINDER<>,
        std::shared_ptr<Bounding_volume>, 2);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-cylinder-boundary",
        BOUNDING_CYLINDER_BOUNDARY<>,
        std::shared_ptr<Bounding_volume>, 2);

    DEFINE_FOREIGN_PRIMITIVE(
        "bounding-cylinder-interior",
        BOUNDING_CYLINDER_INTERIOR<>,
        std::shared_ptr<Bounding_volume>, 2);
}

static void define_selection(void *)
{
    DEFINE_FOREIGN_PROC("vertices-in", 1, 0, 0, vertices_in);
    DEFINE_FOREIGN_PROC("faces-in", 1, 0, 0, faces_in);
    DEFINE_FOREIGN_PROC("faces-partially-in", 1, 0, 0, faces_partially_in);
    DEFINE_FOREIGN_PROC("edges-in", 1, 0, 0, edges_in);
    DEFINE_FOREIGN_PROC("edges-partially-in", 1, 0, 0, edges_partially_in);

    DEFINE_FOREIGN_PROC("expand-selection", 2, 0, 0, relative_selection<+1>);
    DEFINE_FOREIGN_PROC("contract-selection", 2, 0, 0, relative_selection<-1>);

    DEFINE_FOREIGN_PRIMITIVE(
        "edges-by-sharpness-angle", EDGES_BY_SHARPNESS_ANGLE<>,
        std::shared_ptr<Edge_selector>, FT);

    DEFINE_FOREIGN_PRIMITIVE(
        "edges-by-sharpness-mode", EDGES_BY_SHARPNESS_MODE<>,
        std::shared_ptr<Edge_selector>, int);

    DEFINE_FOREIGN_PROC(
        "faces-by-sharpness-angle", 1, 0, 1, faces_by_sharpness_angle);
    DEFINE_FOREIGN_PROC(
        "faces-by-sharpness-mode", 1, 0, 1, faces_by_sharpness_mode);

    DEFINE_FOREIGN_PROC("faces-through-segment", 1, 1, 0, faces_through_segment);
    DEFINE_FOREIGN_PROC("faces-through-ray", 1, 1, 0, faces_through_ray);
    DEFINE_FOREIGN_PROC("faces-through-line", 1, 1, 0, faces_through_line);
    DEFINE_FOREIGN_PROC("faces-through-plane", 1, 1, 0, faces_through_plane);

    DEFINE_FOREIGN_PROC("edges-through-segment", 1, 1, 0, edges_through_segment);
    DEFINE_FOREIGN_PROC("edges-through-ray", 1, 1, 0, edges_through_ray);
    DEFINE_FOREIGN_PROC("edges-through-line", 1, 1, 0, edges_through_line);
    DEFINE_FOREIGN_PROC("edges-through-plane", 1, 1, 0, edges_through_plane);
}

static void define_polygons(void *)
{
    DEFINE_FOREIGN_PROC("simple-polygon", 3, 0, 1, simple_polygon);
    DEFINE_FOREIGN_PRIMITIVE(
        "regular-polygon", REGULAR_POLYGON<>, Boxed_polygon, int, FT);
    DEFINE_FOREIGN_PRIMITIVE(
        "isosceles-triangle", ISOSCELES_TRIANGLE<>, Boxed_polygon, 2);
    DEFINE_FOREIGN_PRIMITIVE(
        "right-triangle", RIGHT_TRIANGLE<>, Boxed_polygon, 2);
    DEFINE_FOREIGN_PRIMITIVE("rectangle", RECTANGLE<>, Boxed_polygon, 2);
    DEFINE_FOREIGN_PRIMITIVE("circle", CIRCLE<>, Boxed_polygon, 1);
    DEFINE_FOREIGN_PRIMITIVE(
        "circular-sector", CIRCULAR_SECTOR<>, Boxed_polygon, 2);
    DEFINE_FOREIGN_PRIMITIVE(
        "circular-segment", CIRCULAR_SEGMENT<>, Boxed_polygon, 2);
    DEFINE_FOREIGN_PRIMITIVE("ellipse", ELLIPSE<>, Boxed_polygon, 2);
    DEFINE_FOREIGN_PRIMITIVE(
        "elliptic-sector", ELLIPTIC_SECTOR<>, Boxed_polygon, 3);
}

static void define_polyhedra(void *)
{
    DEFINE_FOREIGN_PROC("octahedron", 3, 1, 0, octahedron);
    DEFINE_FOREIGN_PROC("regular-bipyramid", 3, 1, 0, regular_bipyramid);

    DEFINE_FOREIGN_PRIMITIVE(
        "tetrahedron", TETRAHEDRON<>, Boxed_polyhedron, 3);
    DEFINE_FOREIGN_PRIMITIVE(
        "square-pyramid", SQUARE_PYRAMID<>, Boxed_polyhedron, 3);
    DEFINE_FOREIGN_PRIMITIVE(
        "regular-pyramid", REGULAR_PYRAMID<>, Boxed_polyhedron, int, FT, FT);
    DEFINE_FOREIGN_PRIMITIVE("cuboid", CUBOID<>, Boxed_polyhedron, 3);
    DEFINE_FOREIGN_PRIMITIVE(
        "icosahedron", ICOSAHEDRON<>, Boxed_polyhedron, 1);
    DEFINE_FOREIGN_PRIMITIVE("sphere", SPHERE<>, Boxed_polyhedron, 1);
    DEFINE_FOREIGN_PRIMITIVE("cylinder", CYLINDER<>, Boxed_polyhedron, 2);
    DEFINE_FOREIGN_PRIMITIVE("prism", PRISM<>, Boxed_polyhedron, int, FT, FT);
}


static void define_operations(void *)
{
    DEFINE_FOREIGN_PRIMITIVE(
        "offset", offset, Boxed_polygon, Boxed_polygon, FT);

    DEFINE_FOREIGN_PROC("extrusion", 1, 0, 1, extrusion);
    DEFINE_FOREIGN_PROC("hull", 0, 0, 1, hull);
    DEFINE_FOREIGN_PROC("minkowski-sum", 2, 0, 0, minkowski_sum);

    DEFINE_FOREIGN_PROC("subdivide-catmull-clark",
                        2, 0, 0, subdivide_catmull_clark);
    DEFINE_FOREIGN_PROC("subdivide-doo-sabin", 2, 0, 0, subdivide_doo_sabin);
    DEFINE_FOREIGN_PROC("subdivide-loop", 2, 0, 0, subdivide_loop);
    DEFINE_FOREIGN_PROC("subdivide-sqrt-3", 2, 0, 0, subdivide_sqrt_3);

    DEFINE_FOREIGN_PROC("union", 1, 0, 1, union_any);
    DEFINE_FOREIGN_PROC("difference", 1, 0, 1, difference_any);
    DEFINE_FOREIGN_PROC("intersection", 1, 0, 1, intersection_any);
    DEFINE_FOREIGN_PROC("complement", 1, 0, 0, complement);
    DEFINE_FOREIGN_PROC("boundary", 1, 0, 0, boundary);
    DEFINE_FOREIGN_PROC("interior", 1, 0, 0, interior);
    DEFINE_FOREIGN_PROC("clip", 1, 0, 1, clip_any);
    DEFINE_FOREIGN_PROC("corefine", 1, 0, 1, corefine_any);

    DEFINE_FOREIGN_PROC("perturb", 1, 0, 1, perturb);
    DEFINE_FOREIGN_PROC("refine", 1, 0, 1, refine);
    DEFINE_FOREIGN_PROC("remesh", 1, 0, 1, remesh);
    DEFINE_FOREIGN_PROC("smooth-shape", 1, 0, 1, smooth_shape);
    DEFINE_FOREIGN_PROC("fair", 2, 1, 0, fair);
    DEFINE_FOREIGN_PROC("deform", 1, 0, 1, deform);
    DEFINE_FOREIGN_PROC("deflate", 1, 0, 1, deflate);
    DEFINE_FOREIGN_PROC("components", 1, 0, 1, components);

    DEFINE_FOREIGN_PROC("chamfer-inner", 1, 0, 1, chamfer_inner);
    DEFINE_FOREIGN_PROC("chamfer-outer", 1, 0, 1, chamfer_outer);

    DEFINE_FOREIGN_PROC("make-inner-chamfer", 1, 0, 1, make_inner_chamfer);
    DEFINE_FOREIGN_PROC("make-outer-chamfer", 1, 0, 1, make_outer_chamfer);

    DEFINE_FOREIGN_PROC("fillet-inner", 1, 0, 1, fillet_inner);
    DEFINE_FOREIGN_PROC("fillet-outer", 1, 0, 1, fillet_outer);

    DEFINE_FOREIGN_PROC("make-inner-fillet", 1, 0, 1, make_inner_fillet);
    DEFINE_FOREIGN_PROC("make-outer-fillet", 1, 0, 1, make_outer_fillet);

    DEFINE_FOREIGN_PROC("color-selection", 2, 0, 1, color_selection);
    DEFINE_FOREIGN_PROC("color-vertices", 1, 0, 1, color_vertices);
    DEFINE_FOREIGN_PROC("color-faces", 1, 0, 1, color_faces);
}

#undef DEFINE_FOREIGN_TYPE
#undef DEFINE_FOREIGN_PROC
#undef DEFINE_FOREIGN_PRIMITIVE

// ## Error Handling

// When Guile encounters an error, it throws an appropriate exception.
// We want to report this error to the user and, if it is not
// recoverable, clean up and abort the execution.  To this end, we run
// user code through `with-exception-handler` which invokes an
// exception handler on error, crucially, before the stack is unwound,
// allowing us to print information about the error, including a stack
// trace.

// The details are a bit more involved of course.  In any case, if
// there is an error, we're going to want to display information about
// where the error occured and what the matter was.  We turn to this
// first.

// ### Reporting Errors

// We want to have a uniform way of printing out errors and warnings,
// regardless of language front end, so we can't just invoke Guile's
// functions for printing exception information and stack traces.  We
// have to make our own.

// The functions below print messages of the form:

// ```
// foo.scm:7:3: in procedure 'foo':
// foo.scm:7:3: error: Unbound variable: bar
// ```

static void print_location(SCM s, SCM port)
{
    scm_puts(ANSI_COLOR(1, 37), port);
    scm_display(scm_cadr(s), port);
    scm_puts(ANSI_COLOR(0, ), port);
    scm_putc(':', port);
    scm_puts(ANSI_COLOR(1, 37), port);

    // Anchor: comment in `print_location`
    // Lifted from the Guile sources:

    //   > Lines are zero-indexed inside Guile, but users expect them
    //   > to be one-indexed. Columns, on the other hand, are
    //   > zero-indexed to both. Go figure.

    scm_display(scm_oneplus(scm_caddr(s)), port);
    scm_puts(ANSI_COLOR(0, ), port);
    scm_putc(':', port);
    scm_puts(ANSI_COLOR(1, 37), port);
    scm_display(scm_cdddr(s), port);
    scm_puts(ANSI_COLOR(0, ), port);
    scm_puts(": ", port);
}

static void print_procedure(SCM s, SCM port)
{
    scm_puts("in procedure '", port);
    scm_puts(ANSI_COLOR(0, 33), port);
    scm_display(scm_symbol_to_string(s), port);
    scm_puts(ANSI_COLOR(0, 37), port);
    scm_putc('\'', port);
}

// In what follows, we need to make numerous calls to procedures
// provided by Guile.  The macros below facilitate this.

#define CALL(N, MOD, NAME, ...)                                 \
scm_call_## N(scm_c_public_ref(MOD, NAME), ##__VA_ARGS__)

#define CALL_G(N, NAME, ...) CALL(N, "guile", NAME, ##__VA_ARGS__)
#define CALL_X(N, NAME, ...) CALL(N, "ice-9 exceptions", NAME, ##__VA_ARGS__)

// To provide information about the details of the exception itself,
// we need to consider each exception type separately.

static void print_exception_message(SCM exn, SCM port)
{
    // Syntax errors contain information about the form in which they
    // are detected and, usually, a message.

    if (scm_is_true(CALL_X(1, "syntax-error?", exn))) {
        SCM a = CALL_X(1, "syntax-error-form", exn);
        SCM b = CALL_X(1, "syntax-error-subform", exn);

        scm_puts("in ", port);
        scm_write(a, port);

        if (!scm_is_false(b)) {
            scm_puts(", at ", port);
            scm_write(b, port);
        }

        scm_puts(": ", port);

        if (scm_is_true(
                CALL_X(1, "exception-with-message?", exn))) {
            SCM c = CALL_X(1, "exception-message", exn);

            scm_display(c, port);
        } else {
            scm_puts("syntax error", port);
        }
    } else if (scm_is_true(
                   CALL_X(1, "exception-with-message?", exn))) {
        // There are many other kinds of exceptions, but we're only
        // concerned with whether they have a message for the user.

        SCM a = CALL_X(1, "exception-message", exn);

        // When they do, they usually follow the convention that the
        // message is a format string, which yields the full message
        // when applied to a list of values called the irritants (at
        // least Guile's exceptions do).

        if (scm_is_true(
                CALL_X(1, "exception-with-irritants?", exn))) {
            SCM b = CALL_X(1, "exception-irritants", exn);

            std::pair<SCM, SCM> ab = {a, b};

            // This may not always be the case, so when irritants are
            // provided, we first try to combine them with the message
            // through `scm_simple_format`.

            if (SCM c = scm_internal_catch(
                    SCM_BOOL_T,
                    [](void *data) -> SCM {
                        auto *p = static_cast<std::pair<SCM, SCM> *>(data);
                        return scm_simple_format(
                            SCM_BOOL_F, p->first, p->second);
                    },
                    static_cast<void *>(&ab),
                    [](void *data, SCM key, SCM args) {
                        return SCM_BOOL_F;
                    },
                    nullptr);
                scm_is_string(c)) {
                scm_display(c, port);
            } else {
                // If this fails, we just print message and irritants
                // separately.

                scm_display(a, port);
                scm_c_put_latin1_chars(
                    port, reinterpret_cast<const uint8_t *>(" "), 1);
                scm_write(b, port);
            }
        } else {
            // Naturally, when all we have is a message, we just print
            // it.

            scm_display(a, port);
        }
    } else {
        // Finally, if an exception doesn't even have a message, we
        // just print the exception as a Scheme value and hope it
        // makes sense.

        scm_c_put_latin1_chars(
            port,
            reinterpret_cast<const uint8_t *>("caught exception "), 17);
        scm_display(exn, port);
    }

    scm_newline(port);
}

// ### Catching Errors

// Returning from an exception handler continues the execution of the
// program from the point the exception was raised, if the exception
// was continuable.  If it was not continuable Guile raises a plain
// non-continuable exception "in the same dynamic environment as the
// handler".

// This means that the handler that that just exited won't handle the
// newly raised exception (and justly so, as it could lead to an
// infinite loop.

// We therefore wrap the loading code inside two handlers: the inner
// handler displays errors with backtrace and exits.  If the exception
// was not continuable the outer handler catches the exception raised
// as a result by Guile and aborts.

static SCM outer_handler(SCM exn)
{
    // If the exception we caught was not the plain `&non-continuable`
    // exception raised by Guile's runtime, then an exception occured
    // within the inner handler.  This shouldn't happen, but if it
    // does, print it for debugging purposes (debugging of the inner
    // handler that is).

    if (scm_is_false(
            scm_equal_p(
                exn,
                CALL_X(0, "make-non-continuable-error")))) {
        SCM s = scm_current_error_port();

        scm_newline(s);
        scm_puts("Exception in exception handler:\n", s);
        print_exception_message(exn, s);
        scm_newline(s);
    }

    // Now abort to the prompt's handler.  See below for more details.

    return CALL_G(
        2, "abort-to-prompt", scm_from_latin1_symbol("%gamma-prompt-tag"),
        SCM_BOOL_F);
}

static SCM inner_handler(SCM exn)
{
#if 0
    scm_display(exn, scm_current_error_port());
    scm_newline(scm_current_error_port());
#endif

    // The stack trace at the point the exception was raised will have
    // a couple of frames (literally 2) from the exception handler at
    // the top, followed by some frames from the executing program and
    // finally a bunch of frames from Guile's runtime at the bottom.

    // We're only interested in the middle section, so we shave the
    // two top frames explicitly.  Discarding the bottom frames is
    // more tricky, but the best way seems to be to run the user code
    // within a *prompt*.  Prompts have many uses, but we mainly use
    // one here to delimit the part of the stack that belongs to the
    // user's program.  We also abort to the prompt on error, as a way
    // to terminate execution an return control to C++.

    SCM s = scm_make_stack(
        SCM_BOOL_T,
        scm_list_2(
            scm_from_int(2), scm_from_latin1_symbol("%gamma-prompt-tag")));
    SCM t = scm_current_error_port();
    const int n = scm_to_int(scm_stack_length(s));

#if 0
    scm_display_backtrace(s, scm_current_error_port(), SCM_BOOL_F, SCM_BOOL_F);
    scm_newline(scm_current_error_port());
    return SCM_UNSPECIFIED;
#endif

    // Now we go through the frames and:

    for (int i = 0; i < n; i++) {
        SCM u = scm_stack_ref(s, scm_from_int(i));
        SCM v = scm_frame_source(u);
        SCM w = scm_frame_procedure_name(u);

        //   1. on the first frame, we print its location in full,
        //   followed by the exception's message and

        if (i == 0) {
            if (scm_is_true(w)) {
                if (scm_is_true(v)) {
                    print_location(v, t);
                }

                print_procedure(w, t);
                scm_puts(":\n", t);
            }

            if (scm_is_true(v)) {
                print_location(v, t);
            }

            if (scm_is_true(CALL_X(1, "warning?", exn))) {
                scm_puts(ANSI_COLOR(1, 33), t);
                scm_puts("warning", t);
            } else {
                scm_puts(ANSI_COLOR(0, 31), t);
                scm_puts("error", t);
            }

            scm_puts(ANSI_COLOR(0, 37), t);
            scm_puts(": ", t);

            // Format and print the exception message.

            print_exception_message(exn, t);
            scm_newline(t);
        }

        //   2. for all frames, we print a trace of the frame in the
        //   form:

        //   ```
        //   #0 in procedure 'foo', at foo.scm:7:3: (foo)
        //   #1 at foo.scm:8:17: (_)
        //   #2 at ice-9/boot-9.scm:4408:12: (_)
        //   ...
        //   ```

        scm_write_char(scm_integer_to_char(scm_from_char('#')), t);
        scm_write(scm_from_int(i), t);
        scm_write_char(scm_integer_to_char(scm_from_char(' ')), t);

        if (scm_is_true(w)) {
            print_procedure(w, t);

            if (scm_is_true(v)) {
                scm_puts(", ", t);
            }
        }

        if (scm_is_true(v)) {
            scm_puts("at ", t);
            print_location(v, t);
        } else if (scm_is_true(w)) {
            scm_puts(": ", t);
        }

        scm_write(scm_frame_call_representation(u), t);
        scm_newline(t);
    }

    return SCM_UNSPECIFIED;
}

#undef CALL
#undef CALL_G
#undef CALL_X

// ## The Scheme Evaluation Environment

// We will use `scm_with_guile` to run the code that initializes Guile
// and runs a Scheme program.  This function accepts a callback
// (`run_scheme_with_guile` below), so we need a way to pass it the
// relevant information (i.e. what file to execute and any command
// line arguments specified for it by the user.

// We use the following struct for that.

struct context {
    char *input, **first, **last;
    int result;
};

// It is often useful to quickly and temporarily create an output from
// intermediate results of a computation, in order to inspect them
// while debugging.  While one could simply wrap the relevant
// expression inside a call to `output`, the following function
// implements a reader extension that allows us to achieve this by
// simply prepending `#>` to it.  Alternatively, using `#>foo` will
// result in the equivalent of `(output "foo" ...)`.

static SCM read_hash_greater(SCM chr, SCM port)
{
    SCM s = scm_read(port);

    // As targets for the output, we allow either:

    //   1. integers (e.g `#>1`),

    if (scm_is_exact_integer(s)) {
        s = scm_list_2(s, scm_read(port));
    }

    //   2. symbols (e.g. `#>part`), or

    else if (scm_is_symbol(s)) {
        s = scm_list_2(scm_symbol_to_string(s), scm_read(port));
    }

    //   3. nothing.

    else {
        s = scm_cons(s, SCM_EOL);
    }

    return scm_cons(scm_from_latin1_symbol("output"), s);
}

// When the user passes `-` as one of the input files, we want to read
// code from the standard input.  Although Guiles `primitive-load`
// reads from a port internally, it exposes no way to pass a port
// directly, so we need the following simple reimplementation.

static SCM load_from_current_input(void)
{
    const SCM t = scm_variable_ref(scm_c_lookup("read-syntax"));

    SCM v;

    while (1) {
        const SCM u = scm_call_0 (t);

        if (SCM_EOF_OBJECT_P(u)) {
          break;
        }

        v = scm_primitive_eval_x(u);
    }

    return v;
}

static void *run_scheme_with_guile(void *data)
{
    // We may be called upon to evaluate user programs more than once
    // (for instance, if the user specified more than one Scheme
    // programs on the command line).  Some parts of the
    // initialization, will only need to be carried out the first
    // time.

    static bool initialized;

    // First we configure the library search path.  We assemble a set
    // of suitable calls to `add-to-load-path` and evaluate them
    // below.  We do this every time and not just on first
    // initialization, because the user may have specified more
    // library paths since the previous invocation (e.g. with
    // something like `... -L ./foo foo.scm -L ./bar bar.scm ...`.

    // Note that add-to-load-path takes care of deduplicating the list
    // of load paths, so we need not worry about adding an entry more
    // than once.

    SCM s = SCM_EOL;

    for (const auto &x: Options::library_directories) {
        s = scm_cons(
            scm_list_2(
                scm_from_latin1_symbol("add-to-load-path"),
                scm_from_latin1_string(x.c_str())), s);
    }

    // We mostly need this call to `install-r7rs!` to set up
    // `%load-extensions` to support libraries in `.sld` files.  We
    // only need to do this once.

    if (!initialized) {
        scm_read_hash_extend(
            scm_integer_to_char(scm_from_char('>')),
            scm_c_make_gsubr(
                "read-hash-greater", 2, 0, 0,
                reinterpret_cast<scm_t_subr>(read_hash_greater)));
        s = scm_cons(scm_list_1(scm_from_latin1_symbol("install-r7rs!")), s);
    }

    // Guile has a tendency to spam the terminal with warnings that
    // will not likely interest most users.  Redirect them to
    // `/dev/null` if requested, before any evaluation takes place.

    scm_set_current_warning_port(
        Flags::print_scheme_warnings
        ? scm_current_error_port()
        : scm_sys_make_void_port(scm_from_latin1_string("w")));

    // We need to evaluate the above now (as opposed to within the
    // exception handlers below), because the load path needs to be
    // set when we create the evaluation environment below.  Still, no
    // errors are expected anyway.

    scm_primitive_eval(scm_cons(scm_from_latin1_symbol("begin"), s));

    // Below we create the core libraries as well as a "library"
    // (`gamma %environment`) that will serve as the execution
    // environment for user code.  In Guile, modules (i.e. libraries)
    // and environments are practically the same thing (a set of
    // bindings of symbols to Scheme objects).

    static SCM env;

    if (!initialized) {
        initialized = true;

        // Operations and primitives

        scm_c_define_module("gamma %base", define_base, nullptr);
        scm_c_define_module("gamma %write", define_write, nullptr);
        scm_c_define_module(
            "gamma %transformation", define_transformation, nullptr);
        scm_c_define_module("gamma %volumes", define_volumes, nullptr);
        scm_c_define_module("gamma %selection", define_selection, nullptr);
        scm_c_define_module("gamma %polygons", define_polygons, nullptr);
        scm_c_define_module("gamma %polyhedra", define_polyhedra, nullptr);
        scm_c_define_module("gamma %operations", define_operations, nullptr);

        // We pre-populate the environment with the base Scheme and
        // Gamma modules, as they're going to be needed by all
        // programs.

        env = scm_c_define_module(
            "gamma %environment",
            [](void *) {
                scm_c_use_module("scheme base");
                scm_c_use_module("gamma base");
            },
            nullptr);
    }

    // When displaying errors or warnings that are related to an
    // operation, we want to be able to inform the user which
    // operation we're talking about.  The best way to do that is to
    // provide the source location where the operation was created.

    // To get that, we need the frame stack at the point of the
    // operation's creation.  We set up a hook for this purpose, that
    // will run from its constructor.

    assert(Operation::hook == nullptr);
    Operation::hook = [](Operation &op) {
        const SCM s = scm_make_stack(SCM_BOOL_T, scm_list_1(scm_from_int(1)));
        const int n = scm_to_int(scm_stack_length(s));

        // At this point the frame corresponding to the call to the
        // procedure that instantiated the operation should be at the
        // top.  Nevertheless, to be on the safe side, we go through
        // the stack and get the source localtion of the first frame
        // with source information.

        for (int i = 0; i < n; i++) {
            const SCM u = scm_stack_ref(s, scm_from_int(i));
            const SCM v = scm_frame_source(u);

            if (scm_is_false(v)) {
                continue;
            }

            char *s = scm_to_locale_string(scm_cadr(v));
            op.annotations.insert({"file", std::string(s)});
            free(s);

            // For the reason behind the `1 + `, ref: comment in
            // `print_location`.

            op.annotations.insert(
                {"line", std::to_string(1 + scm_to_int(scm_caddr(v)))});

            op.annotations.insert(
                {"column", std::to_string(scm_to_int(scm_cdddr(v)))});

            break;
        }

    };

    // The user can define variable specified on the command line with
    // `-Dfoo=bar`.  Together with `define-option`, this allows
    // programs to define parameters that the user can override on the
    // command line.  We want to allow the definition of any sort of
    // value, not just numbers or strings, so we treat `bar` above as
    // Scheme code which we evaluate to get `foo`'s value.

    s = SCM_EOL;
    for (const auto &x: Options::definitions) {
        // If the user specifies no value, we assign `#true`
        // implicitly.  This supports boolean, switch-like options
        // like `(define-option draft)`, defaulting to `#false`, but
        // which can be enabled on the command line with `-Ddraft`.

        SCM t = SCM_BOOL_T;
        const bool p = x.second.empty();

        if (!p) {
            // If a value has been specified, we evaluate it by having
            // `eval` read from the supplied string through a port.

            t = scm_list_3(
                scm_from_latin1_symbol("call-with-input-string"),
                scm_from_locale_string(x.second.c_str()),
                scm_list_4(
                    scm_from_latin1_symbol("lambda"),
                    scm_list_1(scm_from_latin1_symbol("port")),

                    // We also need to jump through a few hoops to
                    // name the port, so as to make potential errors
                    // more descriptive.

                    scm_list_3(
                        scm_from_latin1_symbol("set-port-filename!"),
                        scm_from_latin1_symbol("port"),
                        scm_from_latin1_string("<command line>")),
                    scm_list_3(
                        scm_from_latin1_symbol("eval"),
                        scm_list_2(
                            scm_from_latin1_symbol("read-syntax"),
                            scm_from_latin1_symbol("port")),
                        scm_list_1(scm_from_latin1_symbol("current-module")))));
        }

        // In Scheme, the define special form creates *internal*
        // definitions inside lambdas (i.e. scoped to the lambda), so
        // we need to explicitly evaluate the definitions in the
        // current module.

        s = scm_cons(
            scm_list_3(
                scm_from_latin1_symbol("eval"),
                scm_list_2(
                    scm_from_latin1_symbol("quote"),
                    scm_list_3(
                        scm_from_latin1_symbol("define"),
                        scm_from_locale_symbol(
                            (p ? x.first + "?" : x.first).c_str()),
                        t)),
                scm_list_1(scm_from_latin1_symbol("current-module"))), s);
    }

    // We allow command line arguments to be passed to the program
    // from the shell.  In the program these are made available
    // through the `(command-line)` procedure.

    struct context *context = static_cast<struct context *>(data);

    scm_set_program_arguments(
        context->last - context->first, context->first, context->input);

    // Finally, we're ready to load and evaluate the source file.  To
    // recapitulate, we need to set up:

    SCM t = scm_list_3(
        //   1. the outer handler,

        scm_from_latin1_symbol("with-exception-handler"),
        scm_c_make_gsubr(
            "%gamma-outer-handler", 1, 0, 0,
            reinterpret_cast<void *>(outer_handler)),
        scm_list_3(
            scm_from_latin1_symbol("lambda"),
            SCM_EOL,

            //   2. the inner handler,

            scm_list_3(
                scm_from_latin1_symbol("with-exception-handler"),
                scm_c_make_gsubr(
                    "%gamma-inner-handler", 1, 0, 0,
                    reinterpret_cast<void *>(inner_handler)),
                scm_list_3(
                    scm_from_latin1_symbol("lambda"),
                    SCM_EOL,

                    //   3. the prompt and finally

                    scm_list_4(
                        scm_from_latin1_symbol("call-with-prompt"),
                        scm_list_2(
                            scm_from_latin1_symbol("quote"),
                            scm_from_latin1_symbol("%gamma-prompt-tag")),

                        //   4. a thunk loading the program from the
                        //   supplied file name.

                        scm_list_n(
                            scm_from_latin1_symbol("lambda"),
                            SCM_EOL,
                            scm_cons(scm_from_latin1_symbol("begin"), s),
                            (!std::strcmp(context->input, "-")
                             ? scm_list_1(
                                 scm_c_make_gsubr(
                                     "%load-from-current-input", 0, 0, 0,
                                     reinterpret_cast<void *>(load_from_current_input)))
                             : scm_list_3(
                                 scm_from_latin1_symbol("load"),
                                 scm_from_locale_string(context->input),
                                 scm_from_latin1_symbol("read-syntax"))),
                            SCM_BOOL_T,
                            SCM_UNDEFINED),

                        // If the thunk executes succesfully, i. e. if
                        // no exception, or only continuable
                        // exceptions occured, then `#t` is returned
                        // explicitly above.  In case of a
                        // non-continuable exception, the prompt's
                        // handler is invoked by `abort-to-prompt` and
                        // `#f` is returned.

                        scm_list_3(
                            scm_from_latin1_symbol("lambda"),
                            scm_from_latin1_symbol("args"),
                            SCM_BOOL_F))))));
#if 0
    scm_write(t, scm_current_error_port());
    scm_newline(scm_current_error_port());
#endif

    // Finally we evaluate all this, convert whatever was returned
    // (`#t` or `#f`) to a C++ boolean and pass it to `run_scheme`
    // below and from there back to the front end.

    context->result = scm_is_true(scm_eval(t, env));
    Operation::hook = nullptr;

    return nullptr;
}

int run_scheme(const char *input, char **first, char **last)
{
    struct context context = {
        const_cast<char *>(input), first, last, 0};

    scm_with_guile(&run_scheme_with_guile, &context);

    return !context.result;
}

// When we've evaluated each and every program, we would like to clean
// up, mainly for two reasons:

//   1. To reclaim any resources used by the front end, which we're
//   not going to need any more, but more importantly

//   2. because we don't want the front end to hold any more
//   references to the operations it created (ref: Culling Dead
//   Operations).

// We perform this clean up by "closing" the front end.

static void *close_scheme_with_guile(void *data)
{
    // We call `module-clear!` on the execution environment, which has
    // the effect of deleting all bindings (i.e. all "global
    // variables") made while executing the program, thus turning
    // everything the program created into garbage.

    scm_primitive_eval(
        scm_list_2(
            scm_from_latin1_symbol("module-clear!"),
            scm_list_2(
                scm_from_latin1_symbol("resolve-module"),
                scm_list_2(
                    scm_from_latin1_symbol("quote"),
                    scm_list_2(
                        scm_from_latin1_symbol("gamma"),
                        scm_from_latin1_symbol("%environment"))))));

    // We then trigger the garbage collector manually, and run the
    // finalizers of collected objects, until there are no more.

    do {
        scm_gc();
    } while (scm_run_finalizers() > 0);

    // And we should be done.  Unfortunately, as previously explained
    // (ref: Passing Values between C++ and Scheme), we can't depend
    // on the garbage collector to call the finalizers of all
    // unreachble objects.

    // We therefore do it ourselves by going through the sets we've
    // kept.  We can't delete the left over boxed operations as we do
    // in the finalizer, because the latter may yet me called (albeit
    // normally only in tests, where we reuse the same Guile state
    // repeatedly).  Instead, we reset the contained shared pointers.
    // If and when the finalizer does run, it will just delete a
    // variant containing an empty pointer.

    // Note that we have disabled automatic finalization (see below),
    // so we need not worry about races and locking.

    for (auto &x: boxed_polygons) {
        std::visit(
            [](auto &&y) {
                y.reset();
            }, *x);
    }

    for (auto &x: boxed_polyhedra) {
        std::visit(
            [](auto &&y) {
                y.reset();
            }, *x);
    }

    return nullptr;
}

void close_scheme(void)
{
    scm_set_automatic_finalization_enabled(0);
    scm_with_guile(&close_scheme_with_guile, nullptr);
    scm_set_automatic_finalization_enabled(1);
}
