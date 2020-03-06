--
-- Created by IntelliJ IDEA.
-- User: dc
-- Date: 2020-03-04
-- Time: 12:29 a.m.
-- To change this template use File | Settings | File Templates.
--

local Reporter      = import('sys/reporter')
TestCase, Fixture   = import('sys/testcase')

local Testit = setmetatable({
    applyFilters = function(this, logger, files, filters)
        local function includeFile(file)
            for _,filter in ipairs(filters) do
                if file:find(filter.expr) then
                    if filter.ex then return false,filter.expr else return true end
                end
            end
            return #filters == 0
        end

        local output = {}
        for _, file in ipairs(files) do
            local ok,filter = includeFile(file)
            if ok then
                output[#output+1] = file
            else
                logger:dbg("test file '%s' filtered out by filter ~%s", file, (filter or ''))
            end
        end
        return output
    end,

    resolveFilters = function(this, logger, filters)
        assert(filters and type(filters) == 'table', "invalid filter list provided")
        local output = {}
        for i,filter in ipairs(filters) do
            if type(filter) ~= 'string' or not filter then
                logger:err("ignoring filter #%d is invalid, it has invalid format", i)
            else
                local ex = filter:byte(1) == string.byte('~', 1)
                if ex then
                    filter = filter:sub(2)
                end
                output[#output+1] = {ex = ex, expr = filter}
            end
        end
        return output
    end
}, {
    __call = function(this, config)
        config =  config or {}
        applyDefaults(config, Swept.Config)

        if not pathExists(config.root) then
            Log:err("tests directory '"..config.root.."' does not exist")
            return false
        end
        local files = _find(config.root, '-name', "'test_*.lua'"):s():split('\r\n')
        if #files == 0 then
            Log:wrn("found 0 test cases in directory '"..config.root.."'")
            return false
        end
        local filters = this:resolveFilters(Log, config.filters)
        local enabledFiles = files
        if #filters > 0 then enabledFiles = this:applyFilters(Log, files, filters) end
        if #enabledFiles == 0 then
            Log:wrn("all test files filtered out, nothing to execute")
            return false
        end

        local Report = Reporter.Fanout({
            json = Reporter.Structured(Reporter.Json),
            default = Reporter.Console
        })

        Log:inf("proceeding with %s test files", #enabledFiles)
        for _,testFile in pairs(enabledFiles) do
            local ctx = setmetatable({ reporter = Report, logger = Log }, {
                __index = function (self, key)
                    if self.reporter[key] then
                        return self.reporter[key]
                    elseif self.logger[key] then
                        return self.logger[key]
                    else
                        return nil
                    end
                end,
                __call = function(_ctx, tag, fmt, ...)
                    _ctx.reporter:message(tag, fmt, ...)
                end
            })

            -- start running collection
            Report:startCollection(testFile)
            local func,msg = loadfile(testFile, 't', _ENV)
            if not func then
                -- loading test cases failed
                Log:err("loading test file '%s' failed - %s", testFile, msg)
                Report:failCollection("loading test file '%s' failed - %s", testFile, msg)
            else
                local suite, msg = func()
                if not suite then
                    -- loading test suites failed
                    Log:err("loading test suites failed - %s", msg or 'no returned fixture')
                    Report:failCollection("loading test suites failed - %s", msg or 'no returned fixture')
                else
                    Report:update(suite._name, suite._descr)
                    local ok, msg = pcall(function(_ctx) suite:_exec(_ctx) end, ctx)
                    if not ok then
                        -- unhandled error when executing suite
                        Log:err("unhandled error - %s", msg)
                        Report:failCollection("unhandled error - %s", msg)
                    else
                        Report:endCollection()
                    end
                end
            end
        end
        local path = (config.resdir or Dirs.RESULTS)..'/'..config.filename
        Report:finalize(path)
    end
})

return function() return Testit end