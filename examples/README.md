This directory contains programs initially written with the intent of testing
and driving the design of Gamma and later adapted for use as examples.

Each program defines a number of "outputs", essentially tags associated with
geometry produced at various stages along the computation.  Some of these
outputs produce the final geometry and will typically be used to save these
results to file, but some may represent intermediate stages and facilitate
inspection and debugging during development (but can also help other people who
are trying to understand the code).  Some outputs can also define geometry that
is entirely auxiliary in nature, e.g. an assembly of several mechanical parts,
that will never be used as such, but that can nevertheless help during design or
validation of the separate (and, again, help others understand what they are).

Available outputs can be found by inspecting the code, but an easier way is to
use the `-Woutputs` option.  This option enables warnings about unused outputs,
so that set on its own, it will essentially list them all.  For example, the
following invocation from within the `examples` directory, should list all
outputs of the `funnel.lua` example:

```bash
$ gamma -Woutputs funnel.lua
```

Each of these outputs can be selected with the `-o/--output` option.  To save
the output to file, specify the output name, with the extension corresponding to
the desired output format (e.g. `-o funnel.stl` to write the `funnel` output to
the file `funnel.stl` in the current directory in STL format.  This is useful
for production use, but not very convenient during development.  Another option,
more suitable to interactive use, is to use
[Geomview](http://www.geomview.org/), assuming it is available on your platform.
It can be used to inspect any output and one convenient way to do it, is to
start it as follows:

```bash
$ geomview -nopanels -Mc output -wpos 800,600 -wins 0
```

This will start Geomview with a predefined window size, but it will iniially
wait in the background for incoming geometry.  Then, any output can be built and
inspected by invoking Gamma with, for instance:

```bash
$ gamma -o:assembly funnel.lua
```

Note the colon before the output name (here `assembly` for the funnel-gasket
assembly) and the absence of a suffix.  This requests "debug" output (for now
implemented via Geomview) to the default Geomview pipe (named `output`, which
matches the argument to the `-Mc` option to Geomview above).  You can rotate and
move the part, or change the way it's displayed with the proper key bindings.
(Consult Geomview's manual for more details.  Also, if you alternatively prefer
a GUI, remove the `-nopanels` switch above.)  When you're done, simply close the
window and Geomview will stay running in the background, waiting for the next
build.

Apart from any predefined outputs, you can of course also inspect any other part
of your computation.  In Scheme programs this is particularly simple: you only
need to place a `?` inside an expression resulting in polyhedral geometry.  For
instance you can add the `?`, as shown below, and then use the `-o :` output
option to inspect the result of the `difference` operation:

```Scheme
(import (gamma polyhedra) (gamma operations))

(with-curve-tolerance 1/50
  (? difference
   (cuboid 10 10 10)
   (sphere 7)))
```

You can then simply remove the `?`, when you've lost interest in that particular
intermediate result.  In Lua programs, the same can be done rather less
convenient by inserting an inline call to `output` in the code, as shown below:

```Lua
polyhedra = require "gamma.polyhedra"
operations = require "gamma.operations"

set_curve_tolerance(0.02)

hollow_cube = output(
   operations.difference(
      polyhedra.cuboid(10, 10, 10),
      polyhedra.sphere(7)
   )
)
```

As used here, `output` selects its argument for output as with `?` above and
also returns it, so that you can wrap anything of interest with it temporarily.

Finally, another option that may be useful when running programs with Gamma, is
`--dump-operations`.  This dumps the operations that are carried out internally
during the course of the computation to a file, but you can used it as
`--dump-operations=-` to print them to the standard output.  This can serve to
provide some feedback on the progress of the execution and also some insight
into its inner workings.
