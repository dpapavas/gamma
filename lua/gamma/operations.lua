local math = require "math"
local table = require "table"
local core = require "gamma.operations.core"

local transformation = require "gamma.transformation"
local rotation = transformation.rotation
local translation = transformation.translation

local function angular_extrusion(base, r, a, b)
   if b == nil then
      return angular_extrusion(base, r, -a / 2, a / 2)
   end

   if a == b then
      return core.extrusion(base, rotation(a, 1) * translation(r, 0, 0))
   end

   local tau = set_curve_tolerance(nil)

   n = math.ceil(
      math.abs(b - a) / (math.acos(1 - tau / math.abs(r)) / math.pi * 360)
   )

   delta = (b - a) / n

   t = {}
   for i = 0, n - 1 do
      table.insert(t, rotation(a, 1) * translation(r, 0, 0))
      a = a + delta
   end

   table.insert(t, rotation(b, 1) * translation(r, 0, 0))

   return core.extrusion(base, table.unpack(t))
end

local function linear_extrusion(base, a, b)
   if b == nil then
      return linear_extrusion(base, -a / 2, a / 2)
   end

   if a == b then
      return core.extrusion(base, translation(0, 0, a))
   end

   return core.extrusion(base, translation(0, 0, a), translation(0, 0, b))
end

core.angular_extrusion = angular_extrusion
core.linear_extrusion = linear_extrusion

return core

