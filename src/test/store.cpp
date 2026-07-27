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

#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>
#include <boost/mpl/list.hpp>

#include <filesystem>

#include "assertions.h"
#include "kernel.h"
#include "macros.h"

#include "fixtures.h"
#include "polygon_tests.h"
#include "circle_polygon_tests.h"
#include "polyhedron_tests.h"

#include <CGAL/draw_polygon_set_2.h>

// The basic test procedure is the same for polygons and polyhedra: We
// evaluate a set of operations and ensure the result is stored, then
// re-evaluate them and check that they're properly loaded.  To test
// that load errors (e.g. when a stored operation file has been
// corrupted, or perhaps a newer version has a different stored file
// format and is trying to load files stored by a previous version),
// are handled correctly, we then manually corrupt the stored file and
// evaluate once more, checking for a failed operation (as opposed to
// a segmentation fault for example).

// In order to facilitate uniform testing of polygons and polyhedra,
// we define a set of templates that return operations of the proper
// type and their expected digests.

typedef boost::mpl::list<
    Polyhedron, Nef_polyhedron, Surface_mesh,
    Polygon_set, Circle_polygon_set> result_types;

template<typename T>
static std::vector<std::string> digests;

template<typename T>
static auto generate_operations();

// ## Polyhedra

// These are the expected tag digests of the operations used in the
// test.  They are the SHA-1 sums of:
// 1. `"sphere(1/4,1/1000,1/1000000)"`,
// 2. `"nef(sphere(1/4,1/1000,1/1000000))"` and
// 3. `"mesh(sphere(1/4,1/1000,1/1000000))"`.

// The digest of the operation that is tested is listed first,
// followed by its predecessors, if any.  We list the latter, so that
// we can clean them up at the end.

template<>
std::vector<std::string> digests<Polyhedron> = {
    "607a6064dba223a25e7317574accb6af30131a4d"
};
template<>
std::vector<std::string> digests<Nef_polyhedron> = {
    "976ef1309f9dda6ac794400987b64de033531ba8",
    "607a6064dba223a25e7317574accb6af30131a4d"
};
template<>
std::vector<std::string> digests<Surface_mesh> = {
    "a659f184f08acbb5d364bc807097b2e810a752a3",
    "607a6064dba223a25e7317574accb6af30131a4d"
};

#define DEFINE_POLYHEDRON_GENERATOR(T)          \
template<>                                      \
auto generate_operations<T>()                   \
{                                               \
    return CONVERT_TO<T>(SPHERE(FT::ET(1, 4))); \
};

DEFINE_POLYHEDRON_GENERATOR(Polyhedron)
DEFINE_POLYHEDRON_GENERATOR(Nef_polyhedron)
DEFINE_POLYHEDRON_GENERATOR(Surface_mesh)

#undef DEFINE_POLYHEDRON_GENERATOR

// ## Polygons

// The situation is analogous for polygons, only the geometry is a
// little more complex, just to make sure the tested polygon sets have
// more than one polygons, one with a hole and one without.

template<>
std::vector<std::string> digests<Polygon_set> = {
    "be976eb6c8ed95c6e4580282605ca65fa868a829",
    "503f92c63ebdeab76152d6cc3060097503efbfa8",
    "8506925af526760398e77c344735a06ddb7daf55",
    "80572bd323a577301074b2b35cdee68fa5912f94",
    "ea8b47970b02b305da1770a0e81ed82158629761"
};
template<>
std::vector<std::string> digests<Circle_polygon_set> = {
    "582989c788e58d287145f5a4ea4b7b1eeede7a5e",
    "76a145908981e5f6575bcd12ae2d43274011bdde",
    "f0d420667912c4f5dc200dd9b4bb5195e76983d9",
    "0361717006c37fd868535b23409e739e892519ab",
    "99333db395b7146bdd25f55e122d051e0244d189"
};

template<>
auto generate_operations<Polygon_set>()
{
    return JOIN(
        DIFFERENCE(RECTANGLE(4, 4), RECTANGLE(2, 2)),
        RECTANGLE(1, 1));
};

template<>
auto generate_operations<Circle_polygon_set>()
{
    return JOIN(
        DIFFERENCE(CIRCLE(4), CIRCLE(2)),
        CIRCULAR_SECTOR(1, 270));
};

// We should ideally compare the loaded results with those that were
// stored for identical geometry, but we take the shortcut of
// comparing a measure of both geometries (either area or volume),
// mainly because it's easier and not particularly risky.

template<typename T>
static FT measure_result(const std::shared_ptr<T> &p)
{
    if constexpr (std::is_same_v<T, Polyhedron>
                  || std::is_same_v<T, Nef_polyhedron>
                  || std::is_same_v<T, Surface_mesh>) {
        return polyhedron_volume(*p);
    } else {
        static_assert(std::is_same_v<T, Polygon_set>
                      || std::is_same_v<T, Circle_polygon_set>);
        return polygon_area(*p);
    }
}

BOOST_FIXTURE_TEST_SUITE(store, Evaluation_fixture)

BOOST_TEST_DECORATOR(* boost::unit_test::tolerance(0.01))
BOOST_AUTO_TEST_CASE_TEMPLATE(polyhedron, T, result_types)
{
    // We set the store threshold to zero, to ensure that are
    // operations get store regardless of their evaluation time.

    push(Options::store_threshold, 0);

    // We should perform two sets of tests: one with compression
    // enabled and one without.

    for (auto [k, x]: {std::pair(-1, ".o"), std::pair(6, ".zo")}) {
        push(Options::store_compression, k);

        FT A;

        // This is the expected filename the stored operation
        // (post-conversion, if any) should end up in.

        const std::string n = digests<T>[0] + x;

        {
            // Enable storing and perform the first evaluation.

            push(Flags::store_operations, 1);

            const auto &result = evaluate(generate_operations<T>());

            evaluate_operations();

            // Test that the operation was stored.

            BOOST_TEST(result.annotations.at("stored") == n);
            A = measure_result(result.value);

            {
                std::fstream f(n);
                BOOST_TEST_REQUIRE(f.good());
            }

            pop(Flags::store_operations);
        }

        // Enable loading and perform the second evaluation.  Here
        // we expect the operations to be loaded from the
        // previously stored files.

        {
            push(Flags::load_operations, 1);

            {
                {
                    const auto &result = evaluate(generate_operations<T>());

                    evaluate_operations();

                    // Test the loaded polyhedra.

                    BOOST_TEST(result.annotations.at("loaded") == n);
                    BOOST_TEST(measure_result(result.value) == A);
                }

                // In order to test load failure, we now manually corrupt
                // the stored file and reevaluate the operations.  We now
                // expect them to fail.

                std::filesystem::path q(n);
                std::filesystem::resize_file(q, std::filesystem::file_size(q) / 2);

                {
                    const auto &result = evaluate(generate_operations<T>());

                    evaluate_operations();

                    BOOST_TEST(result.value == nullptr);
                }
            }

            for (const auto &y: digests<T>) {
                std::remove((y + x).c_str());
            }

            pop(Flags::load_operations);
        }

        pop(Options::store_compression);
    }

    pop(Options::store_threshold);
}

BOOST_AUTO_TEST_SUITE_END()
