#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <filesystem>

#include "assertions.h"
#include "kernel.h"
#include "macros.h"

#include "polygon_tests.h"
#include "circle_polygon_tests.h"
#include "polyhedron_tests.h"

#include <CGAL/draw_polygon_set_2.h>

// These are (fail, compress, type) tuples.

using polyhedron_types = boost::mpl::list<
    std::tuple<std::false_type, std::false_type, Polyhedron>,
    std::tuple<std::true_type, std::false_type, Polyhedron>,
    std::tuple<std::false_type, std::true_type, Polyhedron>,
    std::tuple<std::true_type, std::true_type, Polyhedron>,

    std::tuple<std::false_type, std::false_type, Nef_polyhedron>,
    std::tuple<std::true_type, std::false_type, Nef_polyhedron>,
    std::tuple<std::false_type, std::true_type, Nef_polyhedron>,
    std::tuple<std::true_type, std::true_type, Nef_polyhedron>,

    std::tuple<std::false_type, std::false_type, Surface_mesh>,
    std::tuple<std::true_type, std::false_type, Surface_mesh>,
    std::tuple<std::false_type, std::true_type, Surface_mesh>,
    std::tuple<std::true_type, std::true_type, Surface_mesh>>;

using polygon_types = boost::mpl::list<
    std::tuple<std::false_type, std::false_type, Polygon_set>,
    std::tuple<std::true_type, std::false_type, Polygon_set>,
    std::tuple<std::false_type, std::true_type, Polygon_set>,
    std::tuple<std::true_type, std::true_type, Polygon_set>,

    std::tuple<std::false_type, std::false_type, Circle_polygon_set>,
    std::tuple<std::true_type, std::false_type, Circle_polygon_set>,
    std::tuple<std::false_type, std::true_type, Circle_polygon_set>,
    std::tuple<std::true_type, std::true_type, Circle_polygon_set>>;

BOOST_AUTO_TEST_SUITE(store)

///////////////
// Polyhedra //
///////////////

BOOST_AUTO_TEST_CASE_TEMPLATE(polyhedron, T, polyhedron_types)
{
    using P = std::tuple_element_t<0, T>;
    using Q = std::tuple_element_t<1, T>;
    using U = std::tuple_element_t<2, T>;

    Tolerances::curve = FT::ET(1, 100);
    int i = Options::store_compression;
    int j = Options::store_threshold;
    Options::store_compression = Q::value ? 6 : -1;
    Options::store_threshold = 0;

    // Enable storing and perform the first evaluation.

    bool p = Flags::store_operations;
    Flags::store_operations = true;

    begin_unit("store");
    auto s = SPHERE(1);
    auto a = CONVERT_TO<U>(s);
    evaluate_unit();

    Flags::store_operations = p;

    // Test stored files.

    {
        std::fstream f(a->annotations["stored"]);
        BOOST_TEST_REQUIRE(f.good());
    }

    // When testing load failure, manually corrupt the stored
    // file.

    if (P::value) {
        std::filesystem::path p(a->annotations["stored"]);
        std::filesystem::resize_file(p, std::filesystem::file_size(p) / 2);
    }

    // Enable loading and perform the second evaluation.

    p = Flags::load_operations;
    Flags::load_operations = true;

    begin_unit("load");
    auto b = CONVERT_TO<U>(SPHERE(1));
    evaluate_unit();

    Flags::load_operations = p;
    Options::store_compression = i;
    Options::store_threshold = j;

    // Test loaded polyhedra.

    if (P::value) {
        BOOST_TEST((a->annotations.find("stored") != a->annotations.end()));
        BOOST_TEST((b->annotations.find("loaded") == b->annotations.end()));
    } else {
        BOOST_TEST(a->annotations["stored"] == b->annotations["loaded"]);
        BOOST_TEST(polyhedron_volume(*a->get_value())
                   == polyhedron_volume(*b->get_value()));
    }

    std::remove(s->annotations["stored"].c_str());
    std::remove(a->annotations["stored"].c_str());
}

//////////////
// Polygons //
//////////////

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.01))
BOOST_AUTO_TEST_CASE_TEMPLATE(polygon, T, polygon_types)
{
    using P = std::tuple_element_t<0, T>;
    using Q = std::tuple_element_t<1, T>;
    using U = std::tuple_element_t<2, T>;

    Tolerances::curve = FT::ET(1, 100);
    FT x(FT::ET(1, 3));

#define SEGMENT_OPS JOIN(                                               \
        DIFFERENCE(                                                     \
            DIFFERENCE(                                                 \
                RECTANGLE(2, 2),                                        \
                TRANSFORM(RECTANGLE(x, x), TRANSLATION_2(2 * x, 2 * x))), \
            TRANSFORM(RECTANGLE(x, x), TRANSLATION_2(-2 * x, -2 * x))), \
        TRANSFORM(RECTANGLE(2 * x, 2 * x), TRANSLATION_2(2, 2)))

#define CIRCLE_OPS JOIN(                                                \
        DIFFERENCE(                                                     \
            DIFFERENCE(                                                 \
                CIRCLE(1),                                              \
                TRANSFORM_CS(CIRCLE(x), TRANSLATION_2(0.5, 0))),        \
            TRANSFORM_CS(CIRCLE(x), TRANSLATION_2(-0.5, 0))),           \
        TRANSFORM_CS(CIRCULAR_SECTOR(2 * x, 180), TRANSLATION_2(0, 2)))

    std::shared_ptr<Polygon_operation<U>> a, b;
    std::shared_ptr<Polygon_operation<Polygon_set>> c;

    int i = Options::store_compression;
    int j = Options::store_threshold;
    Options::store_compression = Q::value ? 6 : -1;
    Options::store_threshold = 0;

    // Enable storing and perform the first evaluation.

    bool p = Flags::store_operations;
    Flags::store_operations = true;

    begin_unit("store");
    if constexpr (std::is_same_v<U, Polygon_set>) {
        a = SEGMENT_OPS;
    } else if constexpr (std::is_same_v<U, Circle_polygon_set>) {
        a = CIRCLE_OPS;
    } else {
        assert_not_reached();
    }
    evaluate_unit();

    Flags::store_operations = p;

    // Test stored files.

    std::fstream f(a->annotations["stored"]);
    BOOST_TEST_REQUIRE(f.good());

    // When testing load failure, manually corrupt the stored
    // file.

    if (P::value) {
        std::filesystem::path p(a->annotations["stored"]);
        std::filesystem::resize_file(p, std::filesystem::file_size(p) / 2);
    }

    // Enable loading and perform the second evaluation.

    p = Flags::load_operations;
    Flags::load_operations = true;

    begin_unit("load");
    if constexpr (std::is_same_v<U, Polygon_set>) {
        b = SEGMENT_OPS;
    } else if constexpr (std::is_same_v<U, Circle_polygon_set>) {
        b = CIRCLE_OPS;
        c = CONVERT_TO<Polygon_set>(b);
    } else {
        assert_not_reached();
    }
    evaluate_unit();

    Flags::load_operations = p;
    Options::store_compression = i;
    Options::store_threshold = j;

    // Test loaded polygons.

    if (P::value) {
        BOOST_TEST((a->annotations.find("stored") != a->annotations.end()));
        BOOST_TEST((b->annotations.find("loaded") == b->annotations.end()));
    } else {
        BOOST_TEST(a->annotations["stored"] == b->annotations["loaded"]);

        if constexpr (std::is_same_v<U, Polygon_set>) {
            BOOST_TEST(polygon_area(*a->get_value()) == polygon_area(*b->get_value()));
        } else {
            test_polygon_area(*c->get_value(), std::acos(-1));
        }
    }

    std::remove(a->annotations["stored"].c_str());
}

BOOST_AUTO_TEST_SUITE_END()
