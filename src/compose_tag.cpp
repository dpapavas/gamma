#include "compose_tag.h"
#include "kernel.h"

#define SIMPLE_STREAM_INSERTION {               \
    s << x << ",";                              \
}

template<>
void compose_tag_helper<int>::compose(std::ostringstream &s, const int &x)
SIMPLE_STREAM_INSERTION

template<>
void compose_tag_helper<unsigned int>::compose(
    std::ostringstream &s, const unsigned int &x)
SIMPLE_STREAM_INSERTION

template<>
void compose_tag_helper<const char *>::compose(
    std::ostringstream &s, const char * const &t)
{
    s << "\"" << t << "\",";
}

template<>
void compose_tag_helper<FT>::compose(std::ostringstream &s, const FT &a)
{
    s << a.exact() << ",";
}

template<>
void compose_tag_helper<Point_2>::compose(
    std::ostringstream &s, const Point_2 &P)
{
    s << "point(" << P.x().exact() << ","  << P.y().exact() << "),";
}

template<>
void compose_tag_helper<Point_3>::compose(
    std::ostringstream &s, const Point_3 &P)
{
    s << "point("
      << P.x().exact() << ","
      << P.y().exact() << ","
      << P.z().exact() << "),";
}

template<>
void compose_tag_helper<Vector_3>::compose(
    std::ostringstream &s, const Vector_3 &v)
{
    s << "vector("
      << v.x().exact() << ","
      << v.y().exact() << ","
      << v.z().exact() << "),";
}

template<>
void compose_tag_helper<Plane_3>::compose(
    std::ostringstream &s, const Plane_3 &Pi)
{
    s << "plane("
      << Pi.a().exact() << ","
      << Pi.b().exact() << ","
      << Pi.c().exact() << ","
      << Pi.d().exact() << "),";
}
