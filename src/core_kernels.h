#ifndef CORE_KERNELS_H
#define CORE_KERNELS_H

#include <CGAL/Gmpq.h>
#include <CGAL/Cartesian.h>

typedef CGAL::Cartesian<
    CGAL::CORE_algebraic_number_traits::Rational> Rat_kernel;
typedef CGAL::Cartesian<
    CGAL::CORE_algebraic_number_traits::Algebraic> Alg_kernel;

#endif
