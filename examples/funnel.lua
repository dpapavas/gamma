-- This is a simple example of an espresso machine dosing funnel.  (It is
-- designed for the Gaggia classic portafilter, but should be easily adaptable
-- to other models.  If you decide to manufacture it, note that, while it works
-- quite well, the code was rewritten substantially prior to release, so it
-- might require, hopefully minor, adjustments.) To build the models for
-- production, you can invoke Gamma as follows:

-- $ gamma -o funnel.stl -o gasket.stl funnel.lua

local transformation = require 'gamma.transformation'
local volumes = require 'gamma.volumes'
local selection = require 'gamma.selection'
local polygons = require 'gamma.polygons'
local operations = require 'gamma.operations'

-- The variable `draft` is undefined and hence nil by default.  It can be
-- overridden through the command line though, by providing the `-Ddraft` option
-- when invoking Gamma, which defines it to `true`.  It can then be used in the
-- code, to adjust parameters so as to limit detail when it is set.  This is a
-- useful technique, which allows quick and dirty builds when exploring design
-- options, but it has its limitations. Care must be exercised, to make sure
-- that the draft geometry is representative of the production build and, in
-- some cases, tweaks will have to be done in full detail.

if draft then
   remesh_target = 3
   remesh_iterations = 1
   set_curve_tolerance(1e-1)
else
   remesh_target = 1
   remesh_iterations = 2
   set_curve_tolerance(1e-2)
end

-- These are design parameters for the main parts.  We define here for reference
-- and easy tweaking, both during development and for anyone who might want to
-- modify the parts, to adapt them to different use.  Note that to the latter
-- end, we could also make these parameters "build options", for instance by
-- setting:

-- R = radius or 29

-- Then, the inner funnel radius `R`, would default to 28mm, but could be
-- overridden, for instance with `-Dradius=30.5` for a 61mm portafilter.

R = 29                -- The inner funnel radius (which should match the
                      -- portafilter basket)

h_0 = 13              -- Height of the skirt (up to the basket mating surface)
h_1 = h_0 + 2         -- Height of the lower funnel cusp.
h_2 = h_1 + 13        -- Height of the funnel.

w_0 = 3               -- Width (thickness) of the skirt.
w_1 = 9               -- Overall width of the skirt section (including
                      -- portafilter cutout.

-- We use the simple method of taking the Minkowski sum of the section polygons
-- with an octagon to automatically chamfer the parts.  This is convenient, but
-- has the serious drawback of augmenting the dimensions of our input polygons.
-- This is of little consequence where the heights are concerned, as a mm more
-- or less won't make any difference, but this is not so for widths as we'd like
-- the portafilter to fit snugly into the funnel.  We therefore define
-- "adjusted" widths below, taking the chamfering into account, so that
-- diameters come out exactly right (meaning that the funnel opening diameter
-- should be `2 * R` (58mm, matching the filter basket), the inner skirt diameter
-- should be `2 * (R + w_1 - w_0)` (70mm, matching the =portafilter) and the
-- skirt width exactly `w_0` (which is less critical).

chamfer_length = 1
chamfer_kernel = polygons.regular(8, chamfer_length)

w_0adj = w_0 - 2 * chamfer_length
w_1adj = w_1 - 2 * chamfer_length

do
   -- This polygon defines the shape of the cross-section of the funnel.  We
   -- bring the left (inner) side flush with the coordinate axes (through
   -- `flush_east`), after chamfering through `minkowski_sum`.  When we then
   -- take the full angular extrusion of radius `R`, the funnel opening diameter
   -- will end up as `2 * R`.

   local section = (
      transformation.flush_east(
	 operations.minkowski_sum(
	    chamfer_kernel,
	    polygons.simple(
	       point(0, 0),
	       point(w_0adj, 0),
	       point(w_0adj, h_0),
	       point(w_1adj, h_0),
	       point(w_1adj, h_1),
	       point(-(h_2 - h_1) * 2/3, h_2),
	       point(0, h_1)
	    )
	 )
      )
   )

   -- This output of the cross-section with some thickness, can be useful either
   -- for inspection during development (use option `-o :funnel_section` to view
   -- it), or for export to other programs like Meshlab (use option `-o
   -- funnel_section.stl`), e.g. for validation through measurements.

   output(
      "funnel_section",
      operations.linear_extrusion(section, 1)
   )

   -- This is the basic funnel shape; a simple solid of revolution, derived from
   -- the section above.  It should work fine, but it looks a bit crude with all
   -- those sharp cusps and conic surfaces.  (You can view it with the `-o
   -- :unfaired_funnel` option.)

   -- We can "fair" our crude funnel, which, loosely speaking, means make it
   -- assume a "fairer", "more pleasant" shape, given certain constraints (after
   -- all, we want the geometry that interfaces with the portafilter to remain
   -- as designed).

   -- These constraints assume the form of a selection of vertices which are to
   -- be faired.  Anything that is not selected, will remain unaltered, while
   -- the vertices in the selected regions will be moved to assume a smoother
   -- shape.  Note that no extra geoemtry is generated during fairing; vertices
   -- are only moved.  The final result therefore, depends on the density of the
   -- triangulation of the mesh, that is to be faired, so if we want a smooth
   -- shape, we'll need to refine our mesh.  We can do that with `remesh`, but
   -- we first need to select the portion that is to be refined.

   -- There are various ways to select faces, vertices, or edges, the main being
   -- so-called "bounding volumes", which select all elements lying within them.
   -- These come in various shapes, that can be recombined with boolean
   -- operations and here we use two horizontal halfspaces and `faces_in`, to
   -- select the conical region of the funnel. In order to select the portion we
   -- want though, we first need to use corefine to insert vertices in the mesh
   -- at the lower edge of our intended selection, at height `h_0`.
   -- (Corefinement places vertices on one mesh, wherever it intersects another
   -- mesh, or a plane.)  The result can be inspected (thanks to
   -- `color-selection`) via the "unfaired_funnel" output.

   local coarse = operations.corefine(
      transformation.rotation(90, 0)
      * operations.angular_extrusion(section, -R, -180, 180),
      plane(0, 0, 1, -h_0)
   )

   local face_selection = (
      selection.faces_in(volumes.halfspace(0, 0, -1, h_1 - chamfer_length))
      - selection.faces_in(volumes.halfspace(0, 0, -1, h_2 - chamfer_length))
   )

   output(
      "unfaired_funnel",
      operations.color_selection(coarse, face_selection, 1)
   )

   -- Below, we remesh the selected faces so that the resulting triangulation is
   -- made up of triangles with sides of length `remesh_target`.  Naturally,
   -- lower is better, but also more costly.  We also select the vertices we
   -- want to fair, much like we did above.

   local remeshed = operations.remesh(
      coarse, face_selection, remesh_target, remesh_iterations
   )

   local vertex_selection = (
      selection.contract_selection(
	 selection.vertices_in(volumes.halfspace(0, 0, -1, h_1))
	 - selection.vertices_in(volumes.halfspace(0, 0, -1, h_2 - chamfer_length)),
	 0
      )
   )

   output(
      "remeshed_funnel",
      operations.color_selection(remeshed, vertex_selection, 1)
   )

   -- We now fair the funnel.  You can see the result via the "faired_funnel"
   -- output.  It is subtle, but arguably an improvement on our original mesh
   -- with its sharp edges.  The faired shape can be further adjusted, if
   -- desired, by altering the remeshed and faired regions.

   local faired = output(
      "faired_funnel",
      operations.fair(remeshed, vertex_selection, 1)
   )

   -- Finally, we use plain old boolean operations, to cut out slots for the
   -- portafilter tabs and our funnel is ready.

   funnel = output(
      "funnel",
      faired - (
         transformation.translation(0, 0, h_0 / 2 - 2 * chamfer_length)
         * transformation.rotation(90, 1)
         * operations.linear_extrusion(
            operations.minkowski_sum(
               chamfer_kernel, polygons.rectangle(h_0, 25 - 2 * chamfer_length)
            ),
            -2 * R, 2 * R
         )
      )
   )
end

-- For extra credit, we'll also design a rubber gasket that fits snugly around
-- the filter basket lips, providing a more secure fit and preventing coffee
-- grinds from escaping.  First, we define some parameters for the gasket.

d = 3.6               -- The gasket thickness
r = 5/2               -- The gasket cutout radius.

do
   -- We create our gasket much as before, only this time the section shape is a
   -- bit more complex, and we prefer to build it via boolean operations.  Note the
   -- various flush operations.  Polygons are usually defined with their center at
   -- the origin and it can be helpful to shift them so as to bring its left,
   -- right, top, or bottom side flush with the coordinate axes.  Also note the
   -- `hull` operation, which takes the convex hull of a circle and a few points,
   -- to form a rectangle with one rounded corner, which, when subtracted from the
   -- gasket, forms a rounded cutout, which should match the rounded lip of the
   -- filter basket.

   local section = transformation.flush_north(
      transformation.flush_east(
         transformation.flush_north(
            transformation.flush_west(
               polygons.rectangle(w_1, d)
            )
         )
         - (transformation.translation(w_0 + 0.6, -0.6)
            * transformation.flush_north(
               transformation.flush_west(
                  operations.hull(
                     polygons.circle(r),
                     point(-r, -10),
                     point(10, r),
                     point(10, -10)
                  )
               )
                                        )
           )
      )
   )

   output(
      "gasket_section",   
      operations.linear_extrusion(section, 1)
   )

   -- We extrude the section and subtract from it the finished funnel geometry, to
   -- make the final result fit snugly into the funnel, with locating tabs to
   -- facilitate placement and provide some more surface for gluing.

   gasket = output(
      "gasket",
      transformation.translation(0, 0, h_0 - chamfer_length)
      * transformation.rotation(90, 0)
      * operations.angular_extrusion(section, -R, -180, 180) - funnel
   )
end

-- Finally, we create an output of the assembly, to inspect the result for
-- proper fit.

output(
   "assembly",
   operations.color_selection(
      funnel,
      selection.faces_in(volumes.halfspace(0, 0, 0, 0)),
      1
   ),
   operations.color_selection(
      gasket,
      selection.faces_in(volumes.halfspace(0, 0, 0, 0)),
      2
   )
)
