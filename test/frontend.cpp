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

#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <fstream>

#include "operation.h"
#include "evaluation.h"

struct fixture {
private:
    int evaluate;

public:
    const char *filename;
    std::ofstream file;

    void run(const char *lang) {
        const char *argv[] = {
            "test_case", "-x", lang, filename};

        parse_options(4, const_cast<char **>(argv));
    }

    fixture(): filename(std::tmpnam(nullptr)) {
        evaluate = Flags::evaluate;
        Flags::evaluate = 0;

#ifdef HAVE_SCHEME
        Options::scheme_features.push_front("core-gamma");
#endif
    }

    ~fixture() {
#ifdef HAVE_SCHEME
        Options::scheme_features.pop_front();
#endif

        Flags::evaluate = evaluate;
    }
};

#define DEFINE_TEST_CASE(...)                           \
BOOST_AUTO_TEST_CASE(__VA_ARGS__)                       \
{                                                       \
    for (auto [l, s]: std::initializer_list<            \
                          std::pair<const char *, const char *>> {

#ifdef HAVE_SCHEME
#define WITH_SCHEME_SOURCE(SOURCE) std::pair("scheme", SOURCE),
#else
#define WITH_SCHEME_SOURCE(SOURCE)
#endif

#ifdef HAVE_LUA
#define WITH_LUA_SOURCE(SOURCE) std::pair("lua", SOURCE),
#else
#define WITH_LUA_SOURCE(SOURCE)
#endif

#define EXPECTING(...)                                          \
    }) {                                                        \
        file.open(filename);                                    \
        file << s                                               \
             << std::endl;                                      \
                                                                \
        run(l);                                                 \
        file.close();                                           \
                                                                \
        for (const char *x:                                     \
                 std::initializer_list<const char *>(           \
                     {__VA_ARGS__})) {                          \
            BOOST_TEST(                                         \
                !!find_operation(x),                            \
                l << ": tag '" << x << "' not found");          \
        }                                                       \
    }                                                           \
}


BOOST_FIXTURE_TEST_SUITE(frontend, fixture)

DEFINE_TEST_CASE(syntax_error)
WITH_LUA_SOURCE("function f()"
                "    retur"
                "end")
WITH_SCHEME_SOURCE("(define foo 1")
EXPECTING()

DEFINE_TEST_CASE(runtime_error)
WITH_LUA_SOURCE("i = 1"
                "nosuchfunc()")
WITH_SCHEME_SOURCE("(+ 1 2)"
                   "('notaproc 3 4)")
EXPECTING()

#define RECTANGLE_TAG "polygon(point(-1,-1),point(1,-1),point(1,1),point(-1,1))"

////////////////
// Primitives //
////////////////

DEFINE_TEST_CASE(polygon)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "g = require 'gamma.polygons'"
                "op = require 'gamma.operations'"

                "P = g.simple(point(0, 0), point(1, 0), point(0, 1))"
                "Q = g.rectangle(2, 2)"
                "set_projection_tolerance(9.5367431640625e-07)"
                "R = g.regular(3, 5)"
                "S = g.isosceles_triangle(4, 3)"
                "T = g.right_triangle(-5, 4)")
WITH_SCHEME_SOURCE("(import (gamma polygons))"
                   "(simple-polygon (point 0 0)"
                   "         (point 1 0)"
                   "         (point 0 1))"
                   "(rectangle 2 2)"
                   "(set-projection-tolerance! 1/1048576)"
                   "(regular-polygon 3 5)"
                   "(isosceles-triangle 4 3)"
                   "(right-triangle -5 4)")
EXPECTING("polygon(point(0,0),point(1,0),point(0,1))",
          RECTANGLE_TAG,
          "regular_polygon(3,5,1/1048576)",
          "polygon(point(-2,0),point(2,0),point(0,3))",
          "polygon(point(0,0),point(0,4),point(-5,0))")

DEFINE_TEST_CASE(circle_polygon)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "g = require 'gamma.polygons'"

                "P = g.circle(5)"
                "Q = g.circular_sector(5, 65)"
                "R = g.circular_segment(15, 5)")
WITH_SCHEME_SOURCE("(import (gamma polygons))"
                   "(circle 5)"
                   "(circular-sector 5 65)"
                   "(circular-segment 15 5)")
EXPECTING("circle(5)",
          "sector(5,65)",
          "segment(15,5)")

DEFINE_TEST_CASE(conic_polygon)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "g = require 'gamma.polygons'"

                "P = g.ellipse(5, 10)"
                "Q = g.elliptic_sector(5, 10, 65)")
WITH_SCHEME_SOURCE("(import (gamma polygons))"
                   "(ellipse 5 10)"
                   "(elliptic-sector 5 10 65)")
EXPECTING("transform(conics(circle(1)),scaling(5,10))",
          "transform(conics(sector(1,65)),scaling(5,10))")

DEFINE_TEST_CASE(polyhedron)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "h = require 'gamma.polyhedra'"

                "P = h.tetrahedron(1, 1, 1)"
                "PA = h.square_pyramid(2, 2, 2)"
                "PB = h.octahedron(3, 3, 3)"
                "PC = h.octahedron(3, 3, 3, 4)"
                "set_projection_tolerance(9.5367431640625e-07)"
                "PD = h.regular_pyramid(5, 5, 5)"
                "PE = h.regular_bipyramid(6, 6, 6)"
                "PF = h.regular_bipyramid(6, 6, 6, 7)"
                "Q = h.cuboid(2, 4, 6)"
                "R = h.icosahedron(1)"
                "set_curve_tolerance(0.125)"
                "S = h.sphere(\"1/3\")"
                "T = h.cylinder(3, 10)")
WITH_SCHEME_SOURCE("(import (gamma polyhedra))"
                   "(tetrahedron 1 1 1)"
                   "(square-pyramid 2 2 2)"
                   "(octahedron 3 3 3)"
                   "(octahedron 3 3 3 4)"
                   "(set-projection-tolerance! 1/1048576)"
                   "(regular-pyramid 5 5 5)"
                   "(regular-bipyramid 6 6 6)"
                   "(regular-bipyramid 6 6 6 7)"
                   "(cuboid 2 4 6)"
                   "(icosahedron 1)"
                   "(set-curve-tolerance! 1/8)"
                   "(sphere 1/3)"
                   "(cylinder 3 10)")
EXPECTING("tetrahedron(1,1,1)",
          "square_pyramid(2,2,2)",
          "octahedron(3,3,3,3)",
          "octahedron(3,3,3,4)",
          "regular_pyramid(5,5,5,1/1048576)",
          "regular_bipyramid(6,6,6,6,1/1048576)",
          "regular_bipyramid(6,6,6,7,1/1048576)",
          "cuboid(2,4,6)",
          "icosahedron(1,1/1048576)",
          "sphere(1/3,1/8,1/1048576)",
          "extrusion(regular_polygon(11,3,1/1048576),"
          "translation(0,0,-5),translation(0,0,5))")

DEFINE_TEST_CASE(extrusion)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "g = require 'gamma.polygons'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.extrusion(g.rectangle(2, 2),"
                "                 t.translation(0, 0, 0),"
                "                 t.translation(0, 0, 2))"
                "set_curve_tolerance(1)"
                "set_projection_tolerance(10)"
                "G = op.extrusion(g.circle(100),"
                "                 t.translation(0, 0, 0),"
                "                 t.translation(0, 0, 2))")
WITH_SCHEME_SOURCE("(import (gamma transformation) (gamma polygons)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(extrusion"
                   "  (rectangle 2 2)"
                   "  (translation 0 0 0)"
                   "  (translation 0 0 2))"
                   "(set-curve-tolerance! 1)"
                   "(set-projection-tolerance! 10)"
                   "(extrusion"
                   "  (circle 100)"
                   "  (translation 0 0 0)"
                   "  (translation 0 0 2))")
EXPECTING("extrusion(" RECTANGLE_TAG ","
          "translation(0,0,0),translation(0,0,2))",
          "extrusion(segments(circle(100),1,10),"
          "translation(0,0,0),translation(0,0,2))")

/////////////////////
// Transformations //
/////////////////////

DEFINE_TEST_CASE(transform_point)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "g = require 'gamma.polygons'"
                "op = require 'gamma.operations'"

                "P = g.simple(point(0, 0),"
                "             t.translation(1, 0) * point(0, 0),"
                "             t.rotation(90) * point(1, 0))"
                "G = op.hull(point(0, 0, 0),"
                "            t.translation(1, 0, 0) * point(0, 0, 0),"
                "            t.apply(t.rotation(90, 2), point(1, 0, 0)),"
                "            t.apply(t.rotation(90, 1), point(-1, 0, 0)))")
WITH_SCHEME_SOURCE("(import (gamma transformation) (gamma polygons)"
                   "        (gamma operations))"
                   "(simple-polygon"
                   " (point 0 0)"
                   " (transformation-apply (translation 1 0)"
                   "                       (point 0 0))"
                   " (transformation-apply (rotation 90)"
                   "                       (point 1 0)))"
                   "(hull (point 0 0 0)"
                   "      (transformation-apply (translation 1 0 0)"
                   "                            (point 0 0 0))"
                   "      (transformation-apply (rotation 90 2)"
                   "                            (point 1 0 0))"
                   "      (transformation-apply (rotation 90 1)"
                   "                            (point -1 0 0)))")

EXPECTING("polygon(point(0,0),point(1,0),point(0,1))",
          "hull(point(0,0,0),point(1,0,0),point(0,1,0),point(0,0,1))")

DEFINE_TEST_CASE(transform_polygon)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "g = require 'gamma.polygons'"

                "P = t.translation(2, 3) * g.rectangle(2, 2)"
                "Q = t.scaling(1, -1) * g.circle(1)"
                "R = t.apply(t.rotation(90), g.ellipse(1, 2))")
WITH_SCHEME_SOURCE("(import (gamma transformation) (gamma polygons))"
                   "(transformation-apply (translation 2 3) (rectangle 2 2))"
                   "(transformation-apply (scaling 1 -1) (circle 1))"
                   "(transformation-apply (rotation 90) (ellipse 1 2))")
EXPECTING("transform(" RECTANGLE_TAG ",translation(2,3))",
          "transform(circle(1),scaling(1,-1))",
          "transform(conics(circle(1)),transformation(0,-2,1,0))")

DEFINE_TEST_CASE(transform_polyhedron)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "h = require 'gamma.polyhedra'"

                "P = t.translation(1, 2, 3) * h.tetrahedron(1, 1, 1)"
                "Q = t.scaling(2, 2, 2) * h.tetrahedron(1, 1, 1)"
                "R = t.apply(t.rotation(90, 2), h.tetrahedron(1, 1, 1))")
WITH_SCHEME_SOURCE("(import (gamma transformation) (gamma polyhedra))"
                   "(transformation-apply (translation 1 2 3)"
                   "                      (tetrahedron 1 1 1))"
                   "(transformation-apply (scaling 2 2 2)"
                   "                      (tetrahedron 1 1 1))"
                   "(transformation-apply (rotation 90 2)"
                   "                      (tetrahedron 1 1 1))")
EXPECTING("transform(tetrahedron(1,1,1),translation(1,2,3))",
          "transform(tetrahedron(1,1,1),scaling(2,2,2))",
          "transform(tetrahedron(1,1,1),rotation(0,-1,0,1,0,0,0,0,1))")

DEFINE_TEST_CASE(transform_concatenation)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "g = require 'gamma.polygons'"
                "h = require 'gamma.polyhedra'"

                "P = (t.translation(1, 2, 3)"
                "     * t.scaling(2, 2, 2)"
                "     * t.rotation(90, 2)) * h.tetrahedron(1, 1, 1)"

                "Q = t.apply(t.translation(2, 3), t.rotation(90)) * g.circle(1)")
EXPECTING("transform(tetrahedron(1,1,1),"
          "transformation(0,-2,0,1,2,0,0,2,0,0,2,3))",
          "transform(circle(1),transformation(0,-1,2,1,0,3))")

DEFINE_TEST_CASE(flush_polygon)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "g = require 'gamma.polygons'"

                "set_projection_tolerance(9.5367431640625e-07)"
                "P = t.flush_west(g.regular(5, 1))"
                "Q = t.flush_east(g.regular(5, 1))"
                "R = t.flush_south(g.regular(5, 1))"
                "S = t.flush_north(g.regular(5, 1))"
                "T = t.flush(g.regular(5, 1), 1, 1)")
WITH_SCHEME_SOURCE("(import (gamma transformation) (gamma polygons))"
                   "(set-projection-tolerance! 1/1048576)"
                   "(flush-west (regular-polygon 5 1))"
                   "(flush-east (regular-polygon 5 1))"
                   "(flush-south (regular-polygon 5 1))"
                   "(flush-north (regular-polygon 5 1))"
                   "(flush (regular-polygon 5 1) 1 1)")
EXPECTING("flush(regular_polygon(5,1,1/1048576),-1,0,0,0)",
          "flush(regular_polygon(5,1,1/1048576),0,1,0,0)",
          "flush(regular_polygon(5,1,1/1048576),0,0,-1,0)",
          "flush(regular_polygon(5,1,1/1048576),0,0,0,1)",
          "flush(regular_polygon(5,1,1/1048576),0,1,0,1)")

DEFINE_TEST_CASE(flush_polyhedron)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "h = require 'gamma.polyhedra'"

                "P = t.flush_west(h.tetrahedron(1, 1, 1))"
                "Q = t.flush_east(h.tetrahedron(1, 1, 1))"
                "R = t.flush_south(h.tetrahedron(1, 1, 1))"
                "S = t.flush_north(h.tetrahedron(1, 1, 1))"
                "T = t.flush_bottom(h.tetrahedron(1, 1, 1))"
                "U = t.flush_top(h.tetrahedron(1, 1, 1))"
                "V = t.flush(h.tetrahedron(1, 1, 1), 1, 0, -1)")
WITH_SCHEME_SOURCE("(import (gamma transformation) (gamma polyhedra))"
                   "(flush-west (tetrahedron 1 1 1))"
                   "(flush-east (tetrahedron 1 1 1))"
                   "(flush-south (tetrahedron 1 1 1))"
                   "(flush-north (tetrahedron 1 1 1))"
                   "(flush-bottom (tetrahedron 1 1 1))"
                   "(flush-top (tetrahedron 1 1 1))"
                   "(flush (tetrahedron 1 1 1) 1 0 -1)")
EXPECTING("flush(tetrahedron(1,1,1),-1,0,0,0,0,0)",
          "flush(tetrahedron(1,1,1),0,1,0,0,0,0)",
          "flush(tetrahedron(1,1,1),0,0,-1,0,0,0)",
          "flush(tetrahedron(1,1,1),0,0,0,1,0,0)",
          "flush(tetrahedron(1,1,1),0,0,0,0,-1,0)",
          "flush(tetrahedron(1,1,1),0,0,0,0,0,1)",
          "flush(tetrahedron(1,1,1),0,1,0,0,-1,0)")

DEFINE_TEST_CASE(flush_bounding_volume)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.vertices_in("
                "            t.flush_east(v.box(10, 20, 30))))"
                "Q = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.vertices_in("
                "            t.flush_west(v.sphere(2))))"
                "R = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.vertices_in("
                "            t.flush(v.cylinder(4, 2), 1, 1, 1)))")
WITH_SCHEME_SOURCE("(import (gamma transformation)"
                   "        (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (vertices-in (flush-east (bounding-box 10 20 30))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (vertices-in (flush-west (bounding-sphere 2))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (vertices-in (flush (bounding-cylinder 4 2) 1 1 1)))")
EXPECTING("color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "bounding_box(plane(-1,0,0,0),plane(1,0,0,10),plane(0,-1,0,10),"
          "plane(0,1,0,10),plane(0,0,-1,15),plane(0,0,1,15))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "bounding_sphere(point(2,0,0),2)),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "bounding_cylinder(point(-4,-4,-2),vector(0,0,1),4,2)),0,0,0,255)")

////////////
// Offset //
////////////

DEFINE_TEST_CASE(offset)
WITH_LUA_SOURCE("g = require 'gamma.polygons'"
                "op = require 'gamma.operations'"

                "P = op.offset(g.rectangle(2, 2), 3)")
WITH_SCHEME_SOURCE("(import (gamma polygons) (gamma operations))"
                   "(offset (rectangle 2 2) 3)")
EXPECTING("offset(" RECTANGLE_TAG ",3)")

////////////////
// Selections //
////////////////

DEFINE_TEST_CASE(selection)
WITH_LUA_SOURCE("v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.vertices_in(v.plane(0, 0, 1, -1)))"
                "Q = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.faces_in(v.plane(0, 0, 1, -1)))"
                "R = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.faces_partially_in(v.plane(0, 0, 1, -1)))"
                "S = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.expand_selection("
                "            s.vertices_in(v.plane(0, 0, 1, -1)), 1))"
                "T = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.contract_selection("
                "            s.vertices_in(v.halfspace(0, 0, 1, -1)), 1))"
                "U = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "         s.expand_selection("
                "            s.faces_in(v.plane(0, 0, 1, -1)), 1))"
                "V = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.contract_selection("
                "            s.faces_in(v.halfspace(0, 0, 1, -1)), 1))")
WITH_SCHEME_SOURCE("(import (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (vertices-in (bounding-plane 0 0 1 -1)))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (faces-in (bounding-plane 0 0 1 -1)))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (faces-partially-in (bounding-plane 0 0 1 -1)))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (expand-selection"
                   "  (vertices-in (bounding-plane 0 0 1 -1)) 1))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (contract-selection"
                   "  (vertices-in (bounding-halfspace 0 0 1 -1)) 1))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (expand-selection"
                   "  (faces-in (bounding-plane 0 0 1 -1)) 1))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (contract-selection"
                   "  (faces-in (bounding-halfspace 0 0 1 -1)) 1))")
EXPECTING("color_selection(mesh(cuboid(2,2,2)),"
          "contract(faces_in(bounding_halfspace(plane(0,0,1,-1))),1),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "expand(faces_in(bounding_plane(plane(0,0,1,-1))),1),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "contract(vertices_in(bounding_halfspace(plane(0,0,1,-1))),1),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "expand(vertices_in(bounding_plane(plane(0,0,1,-1))),1),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "faces_partially_in(bounding_plane(plane(0,0,1,-1))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "faces_in(bounding_plane(plane(0,0,1,-1))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "vertices_in(bounding_plane(plane(0,0,1,-1))),0,0,0,255)")

DEFINE_TEST_CASE(selection_boolean)
WITH_LUA_SOURCE("v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        (s.vertices_in(v.plane(0, 0, 1, -1))"
                "         + s.vertices_in(v.plane(0, 0, 1, 1))))"
                "Q = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        (s.vertices_in(v.plane(0, 0, 1, -1))"
                "         - s.vertices_in(v.plane(0, 0, 1, 1))))"
                "R = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        (s.vertices_in(v.plane(0, 0, 1, -1))"
                "         * s.vertices_in(v.plane(0, 0, 1, 1))))"
                "S = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        ~s.vertices_in(v.plane(0, 0, 1, -1)))"
                "T = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        (s.faces_in(v.plane(0, 0, 1, -1))"
                "         + s.faces_in(v.plane(0, 0, 1, 1))))"
                "U = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        (s.faces_in(v.plane(0, 0, 1, -1))"
                "         - s.faces_in(v.plane(0, 0, 1, 1))))"
                "V = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        (s.faces_in(v.plane(0, 0, 1, -1))"
                "         * s.faces_in(v.plane(0, 0, 1, 1))))"
                "W = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        op.complement(s.faces_in(v.plane(0, 0, 1, -1))))"
                "X = op.remesh("
                "        h.cuboid(2, 2, 2),"
                "        (s.edges_in(v.plane(0, 0, 1, -1))"
                "         + s.edges_in(v.plane(0, 0, 1, 1))), 1)"
                "Y = op.remesh("
                "        h.cuboid(2, 2, 2),"
                "        (s.edges_in(v.plane(0, 0, 1, -1))"
                "         - s.edges_in(v.plane(0, 0, 1, 1))), 1)"
                "Z = op.remesh("
                "        h.cuboid(2, 2, 2),"
                "        (s.edges_in(v.plane(0, 0, 1, -1))"
                "         * s.edges_in(v.plane(0, 0, 1, 1))), 1)"
                "A = op.remesh("
                "        h.cuboid(2, 2, 2),"
                "        ~s.edges_in(v.plane(0, 0, 1, -1)), 1)")
WITH_SCHEME_SOURCE("(import (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (union"
                   "  (vertices-in (bounding-plane 0 0 1 -1))"
                   "  (vertices-in (bounding-plane 0 0 1 1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (difference"
                   "  (vertices-in (bounding-plane 0 0 1 -1))"
                   "  (vertices-in (bounding-plane 0 0 1 1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (intersection"
                   "  (vertices-in (bounding-plane 0 0 1 -1))"
                   "  (vertices-in (bounding-plane 0 0 1 1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (complement"
                   "  (vertices-in (bounding-plane 0 0 1 -1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (union"
                   "  (faces-in (bounding-plane 0 0 1 -1))"
                   "  (faces-in (bounding-plane 0 0 1 1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (difference"
                   "  (faces-in (bounding-plane 0 0 1 -1))"
                   "  (faces-in (bounding-plane 0 0 1 1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (intersection"
                   "  (faces-in (bounding-plane 0 0 1 -1))"
                   "  (faces-in (bounding-plane 0 0 1 1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (complement"
                   "  (faces-in (bounding-plane 0 0 1 -1))))"
                   "(remesh"
                   " (cuboid 2 2 2)"
                   " (union"
                   "  (edges-in (bounding-plane 0 0 1 -1))"
                   "  (edges-in (bounding-plane 0 0 1 1))) 1)"
                   "(remesh"
                   " (cuboid 2 2 2)"
                   " (difference"
                   "  (edges-in (bounding-plane 0 0 1 -1))"
                   "  (edges-in (bounding-plane 0 0 1 1))) 1)"
                   "(remesh"
                   " (cuboid 2 2 2)"
                   " (intersection"
                   "  (edges-in (bounding-plane 0 0 1 -1))"
                   "  (edges-in (bounding-plane 0 0 1 1))) 1)"
                   "(remesh"
                   " (cuboid 2 2 2)"
                   " (complement"
                   "  (edges-in (bounding-plane 0 0 1 -1))) 1)")
EXPECTING("color_selection(mesh(cuboid(2,2,2)),"
          "complement(vertices_in(bounding_plane(plane(0,0,1,-1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "intersection(vertices_in(bounding_plane(plane(0,0,1,-1))),"
          "vertices_in(bounding_plane(plane(0,0,1,1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "difference(vertices_in(bounding_plane(plane(0,0,1,-1))),"
          "vertices_in(bounding_plane(plane(0,0,1,1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "union(vertices_in(bounding_plane(plane(0,0,1,-1))),"
          "vertices_in(bounding_plane(plane(0,0,1,1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "complement(faces_in(bounding_plane(plane(0,0,1,-1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "intersection(faces_in(bounding_plane(plane(0,0,1,-1))),"
          "faces_in(bounding_plane(plane(0,0,1,1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "difference(faces_in(bounding_plane(plane(0,0,1,-1))),"
          "faces_in(bounding_plane(plane(0,0,1,1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "union(faces_in(bounding_plane(plane(0,0,1,-1))),"
          "faces_in(bounding_plane(plane(0,0,1,1)))),0,0,0,255)",
          "remesh(cuboid(2,2,2),"
          "complement(edges_in(bounding_plane(plane(0,0,1,-1)))),1,1)",
          "remesh(cuboid(2,2,2),"
          "intersection(edges_in(bounding_plane(plane(0,0,1,-1))),"
          "edges_in(bounding_plane(plane(0,0,1,1)))),1,1)",
          "remesh(cuboid(2,2,2),"
          "difference(edges_in(bounding_plane(plane(0,0,1,-1))),"
          "edges_in(bounding_plane(plane(0,0,1,1)))),1,1)",
          "remesh(cuboid(2,2,2),"
          "union(edges_in(bounding_plane(plane(0,0,1,-1))),"
          "edges_in(bounding_plane(plane(0,0,1,1)))),1,1)")

DEFINE_TEST_CASE(selection_conversion)
WITH_LUA_SOURCE("v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.vertices_in(s.faces_in(v.plane(0, 0, 1, -1))))"
                "Q = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.vertices_in(s.edges_in(v.plane(0, 0, 1, -1))))"
                "R = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.faces_in(s.vertices_in(v.plane(0, 0, 1, -1))))"
                "S = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.faces_partially_in("
                "            s.vertices_in(v.plane(0, 0, 1, -1))))"
                "T = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.faces_in(s.edges_in(v.plane(0, 0, 1, -1))))"
                "U = op.color_selection("
                "        h.cuboid(2, 2, 2),"
                "        s.faces_partially_in("
                "            s.edges_in(v.plane(0, 0, 1, -1))))"
                "V = op.remesh("
                "        h.cuboid(2, 2, 2),"
                "        s.edges_in("
                "            s.vertices_in(v.plane(0, 0, 1, -1))), 1)"
                "W = op.remesh("
                "        h.cuboid(2, 2, 2),"
                "        s.edges_partially_in("
                "            s.vertices_in(v.plane(0, 0, 1, -1))), 1)"
                "X = op.remesh("
                "        h.cuboid(2, 2, 2),"
                "        s.edges_in("
                "            s.faces_in(v.plane(0, 0, 1, -1))), 1)"
                "Y = op.remesh("
                "        h.cuboid(2, 2, 2),"
                "        s.edges_partially_in("
                "            s.faces_in(v.plane(0, 0, 1, -1))), 1)")
WITH_SCHEME_SOURCE("(import (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (vertices-in (faces-in (bounding-plane 0 0 1 -1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (vertices-in (edges-in (bounding-plane 0 0 1 -1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (faces-in (vertices-in (bounding-plane 0 0 1 -1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (faces-partially-in"
                   "  (vertices-in (bounding-plane 0 0 1 -1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (faces-in (edges-in (bounding-plane 0 0 1 -1))))"
                   "(color-selection"
                   " (cuboid 2 2 2)"
                   " (faces-partially-in"
                   "  (edges-in (bounding-plane 0 0 1 -1))))"
                   "(remesh"
                   " (cuboid 2 2 2)"
                   " (edges-in"
                   "  (vertices-in (bounding-plane 0 0 1 -1))) 1)"
                   "(remesh"
                   " (cuboid 2 2 2)"
                   " (edges-partially-in"
                   "  (vertices-in (bounding-plane 0 0 1 -1))) 1)"
                   "(remesh"
                   " (cuboid 2 2 2)"
                   " (edges-in"
                   "  (faces-in (bounding-plane 0 0 1 -1))) 1)"
                   "(remesh"
                   " (cuboid 2 2 2)"
                   " (edges-partially-in"
                   "  (faces-in (bounding-plane 0 0 1 -1))) 1)")
EXPECTING("color_selection(mesh(cuboid(2,2,2)),faces_partially_in("
          "vertices_in(bounding_plane(plane(0,0,1,-1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),faces_in("
          "vertices_in(bounding_plane(plane(0,0,1,-1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),faces_partially_in("
          "edges_in(bounding_plane(plane(0,0,1,-1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),faces_in("
          "edges_in(bounding_plane(plane(0,0,1,-1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "faces_in(bounding_plane(plane(0,0,1,-1)))),0,0,0,255)",
          "color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "edges_in(bounding_plane(plane(0,0,1,-1)))),0,0,0,255)",
          "remesh(cuboid(2,2,2),"
          "edges_in(vertices_in(bounding_plane(plane(0,0,1,-1)))),1,1)",
          "remesh(cuboid(2,2,2),"
          "edges_partially_in(vertices_in(bounding_plane(plane(0,0,1,-1)))),"
          "1,1)",
          "remesh(cuboid(2,2,2),"
          "edges_in(faces_in(bounding_plane(plane(0,0,1,-1)))),1,1)",
          "remesh(cuboid(2,2,2),"
          "edges_partially_in(faces_in(bounding_plane(plane(0,0,1,-1)))),"
          "1,1)")

////////////////////////
// Boolean operations //
////////////////////////

DEFINE_TEST_CASE(polygon_boolean)
WITH_LUA_SOURCE("g = require 'gamma.polygons'"
                "P = (g.rectangle(2, 2)"
                "     + g.simple(point(1, -1), point(2, -1), point(1, 1)))"
                "Q = g.rectangle(2, 2) - g.circle(1)"
                "R = g.ellipse(4, 2) * g.circle(1)")
WITH_SCHEME_SOURCE("(import (gamma polygons) (gamma operations))"
                   "(union (rectangle 2 2)"
                   "       (simple-polygon (point 1 -1) (point 2 -1) (point 1 1)))"
                   "(difference (rectangle 2 2) (circle 1))"
                   "(intersection (ellipse 4 2) (circle 1))")
EXPECTING("join(" RECTANGLE_TAG ","
          "polygon(point(1,-1),point(2,-1),point(1,1)))",
          "difference(circles(" RECTANGLE_TAG "),circle(1))",
          "intersection(transform(conics(circle(1)),scaling(4,2)),"
          "conics(circle(1)))")

DEFINE_TEST_CASE(polygon_boolean_many)
WITH_LUA_SOURCE("g = require 'gamma.polygons'"
                "op = require 'gamma.operations'"

                "P = op.union(g.rectangle(2, 2),"
                "             g.simple("
                "                 point(1, -1), point(2, -1), point(1, 1)),"
                "             g.circle(1))"
                "Q = op.difference(g.circle(5), g.circle(1.25), g.rectangle(2, 2))"
                "R = op.intersection(g.circle(3), g.circle(2), g.circle(1))")
WITH_SCHEME_SOURCE("(import (gamma polygons) (gamma operations))"
                   "(union (rectangle 2 2)"
                   "       (simple-polygon (point 1 -1) (point 2 -1) (point 1 1))"
                   "       (circle 1))"
                   "(difference (circle 5) (circle 5/4) (rectangle 2 2))"
                   "(intersection (circle 3) (circle 2) (circle 1))")
EXPECTING("join(circles(join(" RECTANGLE_TAG ","
          "polygon(point(1,-1),point(2,-1),point(1,1)))),"
          "circle(1))",
          "difference(difference(circle(5),circle(5/4)),"
          "circles(" RECTANGLE_TAG "))",
          "intersection(intersection(circle(3),circle(2)),circle(1))")

DEFINE_TEST_CASE(polyhedron_boolean)
WITH_LUA_SOURCE("h = require 'gamma.polyhedra'"
                "P = (h.tetrahedron(1, 1, 1)"
                "     + h.tetrahedron(-1, 1, 1))"
                "Q = (h.tetrahedron(1, 1, 1)"
                "     - h.tetrahedron(1 / 2, 1 / 2, 1 / 2))"
                "R = (h.tetrahedron(1, 1, 1)"
                "     * h.tetrahedron(1 / 2, 1 / 2, 1 / 2))"
                "S = (h.tetrahedron(1, 1, 1) * plane(0, 0, -1, 0))")
WITH_SCHEME_SOURCE("(import (gamma polyhedra) (gamma operations))"
                   "(union (tetrahedron 1 1 1)"
                   "       (tetrahedron -1 1 1))"
                   "(difference (tetrahedron 1 1 1)"
                   "            (tetrahedron 1/2 1/2 1/2))"
                   "(intersection (tetrahedron 1 1 1)"
                   "              (tetrahedron 1/2 1/2 1/2))"
                   "(clip (tetrahedron 1 1 1)"
                   "      (plane 0 0 -1 0))")
EXPECTING("join(tetrahedron(1,1,1),tetrahedron(-1,1,1))",
          "difference(tetrahedron(1,1,1),tetrahedron(1/2,1/2,1/2))",
          "intersection(tetrahedron(1,1,1),tetrahedron(1/2,1/2,1/2))",
          "clip(tetrahedron(1,1,1),plane(0,0,-1,0))")

DEFINE_TEST_CASE(polyhedron_boolean_many)
WITH_LUA_SOURCE("h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "set_projection_tolerance(9.5367431640625e-07)"
                "set_curve_tolerance(0.125)"
                "P = op.union(h.cuboid(2, 2, 2),"
                "             h.tetrahedron(1, 1, 1),"
                "             h.sphere(1))"
                "Q = op.difference(h.sphere(5),"
                "                  h.sphere(1.25),"
                "                  h.cuboid(2, 2, 2))"
                "R = op.intersection("
                "    h.sphere(3), h.sphere(2), h.sphere(1))")
WITH_SCHEME_SOURCE("(import (gamma polyhedra) (gamma operations))"
                   "(set-projection-tolerance! 1/1048576)"
                   "(set-curve-tolerance! 1/8)"
                   "(union (cuboid 2 2 2) (tetrahedron 1 1 1) (sphere 1))"
                   "(difference (sphere 5) (sphere 5/4) (cuboid 2 2 2))"
                   "(intersection (sphere 3) (sphere 2) (sphere 1))")
EXPECTING("join(join(cuboid(2,2,2),tetrahedron(1,1,1)),"
          "sphere(1,1/8,1/1048576))",
          "difference("
          "difference(sphere(5,1/8,1/1048576),sphere(5/4,1/8,1/1048576)),"
          "cuboid(2,2,2))",
          "intersection("
          "intersection(sphere(3,1/8,1/1048576),sphere(2,1/8,1/1048576)),"
          "sphere(1,1/8,1/1048576))")

struct set_booleans_mode {
private:
    Polyhedron_booleans_mode old;

public:

    set_booleans_mode(Polyhedron_booleans_mode mode) {
        old = Options::polyhedron_booleans;
        Options::polyhedron_booleans = mode;
    }

    ~set_booleans_mode() {
        Options::polyhedron_booleans = old;
    }
};

#define WITH_POLYHEDRON_BOOLEANS_SOURCES                                \
WITH_LUA_SOURCE("h = require 'gamma.polyhedra'"                               \
                "op = require 'gamma.operations'"                             \
                                                                        \
                "set_projection_tolerance(9.5367431640625e-07)"         \
                "set_curve_tolerance(0.125)"                            \
                "P = (op.minkowski_sum("                                \
                "         h.tetrahedron(1, 1, 1),"                      \
                "         h.octahedron(0.25, 0.25, 0.125))"             \
                "     - h.sphere(0.5))"                                 \
                "Q = (op.minkowski_sum("                                \
                "         h.tetrahedron(1, 1, 1),"                      \
                "         h.octahedron(0.25, 0.25, 0.125))"             \
                "     - op.minkowski_sum("                              \
                "         h.tetrahedron(-1, 1, 1),"                     \
                "         h.octahedron(0.25, 0.25, 0.125)))"            \
                "R = op.clip("                                          \
                "        op.minkowski_sum("                             \
                "            h.tetrahedron(1, 1, 1),"                   \
                "            h.octahedron(0.25, 0.25, 0.125)),"         \
                "        plane(0, 0, 1, -0.5))"                         \
                "S = op.clip("                                          \
                "        h.tetrahedron(1, 1, 1),"                       \
                "        plane(0, 0, 1, -0.5))")                        \
WITH_SCHEME_SOURCE("(import (gamma polyhedra) (gamma operations))"      \
                   "(set-projection-tolerance! 1/1048576)"              \
                   "(set-curve-tolerance! 1/8)"                         \
                   "(difference (minkowski-sum"                         \
                   "             (tetrahedron 1 1 1)"                   \
                   "             (octahedron 1/4 1/4 1/8))"             \
                   "            (sphere 1/2))"                          \
                   "(difference (minkowski-sum"                         \
                   "             (tetrahedron 1 1 1)"                   \
                   "             (octahedron 1/4 1/4 1/8))"             \
                   "            (minkowski-sum"                         \
                   "             (tetrahedron -1 1 1)"                  \
                   "             (octahedron 1/4 1/4 1/8)))"            \
                   "(clip (minkowski-sum"                               \
                   "       (tetrahedron 1 1 1)"                         \
                   "       (octahedron 1/4 1/4 1/8))"                   \
                   "      (plane 0 0 1 -1/2))"                          \
                   "(clip (tetrahedron 1 1 1)"                          \
                   "      (plane 0 0 1 -1/2))")

DEFINE_TEST_CASE(
    polyhedron_booleans_mode_nef,
    * boost::unit_test::fixture<set_booleans_mode>(
        Polyhedron_booleans_mode::NEF))
WITH_POLYHEDRON_BOOLEANS_SOURCES
EXPECTING("difference(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),nef(sphere(1/2,1/8,1/1048576)))",
          "difference(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),"
          "minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))))",
          "clip(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),plane(0,0,1,-1/2))",
          "clip(nef(tetrahedron(1,1,1)),plane(0,0,1,-1/2))")

DEFINE_TEST_CASE(
    polyhedron_booleans_mode_corefine,
    * boost::unit_test::fixture<set_booleans_mode>(
        Polyhedron_booleans_mode::COREFINE))
WITH_POLYHEDRON_BOOLEANS_SOURCES
EXPECTING("difference(polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))),sphere(1/2,1/8,1/1048576))",
          "difference(polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))),"
          "polyhedron(minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))))",
          "clip(polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))),plane(0,0,1,-1/2))",
          "clip(tetrahedron(1,1,1),plane(0,0,1,-1/2))")

DEFINE_TEST_CASE(
    polyhedron_booleans_mode_auto,
    * boost::unit_test::fixture<set_booleans_mode>(
        Polyhedron_booleans_mode::AUTO))
WITH_POLYHEDRON_BOOLEANS_SOURCES
EXPECTING("difference(polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))),sphere(1/2,1/8,1/1048576))",
          "difference(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),"
          "minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))))",
          "clip(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),plane(0,0,1,-1/2))",
          "clip(tetrahedron(1,1,1),plane(0,0,1,-1/2))")

#undef WITH_POLYHEDRON_BOOLEANS_SOURCES

///////////
// Hulls //
///////////

DEFINE_TEST_CASE(polygon_hull)
WITH_LUA_SOURCE("g = require 'gamma.polygons'"
                "op = require 'gamma.operations'"

                "P = op.hull(g.rectangle(2, 2),"
                "            g.simple(point(1, -1), point(2, -1), point(1, 1)))"
                "Q = op.hull(point(0, 0), point(1, 0), point(0, 1))"
                "R = op.hull(g.rectangle(2, 2), point(2, -1))")
WITH_SCHEME_SOURCE("(import (gamma polygons) (gamma operations))"
                   "(hull (rectangle 2 2)"
                   "      (simple-polygon"
                   "       (point 1 -1) (point 2 -1) (point 1 1)))"
                   "(hull (point 0 0) (point 1 0) (point 0 1))"
                   "(hull (rectangle 2 2) (point 2 -1))")
EXPECTING("hull(" RECTANGLE_TAG ","
          "polygon(point(1,-1),point(2,-1),point(1,1)))",
          "hull(point(0,0),point(1,0),point(0,1))",
          "hull(" RECTANGLE_TAG ",point(2,-1))")

DEFINE_TEST_CASE(polyhedron_hull)
WITH_LUA_SOURCE("h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.hull(h.tetrahedron(1, 1, 1), h.tetrahedron(-1, 1, 1))"
                "Q = op.hull(h.tetrahedron(1, 1, 1), point(-1, 0, 0))"
                "R = op.hull(point(0, 0, 0), point(1, 0, 0),"
                "            point(0, 1, 0), point(0, 0, 1))")
WITH_SCHEME_SOURCE("(import (gamma polyhedra) (gamma operations))"
                   "(hull (tetrahedron 1 1 1) (tetrahedron -1 1 1))"
                   "(hull (tetrahedron 1 1 1) (point -1 0 0))"
                   "(hull (point 0 0 0) (point 1 0 0)"
                   "      (point 0 1 0) (point 0 0 1))")
EXPECTING("hull(tetrahedron(1,1,1),tetrahedron(-1,1,1))",
          "hull(tetrahedron(1,1,1),point(-1,0,0))",
          "hull(point(0,0,0),point(1,0,0),point(0,1,0),point(0,0,1))")

DEFINE_TEST_CASE(subdivision)
WITH_LUA_SOURCE("h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.subdivide_catmull_clark(h.tetrahedron(1, 1, 1), 1)"
                "Q = op.subdivide_doo_sabin(h.tetrahedron(1, 1, 1), 2)"
                "R = op.subdivide_loop(h.tetrahedron(1, 1, 1), 3)"
                "S = op.subdivide_sqrt_3(h.tetrahedron(1, 1, 1), 4)")
WITH_SCHEME_SOURCE("(import (gamma polyhedra) (gamma operations))"
                   "(subdivide-catmull-clark (tetrahedron 1 1 1) 1)"
                   "(subdivide-doo-sabin (tetrahedron 1 1 1) 2)"
                   "(subdivide-loop (tetrahedron 1 1 1) 3)"
                   "(subdivide-sqrt-3 (tetrahedron 1 1 1) 4)")
EXPECTING("sqrt_3(tetrahedron(1,1,1),4)",
          "loop(tetrahedron(1,1,1),3)",
          "doo_sabin(tetrahedron(1,1,1),2)",
          "catmull_clark(tetrahedron(1,1,1),1)")

DEFINE_TEST_CASE(minkowski_sum)
WITH_LUA_SOURCE("g = require 'gamma.polygons'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "set_projection_tolerance(9.5367431640625e-07)"
                "P = op.minkowski_sum(g.rectangle(2, 2), g.regular(4, 0.5))"
                "set_curve_tolerance(0.125)"
                "P = op.minkowski_sum(g.rectangle(2, 2), g.circle(0.5))"
                "T = op.minkowski_sum(h.tetrahedron(5, 5, 5), "
                "                     h.octahedron(1, 1, 0.5))")
WITH_SCHEME_SOURCE("(import (gamma polygons) (gamma polyhedra)"
                   "        (gamma operations))"
                   "(set-projection-tolerance! 1/1048576)"
                   "(minkowski-sum (rectangle 2 2) (regular-polygon 4 1/2))"
                   "(set-curve-tolerance! 1/8)"
                   "(minkowski-sum (rectangle 2 2) (circle 1/2))"
                   "(minkowski-sum (tetrahedron 5 5 5) (octahedron 1 1 1/2))")
EXPECTING("minkowski_sum(" RECTANGLE_TAG ",regular_polygon(4,1/2,1/1048576))",
          "minkowski_sum(" RECTANGLE_TAG ",segments(circle(1/2),1/8,1/1048576))",
          "minkowski_sum(nef(tetrahedron(5,5,5)),nef(octahedron(1,1,1/2,1/2)))")

/////////////////////
// Mesh operations //
/////////////////////

DEFINE_TEST_CASE(perturb)
WITH_LUA_SOURCE("v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.perturb(h.tetrahedron(1, 1, 1), 0.125)"
                "Q = op.perturb(h.cuboid(2, 2, 2),"
                "               s.vertices_in(v.halfspace(0, 0, 1, 0)),"
                "               9.5367431640625e-07)")
WITH_SCHEME_SOURCE("(import (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(perturb (tetrahedron 1 1 1) 1/8)"
                   "(perturb (cuboid 2 2 2)"
                   "         (vertices-in (bounding-halfspace 0 0 1 0))"
                   "                       1/1048576)")
EXPECTING("perturb(tetrahedron(1,1,1),1/8)",
          "perturb(cuboid(2,2,2),"
          "vertices_in(bounding_halfspace(plane(0,0,1,0))),1/1048576)")

DEFINE_TEST_CASE(refine)
WITH_LUA_SOURCE("v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "set_projection_tolerance(9.5367431640625e-07)"
                "set_curve_tolerance(0.125)"
                "P = op.refine(h.sphere(2), 5)"
                "Q = op.refine(h.sphere(2),"
                "              s.faces_in(v.plane(0, 0, 1, 1)"
                "                         + v.plane(0, 0, -1, 1)),"
                "              5)")
WITH_SCHEME_SOURCE("(import (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(set-projection-tolerance! 1/1048576)"
                   "(set-curve-tolerance! 1/8)"
                   "(refine (sphere 2) 5)"
                   "(refine (sphere 2) (faces-in"
                   "                    (union"
                   "                     (bounding-plane 0 0 1 1)"
                   "                     (bounding-plane 0 0 -1 1))) 5)")
EXPECTING("refine(sphere(2,1/8,1/1048576),5)",
          "refine(sphere(2,1/8,1/1048576),"
          "faces_in(join(bounding_plane(plane(0,0,1,1)),"
          "bounding_plane(plane(0,0,-1,1)))),5)")

DEFINE_TEST_CASE(remesh)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.remesh(h.cuboid(1, 1, 1), 0.25)"
                "Q = op.remesh(h.cuboid(1, 1, 1),"
                "              s.faces_partially_in("
                "                  t.translation(0, 0, 0.5)"
                "                  * v.box(1, 1, 1)),"
                "              0.5)"
                "R = op.remesh(h.cuboid(1, 1, 1), 0.125, 2)"
                "S = op.remesh(h.cuboid(1, 1, 1),"
                "           s.faces_partially_in("
                "               v.halfspace(-1, 0, 0, 1)"
                "               - v.halfspace(1, 0, 0, -1)),"
                "           1, 3)")
WITH_SCHEME_SOURCE("(import (gamma transformation)"
                   "        (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(remesh (cuboid 1 1 1) 1/4)"
                   "(remesh (cuboid 1 1 1)"
                   "        (faces-partially-in"
                   "         (transformation-apply"
                   "          (translation 0 0 1/2)"
                   "          (bounding-box 1 1 1))) 1/2)"
                   "(remesh (cuboid 1 1 1) 1/8 2)"
                   "(remesh (cuboid 1 1 1)"
                   "        (faces-partially-in"
                   "         (difference"
                   "          (bounding-halfspace -1 0 0 1)"
                   "          (bounding-halfspace 1 0 0 -1))) 1 3)")
EXPECTING("remesh(cuboid(1,1,1),1/4,1)",
          "remesh(cuboid(1,1,1),"
          "faces_partially_in(bounding_box("
          "plane(-1,0,0,1/2),plane(1,0,0,1/2),plane(0,-1,0,1/2),"
          "plane(0,1,0,1/2),plane(0,0,-1,1),plane(0,0,1,0))),1/2,1)",
          "remesh(cuboid(1,1,1),1/8,2)",
          "remesh(cuboid(1,1,1),"
          "faces_partially_in(difference("
          "bounding_halfspace(plane(-1,0,0,1)),"
          "bounding_halfspace(plane(1,0,0,-1)))),1,3)")

DEFINE_TEST_CASE(remesh_constrained)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "set_projection_tolerance(9.5367431640625e-07)"
                "set_curve_tolerance(0.125)"
                "P = op.remesh("
                "        h.sphere(2),"
                "        s.edges_in(v.halfspace(0, 0, 1, 0)), 0.125)"
                "Q = op.remesh("
                "        h.sphere(2),"
                "        s.faces_in(v.sphere(2)"
                "                   * (t.rotation(90, 1)"
                "                      * v.halfspace(0, 0, 1, 0))),"
                "        s.edges_partially_in("
                "            v.halfspace(0, 0, 1, 0)), 0.5)")
WITH_SCHEME_SOURCE("(import (gamma transformation)"
                   "        (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(set-projection-tolerance! 1/1048576)"
                   "(set-curve-tolerance! 1/8)"
                   "(remesh (sphere 2)"
                   "        (edges-in (bounding-halfspace 0 0 1 0)) 1/8)"
                   "(remesh (sphere 2)"
                   "        (faces-in"
                   "         (intersection"
                   "          (bounding-sphere 2)"
                   "          (bounding-halfspace 1 0 0 0)))"
                   "        (edges-partially-in"
                   "         (bounding-halfspace 0 0 1 0)) 1/2)")
EXPECTING("remesh(sphere(2,1/8,1/1048576),"
          "edges_in(bounding_halfspace(plane(0,0,1,0))),"
          "1/8,1)",
          "remesh(sphere(2,1/8,1/1048576),"
          "faces_in(intersection(bounding_sphere(point(0,0,0),2),"
          "bounding_halfspace(plane(1,0,0,0)))),"
          "edges_partially_in(bounding_halfspace(plane(0,0,1,0))),"
          "1/2,1)")

DEFINE_TEST_CASE(fair)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "set_projection_tolerance(9.5367431640625e-07)"
                "set_curve_tolerance(0.125)"
                "P = op.fair(h.cylinder(1, 5),"
                "            s.vertices_in(v.halfspace(0, 0, 1, 0)), 0)"
                "Q = op.fair(h.cylinder(1, 5),"
                "            s.vertices_in(t.translation(0, 0, 5)"
                "                          * v.cylinder(1, 1)))")
WITH_SCHEME_SOURCE("(import (gamma transformation)"
                   "        (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(set-projection-tolerance! 1/1048576)"
                   "(set-curve-tolerance! 1/8)"
                   "(fair (cylinder 1 5)"
                   "      (vertices-in (bounding-halfspace 0 0 1 0)) 0)"
                   "(fair (cylinder 1 5)"
                   "      (vertices-in"
                   "       (transformation-apply"
                   "        (translation 0 0 5)"
                   "        (bounding-cylinder 1 1))))")
EXPECTING("fair(extrusion(regular_polygon(7,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2)),"
          "vertices_in(bounding_halfspace(plane(0,0,1,0))),0)",
          "fair(extrusion(regular_polygon(7,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2)),"
          "vertices_in(bounding_cylinder(point(0,0,9/2),vector(0,0,1),1,1)),1)")

DEFINE_TEST_CASE(deform)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "set_projection_tolerance(9.5367431640625e-07)"
                "set_curve_tolerance(0.125)"
                "P = op.deform(h.cylinder(1, 5),"
                "              s.vertices_in(v.halfspace(0, 0, 1, -2)),"
                "              t.translation(0, 0, 1),"
                "              s.vertices_in("
                "                  op.complement(v.halfspace(0, 0, -1, -2))),"
                "              t.translation(0, 0, -1), 0.015625)"
                "Q = op.deform(h.cylinder(1, 5),"
                "              s.vertices_in(~v.halfspace(0, 0, 1, -2)),"
                "              s.vertices_in(v.halfspace(0, 0, 1, -2)),"
                "              t.rotation(90, 1), 0.015625, 100)")
WITH_SCHEME_SOURCE("(import (gamma transformation)"
                   "        (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(set-projection-tolerance! 1/1048576)"
                   "(set-curve-tolerance! 1/8)"
                   "(deform (cylinder 1 5)"
                   "        (vertices-in (bounding-halfspace 0 0 1 -2))"
                   "        (translation 0 0 1)"
                   "        (vertices-in"
                   "         (complement (bounding-halfspace 0 0 -1 -2)))"
                   "        (translation 0 0 -1) 1/64)"
                   "(deform (cylinder 1 5)"
                   "        (vertices-in"
                   "         (complement (bounding-halfspace 0 0 1 -2)))"
                   "        (vertices-in (bounding-halfspace 0 0 1 -2))"
                   "        (rotation 90 1) 1/64 100)")
EXPECTING("deform(extrusion(regular_polygon(7,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2)),"
          "vertices_in(bounding_halfspace(plane(0,0,1,-2))),"
          "translation(0,0,1),"
          "vertices_in(complement(bounding_halfspace(plane(0,0,-1,-2)))),"
          "translation(0,0,-1),1/64,4294967295)",
          "deform(extrusion(regular_polygon(7,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2)),"
          "vertices_in(complement(bounding_halfspace(plane(0,0,1,-2)))),"
          "vertices_in(bounding_halfspace(plane(0,0,1,-2))),"
          "rotation(0,0,1,0,1,0,-1,0,0),1/64,100)")

DEFINE_TEST_CASE(deflate)
WITH_LUA_SOURCE("v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.deflate("
                "        op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1), 3)"
                "Q = op.deflate("
                "        op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
                "        s.vertices_in(v.plane(0,0,1,-1)), 3)"
                "R = op.deflate("
                "        op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
                "        s.vertices_in(v.plane(0,0,1,-1)), 3, 1 / 2)"
                "S = op.deflate("
                "        op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
                "        s.vertices_in(v.plane(0,0,1,-1)), 3, 1/4, 1/8)")
WITH_SCHEME_SOURCE("(import (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(deflate"
                   "  (remesh (cuboid 2 2 2) 1/8 1) 3)"
                   "(deflate"
                   "  (remesh (cuboid 2 2 2) 1/8 1)"
                   "  (vertices-in (bounding-plane 0 0 1 -1)) 3)"
                   "(deflate"
                   "  (remesh (cuboid 2 2 2) 1/8 1)"
                   "  (vertices-in (bounding-plane 0 0 1 -1)) 3 1/2)"
                   "(deflate"
                   "  (remesh (cuboid 2 2 2) 1/8 1)"
                   "  (vertices-in (bounding-plane 0 0 1 -1)) 3 1/4 1/8)")
EXPECTING("deflate(remesh(cuboid(2,2,2),1/8,1),3,1/10,0)",
          "deflate(remesh(cuboid(2,2,2),1/8,1),"
          "vertices_in(bounding_plane(plane(0,0,1,-1))),3,1/10,0)",
          "deflate(remesh(cuboid(2,2,2),1/8,1),"
          "vertices_in(bounding_plane(plane(0,0,1,-1))),"
          "3,1/2,0)",
          "deflate(remesh(cuboid(2,2,2),1/8,1),"
          "vertices_in(bounding_plane(plane(0,0,1,-1))),"
          "3,1/4,1/8)")

DEFINE_TEST_CASE(smooth_shape)
WITH_LUA_SOURCE("v = require 'gamma.volumes'"
                "s = require 'gamma.selection'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.smooth_shape("
                "        op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
                "        s.faces_in(v.halfspace(0,1,0,0)),"
                "        s.vertices_in(v.halfspace(0,0,1,0)),"
                "        1 / 64, 1)"
                "Q = op.smooth_shape("
                "        op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
                "        s.faces_in(v.halfspace(0,1,0,0)),"
                "        1 / 64, 1)"
                "R = op.smooth_shape("
                "        op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
                "        s.vertices_in(v.halfspace(0,0,1,0)),"
                "        1 / 64, 1)"
                "S = op.smooth_shape("
                "        op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
                "        1 / 64, 1)")
WITH_SCHEME_SOURCE("(import (gamma volumes) (gamma selection)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(smooth-shape"
                   "  (remesh (cuboid 2 2 2) 1/8 1)"
                   "  (faces-in (bounding-halfspace 0 1 0 0))"
                   "  (vertices-in (bounding-halfspace 0 0 1 0))"
                   "  1/64 1)"
                   "(smooth-shape"
                   "  (remesh (cuboid 2 2 2) 1/8 1)"
                   "  (faces-in (bounding-halfspace 0 1 0 0))"
                   "  1/64 1)"
                   "(smooth-shape"
                   "  (remesh (cuboid 2 2 2) 1/8 1)"
                   "  (vertices-in (bounding-halfspace 0 0 1 0))"
                   "  1/64 1)"
                   "(smooth-shape"
                   "  (remesh (cuboid 2 2 2) 1/8 1)"
                   "  1/64 1)")
EXPECTING("smooth_shape(remesh(cuboid(2,2,2),1/8,1),"
          "faces_in(bounding_halfspace(plane(0,1,0,0))),"
          "vertices_in(bounding_halfspace(plane(0,0,1,0))),"
          "1/64,1)",
          "smooth_shape(remesh(cuboid(2,2,2),1/8,1),"
          "faces_in(bounding_halfspace(plane(0,1,0,0))),"
          "1/64,1)",
          "smooth_shape(remesh(cuboid(2,2,2),1/8,1),"
          "vertices_in(bounding_halfspace(plane(0,0,1,0))),"
          "1/64,1)",
          "smooth_shape(remesh(cuboid(2,2,2),1/8,1),"
          "1/64,1)")

DEFINE_TEST_CASE(corefine)
WITH_LUA_SOURCE("t = require 'gamma.transformation'"
                "h = require 'gamma.polyhedra'"
                "op = require 'gamma.operations'"

                "P = op.corefine(h.tetrahedron(1, 1, 1), plane(0, 0, 1, -0.5))"
                "Q = op.corefine(h.cuboid(2, 2, 2),"
                "                t.translation(1, 1, 1) * h.cuboid(2, 2, 2))")
WITH_SCHEME_SOURCE("(import (gamma transformation)"
                   "        (gamma polyhedra) (gamma operations))"
                   "(corefine (tetrahedron 1 1 1) (plane 0 0 1 -1/2))"
                   "(corefine (cuboid 2 2 2)"
                   "          (transformation-apply (translation 1 1 1)"
                   "          (cuboid 2 2 2)))")
EXPECTING("corefine(tetrahedron(1,1,1),plane(0,0,1,-1/2))",
          "corefine(cuboid(2,2,2),transform(cuboid(2,2,2),translation(1,1,1)))")

////////////
// Output //
////////////

struct enable_output {
private:
    int output_stl, output;

public:

    enable_output() {
        output_stl = Flags::output_stl;
        Flags::output_stl = 1;

        output = Flags::output;
        Flags::output = 1;

        Options::outputs.push_front("foo.off");
        Options::outputs.push_front("bar.wrl:foo");
        Options::outputs.push_front("qux:foo");
        Options::outputs.push_front(":foo");
        Options::outputs.push_front(":");
    }

    ~enable_output() {
        Flags::output_stl = output_stl;
        Flags::output = output;

        Options::outputs.pop_front();
        Options::outputs.pop_front();
        Options::outputs.pop_front();
        Options::outputs.pop_front();
    }
};

DEFINE_TEST_CASE(output, * boost::unit_test::fixture<enable_output>())
WITH_LUA_SOURCE("h = require 'gamma.polyhedra'"

                "output(\"foo\", h.tetrahedron(1, 1, 1))"
                "output(h.tetrahedron(1, 1, 1), h.tetrahedron(-1, 1, 1))")
WITH_SCHEME_SOURCE("(import (gamma polyhedra))"
                   "(define-output foo (tetrahedron 1 1 1))"
                   "(output (tetrahedron 1 1 1) (tetrahedron -1 1 1))")
EXPECTING("write_off(\"foo.off\",mesh(tetrahedron(1,1,1)))",
          "write_stl(\"foo.stl\",mesh(tetrahedron(1,1,1)))",
          "write_wrl(\"bar.wrl\",mesh(tetrahedron(1,1,1)))",
          "pipe(\"qux\",mesh(tetrahedron(1,1,1)))",
          "pipe(mesh(tetrahedron(1,1,1)))",
          "pipe(mesh(tetrahedron(1,1,1)),mesh(tetrahedron(-1,1,1)))")

#undef DEFINE_TEST_CASE
#undef WITH_LUA_SOURCE
#undef WITH_SCHEME_SOURCE
#undef EXPECTING

BOOST_AUTO_TEST_SUITE_END()
