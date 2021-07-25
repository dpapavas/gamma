local core = require "gamma.selection.core"
local operations = require "gamma.operations.core"

function core.vertices_not_in(volume)
   return core.vertices_in(operations.complement(volume))
end

return core
