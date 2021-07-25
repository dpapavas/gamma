#define BOOST_TEST_MODULE main tests
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <CGAL/assertions.h>

#include "options.h"
#include "kernel.h"
#include "transformations.h"
#include "tolerances.h"
#include "projection.h"
#include "macros.h"

#include "fixtures.h"

struct Global_fixture {
  void setup() {
      CGAL::set_error_behaviour(CGAL::THROW_EXCEPTION);
      CGAL::set_warning_behaviour(CGAL::THROW_EXCEPTION);

      Flags::eliminate_dead_operations = 0;
      Flags::store_operations = 0;
      Flags::load_operations = 0;

      parse_options(
          boost::unit_test::framework::master_test_suite().argc,
          boost::unit_test::framework::master_test_suite().argv);
  }

  void teardown() {
  }
};

BOOST_TEST_GLOBAL_FIXTURE(Global_fixture);

//////////////////////////
// Command line options //
//////////////////////////

static int test_options(std::initializer_list<const char *> args)
{
    return parse_options(args.size(), const_cast<char **>(std::data(args)));
}

BOOST_TEST_DONT_PRINT_LOG_VALUE(Polyhedron_booleans_mode)
BOOST_TEST_DONT_PRINT_LOG_VALUE(Diagnostics_color_mode)
BOOST_TEST_DONT_PRINT_LOG_VALUE(Language)

BOOST_AUTO_TEST_SUITE(options)

BOOST_AUTO_TEST_CASE(dump)
{
    const char *s = Options::dump_operations, *t = Options::dump_graph;

    BOOST_TEST(
        test_options({"test", "--dump-operations", "--dump-graph"}) == 3);

    BOOST_TEST(Options::dump_operations);
    BOOST_TEST(Options::dump_graph);

    BOOST_TEST(
        test_options({"test", "--no-dump-operations", "--no-dump-graph"}) == 3);

    BOOST_TEST(!Options::dump_operations);
    BOOST_TEST(!Options::dump_graph);

    Options::dump_operations = s;
    Options::dump_graph = t;
}

BOOST_AUTO_TEST_CASE(dump_short_tags)
{
    int i = Options::dump_short_tags;

    BOOST_TEST(
        test_options({"test", "--dump-short-tags=100"}) == 2);

    BOOST_TEST(Options::dump_short_tags == 100);

    BOOST_TEST(
        test_options({"test", "--dump-short-tags"}) == 2);

    BOOST_TEST(Options::dump_short_tags == 50);

    BOOST_TEST(
        test_options({"test", "--no-dump-short-tags"}) == 2);

    BOOST_TEST(Options::dump_short_tags == -1);

    BOOST_TEST(
        test_options({"test", "--no-dump-short-tags=1"}) == -EXIT_FAILURE);

    BOOST_TEST(
        test_options({"test", "--dump-short-tags=-1"}) == -EXIT_FAILURE);

    Options::dump_short_tags = i;
}

BOOST_AUTO_TEST_CASE(polyhedron_booleans)
{
    Polyhedron_booleans_mode m = Options::polyhedron_booleans;

    BOOST_TEST(test_options({"test", "--polyhedron-booleans=nef"}) == 2);
    BOOST_TEST(Options::polyhedron_booleans == Polyhedron_booleans_mode::NEF);

    BOOST_TEST(test_options({"test", "--polyhedron-booleans=corefine"}) == 2);
    BOOST_TEST(Options::polyhedron_booleans == Polyhedron_booleans_mode::COREFINE);

    BOOST_TEST(test_options({"test", "--polyhedron-booleans=auto"}) == 2);
    BOOST_TEST(Options::polyhedron_booleans == Polyhedron_booleans_mode::AUTO);

    BOOST_TEST(test_options({"test", "--polyhedron-booleans=foo"}) == -EXIT_FAILURE);
    BOOST_TEST(test_options({"test", "--polyhedron-booleans"}) == -EXIT_FAILURE);

    Options::polyhedron_booleans = m;
}

BOOST_AUTO_TEST_CASE(store_compression)
{
    int i = Options::store_compression;

    BOOST_TEST(
        test_options({"test", "--store-compression=9"}) == 2);

    BOOST_TEST(Options::store_compression == 9);

    BOOST_TEST(
        test_options({"test", "--no-store-compression"}) == 2);

    BOOST_TEST(Options::store_compression == -1);

    BOOST_TEST(
        test_options({"test", "--store-compression"}) == 2);

    BOOST_TEST(Options::store_compression == 6);

    BOOST_TEST(
        test_options({"test", "--no-store-compression=1"}) == -EXIT_FAILURE);

    BOOST_TEST(
        test_options({"test", "--store-compression=-1"}) == -EXIT_FAILURE);

    BOOST_TEST(
        test_options({"test", "--store-compression=10"}) == -EXIT_FAILURE);

    Options::store_compression = i;
}

BOOST_AUTO_TEST_CASE(store_threshold)
{
    int i = Options::store_threshold;

    BOOST_TEST(
        test_options({"test", "--store-threshold=42"}) == 2);

    BOOST_TEST(Options::store_threshold == 42);

    BOOST_TEST(
        test_options({"test", "--no-store-threshold"}) == 2);

    BOOST_TEST(Options::store_threshold == 0);

    BOOST_TEST(
        test_options({"test", "--store-threshold"}) == -EXIT_FAILURE);

    BOOST_TEST(
        test_options({"test", "--no-store-threshold=1"}) == -EXIT_FAILURE);

    BOOST_TEST(
        test_options({"test", "--store-threshold=-1"}) == -EXIT_FAILURE);

    Options::store_threshold = i;
}

BOOST_AUTO_TEST_CASE(rewrite_pass_limit)
{
    int i = Options::rewrite_pass_limit;

    BOOST_TEST(
        test_options({"test", "--rewrite-pass-limit=10"}) == 2);

    BOOST_TEST(Options::rewrite_pass_limit == 10);

    BOOST_TEST(
        test_options({"test", "--no-rewrite-pass-limit"}) == 2);

    BOOST_TEST(Options::rewrite_pass_limit == -1);

    BOOST_TEST(
        test_options({"test", "--no-rewrite-pass-limit=1"}) == -EXIT_FAILURE);

    BOOST_TEST(
        test_options({"test", "--rewrite-pass-limit=-1"}) == -EXIT_FAILURE);

    BOOST_TEST(
        test_options({"test", "--rewrite-pass-limit"}) == -EXIT_FAILURE);

    Options::rewrite_pass_limit = i;
}

BOOST_AUTO_TEST_CASE(diagnostics_color)
{
    Diagnostics_color_mode m = Options::diagnostics_color;

    BOOST_TEST(test_options({"test", "--diagnostics-color"}) == 2);
    BOOST_TEST(Options::diagnostics_color == Diagnostics_color_mode::ALWAYS);

    BOOST_TEST(test_options({"test", "--diagnostics-color=never"}) == 2);
    BOOST_TEST(Options::diagnostics_color == Diagnostics_color_mode::NEVER);

    BOOST_TEST(test_options({"test", "--diagnostics-color=always"}) == 2);
    BOOST_TEST(Options::diagnostics_color == Diagnostics_color_mode::ALWAYS);

    BOOST_TEST(test_options({"test", "--diagnostics-color=auto"}) == 2);
    BOOST_TEST(Options::diagnostics_color == Diagnostics_color_mode::AUTO);

    BOOST_TEST(test_options({"test", "--diagnostics-color=foo"}) == -EXIT_FAILURE);

    Options::diagnostics_color = m;
}

BOOST_AUTO_TEST_CASE(diagnostics_elide_tags)
{
    int i = Options::diagnostics_elide_tags;

    BOOST_TEST(
        test_options({"test", "--diagnostics-elide-tags=5"}) == 2);

    BOOST_TEST(Options::diagnostics_elide_tags == 5);

    BOOST_TEST(
        test_options({"test", "--no-diagnostics-elide-tags"}) == 2);

    BOOST_TEST(Options::diagnostics_elide_tags == -1);

    BOOST_TEST(
        test_options({"test", "--diagnostics-elide-tags"}) == 2);

    BOOST_TEST(Options::diagnostics_elide_tags == 1);

    BOOST_TEST(
        test_options({"test", "--no-diagnostics-elide-tags=1"}) == -EXIT_FAILURE);

    BOOST_TEST(
        test_options({"test", "--diagnostics-elide-tags=-1"}) == -EXIT_FAILURE);

    Options::diagnostics_elide_tags = i;
}

BOOST_AUTO_TEST_CASE(language)
{
    Language l = Options::language;

#ifdef HAVE_LUA
    BOOST_TEST(test_options({"test", "-x", "lua"}) == 3);
    BOOST_TEST(Options::language == Language::LUA);
#endif

#ifdef HAVE_SCHEME
    BOOST_TEST(test_options({"test", "-x", "scheme"}) == 3);
    BOOST_TEST(Options::language == Language::SCHEME);
#endif

    BOOST_TEST(test_options({"test", "-x", "auto"}) == 3);
    BOOST_TEST(Options::language == Language::AUTO);

    BOOST_TEST(test_options({"test", "-x", "foo"}) == -EXIT_FAILURE);

    Options::language = l;
}

BOOST_AUTO_TEST_CASE(backend)
{
#ifdef HAVE_SCHEME
    BOOST_TEST(test_options({"test", "-F", "./foo"}) == 3);
    BOOST_TEST(Options::scheme_features.front() == "./foo");
    Options::scheme_features.pop_front();
#endif

    BOOST_TEST(test_options({"test", "-I", "./foo"}) == 3);
    BOOST_TEST(Options::include_directories.front() == "./foo");
    Options::include_directories.pop_front();

    BOOST_TEST(test_options({"test", "-D", "foo"}) == 3);
    BOOST_TEST(test_options({"test", "-D", "bar=hello"}) == 3);

    BOOST_TEST(Options::definitions.front().first == std::string("bar"));
    BOOST_TEST(Options::definitions.front().second == std::string("hello"));
    Options::definitions.pop_front();

    BOOST_TEST(Options::definitions.front().first == std::string("foo"));
    BOOST_TEST(Options::definitions.front().second == std::string());
    Options::definitions.pop_front();
}

BOOST_AUTO_TEST_CASE(threads)
{
    const int n = Options::threads;

    BOOST_TEST(test_options({"test", "--threads=100"}) == 2);

    BOOST_TEST(Options::threads == 100);

    BOOST_TEST(test_options({"test", "--no-threads"}) == 2);

    BOOST_TEST(Options::threads == 0);

    BOOST_TEST(
        test_options({"test", "-t", std::to_string(n).c_str()}) == 3);

    BOOST_TEST(Options::threads == n);

    BOOST_TEST(test_options({"test", "--threads=-1"}) == -EXIT_FAILURE);
    BOOST_TEST(test_options({"test", "-t"}) == -EXIT_FAILURE);
    BOOST_TEST(test_options({"test", "--no-threads=1"}) == -EXIT_FAILURE);
}

BOOST_AUTO_TEST_CASE(flags)
{
#define TEST_FLAG(OPTION, VAR) {                                     \
        int i = Flags::VAR;                                          \
        Flags::VAR = 0;                                              \
        BOOST_TEST(test_options({"test", "--" #OPTION}) == 2);       \
        BOOST_TEST(Flags::VAR == 1);                                 \
        BOOST_TEST(test_options({"test", "--no-" #OPTION}) == 2);    \
        BOOST_TEST(Flags::VAR == 0);                                 \
        Flags::VAR = i;                                              \
    }

    TEST_FLAG(dump-abridged-tags, dump_abridged_tags);
    TEST_FLAG(dump-annotations, dump_annotations);
    TEST_FLAG(fold-transformations, fold_transformations);
    TEST_FLAG(fold-booleans, fold_booleans);
    TEST_FLAG(fold-flushes, fold_flushes);
    TEST_FLAG(eliminate-dead-operations, eliminate_dead_operations);
    TEST_FLAG(store-operations, store_operations);
    TEST_FLAG(load-operations, load_operations);
    TEST_FLAG(stl, output_stl);
    TEST_FLAG(output-stl, output_stl);
    TEST_FLAG(off, output_off);
    TEST_FLAG(output-off, output_off);
    TEST_FLAG(wrl, output_wrl);
    TEST_FLAG(output-wrl, output_wrl);

#ifdef HAVE_SCHEME
    TEST_FLAG(eliminate-tail-calls, eliminate_tail_calls);
#endif
}

BOOST_AUTO_TEST_CASE(warnings)
{
#define TEST_WARNING_S(X, S) {                                  \
        int i = Flags::warn_## X;                               \
        Flags::warn_## X = 0;                                   \
        BOOST_TEST(test_options({"test", "-W" S}) == 2);        \
        BOOST_TEST(Flags::warn_## X == 1);                      \
        BOOST_TEST(test_options({"test", "-Wno-" S}) == 2);     \
        BOOST_TEST(Flags::warn_## X == 0);                      \
        Flags::warn_## X = i;                                   \
    }

#define TEST_GROUPED_WARNING(G, X)              \
    TEST_WARNING_S(G ## _ ## X, #G);            \
    TEST_WARNING_S(G ## _ ## X, #G  "-" #X);

#define TEST_WARNING(X) TEST_WARNING_S(X, #X)
    TEST_WARNING_S(fatal_errors, "fatal-errors");
    TEST_WARNING(error);

    TEST_WARNING(duplicate);
    TEST_WARNING(manifold);
    TEST_WARNING(nef);
    TEST_WARNING(unused);
    TEST_WARNING(store);
    TEST_WARNING(load);
    TEST_WARNING(outputs);

    TEST_GROUPED_WARNING(mesh, valid);
    TEST_GROUPED_WARNING(mesh, closed);
    TEST_GROUPED_WARNING(mesh, manifold);
    TEST_GROUPED_WARNING(mesh, degenerate);
    TEST_GROUPED_WARNING(mesh, degenerate);
    TEST_GROUPED_WARNING(mesh, intersects);
    TEST_GROUPED_WARNING(mesh, bounds);
    TEST_GROUPED_WARNING(mesh, oriented);

#undef TEST_WARNING
#undef TEST_WARNING_S
#undef TEST_GROUPED_WARNING
}

BOOST_AUTO_TEST_SUITE_END()

//////////////////////////
// Operation evaluation //
//////////////////////////

BOOST_FIXTURE_TEST_SUITE(operations, Reset_operations)

BOOST_AUTO_TEST_CASE(messages)
{
    auto p = TETRAHEDRON(1, 1, 1);

    p->message(Operation::ERROR, "error in %, with literal %%");
    p->message(Operation::WARNING, "warning about %");
    p->message(Operation::NOTE, "hello world");
}

 //////////////////////////////////////////////////////////////
 // Sort and evaluate a set of operations.                   //
 //                                                          //
 // +-----------+   +-------+                   +---------+  //
 // |tetrahedron|---|to_mesh|-------------------|write_off|  //
 // +-----------+   +-------+                   +---------+  //
 //                                                  /       //
 //                 +---------+   +-------+         /        //
 //                -|extrude 1|---|to_mesh|---------         //
 // +---------+   / +---------+   +-------+                  //
 // |rectangle|---                                           //
 // +---------+   \ +---------+                              //
 //                -|extrude 2|                              //
 //                 +---------+                              //
 //////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(evaluation)
{
    std::vector<Aff_transformation_3> v({
            TRANSLATION_3(0, 0, 0),
            TRANSLATION_3(0, 0, 1),
            TRANSLATION_3(0, 0, 2)});

    std::vector<Aff_transformation_3> u({
            TRANSLATION_3(0, 0, 0),
            TRANSLATION_3(0, 0, 1)});

    WRITE_OFF(
        "test.out",
        {CONVERT_TO<Surface_mesh>(TETRAHEDRON(1, 1, 1)),
         CONVERT_TO<Surface_mesh>(EXTRUSION(RECTANGLE(2, 3), std::move(v)))});

    EXTRUSION(RECTANGLE(2, 3), std::move(u));

    evaluate_unit();
}

// Test evaluation failure.  With -Wfatal-errors and a single
// evaluation thread, b should never be evaluated and this should
// fail.

BOOST_AUTO_TEST_CASE(error, * boost::unit_test::disabled())
{
    auto b = JOIN(
        JOIN(TETRAHEDRON(1, 1, 1),
             TETRAHEDRON(1, 1, -1)),
        JOIN(TETRAHEDRON(-1, -1, 1),
             TETRAHEDRON(-1, -1, -1)));

    auto a = JOIN(TETRAHEDRON(1, 1, 1),
                  TETRAHEDRON(1, -1, -1));


    evaluate_unit();

    bool failed = a->annotations.find("failure") != a->annotations.end();
    BOOST_TEST(failed);
    BOOST_TEST(b->get_value());
}

BOOST_AUTO_TEST_SUITE_END()

/////////////////
// Miscellanea //
/////////////////

BOOST_AUTO_TEST_SUITE(misc)

BOOST_AUTO_TEST_CASE(simple_duplicate)
{
    auto p = TETRAHEDRON(1, 1, 1);
    std::shared_ptr<Operation> q =
        add_operation<Tetrahedron_operation>(1, 1, 1);

    std::shared_ptr<Operation> r =
        add_operation<Tetrahedron_operation>(2, 2, 2);

    BOOST_TEST(p == q);
    BOOST_TEST(q != r);
}

BOOST_AUTO_TEST_CASE(misc_tags)
{
    BOOST_TEST(compose_tag("x", 5) == "x(5)");
    BOOST_TEST(compose_tag("x", FT(FT::ET(5, 2))) == "x(5/2)");
    BOOST_TEST(
        compose_tag("x", Point_2(3, FT(FT::ET(5, 2)))) == "x(point(3,5/2))");
    BOOST_TEST(
        compose_tag("x", Plane_3(0, 1, 2, FT(FT::ET(3, 4))))
        == "x(plane(0,1,2,3/4))");
}

BOOST_AUTO_TEST_CASE(transformation_tags)
{
    // 2D

    BOOST_TEST(compose_tag(
                   "x", TRANSLATION_2(1, 2)) ==
               "x(translation(1,2))");

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_2(
                       CGAL::ROTATION,
                       Direction_2(RT(cos(1.23)), RT(sin(1.23))),
                       RT(1), RT(10))) ==
               "x(rotation(5/13,-12/13,12/13,5/13))");

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_2(CGAL::SCALING, 2)) ==
               "x(scaling(2,2))");

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_2(
                       CGAL::REFLECTION, Line_2(1, -1, 0))) ==
               "x(reflection(0,1,1,0))");

    // SL2, but not SO2 (i.e. unit determinant, but not orthogonal).

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_2(2, 1, 1, 1)) ==
               "x(transformation(2,1,1,1))");

    // General affine transform.

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_2(2, 3, 5, 7, 11, 13)) ==
               "x(transformation(2,3,5,7,11,13))");
    // 3D

    BOOST_TEST(compose_tag(
                   "x", TRANSLATION_3(1, 2, 3)) ==
               "x(translation(1,2,3))");

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_3(
                       RT::ET(5, 13), 0, RT::ET(-12, 13),
                       0, 1, 0,
                       RT::ET(12, 13), 0, RT::ET(5, 13))) ==
               "x(rotation(5/13,0,-12/13,0,1,0,12/13,0,5/13))");

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_3(
                       CGAL::SCALING, 2)) == "x(scaling(2,2,2))");

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_3(0, 0, 1, 0, 1, 0, 1, 0, 0)) ==
               "x(reflection(0,0,1,0,1,0,1,0,0))");

    // SL3, but not SO3.

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_3(2, 1, 1, 1, 1, 1, 1, 1, 2)) ==
               "x(transformation(2,1,1,1,1,1,1,1,2))");

    // Translation + scaling.

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_3(
                       1, 0, 0, 1, 0, 2, 0, 2, 0, 0, 3, 3)) ==
               "x(transformation(1,0,0,1,0,2,0,2,0,0,3,3))");

    // General affine transform.

    BOOST_TEST(compose_tag(
                   "x", Aff_transformation_3(
                       2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37)) ==
               "x(transformation(2,3,5,7,11,13,17,19,23,29,31,37))");
}

BOOST_AUTO_TEST_CASE(bounding_volume_tags)
{
    BOOST_TEST(
        compose_tag("x", BOUNDING_PLANE(1, 2, 3, 4))
        == ("x(bounding_plane(plane(1,2,3,4)))"));

    BOOST_TEST(
        compose_tag("x", BOUNDING_HALFSPACE(1, 2, 3, 4))
        == ("x(bounding_halfspace(plane(1,2,3,4)))"));

    BOOST_TEST(
        compose_tag("x", BOUNDING_BOX(1, 2, 3))
        == ("x(bounding_box("
            "plane(-1,0,0,1/2),"
            "plane(1,0,0,1/2),"
            "plane(0,-1,0,1),"
            "plane(0,1,0,1),"
            "plane(0,0,-1,3/2),"
            "plane(0,0,1,3/2)))"));

    BOOST_TEST(
        compose_tag("x", BOUNDING_SPHERE(1))
        == "x(bounding_sphere(point(0,0,0),1))");

    BOOST_TEST(
        compose_tag("x", TRANSFORM(BOUNDING_SPHERE(4), TRANSLATION_3(1, 2, 3)))
        == "x(bounding_sphere(point(1,2,3),4))");

    BOOST_TEST(
        compose_tag("x", BOUNDING_CYLINDER(FT::ET(2, 3), 4))
        == "x(bounding_cylinder(point(0,0,-2),vector(0,0,1),2/3,4))");

    BOOST_TEST(
        compose_tag("x", COMPLEMENT(BOUNDING_SPHERE(1)))
        == "x(complement(bounding_sphere(point(0,0,0),1)))");

    BOOST_TEST(
        compose_tag("x", JOIN({BOUNDING_SPHERE(1),
                               BOUNDING_SPHERE(2)}))
        == ("x(join("
            "bounding_sphere(point(0,0,0),1),"
            "bounding_sphere(point(0,0,0),2)))"));

    BOOST_TEST(
        compose_tag("x", INTERSECTION({
                    BOUNDING_SPHERE(1),
                    BOUNDING_SPHERE(2),
                    BOUNDING_SPHERE(3)}))
        == ("x(intersection("
            "bounding_sphere(point(0,0,0),1),"
            "bounding_sphere(point(0,0,0),2),"
            "bounding_sphere(point(0,0,0),3)))"));

    BOOST_TEST(
        compose_tag("x", DIFFERENCE({BOUNDING_SPHERE(4)}))
        == "x(difference(bounding_sphere(point(0,0,0),4)))");

    BOOST_TEST(
        compose_tag("x", COMPLEMENT(BOUNDING_SPHERE(5)))
        == "x(complement(bounding_sphere(point(0,0,0),5)))");
}

BOOST_AUTO_TEST_CASE(selection_tags)
{
    BOOST_TEST(
        compose_tag("x", FACES_IN(BOUNDING_SPHERE(1)))
        == "x(faces_in(bounding_sphere(point(0,0,0),1)))");

    BOOST_TEST(
        compose_tag("x", FACES_PARTIALLY_IN(BOUNDING_SPHERE(2)))
        == "x(faces_partially_in(bounding_sphere(point(0,0,0),2)))");

    BOOST_TEST(
        compose_tag("x", VERTICES_IN(BOUNDING_SPHERE(3)))
        == "x(vertices_in(bounding_sphere(point(0,0,0),3)))");

    BOOST_TEST(
        compose_tag("x", RELATIVE_SELECTION(
                        VERTICES_IN(BOUNDING_SPHERE(4)), 2))
        == "x(expand(vertices_in(bounding_sphere(point(0,0,0),4)),2))");

    BOOST_TEST(
        compose_tag("x", RELATIVE_SELECTION(
                        VERTICES_IN(BOUNDING_SPHERE(5)), -3))
        == "x(contract(vertices_in(bounding_sphere(point(0,0,0),5)),3))");

    BOOST_TEST(
        compose_tag("x", RELATIVE_SELECTION(
                        FACES_IN(BOUNDING_SPHERE(4)), 2))
        == "x(expand(faces_in(bounding_sphere(point(0,0,0),4)),2))");

    BOOST_TEST(
        compose_tag("x", RELATIVE_SELECTION(
                        FACES_IN(BOUNDING_SPHERE(5)), -3))
        == "x(contract(faces_in(bounding_sphere(point(0,0,0),5)),3))");
}

BOOST_AUTO_TEST_CASE(basic, * boost::unit_test::tolerance(0.0001))
{
    for (int j = 0; j < 3; j++) {
        const Point_3 O(CGAL::ORIGIN);
        const Point_3 P(j != 0, j == 0, 0);
        const Point_3 T = basic_rotation(30, j).transform(P);

        BOOST_TEST(CGAL::squared_distance(T, O) == FT(1));
        BOOST_TEST(CGAL::to_double(CGAL::approximate_angle(P, O, T)) == 30.0);

        for (int i = 0; i < 3; i++) {
            if (i == j) {
                BOOST_TEST(T[i] == FT(0));
            } else {
                BOOST_TEST(T[i] != FT(0));
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(axis_angle, * boost::unit_test::tolerance(0.0001))
{
    const double v[3] = {1, 2, 3};
    const Point_3 P(4, 5, 6);
    const Point_3 T = axis_angle_rotation(78.9, v).transform(P);
    const Line_3 L(CGAL::ORIGIN, Point_3(v[0], v[1], v[2]));
    const Point_3 Q = L.projection(P);

    BOOST_TEST(CGAL::to_double(CGAL::squared_distance(P, Q)) ==
               CGAL::to_double(CGAL::squared_distance(T, Q)));
    BOOST_TEST(CGAL::to_double(CGAL::approximate_angle(P, Q, T)) == 78.9);
}

BOOST_AUTO_TEST_CASE(project_2)
{
    const double rho = 3.21;
    const FT epsilon = FT::ET(1, 1'000'000'000);

    const double v[][2] = {{1.23, 3.45},
                           {-6.78, 9.01},
                           {2.34, -5.67},
                           {-8.90, -1.23},
                           {4.56, 0}, {-7.89, 0},
                           {0, 1.23}, {0, -3.45}};

    for (std::size_t i = 0; i < sizeof(v) / sizeof(v[0]); i++) {
        double x = v[i][0], y = v[i][1];
        const double m = std::sqrt(x * x + y * y) / rho;

        Point_2 P = project_to_circle(x, y, FT(rho), epsilon);

        x /= m;
        y /= m;

        BOOST_TEST(
            ((x - P.x()) * (x - P.x())
             + (y - P.y()) * (y - P.y())) <= epsilon * epsilon);
    }
}

BOOST_AUTO_TEST_CASE(project_3)
{
    const double rho = 3.141;
    const FT epsilon = FT::ET(1, 1'000'000'000);

    const double v[][3] = {{-1.23, 3.45, 6.78},
                           {9.01, -2.34, 5.67},
                           {8.90, 1.23, -4.56},
                           {7.89, 0, 0}, {-0.12, 0, 0},
                           {0, 3.45, 0}, {0, -6.78, 0},
                           {0, 0, 9.01}, {0, 0, -2.34}};

    for (std::size_t i = 0; sizeof(v) / sizeof(v[0]) < 9; i++) {
        double x = v[i][0], y = v[i][1], z = v[i][2];
        const double m = std::sqrt(x * x + y * y + z * z) / rho;

        Point_3 P = project_to_sphere(x, y, z, FT(rho), epsilon);

        x /= m;
        y /= m;
        z /= m;

        BOOST_TEST(
            ((x - CGAL::to_double(P.x())) * (x - CGAL::to_double(P.x()))
             + (y - CGAL::to_double(P.y())) * (y - CGAL::to_double(P.y()))
             + (z - CGAL::to_double(P.z())) * (z - CGAL::to_double(P.z())))
            <= epsilon * epsilon);
    }
}

BOOST_AUTO_TEST_SUITE_END()
