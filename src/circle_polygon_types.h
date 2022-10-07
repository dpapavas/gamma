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

#ifndef CIRCLE_POLYGON_TYPES_H
#define CIRCLE_POLYGON_TYPES_H

#include <CGAL/Gps_circle_segment_traits_2.h>
#include <CGAL/General_polygon_set_2.h>

typedef CGAL::Gps_circle_segment_traits_2<Kernel> Circle_segment_traits;
typedef CGAL::General_polygon_set_2<Circle_segment_traits> Circle_polygon_set;
typedef Circle_segment_traits::General_polygon_2 Circle_polygon;
typedef Circle_segment_traits::General_polygon_with_holes_2 Circle_polygon_with_holes;

#endif
