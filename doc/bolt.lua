local table = require "table"
local math = require "gamma.math"
local transformation = require "gamma.transformation"
local polygons = require "gamma.polygons"
local polyhedra = require "gamma.polyhedra"
local operations = require "gamma.operations"

draft = draft or false

function thread(d, P, L)
   H = math.sqrt(3) / 2 * P     -- Height
   d_1 = d - 1.25 * H           -- Minor diameter
   n = draft and 20 or 50       -- No. of steps

   a = P / 16                   -- Coordinates of basic
   b = P * 3 / 8                -- profile vertices
   c = -H / 4
   d = H * 5 / 8

   profile = output(            -- The basic profile
      1, polygons.simple(
         point(b, c), point(b, 0), point(a, d),
         point(-a, d), point(-b, 0), point(-b, c)))

   -- The transformations making up the helical path.

   path = {}
   for s = 0, math.ceil(L / P) - 1 do
      for t = 0, n - 1 do
         u = t / n
         x = (transformation.rotation(90, 1)
              * transformation.rotation(360 * u, 0)
              * transformation.translation(
                 P * (s + u) - L, d_1 / 2, 0))

         if s == 0 then
            x = x *  transformation.scaling(u, u, 1)
         end

         table.insert(path, x)
      end
   end

   return (
      transformation.flush_bottom(
         polyhedra.prism(n, d_1 / 2, L))
      + output(
         2, operations.extrusion(
            profile, table.unpack(path))))
end

output(
   "bolt",
   operations.minkowski_sum(        -- The hexagonal head
      transformation.flush_top(polyhedra.prism(6, 7, 5)),
      polyhedra.octahedron(1, 1, 0.5))
   + thread(8, 1.25, 20))
