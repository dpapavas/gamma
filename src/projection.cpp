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

#include <CGAL/simplest_rational_in_interval.h>

#include "kernel.h"
#include "tolerances.h"

// Although the square root of a rational is generally not rational,
// in certain cases numbers are known to be squares of rational
// numbers (for instance when only the square of a rational radius is
// available).  The roots of these can be recovered exactly, as the
// root is guaranteed by the floating-point standard to be exact, if
// it can be exactly represented as a floating point number, which is
// the case for the integer numerator and denominator of the rational.

FT rational_sqrt(const FT &x)
{
    const FT::ET e = x.exact();
    CGAL::Fraction_traits<FT::ET>::Numerator_type n, d;

    mpz_sqrt(n.get_mpz_t(), e.get_num().get_mpz_t());
    mpz_sqrt(d.get_mpz_t(), e.get_den().get_mpz_t());

    assert (n * n == e.get_num() && d * d == e.get_den());

    return FT::ET(n, d);
}

Point_2 project_to_circle(
    const double x, const double y, const FT &radius, const FT&epsilon)
{
    const bool p = std::fabs(x) > std::fabs(y);
    const double s = ((p ? x : y) < 0) * 2 - 1;
    const double m = std::sqrt(x * x + y * y);

    double f;
    if (p) {
        f = y / (m - s * x);
    } else {
        f = x / (m - s * y);
    }

    const double epsilon_2 = CGAL::to_double(epsilon / radius) / 2;
    const FT X = CGAL::simplest_rational_in_interval<FT>(
        f - epsilon_2, f + epsilon_2);
    const FT S = 1 + X * X;

    if (p) {
        return Point_2(radius * s * (S - 2) / S, radius * 2 * X / S);
    } else {
        return Point_2(radius * 2 * X / S, radius * s * (S - 2) / S);
    }
}

Point_3 project_to_sphere(
    const double x, const double y, const double z,
    const FT &radius, const FT&epsilon)
{
    int d;
    double s;

    if (std::fabs(x) > std::fabs(y)) {
        if (std::fabs(z) > std::fabs(x)) {
            d = 2;
            s = z < 0;
        } else {
            d = 0;
            s = x < 0;
        }
    } else {
        if (std::fabs(z) > std::fabs(y)) {
            d = 2;
            s = z < 0;
        } else {
            d = 1;
            s = y < 0;
        }
    }

    s = s * 2 - 1;

    const double m = std::sqrt(x * x + y * y + z * z);

    double f, g;
    if (d == 0) {
        f = y / (m - s * x);
        g = z / (m - s * x);
    } else if (d == 1) {
        f = x / (m - s * y);
        g = z / (m - s * y);
    } else {
        f = x / (m - s * z);
        g = y / (m - s * z);
    }

    const double epsilon_2 = CGAL::to_double(epsilon / radius) / std::sqrt(8);

    const FT X = CGAL::simplest_rational_in_interval<FT>(
        f - epsilon_2, f + epsilon_2);
    const FT Y = CGAL::simplest_rational_in_interval<FT>(
        g - epsilon_2, g + epsilon_2);
    const FT S = 1 + X * X + Y * Y;

    if (d == 0) {
        return Point_3(
            radius * s * (S - 2) / S, radius * 2 * X / S, radius * 2 * Y / S);
    } else if (d == 1) {
        return Point_3(
            radius * 2 * X / S, radius * s * (S - 2) / S, radius * 2 * Y / S);
    } else {
        return Point_3(
            radius * 2 * X / S, radius * 2 * Y / S, radius * s * (S - 2) / S);
    }
}
