local table = require "table"
local transformation = require "gamma.transformation"
local polygons = require "gamma.polygons"
local volumes = require "gamma.volumes"
local selection = require "gamma.selection"
local operations = require "gamma.operations"

draft = draft or false
remesh_target = draft and 1 or 0.5
remesh_iterations = draft and 1 or 2
fairing_continuity = 1

set_curve_tolerance(draft and 0.02 or 0.01)

function make_fitting(radius, height, width, ...)
   local args = {...}

   -- Distribute the result of applying f on the end
   -- shapes radially.

   local function place(f)
      local placed = {}
      for i = 1, #args / 2 do
         table.insert(
            placed,
            transformation.rotation(args[2 * i], 1)
            * transformation.translation(0, 0, radius)
            * f(args[2 * i - 1]))
      end

      return table.unpack(placed)
   end

   -- Use the placement function to take the hull of the
   -- unextruded bases and add the extruded ends.

   local function solid_with_offset(delta)
      local function extrude_to(h, shape)
         return operations.linear_extrusion(
            operations.offset(shape, -delta), 0, h)
      end

      return operations.union(
         operations.hull(
            place(function(_) return extrude_to(0, _) end)),
         place(function(_) return extrude_to(height, _) end))
   end

   -- Take the difference of two solid versions to produce the
   -- hollow part.

   local unfaired = output(
      1, solid_with_offset(0) - solid_with_offset(width))

   -- Remesh, keeping sharp edges constrained.

   local remeshed = output(
      3, operations.remesh(
         unfaired, selection.edges_by_sharpness_angle(90),
         remesh_target, remesh_iterations))

   -- Fair the central portion.

   return operations.fair(
      remeshed,
      selection.vertices_in(
         operations.intersection(
            place(
               function(_)
                  return volumes.bounding_halfspace(
                     0, 0, 1, 0)
               end))),
      fairing_continuity)
end

output(
   "fitting",
   make_fitting(
      5, 3, 1,
      polygons.circle(4), -45,
      polygons.circle(4), 45,
      polygons.circle(4), 180))
