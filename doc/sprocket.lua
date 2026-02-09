local table = require "table"
local math = require "gamma.math"
local transformation = require "gamma.transformation"
local polygons = require "gamma.polygons"
local operations = require "gamma.operations"

draft = draft or false
set_curve_tolerance(draft and 0.05 or 0.001)

d_1 = d_1 or 5              --  Maximum roller diameter (mm)
p = p or 8                  --  pitch (mm)
z = z or 17                 --  No of sprocket teeth
b_f1 = b_f1 or 2.8          --  Sprocket width

-- Dimensions calculated from the preceding parameters.

d = p / math.sin(180 / z)   -- Pitch-circle diameter
d_a =                       -- Tip diameter
   (d - d_1 + 0.5 * (1.25 + 1 - 1.6 / z) * p)
r_e = 0.12 * d_1 * (z + 2)  -- Tooth flank radius
r_i = 0.505 * d_1           -- Roller seating radius
alpha = 140 - 90 / z        -- Roller seating angle

function place_tooth(part, r, c, n)
   return transformation.rotation(-360 * n / z)
      * transformation.translation(0, d / 2)
      * transformation.rotation(c * alpha)
      * transformation.translation(0, -r)
      * part
end

-- The central plate with roller seats cut out.

points = {}
for t = -1/2, 1/2 do
   for s = 1, z do
      table.insert(
         points, place_tooth(point(0, 0), r_i, t, s))
   end
end

sprocket = operations.hull(table.unpack(points))
for s = 1, z do
   sprocket =
      sprocket - place_tooth(polygons.circle(r_i), 0, 0, s)
end

output(1, sprocket)

-- Tooth flanks, extending above the roller seats.

for s = 1, z do
   sprocket = sprocket + output(
      3, (place_tooth(
             polygons.circle(r_e), r_i + r_e, 0.5, s)
          * place_tooth(
             polygons.circle(r_e), r_i + r_e, -0.5, s + 1)
          * polygons.circle(d_a / 2)))
end

output(
   "sprocket", operations.linear_extrusion(sprocket, b_f1))
