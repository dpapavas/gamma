#ifndef POLYGON_TYPES_H
#define POLYGON_TYPES_H

#include <CGAL/Polygon_2.h>
#include <CGAL/Polygon_with_holes_2.h>
#include <CGAL/Polygon_set_2.h>

typedef CGAL::Polygon_2<Kernel> Polygon;
typedef CGAL::Polygon_with_holes_2<Kernel> Polygon_with_holes;
typedef CGAL::Polygon_set_2<Kernel> Polygon_set;

#endif
