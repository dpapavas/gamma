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
