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

#include <cstring>
#include <iostream>
#include <sanitizer/lsan_interface.h>
#include <CGAL/assertions.h>

#include "options.h"
#include "evaluation.h"
#include "lua_frontend.h"
#include "scheme_frontend.h"

#ifdef __SANITIZE_ADDRESS__
extern "C" const char *__lsan_default_suppressions() {
    return (
        // These allocations are made by Guile and can't be detected
        // properly by LSan (although Valgrind detects they're still
        // reachable at program exit.

        "leak:iconv_open\n"
        "leak:allocate_code_arena\n");
}
#endif

// Document: program

// ## The Main Function

// Although this is the program entry point, not much is going on
// here.

int main(int argc, char *argv[])
{
    // When we're not run from our installed location (meaning most
    // likely that we're running straight from the build directory
    // during development) we want to:

    if (std::strncmp(INSTALL_PREFIX, argv[0], sizeof(INSTALL_PREFIX) - 1)) {
        //   1. instruct Guile to freshly compile all files and

        setenv("GUILE_AUTO_COMPILE", "fresh", 1);

        //   2. add library paths to load libraries straight from the
        //   sources.

#ifdef HAVE_SCHEME
        Options::library_directories.push_front(PROJECT_SOURCE_DIR "/scheme");
#endif

#ifdef HAVE_LUA
        Options::library_directories.push_front(PROJECT_SOURCE_DIR "/lua");
#endif
    } else {
        // Conversely, when running from the installed location, we
        // add the paths for the istalled libraries.

#ifdef HAVE_SCHEME
        Options::library_directories.push_front(INSTALL_DATADIR "/gamma/scheme");
#endif

#ifdef HAVE_LUA
        Options::library_directories.push_front(INSTALL_DATADIR "/gamma/lua");
#endif
    }

    // In any case, we also instruct CGAL to throw exceptions, so we
    // can handle them properly.

    CGAL::set_error_behaviour(CGAL::THROW_EXCEPTION);
    CGAL::set_warning_behaviour(CGAL::THROW_EXCEPTION);

    // Now we only need to parse the command line.  Everything follows
    // from there.

    if (parse_options(argc, argv) >= 0) {
#ifdef HAVE_LUA
        close_lua();
#endif

#ifdef HAVE_SCHEME
        close_scheme();
#endif

        evaluate_operations();

        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
}
