--
-- Created by IntelliJ IDEA.
-- User: dc
-- Date: 2020-03-04
-- Time: 12:29 a.m.
-- To change this template use File | Settings | File Templates.
--

include("sys/sh")
include("sys/utils")
local Json   = import("sys/json")
local Logger,Exts =import('sys/logger')

-- initialize a default global system logger
local fanout = Exts.Fanout({
    consoleLogger = Exts.Console {
        level = Logger.INFO
    }
})

Log = Logger {
    sink = fanout
}

local parser = import("sys/argparse") {
    name = arg[0],
    description = "Swept is the a single file testing framework/runtime used for executing swept tests written in LUA"
}

parser:option("-r --root", "The root directory to scan for swept test cases", nil)
parser:option("--logdir",  "The base directory to save all log files", nil)
parser:option("-f --filters", "A list of test script filters in regex format, prefix with '-' to exclude script", nil)
      :args("*")
      :count("*")

RUNDIR=pwd():s()
Log:inf("swept started from directory %s", RUNDIR)

local startupPath = RUNDIR..'/startup.lua'
local Init, Parse

if pathExists(startupPath) then
    Log:inf("loading startup scripit %s", startupPath)
    Parse, Init = require('startup')()
    if Parse then
        Log:inf("invoking command line parser extension returned by startup script")
        Parse(parser)
    end
end

-- parse and dump command line arguments
local configPath = RUNDIR..'/.config'
Swept.Config = {}
if pathExists(configPath) then
    Log:inf("loading configuration file %s", configPath)
    Swept.Config = Json:decode(cat(configPath):s())
end

Log:inf("parsing command line arguments")
local parsed = parser:parse()

for k,v in pairs(parsed) do
    if v ~= nil then
        if type(v) ~= 'table' or #v > 0 then
            Swept.Config[k] = v
        end
    end
end

if parsed.dump then
    -- only dump arguments if dumping tables is allowed
    tprint(Swept.Config)
end

-- Invoke init function is created
if Init ~= nil then
    Log:inf("startup script return an init function, invoking the function with command line arguments")
    Swept.Data = Init(parsed)
    if parsed.dump and type(Swept.Data) == 'table' then
        Log:inf("init function returned data")
        tprint(Swept.Data)
    else
        Log:inf("init returned %s", S(Swept.Data))
    end
end

-- Initialize log file is enable
local logDir = Swept.Config.logdir or "/tmp/swept/logs"
Log.sink:add('fileLogger', Exts.File{dir = logDir, prefix = Swept.Config.prefix})

return function() return Swept.Config end