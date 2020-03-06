---
--- Generated by EmmyLua(https://github.com/EmmyLua)
--- Created by dc.
--- DateTime: 2020-02-29 1:46 p.m.
---
Swept._loaded = {}
function include(args)
    function loader(name)
        name = name:gsub('/', '_')
        name = 'swept_'..name
        if not Swept._loaded[name] then
            Swept._loaded[name] = Swept:load(name)
        end
        return Swept._loaded[name]
    end

    if type(args) == 'string' then
        return loader(args)
    elseif type(args) == 'table' then
        local rets = {}
        for _,v in ipairs(args) do
            rets[#rets + 1] = loader(v)
        end
        return table.unpack(rets)
    else
        error("un-supported parameter of type '"..type(args).."'")
    end
end

function import(name)
    if type(name) ~= 'string' then
        error("invalid import parameter type, only string's allowed")
    end
    local func = include(name)
    return func()
end

-- loads command line arguments or configuration
local function Start()
    local config = import("init")
    -- loads and executes the test runner
    import("sys/sweeper") (config)
end

-- base temporary directory
TEMPDIR='/tmp/swept'
--- list of temporary directory used as defualts
Dirs = {
    SHELL   = 'sh',
    DATA    = 'data',
    RESULTS = 'res',
    LOGS    = 'logs'
}
-- default test filter
FILTER = {"~DISABLE", ".*"}

for name,dir in pairs(Dirs) do
    Dirs[name] = TEMPDIR..'/'..dir
    os.execute('mkdir -p '..Dirs[name])
end

-- start execution
local ok,msg = pcall(Start)
os.execute('rm -rf '..Dirs.SHELL..'/*')

-- finish
if not ok then
    io.stderr:write(msg..'\n')
    io.stderr:write(debug.traceback()..'\n')
    Swept:exit(1)
end