#include <cmath>

#include <CGAL/rational_rotation.h>
#include <CGAL/Kernel/global_functions.h>

#include "assertions.h"
#include "kernel.h"
#include "transformation_types.h"
#include "transformations.h"
#include "tolerances.h"
#include "compose_tag.h"

Aff_transformation_2 basic_rotation(const double theta)
{
    return Aff_transformation_2(
        CGAL::ROTATION,
        Direction_2(RT(std::cos(ANGLE(theta))), RT(std::sin(ANGLE(theta)))),
        Tolerances::sine, RT(1));
}

static Aff_transformation_3 transpose(Aff_transformation_3 T)
{
    return Aff_transformation_3(T.m(0, 0), T.m(1, 0), T.m(2, 0),
                                T.m(0, 1), T.m(1, 1), T.m(2, 1),
                                T.m(0, 2), T.m(1, 2), T.m(2, 2));
}

Aff_transformation_3 basic_rotation(const double theta, const int axis)
{
    RT s, c, d;

    CGAL::rational_rotation_approximation(RT(std::cos(ANGLE(theta))),
                                          RT(std::sin(ANGLE(theta))),
                                          s, c, d,
                                          Tolerances::sine, RT(1));

    switch (axis) {
    // X
    case 0: return Aff_transformation_3(d, RT(0), RT(0),
                                        RT(0), c, -s,
                                        RT(0), s, c, d);

    // Y
    case 1: return Aff_transformation_3(c, RT(0), s,
                                        RT(0), d, RT(0),
                                        -s, RT(0), c, d);

    // Z
    case 2: return Aff_transformation_3(c, -s, RT(0),
                                        s, c, RT(0),
                                        RT(0), RT(0), d, d);
    }

    assert_not_reached();
}

Aff_transformation_3 axis_angle_rotation(const double theta, const double *axis)
{
    double m = std::sqrt(axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2]);
    double u[3] = {axis[0] / m, axis[1] / m, axis[2] / m};

    // Line up the axis of rotation with the Z axis, perform the
    // rotation and trasnsform back.

    Aff_transformation_3 R_z = basic_rotation(std::atan2(u[1], u[0]) / ANGLE(1), 2);
    Aff_transformation_3 R_y = basic_rotation(std::acos(u[2]) / ANGLE(1), 1);

    return R_z * R_y * basic_rotation(theta, 2) * transpose(R_y) * transpose(R_z);
}

// Transformation tag composition

template<typename A>
static bool compose_simple(std::ostringstream &s, const A &T)
{
    constexpr int n = CGAL::Ambient_dimension<A, Kernel>::value;
    bool p = true;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i == j && T.m(i, j) != FT(1)) {
                p = false;
            }

            if (i != j && T.m(i, j) != FT(0)) {
                return false;
            }
        }
    }

    if (p) {
        s << "translation(";

        for (int i = 0; i < n; i++) {
            s << T.m(i, n).exact() << ",";
        }
    } else {
        for (int i = 0; i < n; i++) {
            if (T.m(i, n) != FT(0)) {
                return false;
            }
        }

        s << "scaling(";

        for (int i = 0; i < n; i++) {
            s << T.m(i, i).exact() << ",";
        }
    }

    s.seekp(-1, std::ios_base::end) << "),";

    return true;
}

template<typename A>
static bool compose_linear(std::ostringstream &s, const A &T)
{
    constexpr int n = CGAL::Ambient_dimension<A, Kernel>::value;

    // No translation?

    for (int i = 0; i < n; i++) {
        if (T.m(i, n) != FT(0)){
            return false;
        }
    }

    const char *x = "transformation";

    // Orthgonal?
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            FT c = FT(0);

            for (int k = 0; k < n; c += T.m(i, k) * T.m(j, k), k++);

            if ((i == j && c != FT(1))
                || (i != j && c != FT(0))) {
                goto print_matrix;
            }
        }
    }

    {
        FT det;

        if constexpr (n == 2) {
            det = CGAL::determinant(
                typename A::R::Vector_2(T.m(0, 0), T.m(0, 1)),
                typename A::R::Vector_2(T.m(1, 0), T.m(1, 1)));
        } else {
            det = CGAL::determinant(
                typename A::R::Vector_3(T.m(0,0), T.m(0,1), T.m(0,2)),
                typename A::R::Vector_3(T.m(1,0), T.m(1,1), T.m(1,2)),
                typename A::R::Vector_3(T.m(2,0), T.m(2,1), T.m(2,2)));
        }

        if (det == FT(1)) {
            x = "rotation";
        } else if (det == FT(-1)) {
            x = "reflection";
        }
    }

    // Print the "rotation part" only.

  print_matrix:
    s << x << "(";

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            s << T.m(i, j).exact() << ",";
        }
    }

    s.seekp(-1, std::ios_base::end) << "),";

    return true;
}

template<typename A>
static bool compose_affine(std::ostringstream &s, const A &T)
{
    constexpr int n = CGAL::Ambient_dimension<A, Kernel>::value;

    s << "transformation(";

    // Print the full matrix.

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n + 1; j++) {
            s << T.m(i, j).exact() << ",";
        }
    }

    s.seekp(-1, std::ios_base::end) << "),";

    return true;
}

template<>
void compose_tag_helper<Aff_transformation_2>::compose(
    std::ostringstream &s, const Aff_transformation_2 &T)
{
    compose_simple(s, T)
        || compose_linear(s, T)
        || compose_affine(s, T);
}

template<>
void compose_tag_helper<Aff_transformation_3>::compose(
    std::ostringstream &s, const Aff_transformation_3 &T)
{
    compose_simple(s, T)
        || compose_linear(s, T)
        || compose_affine(s, T);
}
