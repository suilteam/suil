local _,Console  = import('sys/logger')

local Reporter = setmetatable({
	startCollection = function(this, path)
		assert(not(this.currentFile), "current collection still running")
		this.currentFile = path
		this.pstart = timestamp()
		Console:white("running tests from %s", path)
	end,
	
	endCollection = function(this)
		local duration = timestamp() - this.pstart
		if this.nfailed > 0 then
			Console:red("%s FAIL %0.0f ms", this.currentFile, duration)
			Console:green("    %-8d Passed", this.npassed)
			Console:red("    %-8d Failed", this.nfailed)
		else
			Console:green("%s PASS %0.0f ms", this.currentFile, duration)
			Console:green("    %-8d Passed", this.npassed)
		end

		if this.nignored > 0 then Console:yellow("    %-8d Ignored", this.nignored) end
		this.nignored = 0
		this.npassed  = 0
		this.nfailed  = 0
		this.pstart   = 0
		this.currentFile = nil
		print()
	end,

	failCollection = function(this, fmt, ...)
		local duration = timestamp() - this.pstart
		if fmt and type(fmt) == 'string' then
			Console:red(fmt, ...)
		end

		Console:red("%s FAIL %0.0f ms", this.currentFile, duration)
		Console:green("    %-8d Passed", this.npassed)
		Console:red("    %-8d Failed", this.nfailed)
		if this.nignored > 0 then Console:yellow("    %-8d Ignored", this.nignored) end

		this.nignored = 0
		this.npassed  = 0
		this.nfailed  = 0
		this.pstart   = 0
		this.currentFile = nil
		print()
	end,

	startTestcase = function(this, name, descr)
		assert(this.currentFile,  "reporter not aware of any test file")
		assert(not(this.currentTest), "current test case still running")
		this.currentTest = name
		this.tstart = timestamp()
		Console:white("  Start %s", name)
		if descr then Console:white("    "..descr) end
	end,
	
	endTestcase = function(this, status, msg, stage)
		assert(this.currentTest, "there is no test case currently running")
		local duration = timestamp() - this.tstart
		if status == Test.Ignored then this.nignored = this.nignored + 1
		elseif status == Test.Passed  then this.npassed = this.npassed + 1
		else this.nfailed = this.nfailed + 1 end
		
		stage = stage or 'run'
		msg = msg and '    '..msg..'\n' or ''
		
		if status == Test.Ignored then
			Console:yellow("%s  Done %s:%s SKIP %0.0f ms\n", msg, this.currentTest, stage, duration)
		elseif status == Test.Passed then
			Console:green("%s  Done %s:%s PASS %0.0f ms\n", msg, this.currentTest, stage, duration)
		else
			Console:red("%s  Done %s:%s FAIL %0.0f ms\n", msg, this.currentTest, stage, duration)
		end
		this.currentTest = nil
		this.tstart = 0
	end,
	success = function(this, tag, fmt, ...)
		assert(this.currentTest, "messages can only be appended when there is a running test")
		if fmt then Console:white(('    %s - '..fmt):format((tag or 'run'), ...)) end
	end,
	failure = function(this, tag, fmt, ...)
		assert(this.currentTest, "messages can only be appended when there is a running test")
		if fmt then  Console:red(('    %s - '..fmt):format((tag or 'run'), ...)) end
	end,
	ignore = function(this, tag, fmt, ...)
		assert(this.currentTest, "messages can only be appended when there is a running test")
		if fmt then  Console:red(('    %s - '..fmt):format((tag or 'run'), ...)) end
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
				self:success(fmt, tag, ...)
			end
		})
	end,
	
	__newindex = constify('reporter')
})

return Reporter