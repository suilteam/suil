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
                if file:find(filter.expr.F) then
                    if filter.ex then return false,filter.expr else return true, filter.expr end
                end
            end
            return #filters == 0
        end

        local output = {}
        for _, file in ipairs(files) do
            local ok,expr = includeFile(file)
            if ok then
                output[#output+1] = {file = file, T = expr.T}
            else
                logger:dbg("test file '%s' filtered out by filter ~%s",
                        file, (expr  and expr.F or ''))
            end
        end
        return output
    end,

    resolveFilters = function(this, logger, filters)
        assert(filters and type(filters) == 'table', "invalid filter list provided")
        local output = {}
        local hasInclusive = false
        for i,filter in ipairs(filters) do
            if type(filter) ~= 'string' or not filter then
                logger:err("ignoring filter #%d (%s) is invalid, it has invalid format", i, tostring(filter or ''))
            else
                local ex = filter:byte(1) == string.byte('~', 1)
                if ex then
                    filter = filter:sub(2)
                end
                hasInclusive = hasInclusive or not(ex)
                local parts = filter:split(':')
                assert(#parts < 3, "invalid filter, can be file filter or with a test filter")
                local expr = {F = parts[1]}
                if parts[2] then expr.T = parts[2] end
                output[#output+1] = {ex = ex, expr = expr}
            end
        end
        if not hasInclusive then
            -- this is very important, if all filters are exclusive and a file
            -- does not match any of them, this will ensure the file is included
            output[#output + 1] = {expr = {F = '.*'}}
        end
        return output
    end
}, {
    __call = function(this)
        if not pathExists(Swept.Config.root) then
            Log:err("tests directory '"..Swept.Config.root.."' does not exist")
            return false
        end
        local files = _find(Swept.Config.root, '-name', "'test_*.lua'"):s():split('\r\n')
        if #files == 0 then
            Log:wrn("found 0 test cases in directory '"..Swept.Config.root.."'")
            return false
        end
        local filters = this:resolveFilters(Log, Swept.Config.filters)
        local enabledFiles = files
        local filesTable = false
        if #filters > 0 then
            filesTable = true
            enabledFiles = this:applyFilters(Log, files, filters)

        end
        if #enabledFiles == 0 then
            Log:wrn("all test files filtered out, nothing to execute")
            return false
        end


        local Report = Reporter.Fanout({
            junit   = Reporter.Structured(Reporter.JUnit),
            default = Reporter.Console
        })

        Log:inf("proceeding with %s test files", #enabledFiles)
        for _,value in pairs(enabledFiles) do
            local testFile = filesTable and value.file or value
            local testFilter = filesTable and value.T or nil

            local short_path = testFile:gsub(Swept.Config.root, '${ROOT}')
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
            Report:startCollection(short_path)
            local func,msg = loadfile(testFile, 't', _ENV)
            if not func then
                -- loading test cases failed
                Log:err("loading test file '%s' failed - %s", short_path, msg)
                Report:failCollection("loading test file '%s' failed - %s", short_path, msg)
            else
                local suite, msg = func()
                if not suite then
                    -- loading test suites failed
                    Log:err("loading test suites failed - %s", msg or 'no returned fixture')
                    Report:failCollection("loading test suites failed - %s", msg or 'no returned fixture')
                else
                    Report:update(suite._name, suite._descr)
                    local ok, msg = pcall(function(_ctx) suite:_exec(_ctx, testFilter) end, ctx)
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
        local path = (Swept.Config.resdir or Dirs.RESULTS)..'/'..Swept.Config.filename
        Log:dbg("Finalizing log reports to %s", path)
        return Report:finalize(path)
    end
})

return function() return Testit end