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

_,_,RUNDIR=_pwd_S()
Log:inf("swept started from directory %s", RUNDIR)
_,_,RUNDIRNAME=_basename_S(RUNDIR)
Log:inf("run directory name is %s", RUNDIRNAME)

parser:option("-r --root", "The root directory to scan for swept test cases", nil)
parser:option("--logdir",  "The directory to save all log files", nil)
parser:option("--resdir",  "The directory to save generated result files", nil)
parser:option("--prefix",  "The string to prefix all files (logs and results) generated for this run", nil)
parser:option("-f --filters", "A list of test script filters in regex format, prefix with '-' to exclude script", nil)
      :args("*")
      :count("*")

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
    Swept.Config = Json:decode(_cat(configPath):s())
end

Log:inf("parsing command line arguments")
local parsed = parser:parse()

-- merge config file with passed in command line arguments
for k,v in pairs(parsed) do
    if v ~= nil then
        if type(v) ~= 'table' or #v > 0 then
            Swept.Config[k] = v
        end
    end
end

-- Add defaults for missing arguments
applyDefaults(Swept.Config, {
    root = RUNDIR..'/tests',
    logdir = Dirs.LOGS,
    resdir = Dirs.RESULTS,
    prefix = RUNDIRNAME,
    filters = FILTER
})

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

-- Setup the filename
Swept.Config.filename = Swept.Config.prefix..'_'..os.date('%Y_%m_%d_%H_%M_%S.log')

-- Initialize log file is enable
Log.sink:add('fileLogger', Exts.File{
    dir = Swept.Config.logdir or Dirs.LOGS,
    fname = Swept.Config.filename..'.log'
})

return function() return Swept.Config end
