// Copyright 2022, 2026 Dimitris Papavasiliou

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

#include "compose_tag.h"
#include "kernel.h"

// Document: program

// ## Serialization of Simple Arguments

// For simple values like integers, we just use the insertion operator
// to add them to the argument list, along with a trailing comma.

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

#undef SIMPLE_STREAM_INSERTION

// We do the same for strings, except we also add quotes.

template<>
void compose_tag_helper<const char *>::compose(
    std::ostringstream &s, const char * const &t)
{
    s << "\"" << t << "\",";
}

// For CGAL rationals we output the exact form of the number, as
// `q/r,`.

template<>
void compose_tag_helper<FT>::compose(std::ostringstream &s, const FT &a)
{
    s << a.exact() << ",";
}

// For compound types, we mimick the syntax of operations, outputting
// `type(arg_1,arg_2,...)`.

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
void compose_tag_helper<Segment_3>::compose(
    std::ostringstream &s, const Segment_3 &st)
{
    s << "segment(";

    compose_tag_helper<Point_3>::compose(s, st.source());
    compose_tag_helper<Point_3>::compose(s, st.target());

    s.seekp(-1, std::ios_base::end);
    s << "),";
}

template<>
void compose_tag_helper<Ray_3>::compose(
    std::ostringstream &s, const Ray_3 &st)
{
    s << "ray(";

    compose_tag_helper<Point_3>::compose(s, st.source());
    compose_tag_helper<Point_3>::compose(s, st.point(1));

    s.seekp(-1, std::ios_base::end);
    s << "),";
}

template<>
void compose_tag_helper<Line_3>::compose(
    std::ostringstream &s, const Line_3 &l)
{
    const auto d = l.direction();

    s << "line("
      << d.dx().exact() << ","
      << d.dy().exact() << ","
      << d.dz().exact() << "),";
}

template<>
void compose_tag_helper<Plane_3>::compose(
    std::ostringstream &s, const Plane_3 &pi)
{
    s << "plane("
      << pi.a().exact() << ","
      << pi.b().exact() << ","
      << pi.c().exact() << ","
      << pi.d().exact() << "),";
}
