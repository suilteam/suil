local _,Sinks  = import('sys/logger')
local Json = import('sys/json')

local Console = Sinks.Console

local StructuredReport = setmetatable({
	startCollection = function(this, path)
		assert(not(this.openCollection), "startCollection: another collection still running")
		-- create a new collection
		this.openCollection = {
			path = path,
			name = path,
			descr = nil,
			started = timestamp(),
			tests = {},
			npassed = 0,
			nignored = 0,
			nfailed = 0
		}
		if #this.collections == 0 then this.started = timestamp() end
	end,

	endCollection = function(this)
		local collection = this.openCollection
		assert(collection, "endCollection: no open collection to run")
		collection.duration = timestamp() - collection.started
		collection.started = nil
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

		this:endTestcase(Test.Failed, 'NotInTest', fmt:format(...))
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

	endTestcase = function(this, status, msg, stage)
		local test = this.openTest
		assert(test, "there is no test case currently running")
		test.duration = timestamp() - test.started
		test.status   = status
		test.stage    = stage
		test.msg      = msg
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
			collections = {collections},
			npassed = 0,
			nfailed = 0,
			nignored = 0,
			duration = (this.ended or 0) - (this.started or 0)}
		for _,v in ipairs(collections) do
			report.npassed = report.npassed + v.npassed
			report.nfailed = report.nfailed + v.nfailed
			report.nignored = report.nignored + v.nignored
		end
		this.report = report
		return this.report
	end,

	finalize = function(this, ...)
		return this:_finalize()
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

		local file,err = io.open(path, "w")
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
		return this:save(this:_finalize(), ...)
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
	
	endTestcase = function(this, status, msg, stage)
		assert(this.currentTest, "there is no test case currently running")
		local duration = timestamp() - this.tstart
		if status == Test.Ignored then this.nignored = this.nignored + 1
		elseif status == Test.Passed  then this.npassed = this.npassed + 1
		else this.nfailed = this.nfailed + 1 end
		
		stage = stage or 'run'
		
		if status == Test.Ignored then
			this:_LC(Console.yellow, "  < Done %s:%s SKIP %0.0f", this.currentTest, stage, duration)
			if msg then this:_LC(Console.yellow, "    %s", msg:gsub('\n', '\n    ')) end
		elseif status == Test.Passed then
			this:_LC(Console.green,  "  < Done %s:%s PASS %0.0f", this.currentTest, stage, duration)
			if msg then this:_LC(Console.green, "    %s", msg:gsub('\n', '\n    ')) end
		else
			this:_LC(Console.red,    "  < Done %s:%s FAIL %0.0f", this.currentTest, stage, duration)
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
	end,

	update = function(this)
	end,

	npassed = 0,
	nfailed = 0,
	nignored = 0
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
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:startCollection(path, name, descr)
		end
	end,

	endCollection = function(this)
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:endCollection()
		end
	end,

	failCollection = function(this, fmt, ...)
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

	endTestcase = function(this, status, msg, stage)
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:endTestcase(status, msg, stage)
		end
	end,

	message = function(this, tag, fmt, ...)
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:message(tag, fmt, ...)
		end
	end,

	finalize = function(this, ...)
		for _,s in pairs(this._sinks) do
			-- forward the log to all outputs of the fanout
			s:finalize(...)
		end
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
			__call = function(this, tag, fmt, ...)
				for _,s in pairs(this._sinks) do
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
		Json       = JsonReporter
	}
end