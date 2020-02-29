---
--- @filename utils.lua
---

function Constify(name)
	return function(_,_,_)
		assert(false, ("'%s' table is constant (i.e cannot be modified)"):format(name))
	end
end

function string:split(sep)
   local sep, fields = sep or ":", {}
   local pattern = string.format("([^%s]+)", sep)
   self:gsub(pattern, function(c) fields[#fields+1] = c end)
   return fields
end

function now()
    return os.time(os.date("!*t"))
end

function using(path)
	return require(path)()
end