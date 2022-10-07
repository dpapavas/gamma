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
