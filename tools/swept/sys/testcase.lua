--
-- Created by IntelliJ IDEA.
-- User: dc
-- Date: 2020-03-04
-- Time: 12:29 a.m.
-- To change this template use File | Settings | File Templates.
--

local Testcase = setmetatable({
	before = function(this, func)
		assert(func and type(func) == 'function', "Testcase before handler must be a function")
		this._before = func
		return this
	end,
	after = function(this, func)
		assert(func and type(func) == 'function', "Testcase after handler must be a function")
		this._success = func
		this._failure = func
		return this
	end,
	success = function(this, func)
		assert(func and type(func) == 'function', "Testcase success handler must be a function")
		this._success = func
		return this
	end,
	failure = function(this, func)
		assert(func and type(func) == 'function', "Testcase failure handler must be a function")
		this._failure = func
		return this
	end,
	run = function(this, func)
		assert(func and type(func) == 'function', "Testcase run handler must be a function")
		this._run = func
		return this
	end,

	isTest = function(this, test)
		return test and type(test) == 'table' and test._exec
	end,

	getDescr = function(this, test)
		if this:isTest(test) then return test.description else return nil end
	end,

	_resolve = function(ok, msg)
		if ok then return ok, {s = Test.Passed}
		else
			if type(msg) == 'table' and msg.status then
				return ok, {s = msg.status, m = msg.data or nil}
			elseif msg then
				return ok, {s = Test.Failed, m = tostring(msg)}
			else
				return ok, {s = Test.Failed}
			end
		end
	end,

	_exec = function(this, ctx)
		ctx.reporter:startTestcase(this.name, this.descr)
		if not this._run or type(this._run) == 'string' then
			-- test case ignored
			ctx.reporter:endTestcase(Test.Ignored, this._run or nil, 'ignore')
			return true
		end
		
		local status = {s = Test.Passed }
		local ok = true
		if this._before and type(this._before) == 'function' then
			ok,status = this._resolve(pcall(this._before, ctx))
			if not ok then
				ctx.reporter:endTestcase(status.s, status.m, 'before')
				return ok
			end
		end
		
		if type(this._run) == 'function' then
			-- testcase might fail or pass, but we still have to run post handlers
			ok,status = this._resolve(pcall(this._run, ctx))
		else
			status = {s = Test.Failed, m = ("Test '%s' is malformed, does not have runner"):format(this.name)}
		end
		
		if status.s == Test.Success then
			if this._success and type(this._success) == 'function' then
				ctx.reporter:success('run', status.m)
				ok,status = this._resolve(pcall(this._success, ctx))
			end
		elseif status.s ~= Test.Ignore then
			if this._failure and type(this._failure) == 'function' then
				ctx.reporter:failure('run', status.m)
				local ok,tmp = this._resolve(pcall(this._success, ctx))
				status.m = tmp.m
			end
		end
		ctx.reporter:endTestcase(status.s, status.m, 'run')
	end
}, {
	__call = function(this, name, description)
		assert(name, "Testcase must have a unique name")
		return setmetatable({ name = name, descr = description }, {
			__index = this,
			__call  = this._exec
		})
	end
})

local Fixture = setmetatable({
	_exec = function(this, ctx)
		local logger = ctx.logger
		local reporter = ctx.reporter

		for name, test in pairs(this._tests) do
			logger:dbg("executing test %s", name)
			if not TestCase:isTest(test) then
				reporter:endTestcase(Test.Failed, "test is not a valid test case", 'load')
			else
				-- run testcase
				test(ctx)
			end
		end
	end
}, {
	__call = function(this)
		return setmetatable({_tests = {}}, {
			__index = this,
			__call = function(self, name, description)
				assert(not(self._tests[name]), "test with name '"..name.."' already added to fixture")
				self._tests[name] = TestCase(name, description)
				return self._tests[name]
			end
		})
	end
})
return function() return Testcase, Fixture end