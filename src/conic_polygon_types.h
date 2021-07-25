#ifndef CONIC_POLYGON_TYPES_H
#define CONIC_POLYGON_TYPES_H

#include <CGAL/CORE_algebraic_number_traits.h>
#include <CGAL/Arr_conic_traits_2.h>
#include <CGAL/Gps_traits_2.h>
#include <CGAL/General_polygon_set_2.h>

#include "core_kernels.h"

typedef CGAL::Gps_traits_2<
    CGAL::Arr_conic_traits_2<
        Rat_kernel, Alg_kernel,
        CGAL::CORE_algebraic_number_traits>> Conic_traits;
typedef Conic_traits::General_polygon_2 Conic_polygon;
typedef Conic_traits::General_polygon_with_holes_2 Conic_polygon_with_holes;
typedef CGAL::General_polygon_set_2<Conic_traits> Conic_polygon_set;

#endif
