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

#ifndef TRANSFORMATIONS_H
#define TRANSFORMATIONS_H

#include "transformation_types.h"

// Convert user-input angles to radians.  By default, angles are
// expected in degrees (which are more rationals-friendly than
// radians), but this macro can be changed to different units, if
// necessary.

#define ANGLE(x) (x / 180.0 * std::acos(-1))

Aff_transformation_2 basic_rotation(const double theta);
Aff_transformation_3 basic_rotation(const double theta, const int axis);
Aff_transformation_3 axis_angle_rotation(const double theta, const double *axis);

#endif
