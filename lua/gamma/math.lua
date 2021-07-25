local luamath = require "math"

math = {}
for k, v in pairs(luamath) do
   math[k] = v
end

function math.cos(x)
   return luamath.cos(luamath.rad(x))
end

function math.sin(x)
   return luamath.sin(luamath.rad(x))
end

function math.tan(x)
   return luamath.tan(luamath.rad(x))
end

function math.acos(x)
   return luamath.deg(luamath.acos(x))
end

function math.asin(x)
   return luamath.deg(luamath.asin(x))
end

function math.atan(y, x)
   return luamath.deg(luamath.atan(y, x))
end

return math
