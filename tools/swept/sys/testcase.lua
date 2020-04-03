--
-- Created by IntelliJ IDEA.
-- User: dc
-- Date: 2020-03-04
-- Time: 12:29 a.m.
-- To change this template use File | Settings | File Templates.
--

local _resolve = function(ok, msg)
	if ok then return ok, {s = Test.Passed}
	else
		if type(msg) == 'table' and msg.status then
			return ok, {s = msg.status, m = msg.data or nil}
		elseif msg then
			Log:trc(debug.traceback())
			return ok, {s = Test.Failed, m = tostring(msg)}
		else
			return ok, {s = Test.Failed}
		end
	end
end

local Testcase = setmetatable({
	Disable = 'Disable',
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

	tags = function(this, ...)
		for i,t in ipairs({args}) do
			this._tags[t] = true
		end
		return this
	end,
	attrs = function(this, attrs)
		if attrs and type(attrs) == 'table' then
			for name,value in pairs(attrs) do
				this._attrs[name] = value
			end
		end
	end,
	tag = function(this, tag)
		return tag and this._tags[tostring(tag)]
	end,

	disable = function(this, why)
		this._run = tostring(why or 'not reason provided')
		return this
	end,

	isTest = function(this, test)
		return test and type(test) == 'table' and test._exec
	end,

	getDescr = function(this, test)
		if this:isTest(test) then return test.description else return nil end
	end,

	_exec = function(this, ctx)
		Assertions:reset()
		ctx.attrs = this._attrs
		ctx.reporter:startTestcase(this.name, this.descr)
		if not this._run or type(this._run) == 'string' then
			-- test case ignored
			ctx.reporter:endTestcase(Test.Ignored, this._run or nil, 'ignore', Assertions:reset())
			return true
		end
		
		local status = {s = Test.Passed }
		local ok = true
		if this._before and type(this._before) == 'function' then
			ok,status = _resolve(pcall(this._before, ctx))
			if not ok then
				ctx.reporter:endTestcase(status.s, status.m, 'before', Assertions:reset())
				return ok
			end
		end
		
		if type(this._run) == 'function' then
			-- testcase might fail or pass, but we still have to run post handlers
			ok,status = _resolve(pcall(this._run, ctx))
		else
			status = {s = Test.Failed, m = ("Test '%s' is malformed, does not have runner"):format(this.name)}
		end

		if status.s == Test.Passed then
			if this._success and type(this._success) == 'function' then
				if status.m then ctx.reporter:message('run', status.m) end
				ok,status = _resolve(pcall(this._success, ctx))
			end
		elseif status.s ~= Test.Ignore then
			if this._failure and type(this._failure) == 'function' then
				if status.m then ctx.reporter:message('run', status.m) end
				local ok,tmp = _resolve(pcall(this._success, ctx))
				status.m = tmp.m
			end
		end
		ctx.reporter:endTestcase(status.s, status.m, 'run', Assertions:reset())
	end
}, {
	__call = function(this, name, description)
		assert(name, "Testcase must have a unique name")
		return setmetatable({ name = name, descr = description, _attrs = {} }, {
			__index = this,
			__call  = this._exec
		})
	end
})

local Fixture = setmetatable({
	_exec = function(this, ctx, tf)
		local logger = ctx.logger
		local reporter = ctx.reporter

		local ex = tf and tf:byte(1) == string.byte('~', 1) or false
		tf = tf and tf:sub(2) or nil

		local function includeTest(name)
			if not tf then return true end
			if name:find(tf) then return not ex
			else return ex end
		end

		for _, name in ipairs(this._order) do
			if includeTest(name) then
				logger:dbg("executing test %s", name)
				local test = this._tests[name]
				if TestCase:isTest(test) then
					-- run test case
					test(ctx)
				else
					logger:err('%s is not valid test case, ignoring', name)
				end
			else
				logger:dbg("skipping test as it filtered out {name: %s, filter: %s, ex: %s}",
				            name, tf, tostring(ex))
			end
		end
	end,

	desc = function(this, d)
		this._desc = d
	end,

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
	setup = function(this, func)
		assert(func and type(func) == 'function', "Fixture setup handler must be a function")
		this._setup = func
		return this
	end,
	teardown = function(this, func)
		assert(func and type(func) == 'function', "Fixture teardown handler must be a function")
		this._teardown = func
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
}, {
	__call = function(this, name, descr)
		return setmetatable({_name = name, descr = descr, _tests = {}, _order = {}, _tags = {}}, {
			__index = this,
			__call = function(self, name, description)
				assert(not(self._tests[name]), "test with name '"..name.."' already added to fixture")
				self._tests[name] = TestCase(name, description)
				self._order[#self._order + 1] = name
				local T = self._tests[name]
				-- inherit callbacks from fixture
				if self._before then T:before(self._before) end
				if self._success then T:success(self._success) end
				if self._failure then T:failure(self._failure) end
				-- point back to fixture
				T.fixture = function(s) return self end
				return T
			end
		})
	end
})

return function() return Testcase, Fixture end