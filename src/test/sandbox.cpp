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
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <chrono>

#include "options.h"
#include "kernel.h"
#include "polyhedron_types.h"
#include "polyhedron_tests.h"
#include "projection.h"
#include "macros.h"
#include "transformations.h"

#include <CGAL/draw_polyhedron.h>

BOOST_AUTO_TEST_SUITE(sandbox, * boost::unit_test::disabled())

BOOST_AUTO_TEST_CASE(a)
{
}

BOOST_AUTO_TEST_CASE(benchmark)
{
    long unsigned int ns = 0;

    for (int i = 0; i < 1'000'000; i++) {
        auto t_0 = std::chrono::high_resolution_clock::now();

        auto t_1 = std::chrono::high_resolution_clock::now();

        ns += std::chrono::duration_cast<std::chrono::nanoseconds>(
            t_1 - t_0).count();
    }

    std::cout << ns / 1e6 << "ns" << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()
