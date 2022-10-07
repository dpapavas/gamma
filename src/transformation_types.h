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

#ifndef TRANSFORMATION_TYPES_H
#define TRANSFORMATION_TYPES_H

#include <CGAL/Aff_transformation_2.h>
#include <CGAL/Aff_transformation_3.h>

typedef CGAL::Aff_transformation_2<Kernel> Aff_transformation_2;
typedef CGAL::Aff_transformation_3<Kernel> Aff_transformation_3;

#endif
