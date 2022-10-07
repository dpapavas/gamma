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

 #ifndef PROJECTION_H
#define PROJECTION_H

FT rational_sqrt(const FT &x);

Point_2 project_to_circle(
    const double x, const double y, const FT &radius, const FT&epsilon);

Point_3 project_to_sphere(
    const double x, const double y, const double z,
    const FT &radius, const FT&epsilon);

#endif
