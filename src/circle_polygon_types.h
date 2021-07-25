#ifndef CIRCLE_POLYGON_TYPES_H
#define CIRCLE_POLYGON_TYPES_H

#include <CGAL/Gps_circle_segment_traits_2.h>
#include <CGAL/General_polygon_set_2.h>

typedef CGAL::Gps_circle_segment_traits_2<Kernel> Circle_segment_traits;
typedef CGAL::General_polygon_set_2<Circle_segment_traits> Circle_polygon_set;
typedef Circle_segment_traits::General_polygon_2 Circle_polygon;
typedef Circle_segment_traits::General_polygon_with_holes_2 Circle_polygon_with_holes;

#endif
