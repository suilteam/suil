--
-- Created by IntelliJ IDEA.
-- User: dc
-- Date: 2020-03-04
-- Time: 12:29 a.m.
-- To change this template use File | Settings | File Templates.
--

include("sys/sh")
include("sys/utils")
local Json = import("sys/json")

local parser = import("sys/argparse") {
    name = arg[0],
    description = "Swept is the a single file testing framework/runtime used for executing swept tests written in LUA"
}

parser:option("-r --root", "The root directory to scan for swept test cases", nil)
parser:option("-f --filters", "A list of test script filters in regex format, prefix with '-' to exclude script", nil)
      :args("*")
      :count("*")


local configPath = pwd():s()..'/.config'
local config = {}
if pathExists(configPath) then
    config = Json:decode(cat(configPath):s())
end

local parsed = parser:parse()
for k,v in pairs(parsed) do
    if v ~= nil then
        if type(v) ~= 'table' or #v > 0 then
            config[k] = v
        end
    end
end

return function() return config end