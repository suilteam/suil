local _,Sinks  = import('sys/logger')
local Json = import('sys/json')

local Console = Sinks.Console

local function status2str(s)
	if s == Test.Passed then return 'passed'
	elseif s == Test.Failed then return 'failed'
	elseif s == Test.Ignored then return 'skipped' end
	return 'error'
end

local StructuredReport = setmetatable({
	startCollection = function(this, path)
		assert(not(this.openCollection), "startCollection: another collection still running")
		-- create a new collection
		this.openCollection = {
			path = path,
			name = path,
			descr = nil,
			timestamp = timestamp(),
			tests = {},
			npassed = 0,
			nignored = 0,
			nfailed = 0,
			ndisabled = 0,
			nerrors = 0,
			id = #this.collections
		}
		if #this.collections == 0 then this.started = timestamp() end
	end,

	endCollection = function(this)
		local collection = this.openCollection
		assert(collection, "endCollection: no open collection to run")
		collection.duration = timestamp() - collection.timestamp
		-- add collection to a list of collections
		this.collections[#this.collections + 1] = collection
		this.openCollection = nil
		this.ended = timestamp()
	end,

	failCollection = function(this, fmt, ...)
		assert(this.openCollection,  "reporter not aware of any test file")
		if fmt and type(fmt) == 'string' then
			Console:red(fmt, ...)
		end
		if not this.openTest then
			this:startTestcase(this.openCollection.name, 'This shows that a collection failed outside test case')
		end

		this:endTestcase(Test.Failed, 'NotInTest', fmt:format(...), Assertions:reset())
		this:endCollection()
	end,

	startTestcase = function(this, name, descr)
		assert(this.openCollection,  "reporter not aware of any test file")
		assert(not(this.openTest), "startTestcase: another test is still running")
		this.openTest = {
			name = name,
			started = timestamp(),
			descr = descr or '',
			messages = {}
		}
	end,

	endTestcase = function(this, status, msg, stage, assertions)
		local test = this.openTest
		assert(test, "there is no test case currently running")
		test.duration = timestamp() - test.started
		test.status   = status
		test.stage    = stage
		test.msg      = msg
		test.assertions = assertions
		test.started  = nil
		-- update collection stats
		local collection = this.openCollection
		if status == Test.Ignored then collection.nignored = collection.nignored + 1
		elseif status == Test.Passed  then collection.npassed = collection.npassed + 1
		else collection.nfailed = collection.nfailed + 1 end
		-- set test message
		collection.tests[test.name] = test
		this.openTest = nil
	end,

	message = function(this, tag, fmt, ...)
		local test = this.openTest
		assert(test, "messages can only be appended when there is a running test")
		test.messages[#test.messages + 1] = ('%s: '..fmt):format((tag or 'run'), ...)
	end,

	_finalize = function(this)
		if this.report then
			return report
		end

		if this.openCollection then
			local msg = "finalizing an incomplete collection "..this.openCollection.path
			Log:err(msg)
			if this.openTest then this:message('run', msg) end
			this.ended = timestamp()
		end
		local collections = this.collections
		local report = {
			collections = collections,
			name      = this.name or Swept.Config.prefix,
			root      = Swept.Config.root,
			npassed   = 0,
			nfailed   = 0,
			nignored  = 0,
			nerrors   = 0,
			ndisabled = 0,
			duration  = (this.ended or 0) - (this.started or 0)}
		for _,v in ipairs(collections) do
			report.npassed = report.npassed + v.npassed
			report.nfailed = report.nfailed + v.nfailed
			report.nignored = report.nignored + v.nignored
		end
		this.report = report
		this.report.ntests = report.npassed + report.nfailed + report.nignored
		this.report.status = report.nfailed ~= 0 and Test.Failed or Test.Passed
		return this.report
	end,

	finalize = function(this, ...)
		return this:_finalize(), this:_finalize().status ~= Test.Failed
	end,

	update = function(this, name, descr)
		if this.openCollection then
			this.openCollection.name = name
			this.openCollection.descr = descr
		end
	end
}, {
	__call = function(this, attrs)
		if not attrs then attrs = {} end
		assert(type(attrs) == 'table', "StructuredReport only takes a table to extend or nothing")
		attrs.collections = {}
		-- generate a new structured report
		return setmetatable(attrs, {
			__index = this,
			-- finalize the report and return it
			__call = function(self, ...)
				return self:finalize(...)
			end
		})
	end,

	__newindex = constify('ConsoleReporter')
})

local JsonReporter  = {
	save = function(this, report, path)
		if not report or path == nil or #path == 0 then
			-- There is no report to save
			local msg = ("There is no report to save or path '%s' is empty"):format(path)
			Log:wrn(msg)
			return nil, msg
		end

		-- Encode the finalized report and save to disk
		local _,ok,dir = _dirname_S(path)
		if not ok or not pathExists(dir) then
			-- given path is invalid
			local msg = ("JsonReporter:save - give path %s is invalid"):format(path)
			Log:err(msg)
			return nil, msg
		end

		local file,err = io.open(path..'.json', "w")
		if not file then
			-- failed to open file
			Log:err(err)
			return nil, err
		end

		-- encode data
		local ok,data = pcall(Json.encode, Json, report)
		if not ok then
			-- failed to encoded the report
			Log:err(data)
			return nil, tostring(data)
		end

		file:write(data)
		file:close()
		return true
	end,

	finalize = function(this, ...)
		local report,status = this:_finalize()
		return this:save(report, ...)
	end
}

local JUnitReporter = {
	save = function(this, report, path)
		if not report or path == nil or #path == 0 then
			-- There is no report to save
			local msg = ("There is no report to save or path '%s' is empty"):format(path)
			Log:wrn(msg)
			return nil, msg
		end

		-- Encode the finalized report and save to disk
		local _,ok,dir = _dirname_S(path)
		if not ok or not pathExists(dir) then
			-- given path is invalid
			local msg = ("JsonReporter:save - give path %s is invalid"):format(path)
			Log:err(msg)
			return nil, msg
		end

		local file,err = io.open(path..'.xml', "w")
		if not file then
			-- failed to open file
			Log:err(err)
			return nil, err
		end

		-- encode data
		local function fwr(s) file:write(s) end
		fwr('<?xml version="1.0" encoding="UTF-8"?>\n')
		fwr(('<testsuites name="%s" skipped="%d" errors="%d" failures="%d" disabled="%d" tests="%d" time="%d">\n')
			:format(report.name,
					report.nignored,
					report.nerrors,
					report.nfailed,
					report.ndisabled,
					report.ntests,
					math.floor(report.duration/60)))

		function addTest(ts, tc)
			fwr('  ')
			fwr(('<testcase name="%s - %s" classname="%s" status="%s" time="%d">\n')
			    :format(tc.name, tc.descr, ts.name, status2str(tc.status), math.floor(tc.duration/60)))
			if tc.status == Test.Ignored then
				fwr('    <skipped') if tc.msg then fwr((' message="%s"'):format(tc.msg)) end fwr('/>\n')
			elseif tc.status == Test.Failed then
				fwr('    <failure') if tc.msg then fwr((' message="%s"'):format(tc.msg)) end fwr('/>\n')
			end
			if #tc.messages ~= 0 then
				fwr('    <system-out>\n')
				for _,msg in ipairs(tc.messages) do
					fwr(msg) ; fwr('\n')
				end
				fwr('    </system-out>\n')
			end
			fwr('  </testcase>\n')
		end

		function addSuite(ts)
			fwr(('<testsuite name="%s" tests="%d" disabled="%d" errors="%d" failures="%d" skipped="%d" time="%d" timestamp="%d">\n')
			    :format(ts.name, #ts.tests, ts.ndisabled, ts.nerrors, ts.nfailed, ts.nignored, math.floor(ts.duration/60),
					    ts.timestamp))
			fwr('  <properties>\n')
			fwr(('    <property name="path" value="%s"/>\n'):format(ts.path))
			fwr(('    <property name="ROOT" value="%s"/>\n'):format(ts.root))
			fwr('  </properties>\n')
			for _,tc in pairs(ts.tests) do
				addTest(ts, tc)
			end
			fwr('</testsuite>\n')
		end

		for _,ts in ipairs(report.collections) do
			addSuite(ts)
		end
		fwr('</testsuites>')

		file:close()
		return true
	end,

	finalize = function(this, ...)
		local report,status = this:_finalize()
		local ok,data = pcall(this.save, this, report, ...)
		local cleanup = function(path)
			if pathExists(path) then
				-- clean up path as it have be corrupt
				_mv(path, path..'CORRUPT')
			end
		end
		if not ok then
			cleanup(...)
			Log:err("JUnit reporter error: %s", tostring(data)) end
		return ok,data
	end
}

local ConsoleReporter = setmetatable({
	_LC = function(self, func, fmt, ...)
		Log:dbg(fmt, ...)
		if func then
			func(Console, fmt, ...)
		else print(debug.traceback()) end
	end,

	startCollection = function(this, path)
		assert(not(this.currentFile), "current collection still running")
		this.currentFile = path
		this.pstart = timestamp()
		this:_LC(Console.white, "> Running tests from %s", path)
	end,
	
	endCollection = function(this)
		local duration = timestamp() - this.pstart
		if this.nfailed > 0 then
			this.status = false
			this:_LC(Console.red, "< %s FAIL %0.0f ms", this.currentFile, duration)
			this:_LC(Console.green, "    %-8d Passed", this.npassed)
			this:_LC(Console.red,   "    %-8d Failed", this.nfailed)
		else
			this:_LC(Console.green, "< %s PASS %0.0f ms", this.currentFile, duration)
			this:_LC(Console.green, "    %-8d Passed", this.npassed)
		end
		if this.nignored > 0 then this:_LC(Console.yellow, "    %-8d Ignored", this.nignored) end
		print()

		this.nignored = 0
		this.npassed  = 0
		this.nfailed  = 0
		this.pstart   = 0
		this.currentFile = nil
	end,

	failCollection = function(this, fmt, ...)
		this.status = false
		local duration = timestamp() - this.pstart
		if fmt and type(fmt) == 'string' then
			this:_LC(Console.red, fmt, ...)
		end

		this:_LC(Console.red,  "%s FAIL %0.0f ms", this.currentFile, duration)
		this:_LC(Console.green,"    %-8d Passed", this.npassed)
		this:_LC(Console.red,  "    %-8d Failed", this.nfailed)
		if this.nignored > 0 then this:_LC(Console.yellow, "    %-8d Ignored", this.nignored) end

		this.nignored = 0
		this.npassed  = 0
		this.nfailed  = 0
		this.pstart   = 0
		this.currentFile = nil
	end,

	startTestcase = function(this, name, descr)
		assert(this.currentFile,  "reporter not aware of any test file")
		assert(not(this.currentTest), "current test case still running")
		this.currentTest = name
		this.tstart = timestamp()
		this:_LC(Console.white, "  > Start %s %s", name, descr or '')
	end,
	
	endTestcase = function(this, status, msg, stage, assetions)
		assert(this.currentTest, "there is no test case currently running")
		local duration = timestamp() - this.tstart
		if status == Test.Ignored then this.nignored = this.nignored + 1
		elseif status == Test.Passed  then this.npassed = this.npassed + 1
		else this.nfailed = this.nfailed + 1 end
		
		stage = stage or 'run'
		
		if status == Test.Ignored then
			this:_LC(Console.yellow, "  < Done %s:%s SKIP %0.0f ms (%06d Checks)", this.currentTest, stage, duration, assetions)
			if msg then this:_LC(Console.yellow, "    %s", msg:gsub('\n', '\n    ')) end
		elseif status == Test.Passed then
			this:_LC(Console.green,  "  < Done %s:%s PASS %0.0f ms (%06d Checks)", this.currentTest, stage, duration, assetions)
			if msg then this:_LC(Console.green, "    %s", msg:gsub('\n', '\n    ')) end
		else
			this:_LC(Console.red,    "  < Done %s:%s FAIL %0.0f ms (%06d Checks)", this.currentTest, stage, duration, assetions)
			if msg then this:_LC(Console.red, "    %s", msg:gsub('\n', '\n    ')) end
		end
		this.currentTest = nil
		this.tstart = 0
	end,
	message = function(this, tag, fmt, ...)
		assert(this.currentTest, "messages can only be appended when there is a running test")
		if fmt then this:_LC(Console.white, ('    %s - '..fmt):format((tag or 'run'), ...)) end
	end,

	finalize = function(this, ...)
		return true
	end,

	update = function(this)
	end,

	npassed = 0,
	nfailed = 0,
	nignored = 0,
	status = true
}, {
	__call = function(this, attrs)
		attrs = attrs or {}
		return setmetatable(attrs,{
			__index = this,
			
			__call = function(self, tag, fmt, ...)
				self:message(tag, fmt, ...)
			end
		})
	end
})

local Fanout = setmetatable({
	startCollection = function(this, path, name, descr)
		Log:pop(path)
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:startCollection(path, name, descr)
		end
	end,

	endCollection = function(this)
		Log:pop()
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:endCollection()
		end
	end,

	failCollection = function(this, fmt, ...)
		Log:pop()
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:failCollection(fmt, ...)
		end
	end,

	startTestcase = function(this, name, descr)
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:startTestcase(name, descr)
		end
	end,

	endTestcase = function(this, status, msg, stage, assertions)
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:endTestcase(status, msg, stage, assertions)
		end
	end,

	message = function(this, tag, fmt, ...)
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:message(tag, fmt, ...)
		end
	end,

	finalize = function(this, ...)
		local status = true
		for name,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			local ok = s:finalize(...)
			status = status and ok
		end
		return status
	end,

	update = function(this, name, descr)
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:update(name, descr)
		end
	end,

	add = function(this, name, sink)
		assert(sink ~= this, "Adding Fanout to self not allowed")
		this._sinks[name] = sink
	end
}, {
	__call = function(this, sinks)
		local fo = setmetatable({ _sinks = {}}, {
			__index = this,
			__call = function(sel, tag, fmt, ...)
				for _,s in pairs(self._sinks) do
					-- forward the log to all outputs of the fanout
					s(tag, fmt, ...)
				end
			end
		})

		if sinks and type(sinks) == 'table' then
			for name,sink in pairs(sinks) do
				Log:dbg("adding sink %s to fanout", name)
				fo:add(name, sink)
			end
		end
		return fo
	end,

	__newindex = constify('ConsoleReporter')
})

return function()
	return {
		Fanout     = Fanout,
		Structured = StructuredReport,
		Console    = ConsoleReporter,
		Json       = JsonReporter,
		JUnit      = JUnitReporter
	}
end