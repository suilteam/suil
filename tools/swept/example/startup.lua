--
-- Created by IntelliJ IDEA.
-- User: dc
-- Date: 2020-03-05
-- Time: 12:16 a.m.
-- To change this template use File | Settings | File Templates.
--
local function Parse(p)
    p:flag('--dump', "A flag used to enable/disable dumping", false)
end

local function Init(argv)
    if argv.verbose then
        Log:dbg("verbose argument provided")
    end
    return {name = id({u  = true, n = true}):s(), id = id({u = true}):s()}
end

return function() return Parse, Init end