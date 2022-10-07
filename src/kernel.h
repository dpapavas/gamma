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

#ifndef KERNEL_H
#define KERNEL_H

#include <CGAL/Exact_predicates_exact_constructions_kernel.h>

typedef CGAL::Exact_predicates_exact_constructions_kernel Kernel;

typedef Kernel::FT FT;
typedef Kernel::RT RT;

typedef Kernel::Point_2 Point_2;
typedef Kernel::Line_2 Line_2;
typedef Kernel::Circle_2 Circle_2;
typedef Kernel::Direction_2 Direction_2;
typedef Kernel::Vector_2 Vector_2;

typedef Kernel::Point_3 Point_3;
typedef Kernel::Line_3 Line_3;
typedef Kernel::Vector_3 Vector_3;
typedef Kernel::Plane_3 Plane_3;
typedef Kernel::Sphere_3 Sphere_3;

#endif
