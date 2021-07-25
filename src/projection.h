 #ifndef PROJECTION_H
#define PROJECTION_H

FT rational_sqrt(const FT &x);

Point_2 project_to_circle(
    const double x, const double y, const FT &radius, const FT&epsilon);

Point_3 project_to_sphere(
    const double x, const double y, const double z,
    const FT &radius, const FT&epsilon);

#endif
