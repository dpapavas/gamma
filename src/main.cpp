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

#include <iostream>
#include <CGAL/assertions.h>

#include "options.h"

int main(int argc, char *argv[])
{
    // Instruct CGAL to throw exceptions, so we can handle them as
    // we please.

    CGAL::set_error_behaviour(CGAL::THROW_EXCEPTION);
    CGAL::set_warning_behaviour(CGAL::THROW_EXCEPTION);

    // Parse the command line.

    return parse_options(argc, argv) < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}
