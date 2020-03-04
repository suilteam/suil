--
-- Created by IntelliJ IDEA.
-- User: dc
-- Date: 2020-03-04
-- Time: 12:29 a.m.
-- To change this template use File | Settings | File Templates.
--

function string:split(sep)
	local sep, fields = sep or ":", {}
	local pattern = string.format("([^%s]+)", sep)
	self:gsub(pattern, function(c)
		fields[#fields + 1] = c
	end)
	return fields
end

---
--- print contents of `tbl`, with indentation.
--- `indent` sets the initial level of indentation.
---
--- @param tbl table the table to print
--- @param indent number the initial indentation level
---
function tprint (tbl, indent)
	if not indent then indent = 0 end
	for k, v in pairs(tbl) do
		local formatting = string.rep("  ", indent) .. k .. ": "
		if type(v) == "table" then
			print(formatting)
			tprint(v, indent+1)
		elseif type(v) == 'boolean' or type(v) == 'function' then
			print(formatting .. tostring(v))
		else
			print(formatting .. v)
		end
	end
end

function constify(what)
	return function(this, key, val)
		assert(false, ("'%s' is readyonly"):format(what))
	end
end

function applyDefaults(to, defaults)
	for key,val in pairs(defaults) do
		if not to[key] then
			to[key] = val
		end
	end
end

function timestamp()
	return Swept:now()
end

function pathExists(path)
	return tostring(stat(path..'> /dev/null 2>&1 ; echo $?')) == '0'
end

S = tostring

Test = setmetatable({
	Passed  = 0,
	Ignored = 2,
	Failed  = 3,
	check = function(this, cond, fmt, ...)
		if not(cond) then
			local info = debug.getinfo(3)
			error({
				status = this.Failed,
				data = ('%s:%d - '..fmt):format(info.short_src, info.currentline, ...)})
		end
	end
}, {
	__call = function(this, cond, fmt, ...)
		Test:check(cond, fmt, ...)
	end,
	__newindex = constify('Test')
})

function Equal(a, b, fmt, ...)
	Test:check(a == b, ("%s == %s? "..fmt):format(tostring(a), tostring(b), ...))
end

function NotEqual(a, b, fmt, ...)
	Test(a ~= b, ("%s ~= %s? "..fmt):format(tostring(a), tostring(b), ...))
end