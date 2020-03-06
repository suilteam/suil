--
-- @filename sh.lua
--
-- A simple module that allows users to execute
-- commands from from within as function calls.
--
-- @usage
--	```lua
---	local sh = require('sh') 
---
--- ls() -- executes command ls and prints outout to stdout or stderr
---
--- local res,ok = _ls() -- executes the conmand ls and returns and output object
--- if ok then print('command was successful')
--- else print('command failed') end
---
--- print(tostring(res)) -- get command output, if failed stderr will be prefered to stdout
--- ```
---

local function tmpio()
	local out = os.tmpname()
	os.remove(out) -- need to remove created file
	out = Dirs.SHELL..out:gsub('/tmp', '')
	local err = out..'.err'
	return out, err
end

-- converts key and it's argument to "-k" or "-k=v" or just ""
local function arg(k, a)
	if not a == nil then return k
	elseif type(a) == 'string' and #a > 0 then return k..' \''..a..'\''
	elseif type(a) == 'number' then return k..' '..tostring(a)
	elseif type(a) == 'boolean' then if a == true then return k else return ''; end
	elseif type(a) == 'table' then
		local list = ''
		for kk,v in ipairs(a) do
			if not type(kk) == 'number' then
				error("invalid argument type, expecting array not table")
			end
			list = list.." "..k.." '"..v.."'"
		end
		return list
	end
	error('invalid argument type '..type(a)..' '..a)
end

-- converts nested tables into a flat list of arguments and concatenated input
local function flatten(t)
	local result = {args = {}, input = ''}

	local function f(t)
		local keys = {}
		for k = 1, #t do
			keys[k] = true
			local v = t[k]
			if type(v) == 'table' then
				f(v)
			else
				table.insert(result.args, v)
			end
		end
		for k, v in pairs(t) do
			if k == '__input' then
				result.input = result.input .. v
			elseif not keys[k] and k:sub(1, 1) ~= '_' then
				local key = '-'..k
				if #k > 1 then key = '-' ..key end
				table.insert(result.args, arg(key, v))
			end
		end
	end

	f(t)
	return result
end
local function readAll(file)
	local fd, msg = io.open(file, 'r')
	local data = fd:read('*a')
	fd:close()
	return data
end

local function buildCommand(cmd, ...)
	local args = flatten({...})
	local s = cmd
	for k, v in pairs(args.args) do
		s = s .. ' ' .. v
	end
	return s
end

local Command = setmetatable({
	stderr = function(this)
		if this._stderr then
			this.edata = readAll(this._stderr)
			os.execute('rm -f '..this._stderr)
			this._stderr = nil
		end
		return this.edata
	end,
	stdout = function(this)
		if this._stdout then
			this.data = readAll(this._stdout)
			os.execute('rm -f '..this._stdout)
			this._stdout = nil
		end
		return this.data
	end,
	s = function(self)
		return tostring(self)
	end
}, {
	__call = function(this, cmd, ...)
		local ass
		local cmd,cap = cmd:gsub('^_', '')
		if cap ~= 0 then
			-- command is capturing, check if string output is requested
			cmd,ass = cmd:gsub('_S$', '')
		end

		local attrs = { cmd = buildCommand(cmd, ...), cap = cap ~= 0, ass = ass ~= 0}
		return setmetatable(attrs, {
			__index = this,
			__call = function(self, ...)
				local s = buildCommand(self.cmd, ...)
				if self.cap then
					self._stdout, self._stderr = tmpio()
					s = ('%s > %s 2>%s'):format(s, self._stdout, self._stderr)
				end

				-- execute command
				if Log then Log:trc(s) end

				self.exitcode = 0
				local ok, what,code = os.execute(s)
				what = (what ~= 'exit' and what ~= 'signal') and what or nil

				if code == 10 then
					self.exitcode =  0
				else
					self.exitcode = what == nil and code or 127
				end

				if self.exitcode == 127 then
					return self, nil, what or tostring(self)
				elseif self.ass then
					return self, self.exitcode == 0, tostring(self)
				else
					return self, self.exitcode == 0, nil
				end
			end,
			__gc = function(self)
				if self._stdout then os.remove(self._stdout) end
				if self._stderr then os.remove(self._stderr) end
			end,
			__tostring = function(self)
				local str
				if self.exitcode  == 0 then
					str = self:stdout() or self:stderr() or ''
				else
					str = self:stderr() or self:stdout() or ''
				end
				return str:gsub('\n$', '')
			end,
			__bor = function(lhs, rhs)
				assert((lhs.exitcode == nil) and (rhs.exitcode == nil), "cannot pipe already exited command")
				local tmp = this(lhs.cmd, '|', rhs.cmd)
				tmp.cap = lhs.cap
				tmp.ass = lhs.ass
				return tmp
			end
		})
	end
})

local mt = getmetatable(_G)
if mt == nil then
	mt = {}
	setmetatable(_G, mt)
end

--
-- hook for undefined variables, not that this modifies
-- the expected 
mt.__index = function(t, cmd)
	return Command(cmd)
end

return Command