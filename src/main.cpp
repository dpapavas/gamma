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

// --- manual

// # Introduction

// Gamma is a CAD modeller posing as a compiler collection.  It
// produces 3D or 2D objects from programs written in one of its
// supported languages^[These currently include Scheme and Lua.].
// Being based on CGAL, the Computational Geometry Algorithms Library,
// it can generate geometric models that are accurate and free of
// defects using a wide range of techniques, such as CSG modeling,
// generalized 2D to 3D extrusion, subdivision methods or mesh-based
// techniques, such as surface fairing and deformation.

// Gamma can be used like a traditional compiler, by invoking it on
// the command line, to "compile" geometry out of a program composed
// using an editor, or it can be used as a graphical application
// through Gamma Debugger, its accompanying geometry inspection tool.

// Gamma is freely redistributable software. You may redistribute it
// and/or modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation. Ref: GNU General
// Public License.

// This manual provides comprehensive documentation on how to install,
// run and use Gamma and the Gamma Debugger.  It is covered by the GNU
// Free Documentation License. Ref: GNU Free Documentation License.

// ## Running Gamma

// The simplest way to use Gamma, is by invoking it much like one
// would a compiler on the command line^[In fact the command line
// interface was designed to resemble that of the GNU Compiler
// Collection.].  For example, if a file called @file{cube.scm} exists in
// the current directory, containing the Scheme source code shown in
// ref:rounded-cube-source, you can then produce a model of a cube
// with rounded edges and save it in a file named `cube.stl`, in STL
// format, by running Gamma as follows:

// ```
// gamma -o cube.stl cube.scm
// ```

// Figure: rounded-cube-source
// ```
// (import (gamma polyhedra) (gamma operations))
// (set-curve-tolerance! 1/1000)
//
// (define-output cube
//   (intersection (cuboid 1 1 1) (sphere 2/3)))
// ```
//   A simple Scheme program that produces the intersection of a cube
//   with a sphere, i.e. a cube with its corners rounded off.


// If we then view the result in a model viewer, we should see the
// object shown in ref:rounded-cube.

// Figure: rounded-cube
// ```print;;rx:45,ry:35
// (import (gamma polyhedra) (gamma operations))
// (set-curve-tolerance! 1/1000)
//
// #>out1 (intersection (cuboid 1 1 1) (sphere 2/3))
// #>out2 (intersection (cuboid 1 1 1) (sphere 2/3))
// ```
//   Our first simple model.

// --- manual

// --- program

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
