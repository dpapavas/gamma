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

#include <filesystem>
#include <fstream>
#include <set>

#include "kernel.h"
#include "operation.h"
#include "tolerances.h"
#include "evaluation.h"
#include "lua_frontend.h"
#include "scheme_frontend.h"
#include "fixtures.h"

// Document: program

// # Front End Tests

// These tests exercise the language front ends.  They do so by
// executing test programs in each language and testing whether the
// front end produces the correct operations; evaluation of those
// operations is tested elsewhere.

// We therefore execute programs that should produce the same set of
// operations in each language and, after each execution, look through
// the operations map to ensure that all expected operations, and only
// those, have been created.

// All tests work by the following fixture, that takes care of placing
// the source for each language into a temporary file which is then
// loaded by synthesizing an appropriate command line.

std::unordered_map<
    std::string, std::weak_ptr<Operation>> &_get_operations_map(void);

struct Frontend_fixture: Main_fixture {
    std::forward_list<std::pair<std::string, std::ofstream>> files;

    const char *add_file(const char *s) {
        std::string t = std::tmpnam(nullptr);
        files.emplace_front(t, t);
        files.front().second << s << std::endl;

        return files.front().first.c_str();
    }

    Frontend_fixture() {
        push(Flags::dry_run, 1);
        push(Tolerances::curve, FT(FT::ET(1, 1024)));
        push(Tolerances::projection, FT(FT::ET(1, 1'048'576)));
    }

    ~Frontend_fixture() {
        for (auto &[a, b]: files) {
            b.close();

            if (!std::getenv("KEEP")) {
                std::filesystem::remove(a);
            }
        }

        pop(Flags::dry_run);
        pop(Tolerances::curve);
        pop(Tolerances::projection);
    }
};

// The test body itself is made up of invocations of the following
// macros.  A typical test would look like:

// ```
// DEFINE_TEST_CASE(..)
// WITH_SOURCE("lua",
//             "h = require 'gamma.polyhedra'"
//
//             "h.tetrahedron(1, 1, 1)"
//             "g.sphere(1)")
// WITH_SOURCE("scheme",
//             "(import (gamma polygons))"
//
//             "(tetrahedron 1 1 1)
//             "(sphere 1)")
// EXPECTING("tetrahedron(1,1,1)",
//           "sphere(1/3,1/1024,1/1048576)")
// ```

#ifdef HAVE_SCHEME
#define SCHEME_ITEM {"scheme", {"test_case", "-x", "scheme"}}
#define CLOSE_SCHEME() close_scheme();
#else
#define SCHEME_ITEM
#define CLOSE_SCHEME()
#endif

#ifdef HAVE_LUA
#define LUA_ITEM {"lua", {"test_case", "-x", "lua"}}
#define CLOSE_LUA() close_lua();
#else
#define LUA_ITEM
#define CLOSE_LUA()
#endif

// Define the Boost test case and declare a language -> sources map.

#define DEFINE_TEST_CASE(...)                                           \
BOOST_AUTO_TEST_CASE(__VA_ARGS__)                                       \
{                                                                       \
    std::map<const char *, std::vector<const char *>> argv_map = {      \
        SCHEME_ITEM,                                                    \
        LUA_ITEM,                                                       \
    };

// Add source code for a language.  We write out the source to a
// temporary file and add it to the command line.  Additional
// arguments to be added to the command line can be specified as
// variadic arguments.

#define WITH_SOURCE(LANG, SOURCE, ...)                                  \
    if (const auto &p = argv_map.find(LANG); p != argv_map.end()) {     \
        const auto l = std::initializer_list<const char *>{__VA_ARGS__}; \
        if (l.size() > 0 && !std::strcmp(l.begin()[0], "--")) {         \
            p->second.push_back(add_file(SOURCE));                      \
            for (const char *x: l) {                                    \
                p->second.push_back(x);                                 \
            }                                                           \
        } else {                                                        \
            for (const char *x: l) {                                    \
                p->second.push_back(x);                                 \
            }                                                           \
            p->second.push_back(add_file(SOURCE));                      \
        }                                                               \
    }

// Run all sources for all front ends.

#define RUN(THEN)                                                       \
                                                                        \
    for (const auto [a, b]: argv_map) {                                 \
        if (b.size() == 3) {                                            \
            continue;                                                   \
        }                                                               \
                                                                        \
        const int i = parse_options(                                    \
            b.size(), const_cast<char **>(b.data()));                   \
                                                                        \
        THEN

// Execute the sources as above and compare the resulting operations
// to a set of expected operations.

// Note that we don't need operations to be succeeded by sink
// operations, so that they would be ultimately evaluated in a normal
// run.  They need not even be referenced by the source file that
// created them, even though they risk being garbage collected.  Every
// operation generated during a normal run should end up in the
// operations map as a (potentially expired) weak pointer.  By
// comparing the keys in the map with the expected set, we ensure that
// exactly those expected operations were generated and only those.

// Evaluating the operations is not necessary for the testing itself,
// but it ensures that the operations map and associated evaluation
// state is reset after each test case.

#define EXPECTING(...)                                                  \
        RUN()                                                           \
        BOOST_TEST(i >= 0, a << ": run failed");                        \
                                                                        \
        CLOSE_SCHEME();                                                 \
        CLOSE_LUA();                                                    \
                                                                        \
        std::set<std::string> s, t = {__VA_ARGS__};                     \
        for (auto &[k, x]: _get_operations_map()) {                     \
            s.insert(k);                                                \
        }                                                               \
                                                                        \
        for (auto it_1 = s.cbegin(), it_2 = t.cbegin(),                 \
                 end_1 = s.cend(), end_2 = t.cend();                    \
             it_1 != end_1 || it_2 != end_2;) {                         \
            if (it_2 == end_2 || (it_1 != end_1 && *it_1 < *it_2)) {    \
                BOOST_ERROR(a << ": extra tag '" << *it_1 << "' found"); \
                it_1++;                                                 \
            } else if (it_1 == end_1 || (it_2 != end_2 && *it_1 > *it_2)) { \
                BOOST_ERROR(a << ": tag '" << *it_2 << "' not found");  \
                it_2++;                                                 \
            } else {                                                    \
                it_1++;                                                 \
                it_2++;                                                 \
            }                                                           \
        }                                                               \
                                                                        \
        evaluate_operations();                                          \
    }                                                                   \
}

#define EXPECTING_SUCCESS(P)                                            \
        if (P) {                                                        \
            BOOST_TEST(i >= 0, a << ": run failed");                    \
        } else {                                                        \
            BOOST_TEST(                                                 \
                i == -EXIT_FAILURE, a << ": run succeeded");            \
        }                                                               \
                                                                        \
        CLOSE_SCHEME();                                                 \
        CLOSE_LUA();                                                    \
    }                                                                   \
}

BOOST_FIXTURE_TEST_SUITE(frontend, Frontend_fixture)

// ## Front End Invocation Tests

// We start with some simple tests for basic execution behavior.
// Below we test execution of scripts containing errors.

DEFINE_TEST_CASE(syntax_error)
WITH_SOURCE("lua",
            "function f()"
            "    retur"
            "end")
WITH_SOURCE("scheme",
            "(define foo 1")
RUN()
EXPECTING_SUCCESS(false)

DEFINE_TEST_CASE(runtime_error)
WITH_SOURCE("lua",
            "i = 1"
            "nosuchfunc()")
WITH_SOURCE("scheme",
            "(+ 1 2)"
            "('notaproc 3 4)")
RUN()
EXPECTING_SUCCESS(false)

DEFINE_TEST_CASE(arguments)

// The following is equivalent to:

// ```
// gamma source -- hello world
// ```

// This passes two parameters to the input script, which are handled
// according to the conventions of the respective language's
// standalone interpreter.

WITH_SOURCE("lua",
            "l = {\"hello\", \"world\"};"
            "m = {...};"
            "for i = 1, #arg do;"
            "    assert(l[i] == m[i]);"
            "    assert(l[i] == arg[i]);"
            "end;",
            "--", "hello", "world")
WITH_SOURCE("scheme",
            "(assert (string-prefix? \"/tmp/file\" (car (command-line))))"
            "(assert (equal? (cdr (command-line)) (list \"hello\" \"world\")))",
            "--", "hello", "world")
RUN()
EXPECTING_SUCCESS(true)

DEFINE_TEST_CASE(multiple_files)

// Normally command line arguments are parsed once, so there is no
// need to clear any previously set values.  Here we run tests for
// multiple languages, so we need to clear definitions explicitly
// below.

// These tests run the equivalent of:

// ```
// gamma -Dfirst=1 -L/tmp/test/first first_source
//       -Dsecond=1 -L/tmp/test/second second_source
// ```

// Each source should run as soon as it is encountered in the command
// line arguments, with only preceding definitions etc. set.  Both
// should run in the same environment so global variables set in the
// first source should be visible in the second.  Definitions on the
// command line are freshly set before each source runs.

WITH_SOURCE("lua",
            "assert(first == 1, \"wrong first\");"
            "function assert_path(p);"
            "    local s = require(\"package\").path;"
            "    local i = 0;"
            "    for x in require(\"string\").gmatch(s, \"[^;]+\") do;"
            "        if x:sub(1, 4) == \"/tmp\" then;"
            "            i = i + 1;"
            "            assert(p[i] == x, p[i] .. \" != \" .. tostring(x));"
            "        end;"
            "    end;"
            "    assert(i == #p, \"path missing elements\");"
            "end;"
            "assert_path {\"/tmp/?.lua\", \"/tmp/test/first/?.lua\"};"
            "first = 11;"
            "other_first = 1;",
            "-Dfirst=1", "-L/tmp/test/first")
WITH_SOURCE("lua",
            "assert(first == 1, \"wrong first\");"
            "assert(other_first == 1, \"wrong other_first\");"
            "assert(second == 1, \"wrong second\");"
            "assert_path {\"/tmp/?.lua\","
            "             \"/tmp/test/second/?.lua\","
            "             \"/tmp/test/first/?.lua\"};",
            "-Dsecond=1", "-L/tmp/test/second")
WITH_SOURCE("scheme",
            "(assert (member \".sld\" %load-extensions)"
            "        (equal?"
            "          (filter"
            "            (lambda (x) (equal? (string-take x 4) \"/tmp\"))"
            "            %load-path)"
            "          (list \"/tmp\" \"/tmp/test/first\"))"
            "        (= first 1))"
            "(define first 11)"
            "(define other-first 1)",
            "-Dfirst=1", "-L/tmp/test/first")
WITH_SOURCE("scheme",
            "(assert (equal?"
            "          (filter"
            "            (lambda (x) (equal? (string-take x 4) \"/tmp\"))"
            "            %load-path)"
            "          (list \"/tmp\""
            "                \"/tmp/test/second\""
            "                \"/tmp/test/first\"))"
            "        (= first 1)"
            "        (= other-first 1)"
            "        (= second 1))",
            "-Dsecond=1", "-L/tmp/test/second")
RUN({
    Options::definitions.clear();
    Options::library_directories.pop_front();
    Options::library_directories.pop_front();
})
EXPECTING_SUCCESS(true)

// Here we test setting of boolean options and parameters with numeric
// values.

DEFINE_TEST_CASE(boolean_definition)
WITH_SOURCE("lua",
            "option = (option or false)"
            "assert(option == true)",
            "-Doption")
WITH_SOURCE("scheme",
            "(define-option option? #f)"
            "(assert option?)",
            "-Doption")
RUN({
    Options::definitions.clear();
})
EXPECTING_SUCCESS(true)

DEFINE_TEST_CASE(definition)
WITH_SOURCE("lua",
            "option = (option or 1)"
            "assert(option == 123)",
            "-Doption=(30 * 4 + 3)")
WITH_SOURCE("scheme",
            "(define-option option 1)"
            "(assert (= option 123))",
            "-Doption=(+ (* 20 6) 3)")
RUN({
    Options::definitions.clear();
})
EXPECTING_SUCCESS(true)

DEFINE_TEST_CASE(default_definition)
WITH_SOURCE("lua",
            "option = (option or 1)"
            "assert(option == 1)")
WITH_SOURCE("scheme",
            "(define-option option 1)"
            "(assert (= option 1))")
RUN()
EXPECTING_SUCCESS(true)

// These test the setting/getting functions for the various
// tolerances.

DEFINE_TEST_CASE(tolerances)
WITH_SOURCE("lua",
            "assert(set_curve_tolerance(0.5) > 0, \"set curve failed\");"
            "assert(set_projection_tolerance(0.25) > 0, \"set projection failed\");"
            "assert(set_sine_tolerance(0.125) > 0, \"set sine failed\");"
            "assert(set_curve_tolerance() == 0.5, \"get curve failed\");"
            "assert(set_projection_tolerance() == 0.25, \"get projection failed\");"
            "assert(set_sine_tolerance() == 0.125, \"get sine failed\");")
WITH_SOURCE("scheme",
            "(assert (positive? (set-curve-tolerance! 10)))"
            "(assert (positive? (set-projection-tolerance! 1/10)))"
            "(assert (positive? (set-sine-tolerance! 0.0625)))"
            "(assert (= (set-curve-tolerance!) 10))"
            "(assert (= (set-projection-tolerance!) 1/10))"
            "(assert (= (set-sine-tolerance!) 0.0625))")
RUN()
EXPECTING_SUCCESS(true)

#define RECTANGLE_TAG "polygon(point(-1,-1),point(1,-1),point(1,1),point(-1,1))"

// ## Front End Tests for Primitives

// These are simple tests for polygon or polyhedron primitive
// functions.

DEFINE_TEST_CASE(polygon)
WITH_SOURCE("lua",
            "g = require 'gamma.polygons'"

            "g.simple(point(0, 0), point(1, 0), point(0, 1))"
            "g.rectangle(2, 2)"
            "g.regular(3, 5)"
            "g.isosceles_triangle(4, 3)"
            "g.right_triangle(-5, 4)")
WITH_SOURCE("scheme",
            "(import (gamma polygons))"

            "(simple-polygon (point 0 0) (point 1 0) (point 0 1))"
            "(rectangle 2 2)"
            "(regular-polygon 3 5)"
            "(isosceles-triangle 4 3)"
            "(right-triangle -5 4)")
EXPECTING("polygon(point(0,0),point(1,0),point(0,1))",
          RECTANGLE_TAG,
          "regular_polygon(3,5,1/1048576)",
          "polygon(point(-2,0),point(2,0),point(0,3))",
          "polygon(point(0,0),point(0,4),point(-5,0))")

DEFINE_TEST_CASE(circle_polygon)
WITH_SOURCE("lua",
            "g = require 'gamma.polygons'"

            "g.circle(5)"
            "g.circular_sector(5, 65)"
            "g.circular_segment(15, 5)")
WITH_SOURCE("scheme",
            "(import (gamma polygons))"

            "(circle 5)"
            "(circular-sector 5 65)"
            "(circular-segment 15 5)")
EXPECTING("circle(5)",
          "sector(5,65)",
          "segment(15,5)")

DEFINE_TEST_CASE(conic_polygon)
WITH_SOURCE("lua",
            "g = require 'gamma.polygons'"

            "g.ellipse(5, 10)"
            "g.elliptic_sector(5, 10, 65)")
WITH_SOURCE("scheme",
            "(import (gamma polygons))"

            "(ellipse 5 10)"
            "(elliptic-sector 5 10 65)")
EXPECTING("circle(1)",
          "sector(1,65)",
          "conics(circle(1))",
          "conics(sector(1,65))",
          "transform(conics(circle(1)),scaling(5,10))",
          "transform(conics(sector(1,65)),scaling(5,10))")

DEFINE_TEST_CASE(polyhedron)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "h = require 'gamma.polyhedra'"

            "h.tetrahedron(1, 1, 1)"
            "h.square_pyramid(2, 2, 2)"
            "h.octahedron(3, 3, 3)"
            "h.octahedron(3, 3, 3, 4)"
            "h.regular_pyramid(5, 5, 5)"
            "h.regular_bipyramid(6, 6, 6)"
            "h.regular_bipyramid(6, 6, 6, 7)"
            "h.cuboid(2, 4, 6)"
            "h.icosahedron(1)"
            "h.sphere(\"1/3\")"
            "h.cylinder(3, 10)")
WITH_SOURCE("scheme",
            "(import (gamma polyhedra))"

            "(tetrahedron 1 1 1)"
            "(square-pyramid 2 2 2)"
            "(octahedron 3 3 3)"
            "(octahedron 3 3 3 4)"
            "(regular-pyramid 5 5 5)"
            "(regular-bipyramid 6 6 6)"
            "(regular-bipyramid 6 6 6 7)"
            "(cuboid 2 4 6)"
            "(icosahedron 1)"
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
          "sphere(1/3,1/1024,1/1048576)",
          "regular_polygon(124,3,1/1048576)",
          "extrusion(regular_polygon(124,3,1/1048576),"
          "translation(0,0,-5),translation(0,0,5))")

// ## Front End Tests for Transformations

// Here we test application and manipulation of transformations,
// including flushing.

DEFINE_TEST_CASE(transform_point)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "g = require 'gamma.polygons'"
            "op = require 'gamma.operations'"

            "g.simple(point(0, 0),"
            "    t.translation(1, 0) * point(0, 0),"
            "    t.rotation(90) * point(1, 0))"
            "op.hull(point(0, 0, 0),"
            "    t.translation(1, 0, 0) * point(0, 0, 0),"
            "    t.apply(t.rotation(90, 2), point(1, 0, 0)),"
            "    t.apply(t.rotation(90, 1), point(-1, 0, 0)))")
WITH_SOURCE("scheme",
            "(import (gamma transformation) (gamma polygons)"
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
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "g = require 'gamma.polygons'"

            "a = t.translation(2, 3) * g.rectangle(2, 2)"
            "b = t.scaling(4, 5) * g.circle(1)"
            "c = t.scaling(1, -1) * g.circle(1)"
            "t.apply(t.rotation(90), g.ellipse(1, 2))")
WITH_SOURCE("scheme",
            "(import (gamma transformation) (gamma polygons))"

            "(transformation-apply (translation 2 3) (rectangle 2 2))"
            "(transformation-apply (scaling 4 5) (circle 1))"
            "(transformation-apply (scaling 1 -1) (circle 1))"
            "(transformation-apply (rotation 90) (ellipse 1 2))")
EXPECTING(RECTANGLE_TAG,
          "transform(" RECTANGLE_TAG ",translation(2,3))",
          "circle(1)",
          "transform(circle(1),scaling(1,-1))",
          "conics(circle(1))",
          "transform(conics(circle(1)),scaling(4,5))",
          "transform(conics(circle(1)),scaling(1,2))",
          "transform(transform(conics(circle(1)),scaling(1,2)),"
          "rotation(0,-1,1,0))")

DEFINE_TEST_CASE(transform_polyhedron)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "h = require 'gamma.polyhedra'"

            "a = t.translation(1, 2, 3) * h.tetrahedron(1, 1, 1)"
            "b = t.scaling(2, 2, 2) * h.tetrahedron(1, 1, 1)"
            "t.apply(t.rotation(90, 2), h.tetrahedron(1, 1, 1))")
WITH_SOURCE("scheme",
            "(import (gamma transformation) (gamma polyhedra))"

            "(transformation-apply (translation 1 2 3)"
            "                      (tetrahedron 1 1 1))"
            "(transformation-apply (scaling 2 2 2)"
            "                      (tetrahedron 1 1 1))"
            "(transformation-apply (rotation 90 2)"
            "                      (tetrahedron 1 1 1))")
EXPECTING("tetrahedron(1,1,1)",
          "transform(tetrahedron(1,1,1),translation(1,2,3))",
          "transform(tetrahedron(1,1,1),scaling(2,2,2))",
          "transform(tetrahedron(1,1,1),rotation(0,-1,0,1,0,0,0,0,1))")

DEFINE_TEST_CASE(transform_concatenation)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "g = require 'gamma.polygons'"
            "h = require 'gamma.polyhedra'"

            "a = (t.translation(1, 2, 3)"
            "     * t.scaling(2, 2, 2)"
            "     * t.rotation(90, 2)) * h.tetrahedron(1, 1, 1)"
            "b = t.apply(t.translation(2, 3), t.rotation(90)) * g.circle(1)")
WITH_SOURCE("scheme",
            "(import (gamma transformation) (gamma polygons) (gamma polyhedra))"

            "(transformation-apply"
            "  (transformation-append"
            "    (translation 1 2 3)"
            "    (scaling 2 2 2)"
            "    (rotation 90 2)) (tetrahedron 1 1 1))"
            "(transformation-apply"
            "  (transformation-append"
            "    (translation 2 3)"
            "    (rotation 90)) (circle 1))")
EXPECTING("tetrahedron(1,1,1)",
          "transform(tetrahedron(1,1,1),"
          "transformation(0,-2,0,1,2,0,0,2,0,0,2,3))",
          "circle(1)",
          "transform(circle(1),transformation(0,-1,2,1,0,3))")

DEFINE_TEST_CASE(flush_polygon)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "g = require 'gamma.polygons'"

            "t.flush_west(g.regular(5, 1))"
            "t.flush_east(g.regular(5, 1))"
            "t.flush_south(g.regular(5, 1))"
            "t.flush_north(g.regular(5, 1))"
            "t.flush(g.regular(5, 1), 1, 1)")
WITH_SOURCE("scheme",
            "(import (gamma transformation) (gamma polygons))"

            "(flush-west (regular-polygon 5 1))"
            "(flush-east (regular-polygon 5 1))"
            "(flush-south (regular-polygon 5 1))"
            "(flush-north (regular-polygon 5 1))"
            "(flush (regular-polygon 5 1) 1 1)")
EXPECTING("regular_polygon(5,1,1/1048576)",
          "flush(regular_polygon(5,1,1/1048576),-1,0,0,0)",
          "flush(regular_polygon(5,1,1/1048576),0,1,0,0)",
          "flush(regular_polygon(5,1,1/1048576),0,0,-1,0)",
          "flush(regular_polygon(5,1,1/1048576),0,0,0,1)",
          "flush(regular_polygon(5,1,1/1048576),0,1,0,1)")

DEFINE_TEST_CASE(flush_polyhedron)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "h = require 'gamma.polyhedra'"

            "t.flush_west(h.tetrahedron(1, 1, 1))"
            "t.flush_east(h.tetrahedron(1, 1, 1))"
            "t.flush_south(h.tetrahedron(1, 1, 1))"
            "t.flush_north(h.tetrahedron(1, 1, 1))"
            "t.flush_bottom(h.tetrahedron(1, 1, 1))"
            "t.flush_top(h.tetrahedron(1, 1, 1))"
            "t.flush(h.tetrahedron(1, 1, 1), 1, 0, -1)")
WITH_SOURCE("scheme",
            "(import (gamma transformation) (gamma polyhedra))"

            "(flush-west (tetrahedron 1 1 1))"
            "(flush-east (tetrahedron 1 1 1))"
            "(flush-south (tetrahedron 1 1 1))"
            "(flush-north (tetrahedron 1 1 1))"
            "(flush-bottom (tetrahedron 1 1 1))"
            "(flush-top (tetrahedron 1 1 1))"
            "(flush (tetrahedron 1 1 1) 1 0 -1)")
EXPECTING("tetrahedron(1,1,1)",
          "flush(tetrahedron(1,1,1),-1,0,0,0,0,0)",
          "flush(tetrahedron(1,1,1),0,1,0,0,0,0)",
          "flush(tetrahedron(1,1,1),0,0,-1,0,0,0)",
          "flush(tetrahedron(1,1,1),0,0,0,1,0,0)",
          "flush(tetrahedron(1,1,1),0,0,0,0,-1,0)",
          "flush(tetrahedron(1,1,1),0,0,0,0,0,1)",
          "flush(tetrahedron(1,1,1),0,1,0,0,-1,0)")

DEFINE_TEST_CASE(flush_bounding_volume)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.vertices_in("
            "        t.flush_east(v.bounding_box(10, 20, 30))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.vertices_in("
            "        t.flush_west(v.bounding_sphere(2))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.vertices_in("
            "        t.flush(v.bounding_cylinder(4, 2), 1, 1, 1)))")
WITH_SOURCE("scheme",
            "(import (gamma transformation)"
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
EXPECTING("cuboid(2,2,2)",
          "mesh(cuboid(2,2,2))",
          "color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "bounding_box(plane(-1,0,0,0),plane(1,0,0,10),plane(0,-1,0,10),"
          "plane(0,1,0,10),plane(0,0,-1,15),plane(0,0,1,15))),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "bounding_sphere(point(2,0,0),2)),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "bounding_cylinder(point(-4,-4,-2),vector(0,0,1),4,2)),55,55,55,255)")

// ## Front End Tests for Polygon Operations

// Here we test operations that can be applied exlusively to polygons.

DEFINE_TEST_CASE(extrusion)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "g = require 'gamma.polygons'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.extrusion(g.rectangle(2, 2))"
            "op.extrusion(g.rectangle(2, 2),"
            "    t.translation(0, 0, 0),"
            "    t.translation(0, 0, 2))"
            "op.extrusion(g.circle(100),"
            "    t.translation(0, 0, 0),"
            "    t.translation(0, 0, 2))")
WITH_SOURCE("scheme",
            "(import (gamma transformation) (gamma polygons)"
            "        (gamma polyhedra) (gamma operations))"

            "(extrusion"
            " (rectangle 2 2))"
            "(extrusion"
            " (rectangle 2 2)"
            " (translation 0 0 0)"
            " (translation 0 0 2))"
            "(extrusion"
            " (circle 100)"
            " (translation 0 0 0)"
            " (translation 0 0 2))")
EXPECTING(RECTANGLE_TAG,
          "extrusion(" RECTANGLE_TAG ","
          "translation(0,0,0))",
          "extrusion(" RECTANGLE_TAG ","
          "translation(0,0,0),translation(0,0,2))",
          "circle(100)",
          "segments(circle(100),1/1024,1/1048576)",
          "extrusion(segments(circle(100),1/1024,1/1048576),"
          "translation(0,0,0),translation(0,0,2))")

DEFINE_TEST_CASE(offset)
WITH_SOURCE("lua",
            "g = require 'gamma.polygons'"
            "op = require 'gamma.operations'"

            "op.offset(g.rectangle(2, 2), 3)")
WITH_SOURCE("scheme",
            "(import (gamma polygons) (gamma operations))"

            "(offset (rectangle 2 2) 3)")
EXPECTING(RECTANGLE_TAG,
          "offset(" RECTANGLE_TAG ",3)")

// ## Front End Tests for Boolean Operations

// Boolean operations accept both polygons (of all kinds) and
// polyhedra.  They're binary in nature but the functions accept any
// number of operands.

DEFINE_TEST_CASE(polygon_boolean)
WITH_SOURCE("lua",
            "g = require 'gamma.polygons'"

            "a = (g.rectangle(2, 2)"
            "     + g.simple(point(1, -1), point(2, -1), point(1, 1)))"
            "b = g.rectangle(2, 2) - g.circle(1)"
            "c = g.ellipse(4, 2) * g.circle(1)")
WITH_SOURCE("scheme",
            "(import (gamma polygons) (gamma operations))"

            "(union (rectangle 2 2)"
            "       (simple-polygon (point 1 -1) (point 2 -1) (point 1 1)))"
            "(difference (rectangle 2 2) (circle 1))"
            "(intersection (ellipse 4 2) (circle 1))")
EXPECTING(RECTANGLE_TAG,
          "polygon(point(1,-1),point(2,-1),point(1,1))",
          "join(" RECTANGLE_TAG ","
          "polygon(point(1,-1),point(2,-1),point(1,1)))",
          "circles(" RECTANGLE_TAG ")",
          "circle(1)",
          "difference(circles(" RECTANGLE_TAG "),circle(1))",
          "conics(circle(1))",
          "transform(conics(circle(1)),scaling(4,2))",
          "intersection(transform(conics(circle(1)),scaling(4,2)),"
          "conics(circle(1)))")

DEFINE_TEST_CASE(polygon_boolean_many)
WITH_SOURCE("lua",
            "g = require 'gamma.polygons'"
            "op = require 'gamma.operations'"

            "op.union("
            "     g.rectangle(2, 2),"
            "     g.simple(point(1, -1), point(2, -1), point(1, 1)),"
            "     g.circle(1))"
            "op.difference(g.circle(5), g.circle(1.25), g.rectangle(2, 2))"
            "op.intersection(g.circle(3), g.circle(2), g.circle(1))")
WITH_SOURCE("scheme",
            "(import (gamma polygons) (gamma operations))"

            "(union (rectangle 2 2)"
            "       (simple-polygon (point 1 -1) (point 2 -1) (point 1 1))"
            "       (circle 1))"
            "(difference (circle 5) (circle 5/4) (rectangle 2 2))"
            "(intersection (circle 3) (circle 2) (circle 1))")
EXPECTING(RECTANGLE_TAG,
          "polygon(point(1,-1),point(2,-1),point(1,1))",
          "join(" RECTANGLE_TAG ",polygon(point(1,-1),point(2,-1),point(1,1)))",
          "circles(join(" RECTANGLE_TAG ","
          "polygon(point(1,-1),point(2,-1),point(1,1))))",
          "join(circles(join(" RECTANGLE_TAG ","
          "polygon(point(1,-1),point(2,-1),point(1,1)))),circle(1))",
          "circle(5)",
          "circle(5/4)",
          "difference(circle(5),circle(5/4))",
          "circles(" RECTANGLE_TAG ")",
          "difference(difference(circle(5),circle(5/4)),"
          "circles(" RECTANGLE_TAG "))",
          "circle(3)",
          "circle(2)",
          "intersection(circle(3),circle(2))",
          "circle(1)",
          "intersection(intersection(circle(3),circle(2)),circle(1))")

DEFINE_TEST_CASE(polyhedron_boolean)
WITH_SOURCE("lua",
            "h = require 'gamma.polyhedra'"

            "a = (h.tetrahedron(1, 1, 1) + h.tetrahedron(-1, 1, 1))"
            "b = (h.tetrahedron(1, 1, 1) - h.tetrahedron(1 / 2, 1 / 2, 1 / 2))"
            "c = (h.tetrahedron(1, 1, 1) * h.tetrahedron(1 / 2, 1 / 2, 1 / 2))"
            "d = (h.tetrahedron(1, 1, 1) * plane(0, 0, -1, 0))")
WITH_SOURCE("scheme",
            "(import (gamma polyhedra) (gamma operations))"

            "(union (tetrahedron 1 1 1) (tetrahedron -1 1 1))"
            "(difference (tetrahedron 1 1 1) (tetrahedron 1/2 1/2 1/2))"
            "(intersection (tetrahedron 1 1 1) (tetrahedron 1/2 1/2 1/2))"
            "(clip (tetrahedron 1 1 1) (plane 0 0 -1 0))")
EXPECTING("tetrahedron(1,1,1)",
          "tetrahedron(-1,1,1)",
          "join(tetrahedron(1,1,1),tetrahedron(-1,1,1))",
          "tetrahedron(1/2,1/2,1/2)",
          "difference(tetrahedron(1,1,1),tetrahedron(1/2,1/2,1/2))",
          "intersection(tetrahedron(1,1,1),tetrahedron(1/2,1/2,1/2))",
          "clip(tetrahedron(1,1,1),plane(0,0,-1,0))")

DEFINE_TEST_CASE(polyhedron_boolean_many)
WITH_SOURCE("lua",
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.union(h.cuboid(2, 2, 2), h.tetrahedron(1, 1, 1), h.sphere(1))"
            "op.difference(h.sphere(5), h.sphere(1.25), h.cuboid(2, 2, 2))"
            "op.intersection(h.sphere(3), h.sphere(2), h.sphere(1))")
WITH_SOURCE("scheme",
            "(import (gamma polyhedra) (gamma operations))"

            "(union (cuboid 2 2 2) (tetrahedron 1 1 1) (sphere 1))"
            "(difference (sphere 5) (sphere 5/4) (cuboid 2 2 2))"
            "(intersection (sphere 3) (sphere 2) (sphere 1))")
EXPECTING("cuboid(2,2,2)",
          "tetrahedron(1,1,1)",
          "join(cuboid(2,2,2),tetrahedron(1,1,1))",
          "sphere(1,1/1024,1/1048576)",
          "join(join(cuboid(2,2,2),tetrahedron(1,1,1)),"
          "sphere(1,1/1024,1/1048576))",
          "sphere(5,1/1024,1/1048576)",
          "sphere(5/4,1/1024,1/1048576)",
          "difference(sphere(5,1/1024,1/1048576),sphere(5/4,1/1024,1/1048576))",
          "difference("
          "difference(sphere(5,1/1024,1/1048576),sphere(5/4,1/1024,1/1048576)),"
          "cuboid(2,2,2))",
          "sphere(3,1/1024,1/1048576)",
          "sphere(2,1/1024,1/1048576)",
          "intersection(sphere(3,1/1024,1/1048576),sphere(2,1/1024,1/1048576))",
          "sphere(1,1/1024,1/1048576)",
          "intersection("
          "intersection(sphere(3,1/1024,1/1048576),sphere(2,1/1024,1/1048576)),"
          "sphere(1,1/1024,1/1048576))")

// Below we test the behavior of the various boolean modes for
// polyhedra.  We use the fixture below to set one of the modes and
// test whether the operations are carried out with Nef polyhedra or
// corefinement, as required.

struct Booleans_mode_fixture: Main_fixture {
    Booleans_mode_fixture(Polyhedron_booleans_mode mode) {
        push(Options::polyhedron_booleans, mode);
    }

    ~Booleans_mode_fixture() {
        pop(Options::polyhedron_booleans);
    }
};

#define WITH_POLYHEDRON_BOOLEANS_SOURCES                        \
WITH_SOURCE("lua",                                              \
            "h = require 'gamma.polyhedra'"                     \
            "op = require 'gamma.operations'"                   \
                                                                \
            "a = (op.minkowski_sum("                            \
            "         h.tetrahedron(1, 1, 1),"                  \
            "         h.octahedron(0.25, 0.25, 0.125))"         \
            "     - h.sphere(0.5))"                             \
            "b = (op.minkowski_sum("                            \
            "         h.tetrahedron(1, 1, 1),"                  \
            "         h.octahedron(0.25, 0.25, 0.125))"         \
            "     - op.minkowski_sum("                          \
            "         h.tetrahedron(-1, 1, 1),"                 \
            "         h.octahedron(0.25, 0.25, 0.125)))"        \
            "op.clip("                                          \
            "    op.minkowski_sum("                             \
            "        h.tetrahedron(1, 1, 1),"                   \
            "        h.octahedron(0.25, 0.25, 0.125)),"         \
            "    plane(0, 0, 1, -0.5))"                         \
            "op.clip("                                          \
            "    h.tetrahedron(1, 1, 1),"                       \
            "    plane(0, 0, 1, -0.5))")                        \
WITH_SOURCE("scheme",                                           \
            "(import (gamma polyhedra) (gamma operations))"     \
                                                                \
            "(difference (minkowski-sum"                        \
            "             (tetrahedron 1 1 1)"                  \
            "             (octahedron 1/4 1/4 1/8))"            \
            "            (sphere 1/2))"                         \
            "(difference (minkowski-sum"                        \
            "             (tetrahedron 1 1 1)"                  \
            "             (octahedron 1/4 1/4 1/8))"            \
            "            (minkowski-sum"                        \
            "             (tetrahedron -1 1 1)"                 \
            "             (octahedron 1/4 1/4 1/8)))"           \
            "(clip (minkowski-sum"                              \
            "       (tetrahedron 1 1 1)"                        \
            "       (octahedron 1/4 1/4 1/8))"                  \
            "      (plane 0 0 1 -1/2))"                         \
            "(clip (tetrahedron 1 1 1)"                         \
            "      (plane 0 0 1 -1/2))")

DEFINE_TEST_CASE(
    polyhedron_booleans_mode_nef,
    * boost::unit_test::fixture<Booleans_mode_fixture>(
        Polyhedron_booleans_mode::NEF))
WITH_POLYHEDRON_BOOLEANS_SOURCES
EXPECTING("tetrahedron(1,1,1)",
          "nef(tetrahedron(1,1,1))",
          "octahedron(1/4,1/4,1/8,1/8)",
          "nef(octahedron(1/4,1/4,1/8,1/8))",
          "minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))",
          "sphere(1/2,1/1024,1/1048576)",
          "nef(sphere(1/2,1/1024,1/1048576))",
          "difference(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),"
          "nef(sphere(1/2,1/1024,1/1048576)))",
          "tetrahedron(-1,1,1)",
          "nef(tetrahedron(-1,1,1))",
          "minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))",
          "difference(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),"
          "minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))))",
          "clip(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),plane(0,0,1,-1/2))",
          "clip(nef(tetrahedron(1,1,1)),plane(0,0,1,-1/2))")

DEFINE_TEST_CASE(
    polyhedron_booleans_mode_corefine,
    * boost::unit_test::fixture<Booleans_mode_fixture>(
        Polyhedron_booleans_mode::COREFINE))
WITH_POLYHEDRON_BOOLEANS_SOURCES
EXPECTING("tetrahedron(1,1,1)",
          "nef(tetrahedron(1,1,1))",
          "octahedron(1/4,1/4,1/8,1/8)",
          "nef(octahedron(1/4,1/4,1/8,1/8))",
          "minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))",
          "polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))))",
          "sphere(1/2,1/1024,1/1048576)",
          "difference(polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))),sphere(1/2,1/1024,1/1048576))",
          "tetrahedron(-1,1,1)",
          "nef(tetrahedron(-1,1,1))",
          "minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))",
          "polyhedron(minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))))",
          "difference(polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))),"
          "polyhedron(minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))))",
          "clip(polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))),plane(0,0,1,-1/2))",
          "clip(tetrahedron(1,1,1),plane(0,0,1,-1/2))")

DEFINE_TEST_CASE(
    polyhedron_booleans_mode_auto,
    * boost::unit_test::fixture<Booleans_mode_fixture>(
        Polyhedron_booleans_mode::AUTO))
WITH_POLYHEDRON_BOOLEANS_SOURCES
EXPECTING("tetrahedron(1,1,1)",
          "nef(tetrahedron(1,1,1))",
          "octahedron(1/4,1/4,1/8,1/8)",
          "nef(octahedron(1/4,1/4,1/8,1/8))",
          "minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))",
          "polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))))",
          "sphere(1/2,1/1024,1/1048576)",
          "difference(polyhedron(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))),sphere(1/2,1/1024,1/1048576))",
          "tetrahedron(-1,1,1)",
          "nef(tetrahedron(-1,1,1))",
          "minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8)))",
          "difference(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),"
          "minkowski_sum(nef(tetrahedron(-1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))))",
          "clip(minkowski_sum(nef(tetrahedron(1,1,1)),"
          "nef(octahedron(1/4,1/4,1/8,1/8))),plane(0,0,1,-1/2))",
          "clip(tetrahedron(1,1,1),plane(0,0,1,-1/2))")

#undef WITH_POLYHEDRON_BOOLEANS_SOURCES

DEFINE_TEST_CASE(clip_many)
WITH_SOURCE("lua",
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.clip(h.cuboid(10, 10, 10),"
            "        plane(-1, -1, -1, -9),"
            "        plane(1, 1, 1, -9))")
WITH_SOURCE("scheme",
            "(import (gamma polyhedra) (gamma operations))"

            "(clip (cuboid 10 10 10) (plane -1 -1 -1 -9) (plane 1 1 1 -9))")
EXPECTING("cuboid(10,10,10)",
          "clip(cuboid(10,10,10),plane(-1,-1,-1,-9))",
          "clip(clip(cuboid(10,10,10),plane(-1,-1,-1,-9)),plane(1,1,1,-9))")

// ## Front End Tests for Selections

// We test selection application and manipulation below.  These are
// mostly useful for the mesh operations that follow.

DEFINE_TEST_CASE(selection)
WITH_SOURCE("lua",
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.vertices_in(v.bounding_plane(0, 0, 1, -1)))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.faces_in(v.bounding_plane(0, 0, 1, -1)))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.faces_partially_in(v.bounding_plane(0, 0, 1, -1)))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.expand_selection("
            "        s.vertices_in(v.bounding_plane(0, 0, 1, -1)), 1))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.contract_selection("
            "        s.vertices_in(v.bounding_halfspace(0, 0, 1, -1)), 1))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "     s.expand_selection("
            "        s.faces_in(v.bounding_plane(0, 0, 1, -1)), 1))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.contract_selection("
            "        s.faces_in(v.bounding_halfspace(0, 0, 1, -1)), 1))")
WITH_SOURCE("scheme",
            "(import (gamma volumes) (gamma selection)"
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
EXPECTING("cuboid(2,2,2)",
          "mesh(cuboid(2,2,2))",
          "color_selection(mesh(cuboid(2,2,2)),"
          "contract(faces_in(bounding_halfspace(plane(0,0,1,-1))),1),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "expand(faces_in(bounding_plane(plane(0,0,1,-1))),1),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "contract(vertices_in(bounding_halfspace(plane(0,0,1,-1))),1),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "expand(vertices_in(bounding_plane(plane(0,0,1,-1))),1),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "faces_partially_in(bounding_plane(plane(0,0,1,-1))),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "faces_in(bounding_plane(plane(0,0,1,-1))),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),"
          "vertices_in(bounding_plane(plane(0,0,1,-1))),55,55,55,255)")

#define EXPECTING_SELECTION_BOOLEAN_TAGS                                \
EXPECTING("cuboid(2,2,2)",                                              \
          "mesh(cuboid(2,2,2))",                                        \
          "color_selection(mesh(cuboid(2,2,2)),"                        \
          "complement(vertices_in(bounding_plane(plane(0,0,1,-1)))),55,55,55,255)", \
          "color_selection(mesh(cuboid(2,2,2)),"                        \
          "intersection(vertices_in(bounding_plane(plane(0,0,1,-1))),"  \
          "vertices_in(bounding_plane(plane(0,0,1,1)))),55,55,55,255)", \
          "color_selection(mesh(cuboid(2,2,2)),"                        \
          "difference(vertices_in(bounding_plane(plane(0,0,1,-1))),"    \
          "vertices_in(bounding_plane(plane(0,0,1,1)))),55,55,55,255)", \
          "color_selection(mesh(cuboid(2,2,2)),"                        \
          "union(vertices_in(bounding_plane(plane(0,0,1,-1))),"         \
          "vertices_in(bounding_plane(plane(0,0,1,1)))),55,55,55,255)", \
          "color_selection(mesh(cuboid(2,2,2)),"                        \
          "complement(faces_in(bounding_plane(plane(0,0,1,-1)))),55,55,55,255)", \
          "color_selection(mesh(cuboid(2,2,2)),"                        \
          "intersection(faces_in(bounding_plane(plane(0,0,1,-1))),"     \
          "faces_in(bounding_plane(plane(0,0,1,1)))),55,55,55,255)",    \
          "color_selection(mesh(cuboid(2,2,2)),"                        \
          "difference(faces_in(bounding_plane(plane(0,0,1,-1))),"       \
          "faces_in(bounding_plane(plane(0,0,1,1)))),55,55,55,255)",    \
          "color_selection(mesh(cuboid(2,2,2)),"                        \
          "union(faces_in(bounding_plane(plane(0,0,1,-1))),"            \
          "faces_in(bounding_plane(plane(0,0,1,1)))),55,55,55,255)",    \
          "remesh(cuboid(2,2,2),"                                       \
          "complement(edges_in(bounding_plane(plane(0,0,1,-1)))),1,1)", \
          "remesh(cuboid(2,2,2),"                                       \
          "intersection(edges_in(bounding_plane(plane(0,0,1,-1))),"     \
          "edges_in(bounding_plane(plane(0,0,1,1)))),1,1)",             \
          "remesh(cuboid(2,2,2),"                                       \
          "difference(edges_in(bounding_plane(plane(0,0,1,-1))),"       \
          "edges_in(bounding_plane(plane(0,0,1,1)))),1,1)",             \
          "remesh(cuboid(2,2,2),"                                       \
          "union(edges_in(bounding_plane(plane(0,0,1,-1))),"            \
          "edges_in(bounding_plane(plane(0,0,1,1)))),1,1)")

DEFINE_TEST_CASE(selection_boolean)
WITH_SOURCE("lua",
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    op.union(s.vertices_in(v.bounding_plane(0, 0, 1, -1)),"
            "             s.vertices_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    op.difference(s.vertices_in(v.bounding_plane(0, 0, 1, -1)),"
            "                  s.vertices_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    op.intersection(s.vertices_in(v.bounding_plane(0, 0, 1, -1)),"
            "                    s.vertices_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    v.complement(s.vertices_in(v.bounding_plane(0, 0, 1, -1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    op.union(s.faces_in(v.bounding_plane(0, 0, 1, -1)),"
            "             s.faces_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    op.difference(s.faces_in(v.bounding_plane(0, 0, 1, -1)),"
            "                  s.faces_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    op.intersection(s.faces_in(v.bounding_plane(0, 0, 1, -1)),"
            "                    s.faces_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.complement(s.faces_in(v.bounding_plane(0, 0, 1, -1))))"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    op.union(s.edges_in(v.bounding_plane(0, 0, 1, -1)),"
            "             s.edges_in(v.bounding_plane(0, 0, 1, 1))), 1)"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    op.difference(s.edges_in(v.bounding_plane(0, 0, 1, -1)),"
            "                  s.edges_in(v.bounding_plane(0, 0, 1, 1))), 1)"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    op.intersection(s.edges_in(v.bounding_plane(0, 0, 1, -1)),"
            "                    s.edges_in(v.bounding_plane(0, 0, 1, 1))), 1)"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    s.complement(s.edges_in(v.bounding_plane(0, 0, 1, -1))), 1)")
WITH_SOURCE("scheme",
            "(import (gamma volumes) (gamma selection)"
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
EXPECTING_SELECTION_BOOLEAN_TAGS

DEFINE_TEST_CASE(selection_boolean_metamethods)
WITH_SOURCE("lua",
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    (s.vertices_in(v.bounding_plane(0, 0, 1, -1))"
            "     + s.vertices_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    (s.vertices_in(v.bounding_plane(0, 0, 1, -1))"
            "     - s.vertices_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    (s.vertices_in(v.bounding_plane(0, 0, 1, -1))"
            "     * s.vertices_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    ~s.vertices_in(v.bounding_plane(0, 0, 1, -1)))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    (s.faces_in(v.bounding_plane(0, 0, 1, -1))"
            "     + s.faces_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    (s.faces_in(v.bounding_plane(0, 0, 1, -1))"
            "     - s.faces_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    (s.faces_in(v.bounding_plane(0, 0, 1, -1))"
            "     * s.faces_in(v.bounding_plane(0, 0, 1, 1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    ~s.faces_in(v.bounding_plane(0, 0, 1, -1)))"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    (s.edges_in(v.bounding_plane(0, 0, 1, -1))"
            "     + s.edges_in(v.bounding_plane(0, 0, 1, 1))), 1)"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    (s.edges_in(v.bounding_plane(0, 0, 1, -1))"
            "     - s.edges_in(v.bounding_plane(0, 0, 1, 1))), 1)"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    (s.edges_in(v.bounding_plane(0, 0, 1, -1))"
            "     * s.edges_in(v.bounding_plane(0, 0, 1, 1))), 1)"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    ~s.edges_in(v.bounding_plane(0, 0, 1, -1)), 1)")
EXPECTING_SELECTION_BOOLEAN_TAGS

#undef EXPECTING_SELECTION_BOOLEAN_TAGS

DEFINE_TEST_CASE(selection_conversion)
WITH_SOURCE("lua",
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.vertices_in(s.faces_in(v.bounding_plane(0, 0, 1, -1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.vertices_in(s.edges_in(v.bounding_plane(0, 0, 1, -1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.faces_in(s.vertices_in(v.bounding_plane(0, 0, 1, -1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.faces_partially_in("
            "        s.vertices_in(v.bounding_plane(0, 0, 1, -1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.faces_in(s.edges_in(v.bounding_plane(0, 0, 1, -1))))"
            "op.color_selection("
            "    h.cuboid(2, 2, 2),"
            "    s.faces_partially_in("
            "        s.edges_in(v.bounding_plane(0, 0, 1, -1))))"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    s.edges_in("
            "        s.vertices_in(v.bounding_plane(0, 0, 1, -1))), 1)"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    s.edges_partially_in("
            "        s.vertices_in(v.bounding_plane(0, 0, 1, -1))), 1)"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    s.edges_in("
            "        s.faces_in(v.bounding_plane(0, 0, 1, -1))), 1)"
            "op.remesh("
            "    h.cuboid(2, 2, 2),"
            "    s.edges_partially_in("
            "        s.faces_in(v.bounding_plane(0, 0, 1, -1))), 1)")
WITH_SOURCE("scheme",
            "(import (gamma volumes) (gamma selection)"
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
EXPECTING("cuboid(2,2,2)",
          "mesh(cuboid(2,2,2))",
          "color_selection(mesh(cuboid(2,2,2)),faces_partially_in("
          "vertices_in(bounding_plane(plane(0,0,1,-1)))),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),faces_in("
          "vertices_in(bounding_plane(plane(0,0,1,-1)))),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),faces_partially_in("
          "edges_in(bounding_plane(plane(0,0,1,-1)))),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),faces_in("
          "edges_in(bounding_plane(plane(0,0,1,-1)))),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "faces_in(bounding_plane(plane(0,0,1,-1)))),55,55,55,255)",
          "color_selection(mesh(cuboid(2,2,2)),vertices_in("
          "edges_in(bounding_plane(plane(0,0,1,-1)))),55,55,55,255)",
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

// ## Front End Tests for Mesh Operations

// These operations process geometry at the mesh level.  They're
// derived from the Polygon Mesh Processing CGAL package.

DEFINE_TEST_CASE(perturb)
WITH_SOURCE("lua",
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.perturb(h.tetrahedron(1, 1, 1), 0.125)"
            "op.perturb("
            "    h.cuboid(2, 2, 2),"
            "    s.vertices_in(v.bounding_halfspace(0, 0, 1, 0)),"
            "    9.5367431640625e-07)")
WITH_SOURCE("scheme",
            "(import (gamma volumes) (gamma selection)"
            "        (gamma polyhedra) (gamma operations))"

            "(perturb (tetrahedron 1 1 1) 1/8)"
            "(perturb (cuboid 2 2 2)"
            "         (vertices-in (bounding-halfspace 0 0 1 0))"
            "                       1/1048576)")
EXPECTING("tetrahedron(1,1,1)",
          "perturb(tetrahedron(1,1,1),1/8)",
          "cuboid(2,2,2)",
          "perturb(cuboid(2,2,2),"
          "vertices_in(bounding_halfspace(plane(0,0,1,0))),1/1048576)")

DEFINE_TEST_CASE(refine)
WITH_SOURCE("lua",
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.refine(h.sphere(2), 5)"
            "op.refine("
            "    h.sphere(2),"
            "    s.faces_in(v.bounding_plane(0, 0, 1, 1)"
            "    + v.bounding_plane(0, 0, -1, 1)), 5)")
WITH_SOURCE("scheme",
            "(import (gamma volumes) (gamma selection)"
            "        (gamma polyhedra) (gamma operations))"

            "(refine (sphere 2) 5)"
            "(refine (sphere 2) (faces-in"
            "                    (union"
            "                     (bounding-plane 0 0 1 1)"
            "                     (bounding-plane 0 0 -1 1))) 5)")
EXPECTING("sphere(2,1/1024,1/1048576)",
          "refine(sphere(2,1/1024,1/1048576),5)",
          "refine(sphere(2,1/1024,1/1048576),"
          "faces_in(join(bounding_plane(plane(0,0,1,1)),"
          "bounding_plane(plane(0,0,-1,1)))),5)")

DEFINE_TEST_CASE(corefine)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.corefine(h.tetrahedron(1, 1, 1), plane(0, 0, 1, -0.5))"
            "op.corefine("
            "    h.cuboid(2, 2, 2),"
            "    t.translation(1, 1, 1) * h.cuboid(2, 2, 2))")
WITH_SOURCE("scheme",
            "(import (gamma transformation)"
            "        (gamma polyhedra) (gamma operations))"

            "(corefine (tetrahedron 1 1 1) (plane 0 0 1 -1/2))"
            "(corefine (cuboid 2 2 2)"
            "          (transformation-apply (translation 1 1 1)"
            "          (cuboid 2 2 2)))")
EXPECTING("tetrahedron(1,1,1)",
          "corefine(tetrahedron(1,1,1),plane(0,0,1,-1/2))",
          "cuboid(2,2,2)",
          "transform(cuboid(2,2,2),translation(1,1,1))",
          "corefine(cuboid(2,2,2),transform(cuboid(2,2,2),translation(1,1,1)))")

DEFINE_TEST_CASE(corefine_many)
WITH_SOURCE("lua",
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.corefine("
            "    h.tetrahedron(1, 1, 1), h.sphere(0.5), plane(0, 0, 1, -0.5))")
WITH_SOURCE("scheme",
            "(import (gamma polyhedra) (gamma operations))"

            "(corefine (tetrahedron 1 1 1) (sphere 1/2) (plane 0 0 1 -1/2))")
EXPECTING("tetrahedron(1,1,1)",
          "sphere(1/2,1/1024,1/1048576)",
          "corefine(tetrahedron(1,1,1),sphere(1/2,1/1024,1/1048576))",
          "corefine(corefine(tetrahedron(1,1,1),sphere(1/2,1/1024,1/1048576)),"
          "plane(0,0,1,-1/2))")

DEFINE_TEST_CASE(remesh)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.remesh(h.cuboid(1, 1, 1), 0.25)"
            "op.remesh("
            "    h.cuboid(1, 1, 1),"
            "    s.faces_partially_in("
            "        t.translation(0, 0, 0.5) * v.bounding_box(1, 1, 1)), 0.5)"
            "op.remesh(h.cuboid(1, 1, 1), 0.125, 2)"
            "op.remesh(h.cuboid(1, 1, 1),"
            "    s.faces_partially_in("
            "        v.bounding_halfspace(-1, 0, 0, 1)"
            "        - v.bounding_halfspace(1, 0, 0, -1)),"
            "        1, 3)")
WITH_SOURCE("scheme",
            "(import (gamma transformation)"
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
EXPECTING("cuboid(1,1,1)",
          "remesh(cuboid(1,1,1),1/4,1)",
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
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.remesh("
            "    h.sphere(2),"
            "    s.edges_in(v.bounding_halfspace(0, 0, 1, 0)), 0.125)"
            "op.remesh("
            "    h.sphere(2),"
            "    s.faces_in("
            "        v.bounding_sphere(2)"
            "        * (t.rotation(90, 1) * v.bounding_halfspace(0, 0, 1, 0))),"
            "    s.edges_partially_in(v.bounding_halfspace(0, 0, 1, 0)), 0.5)")
WITH_SOURCE("scheme",
            "(import (gamma transformation)"
            "        (gamma volumes) (gamma selection)"
            "        (gamma polyhedra) (gamma operations))"

            "(remesh (sphere 2)"
            "        (edges-in (bounding-halfspace 0 0 1 0)) 1/8)"
            "(remesh (sphere 2)"
            "        (faces-in"
            "         (intersection"
            "          (bounding-sphere 2)"
            "          (bounding-halfspace 1 0 0 0)))"
            "        (edges-partially-in"
            "         (bounding-halfspace 0 0 1 0)) 1/2)")
EXPECTING("sphere(2,1/1024,1/1048576)",
          "remesh(sphere(2,1/1024,1/1048576),"
          "edges_in(bounding_halfspace(plane(0,0,1,0))),"
          "1/8,1)",
          "remesh(sphere(2,1/1024,1/1048576),"
          "faces_in(intersection(bounding_sphere(point(0,0,0),2),"
          "bounding_halfspace(plane(1,0,0,0)))),"
          "edges_partially_in(bounding_halfspace(plane(0,0,1,0))),"
          "1/2,1)")

DEFINE_TEST_CASE(fair)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.fair(h.cylinder(1, 5),"
            "        s.vertices_in(v.bounding_halfspace(0, 0, 1, 0)), 0)"
            "op.fair(h.cylinder(1, 5),"
            "        s.vertices_in(t.translation(0, 0, 5)"
            "        * v.bounding_cylinder(1, 1)))")
WITH_SOURCE("scheme",
            "(import (gamma transformation)"
            "        (gamma volumes) (gamma selection)"
            "        (gamma polyhedra) (gamma operations))"

            "(fair (cylinder 1 5)"
            "      (vertices-in (bounding-halfspace 0 0 1 0)) 0)"
            "(fair (cylinder 1 5)"
            "      (vertices-in"
            "       (transformation-apply"
            "        (translation 0 0 5)"
            "        (bounding-cylinder 1 1))))")
EXPECTING("regular_polygon(72,1,1/1048576)",
          "extrusion(regular_polygon(72,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2))",
          "fair(extrusion(regular_polygon(72,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2)),"
          "vertices_in(bounding_halfspace(plane(0,0,1,0))),0)",
          "fair(extrusion(regular_polygon(72,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2)),"
          "vertices_in(bounding_cylinder(point(0,0,9/2),vector(0,0,1),1,1)),1)")

DEFINE_TEST_CASE(smooth_shape)
WITH_SOURCE("lua",
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.smooth_shape("
            "    op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
            "    s.faces_in(v.bounding_halfspace(0,1,0,0)),"
            "    s.vertices_in(v.bounding_halfspace(0,0,1,0)), 1 / 64, 1)"
            "op.smooth_shape("
            "    op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
            "    s.faces_in(v.bounding_halfspace(0,1,0,0)), 1 / 64, 1)"
            "op.smooth_shape("
            "    op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
            "    s.vertices_in(v.bounding_halfspace(0,0,1,0)), 1 / 64, 1)"
            "op.smooth_shape("
            "    op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1), 1 / 64, 1)")
WITH_SOURCE("scheme",
            "(import (gamma volumes) (gamma selection)"
            "        (gamma polyhedra) (gamma operations))"

            "(smooth-shape"
            " (remesh (cuboid 2 2 2) 1/8 1)"
            " (faces-in (bounding-halfspace 0 1 0 0))"
            " (vertices-in (bounding-halfspace 0 0 1 0)) 1/64 1)"
            "(smooth-shape"
            " (remesh (cuboid 2 2 2) 1/8 1)"
            " (faces-in (bounding-halfspace 0 1 0 0)) 1/64 1)"
            "(smooth-shape"
            " (remesh (cuboid 2 2 2) 1/8 1)"
            " (vertices-in (bounding-halfspace 0 0 1 0)) 1/64 1)"
            "(smooth-shape"
            " (remesh (cuboid 2 2 2) 1/8 1) 1/64 1)")
EXPECTING("cuboid(2,2,2)",
          "remesh(cuboid(2,2,2),1/8,1)",
          "smooth_shape(remesh(cuboid(2,2,2),1/8,1),"
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

DEFINE_TEST_CASE(deform)
WITH_SOURCE("lua",
            "t = require 'gamma.transformation'"
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.deform("
            "    h.cylinder(1, 5),"
            "    s.vertices_in(v.bounding_halfspace(0, 0, 1, -2)),"
            "    t.translation(0, 0, 1),"
            "    s.vertices_in("
            "        v.complement(v.bounding_halfspace(0, 0, -1, -2))),"
            "    t.translation(0, 0, -1), 0.015625)"
            "op.deform("
            "    h.cylinder(1, 5),"
            "    s.vertices_in(~v.bounding_halfspace(0, 0, 1, -2)),"
            "    s.vertices_in(v.bounding_halfspace(0, 0, 1, -2)),"
            "    t.rotation(90, 1), 0.015625, 100)")
WITH_SOURCE("scheme",
            "(import (gamma transformation)"
            "        (gamma volumes) (gamma selection)"
            "        (gamma polyhedra) (gamma operations))"

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
EXPECTING("regular_polygon(72,1,1/1048576)",
          "extrusion(regular_polygon(72,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2))",
          "deform(extrusion(regular_polygon(72,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2)),"
          "vertices_in(bounding_halfspace(plane(0,0,1,-2))),"
          "translation(0,0,1),"
          "vertices_in(complement(bounding_halfspace(plane(0,0,-1,-2)))),"
          "translation(0,0,-1),1/64,4294967295)",
          "deform(extrusion(regular_polygon(72,1,1/1048576),"
          "translation(0,0,-5/2),translation(0,0,5/2)),"
          "vertices_in(complement(bounding_halfspace(plane(0,0,1,-2)))),"
          "vertices_in(bounding_halfspace(plane(0,0,1,-2))),"
          "rotation(0,0,1,0,1,0,-1,0,0),1/64,100)")

DEFINE_TEST_CASE(deflate)
WITH_SOURCE("lua",
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.deflate(op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1), 3)"
            "op.deflate("
            "    op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
            "    s.vertices_in(v.bounding_plane(0,0,1,-1)), 3)"
            "op.deflate("
            "    op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
            "    s.vertices_in(v.bounding_plane(0,0,1,-1)), 3, 1 / 2)"
            "op.deflate("
            "    op.remesh(h.cuboid(2, 2, 2), 1 / 8, 1),"
            "    s.vertices_in(v.bounding_plane(0,0,1,-1)), 3, 1/4, 1/8)")
WITH_SOURCE("scheme",
            "(import (gamma volumes) (gamma selection)"
            "        (gamma polyhedra) (gamma operations))"

            "(deflate"
            " (remesh (cuboid 2 2 2) 1/8 1) 3)"
            "(deflate"
            " (remesh (cuboid 2 2 2) 1/8 1)"
            " (vertices-in (bounding-plane 0 0 1 -1)) 3)"
            "(deflate"
            " (remesh (cuboid 2 2 2) 1/8 1)"
            " (vertices-in (bounding-plane 0 0 1 -1)) 3 1/2)"
            "(deflate"
            " (remesh (cuboid 2 2 2) 1/8 1)"
            " (vertices-in (bounding-plane 0 0 1 -1)) 3 1/4 1/8)")
EXPECTING("cuboid(2,2,2)",
          "remesh(cuboid(2,2,2),1/8,1)",
          "deflate(remesh(cuboid(2,2,2),1/8,1),3,1/10,0)",
          "deflate(remesh(cuboid(2,2,2),1/8,1),"
          "vertices_in(bounding_plane(plane(0,0,1,-1))),3,1/10,0)",
          "deflate(remesh(cuboid(2,2,2),1/8,1),"
          "vertices_in(bounding_plane(plane(0,0,1,-1))),"
          "3,1/2,0)",
          "deflate(remesh(cuboid(2,2,2),1/8,1),"
          "vertices_in(bounding_plane(plane(0,0,1,-1))),"
          "3,1/4,1/8)")

DEFINE_TEST_CASE(color_selection)
WITH_SOURCE("lua",
            "v = require 'gamma.volumes'"
            "s = require 'gamma.selection'"
            "h = require 'gamma.polyhedra'"
            "g = require 'gamma.polygons'"
            "op = require 'gamma.operations'"

            "op.color_selection("
            "    g.regular(3, 1), s.vertices_in(v.bounding_plane(1, 0, 0, 0)))"
            "op.color_selection("
            "    g.regular(3, 1),"
            "    s.vertices_in(v.bounding_plane(0, 1, 0, 0)), 5)"
            "op.color_selection("
            "    h.sphere(1),"
            "    s.faces_in(v.bounding_plane(0, 0, 1, 0)),"
            "    0.4, 0.5, 0.6)")
WITH_SOURCE("scheme",
            "(import (gamma volumes) (gamma selection)"
            "        (gamma polyhedra) (gamma polygons)"
            "        (gamma operations))"

            "(color-selection"
            " (regular-polygon 3 1)"
            " (vertices-in (bounding-plane 1 0 0 0)))"
            "(color-selection"
            " (regular-polygon 3 1)"
            " (vertices-in (bounding-plane 0 1 0 0)) 5)"
            "(color-selection"
            " (sphere 1) (faces-in (bounding-plane 0 0 1 0))"
            " 0.4 0.5 0.6)")
EXPECTING("regular_polygon(3,1,1/1048576)",
          "sphere(1,1/1024,1/1048576)",
          "extrusion(regular_polygon(3,1,1/1048576),translation(0,0,0))",
          "mesh(extrusion(regular_polygon(3,1,1/1048576),translation(0,0,0)))",
          "mesh(sphere(1,1/1024,1/1048576))",
          "color_selection(mesh(extrusion(regular_polygon(3,1,1/1048576),"
          "translation(0,0,0))),vertices_in(bounding_plane(plane(1,0,0,0)))"
          ",55,55,55,255)",
          "color_selection(mesh(extrusion(regular_polygon(3,1,1/1048576),"
          "translation(0,0,0))),vertices_in(bounding_plane(plane(0,1,0,0)))"
          ",0,142,93,255)",
          "color_selection(mesh(sphere(1,1/1024,1/1048576)),"
          "faces_in(bounding_plane(plane(0,0,1,0))),102,127,153,255)")

#define DEFINE_COLOR_TEST_CASE(WHAT)                                    \
DEFINE_TEST_CASE(color_## WHAT)                                         \
WITH_SOURCE("lua",                                                      \
            "v = require 'gamma.volumes'"                               \
            "s = require 'gamma.selection'"                             \
            "h = require 'gamma.polyhedra'"                             \
            "g = require 'gamma.polygons'"                              \
            "op = require 'gamma.operations'"                           \
                                                                        \
            "op.color_" #WHAT "(g.regular(3, 1))"                       \
            "op.color_" #WHAT "(g.regular(3, 1), 5)"                    \
            "op.color_" #WHAT "(h.tetrahedron(1, 1, 1), 0.4, 0.5, 0.6)") \
WITH_SOURCE("scheme",                                                   \
            "(import (gamma volumes) (gamma selection)"                 \
            "        (gamma polyhedra) (gamma polygons)"                \
            "        (gamma operations))"                               \
                                                                        \
            "(color-" #WHAT " (regular-polygon 3 1))"                   \
            "(color-" #WHAT " (regular-polygon 3 1) 5)"                 \
            "(color-" #WHAT " (tetrahedron 1 1 1) 0.4 0.5 0.6)")        \
EXPECTING("regular_polygon(3,1,1/1048576)",                             \
          "tetrahedron(1,1,1)",                                         \
          "extrusion(regular_polygon(3,1,1/1048576),translation(0,0,0))", \
          "mesh(extrusion(regular_polygon(3,1,1/1048576),translation(0,0,0)))", \
          "mesh(tetrahedron(1,1,1))",                                   \
          "color_" #WHAT "(mesh(extrusion(regular_polygon(3,1,1/1048576)," \
          "translation(0,0,0))),55,55,55,255)",                         \
          "color_" #WHAT "(mesh(extrusion(regular_polygon(3,1,1/1048576)," \
          "translation(0,0,0))),0,142,93,255)",                         \
          "color_" #WHAT "(mesh(tetrahedron(1,1,1)),102,127,153,255)")

DEFINE_COLOR_TEST_CASE(vertices)
DEFINE_COLOR_TEST_CASE(faces)

#undef DEFINE_COLOR_TEST_CASE

// ## Front End Tests for Other Operations

// The tests below are for miscellaneous operations, that don't fall
// into any of the other categories and don't require extensive enough
// testing to merit their own.

DEFINE_TEST_CASE(polygon_hull)
WITH_SOURCE("lua",
            "g = require 'gamma.polygons'"
            "op = require 'gamma.operations'"

            "op.hull("
            "    g.rectangle(2, 2),"
            "    g.simple(point(1, -1), point(2, -1), point(1, 1)))"
            "op.hull(point(0, 0), point(1, 0), point(0, 1))"
            "op.hull(g.rectangle(2, 2), point(2, -1))")
WITH_SOURCE("scheme",
            "(import (gamma polygons) (gamma operations))"

            "(hull (rectangle 2 2)"
            "      (simple-polygon"
            "       (point 1 -1) (point 2 -1) (point 1 1)))"
            "(hull (point 0 0) (point 1 0) (point 0 1))"
            "(hull (rectangle 2 2) (point 2 -1))")
EXPECTING(RECTANGLE_TAG,
          "polygon(point(1,-1),point(2,-1),point(1,1))",
          "hull(" RECTANGLE_TAG ","
          "polygon(point(1,-1),point(2,-1),point(1,1)))",
          "hull(point(0,0),point(1,0),point(0,1))",
          "hull(" RECTANGLE_TAG ",point(2,-1))")

DEFINE_TEST_CASE(polyhedron_hull)
WITH_SOURCE("lua",
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.hull(h.tetrahedron(1, 1, 1), h.tetrahedron(-1, 1, 1))"
            "op.hull(h.tetrahedron(1, 1, 1), point(-1, 0, 0))"
            "op.hull(point(0, 0, 0), point(1, 0, 0),"
            "        point(0, 1, 0), point(0, 0, 1))")
WITH_SOURCE("scheme",
            "(import (gamma polyhedra) (gamma operations))"

            "(hull (tetrahedron 1 1 1) (tetrahedron -1 1 1))"
            "(hull (tetrahedron 1 1 1) (point -1 0 0))"
            "(hull (point 0 0 0) (point 1 0 0)"
            "      (point 0 1 0) (point 0 0 1))")
EXPECTING("tetrahedron(1,1,1)",
          "tetrahedron(-1,1,1)",
          "hull(tetrahedron(1,1,1),tetrahedron(-1,1,1))",
          "hull(tetrahedron(1,1,1),point(-1,0,0))",
          "hull(point(0,0,0),point(1,0,0),point(0,1,0),point(0,0,1))")

DEFINE_TEST_CASE(minkowski_sum)
WITH_SOURCE("lua",
            "g = require 'gamma.polygons'"
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.minkowski_sum(g.rectangle(2, 2), g.regular(4, 0.5))"
            "op.minkowski_sum(g.rectangle(2, 2), g.circle(0.5))"
            "op.minkowski_sum(h.tetrahedron(5, 5, 5), h.octahedron(1, 1, 0.5))")
WITH_SOURCE("scheme",
            "(import (gamma polygons) (gamma polyhedra)"
            "        (gamma operations))"

            "(minkowski-sum (rectangle 2 2) (regular-polygon 4 1/2))"
            "(minkowski-sum (rectangle 2 2) (circle 1/2))"
            "(minkowski-sum (tetrahedron 5 5 5) (octahedron 1 1 1/2))")
EXPECTING(RECTANGLE_TAG,
          "regular_polygon(4,1/2,1/1048576)",
          "minkowski_sum(" RECTANGLE_TAG ",regular_polygon(4,1/2,1/1048576))",
          "circle(1/2)",
          "segments(circle(1/2),1/1024,1/1048576)",
          "minkowski_sum(" RECTANGLE_TAG ","
          "segments(circle(1/2),1/1024,1/1048576))",
          "tetrahedron(5,5,5)",
          "nef(tetrahedron(5,5,5))",
          "octahedron(1,1,1/2,1/2)",
          "nef(octahedron(1,1,1/2,1/2))",
          "minkowski_sum(nef(tetrahedron(5,5,5)),nef(octahedron(1,1,1/2,1/2)))")

DEFINE_TEST_CASE(subdivision)
WITH_SOURCE("lua",
            "h = require 'gamma.polyhedra'"
            "op = require 'gamma.operations'"

            "op.subdivide_catmull_clark(h.tetrahedron(1, 1, 1), 1)"
            "op.subdivide_doo_sabin(h.tetrahedron(1, 1, 1), 2)"
            "op.subdivide_loop(h.tetrahedron(1, 1, 1), 3)"
            "op.subdivide_sqrt_3(h.tetrahedron(1, 1, 1), 4)")
WITH_SOURCE("scheme",
            "(import (gamma polyhedra) (gamma operations))"

            "(subdivide-catmull-clark (tetrahedron 1 1 1) 1)"
            "(subdivide-doo-sabin (tetrahedron 1 1 1) 2)"
            "(subdivide-loop (tetrahedron 1 1 1) 3)"
            "(subdivide-sqrt-3 (tetrahedron 1 1 1) 4)")
EXPECTING("tetrahedron(1,1,1)",
          "sqrt_3(tetrahedron(1,1,1),4)",
          "loop(tetrahedron(1,1,1),3)",
          "doo_sabin(tetrahedron(1,1,1),2)",
          "catmull_clark(tetrahedron(1,1,1),1)")

// ## Front End Tests for Output Operations

// Finally, here we test for the creation of output operations with
// any of the various switches that enable them.

struct Output_fixture: Main_fixture {
    Output_fixture() {
        push(Flags::output_stl, 1);

        Options::outputs.push_front("foo.off");
        Options::outputs.push_front("bar.wrl:foo");
        Options::outputs.push_front("qux:foo");
        Options::outputs.push_front(":foo");
        Options::outputs.push_front(":");
    }

    ~Output_fixture() {
        pop(Flags::output_stl);

        Options::outputs.pop_front();
        Options::outputs.pop_front();
        Options::outputs.pop_front();
        Options::outputs.pop_front();
    }
};

DEFINE_TEST_CASE(output, * boost::unit_test::fixture<Output_fixture>())
WITH_SOURCE("lua",
            "h = require 'gamma.polyhedra'"

            "output(\"foo\", h.tetrahedron(1, 1, 1))"
            "output(h.tetrahedron(1, 1, 1), h.tetrahedron(-1, 1, 1))")
WITH_SOURCE("scheme",
            "(import (gamma polyhedra))"

            "(define-output foo (tetrahedron 1 1 1))"
            "(output (tetrahedron 1 1 1) (tetrahedron -1 1 1))")
EXPECTING("tetrahedron(1,1,1)",
          "mesh(tetrahedron(1,1,1))",
          "tetrahedron(-1,1,1)",
          "mesh(tetrahedron(-1,1,1))",
          "write_off(\"foo.off\",mesh(tetrahedron(1,1,1)))",
          "write_stl(\"foo.stl\",mesh(tetrahedron(1,1,1)))",
          "write_wrl(\"bar.wrl\",mesh(tetrahedron(1,1,1)))",
          "inspect(\"qux\",mesh(tetrahedron(1,1,1)))",
          "inspect(mesh(tetrahedron(1,1,1)))",
          "inspect(mesh(tetrahedron(1,1,1)),mesh(tetrahedron(-1,1,1)))")

#undef DEFINE_TEST_CASE
#undef RECTANGLE_TAG

#undef LUA_ITEM
#undef CLOSE_LUA

#undef SCHEME_ITEM
#undef CLOSE_SCHEME

#undef WITH_SOURCE
#undef RUN
#undef EXPECTING
#undef EXPECTING_SUCCESS

BOOST_AUTO_TEST_SUITE_END()
