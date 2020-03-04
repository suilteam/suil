local function applyDefaults(to, defaults)
	for key,value in pairs(defaults) do
		if not to[key] then to[key] = value end
	end
end

local Scripter = setmetatable({
	startScript = function(this)
		this:cmds(("#!%s %s"):format(this.shbang, this.shbangargs))
		this:cmds(('# file: %s'):format(this.fname))
		this:cmds('# automatically generated script file, do not modify')
		this:cmds()
		return this
	end,

	cmds = function(this, cmd)
		if not cmd then
			this('\n')
		elseif type(cmd) == 'string' then
			this(cmd)('\n')
		elseif type(cmd) == 'table' then
			for n, val in pairs(cmd) do
				if type(val) ~= 'string' then
					error('Command #' .. n .. " has invalid type " .. type(val) .. " expecting string command")
				end
				this(val)
			end
		else
			error("invalid type '" .. type(val) .. "' expecting string command")
		end

		return this
	end
}, {
	__call = function(this, attrs)
		assert(attrs.dir, "missing required attribute 'dir'")
		assert(attrs.name, "missing required attribute 'name'")
		applyDefaults(attrs, {
			shbang = '/bin/bash',
			shbangargs = '-ex'
		})
		attrs.fname = attrs.dir .. '/' .. attrs.name
		local file, msg = io.open(attrs.fname, 'w+')
		if not file then
			error("failed to open '"..attrs.fname.."': " .. msg)
		end

		attrs.file = file
		return setmetatable(attrs, {
			__index = this,
			__call = function(self, ...)
				local args = {...}
				self.file:write(table.concat(args))
				return self
			end
		})
	end
})

function job(name)
	assert(name and type(name) == 'string', "job declaration requires a name parameter")
	assert(not(Pipeline.jobs[name]),
			("job with name '%s' already registered"):format(name))

	local function section(attrs, id, steps)
		assert(type(steps) == 'table',
				("job '%s' before section is must be sequence of steps"):format(id))
		for n,s in pairs(steps) do
			assert(s and type(s) == 'string',
					("job '%s' section %s step number %d is invalid"):format(name, steps, n))
		end
		local sp = Scripter{
			dir   = LAMINAR_JOBS..'/jobs',
			name  = id..'.before',
			shell = attrs.shell
		}
		sp:startScript()
		sp:cmds(steps)
		sp:closeScript()
	end

	return function(attrs)
		assert(attrs.run, "job '%s' run section is required")
		section(attrs.run)
		if attrs.before then
			-- add script to be executed before job
			section(attrs, 'before', attrs.before)
		end
		if attrs.after then
			-- add script to be executed after job
			section(attrs.after)
		end
		-- generate environment variable
		if attrs.env then
			assert(type(attrs.env) == 'table',
					("job '%s' env section must be a list of environment variables"):format(name))
			local sp = Scripter {
				dir = LAMINAR_JOBS..'/jobs',
				name = name..".env"
			}
			sp:startScript()
			for key,value in pairs(attrs.env) do
				assert(type(key) == 'string',
						("job '%s' env section has invalid key '%s'"):format(name, tostring(key)))
				sp(key, '=', tostring(value or ''), '\n')
			end
			sp:closeScript()
		end
		-- generate job configuration
		local sp = Scripter {
			dir = LAMINAR_JOBS..'/jobs',
			name = name..".conf"
		}
		sp:startScript()
		sp('JOB_NAME=', name, '\n')
		if attrs.timeout or attrs.TIMEOUT then
			sp('TIMEOUT=', (attrs.tags or attrs.TIMEOUT), '\n')
		end
		if attrs.tags or attrs.TAGS then
			sp('TAGS=', (attrs.tags or attrs.TAGS), '\n')
		end
	end
end