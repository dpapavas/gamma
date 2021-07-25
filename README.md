# Gamma

Gamma can perhaps best be described as a compiler collection for computational
geometry.  Although in terms of construction it is not really a compiler, its
function nevertheless is to transform code, written in one of the supported
languages, into geometry, which can be output in one of the supported formats.
Currently, it supports two language frontends, Scheme (though
[Chibi-Scheme](https://github.com/ashinn/chibi-scheme)) and
[Lua](https://www.lua.org/) and a rich set of operations on both polygons and
polyhedra, including:

* simple primitive generation (polyhedra such as spheres and boxes, as well as
  linear, circular and elliptic polygons),
* exact boolean operations on both polygons and polyhedra,
* generalized polygon extrusion,
* other geometric operations, such as Minkowski sums, convex hulls, polygon
  offsetting,
* mesh-oriented operations, such as remeshing, refinement, surface fairing and
* deformation operations.

Gamma was written with solid modeling for CAD/CAM applications in mind, but,
being essentially a wrapper around a subset of [CGAL](https://www.cgal.org/), it
might well be adaptable to other uses.  Computations are generally exact and
robust and although this can take its toll on execution speed, Gamma
nevertheless strives for interactive use.  It tries to achieve that through
optimization at the language level (e.g. common subexpression elimination, dead
code elimination, expression rewriting, etc.), at the execution level,
(e.g. multi-threaded evaluation), and perhaps most importantly, by caching
intermediate results, so that only the parts of the computation that have
changed since the last execution need to be reevaluated.

Gamma is currently stable and, although not yet complete, it can already be used
productively.  Perhaps the most prominent missing feature, is a graphical
inspector that can view the geometry as it is being developed (although there is
currently some support for this through [Geomview](http://www.geomview.org/), on
platforms where it is available).  Apart from that, there's a host of useful
geometric operations available in [CGAL](https://www.cgal.org/), which are not
yet supported, but will be added in due time.

Which brings us to another missing piece...

## Documentation

Well, there isn't yet any.  Adventurous souls, who want to try out Gamma, might
find some inspiration and guidance in the study of the code in the
[examples](examples/) directory as well as the [Orb
trackball](https://github.com/dpapavas/orb-trackball), a more complex design.
I've tried to document these sources extensively, but Gamma is quite complex, so
this is certainly no substitute.  If you'd like to use Gamma, and are frustrated
by the lack of documentation, open an issue to let me know (or post in an
existing issue on the subject, if there is one).  Writing documentation is not
much fun at the best of times and knowing it will be of use to others can
certainly help.

## Building

Gamma should, in theory, be buildable on multiple platforms, but it is currently
developed solely on Linux, so detailed instructions exist for that platform
only.  If you can build it for other platforms, please consider creating a PR,
with build instrucitons and any other necessary changes.

### Linux

Apart from the from the [GCC](https://gcc.gnu.org/) compiler, with support for
the C++ language, you'll also need [CMake](https://cmake.org/) and
[Git](https://git-scm.com/). Use your distribution's package system to install
them, then get Gamma's latest source via Git and prepare the build directory.

```bash
$ git clone https://github.com/dpapavas/gamma.git
$ mkdir gamma/build
$ cd gamma/build/
```

Gamma requires the following libraries, which should be installed via your
distribution's package system (you'll need the development packages of course):

* [GMP](https://gmplib.org/)
* [MPFR](https://www.mpfr.org/)
* [Boost](https://www.boost.org/)
* [zlib](https://zlib.net/)
* [Eigen](https://eigen.tuxfamily.org/)

In addition to that, you'll need [CGAL](https://github.com/CGAL/cgal) and,
although that might be available as a package as well, it would probably be a
better idea to check out the latest stable release via Git, or perhaps even the
master branch.

```bash
$ git clone --depth=1 https://github.com/CGAL/cgal.git
```

Depending on the language frontends you want to enable, you'll also need
[Chibi-Scheme](https://github.com/ashinn/chibi-scheme) and
[Lua](https://www.lua.org/ftp/).  These are optional and if one is not
available, the respective language frontend will be disabled.  Here too, we will
avoid system packages and instead build from source for static linking, as the
system packages might not have been built with the appropriate configuration.

Chibi-Scheme is typically in flux and the latest stable release can be quite
old, so it might be best to check it out from Git.  Either use the master
branch, or, if you prefer a bit less risk, you can use the master branch of the
fork below, which will hopefully be kept pointing to a usable snapshot of the
code.

```bash
$ git clone --depth=1 https://github.com/dpapavas/chibi-scheme.git

$ cd chibi-scheme
$ make clibs.c
$ make distclean
$ make libchibi-scheme.a SEXP_USE_DL=0 "CPPFLAGS=-DSEXP_USE_STATIC_LIBS -DSEXP_USE_STATIC_LIBS_NO_INCLUDE=0"
$ cd ..
```

To build Lua, get the latest release of the 5.4 branch and follow the
instructions below:

```bash
$ wget https://www.lua.org/ftp/lua-5.4.4.tar.gz
$ tar -zxf lua-5.4.4.tar.gz
$ cd lua-5.4.4/src/
$ make CC=g++ liblua.a
$ cd ../..
```

Finally, if all went according to plan, you should be able to configure and
build Gamma with:

```bash
$ cmake -DCMAKE_BUILD_TYPE=Release -DCGAL_ROOT=./cgal -DChibi_ROOT=./chibi-scheme -DLua_ROOT=./lua-5.4.4/src ..
$ make -j4
$ sudo make install
```

You can substitute a higher or lower number in `-j4` above, depending on the
number of cores and RAM available on your machine.  The final command, will
install Gamma in the default location (typically `/usr/local`).  Consult CMake's
documentation if you'd like to change this, or other aspects of the build.
