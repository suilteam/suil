include( 'sys/sh')

local _levelMsg = {"ERR", "WRN", "INF", "DBG", "TRC", "TRC"}

local Console = setmetatable({
	CGREEN    = '\27[1;32m',
	CYELLOW   = '\27[1;33m',
	CRED      = '\27[1;31m',
	CBLUE     = '\27[1;34m',
	CCYAN     = '\27[1;36m',
	CMAGENTA  = '\27[1;35m',
	CRESET    = '\27[0m',
	
	green = function(this, fmt, ...)
		print('\27[1;32m'..fmt:format(...)..'\27[0m')
	end,
	yellow = function(this, fmt, ...)
		print('\27[1;33m'..fmt:format(...)..'\27[0m')
	end,
	red = function(this, fmt, ...)
		print('\27[1;31m'..fmt:format(...)..'\27[0m')
	end,
	blue = function(this, fmt, ...)
		print('\27[1;34m'..fmt:format(...)..'\27[0m')
	end,
	cyan = function(this, fmt, ...)
		print('\27[1;36m'..fmt:format(...)..'\27[0m')
	end,
	magenta = function(this, fmt, ...)
		print('\27[1;35m'..fmt:format(...)..'\27[0m')
	end,
	print = function(this, color, fmt, ...)
		color = color or this.RESET
		print(color..fmt:format(...)..this.RESET)
	end,
	white = function(this, fmt, ...)
		print(fmt:format(...))
	end
}, {
	__call = function(this, attrs)
		return setmetatable(attrs, {
			__index = this,
			__call = function(self, lvl, tag, msg)
				if self.level == nil or self.level >= lvl then
					local colors = {
						'\27[1;31m',
						'\27[1;33m',
						'\27[1;0m',
						'\27[1;36m',
						'\27[1;32m'
					}
					print(colors[lvl]..msg..'\27[0m')
				end
			end
		})
	end,
	__newindex = constify('Logger')
})

local File = setmetatable({
}, {
	__call = function(this, attrs)
		attrs = attrs or {}
		if not attrs.dir then attrs.dir = Dirs.LOGS end
		if not attrs.fname then attrs.fname = (Swept.Config.prefix or '') .. os.date('%Y_%m_%d_%H_%M_%S.txt') end
		attrs.path   = attrs.dir..'/'..attrs.fname
		attrs.handle = io.open(attrs.path, "a")
		return setmetatable(attrs, {
			__index = this,
			__call = function(self, lvl, tag, msg)
				if self.level == nil or self.level >= lvl then
					self.handle:write(msg..'\n')
					return
				end
			end
		})
	end,
	__newindex = constify('File')
})

local Fanout = setmetatable({
	_outputs = {},
	add = function(this, name, sink)
		assert(this ~= sink, "Try avoiding loops while adding sinks to a fanout")
		if not this._outputs[name] then
			this._outputs[name] = sink
		end
	end,

	get = function(this, name)
		return this._outputs[name]
	end
}, {
	__call = function(this, lgs)
		local fo = setmetatable({}, {
			__index = this,
			__call = function(this, lvl, tag, msg)
				for k,s in pairs(this._outputs) do
					-- forward the log to all outputs of the fanout
					s(lvl, tag, msg)
				end
			end
		})

		if lgs and type(lgs) == 'table' then
			for k,s in pairs(lgs) do
				-- add the sink to the fanout
				fo:add(k, s)
			end
		end
		return fo
	end,
	__newindex = constify('Fanout')
})

Logger = setmetatable({
	ERROR   = 1,
	WARNING = 2,
	INFO    = 3,
	DEBUG   = 4,
	TRACE0  = 5,
	TRACE1  = 6,
	
	err = function(this, fmt, ...)
		if this.level >= this.ERROR then
			this:log(this.ERROR, fmt, ...)
		end
	end,
	wrn = function(this, fmt, ...)
		if this.level >= this.WARNING then
			this:log(this.WARNING, fmt, ...)
		end
	end,
	inf = function(this, fmt, ...)
		if this.level >= this.INFO then
			this:log(this.INFO, fmt, ...)
		end
	end,
	dbg = function(this, fmt, ...)
		if this.level >= Logger.DEBUG then
			this:log(Logger.DEBUG, fmt, ...)
		end
	end,
	trc = function(this, fmt, ...)
		if this.level >= this.TRACE0 then
			this:log(this.TRACE0, fmt, ...)
		end
	end,
	trc1 = function(this, fmt, ...)
		if this.level >= this.TRACE1 then
			this:log(this.TRACE1, fmt, ...)
		end
	end,
	log = function(this, lvl, fmt, ...)
		-- date lvl tag msg
		lvl = lvl > this.TRACE1 and this.TRACE1 or lvl
		local msg
		local formatted = fmt:format(...)
		if lvl < this.DEBUG then
			if not fmt then print(debug.traceback()) end
			msg = ("%s %s %8s "):format(
				os.date('%Y-%m-%d %H:%M:%S'),
				_levelMsg[lvl],
				this.tag)
			msg = msg..formatted
		else
			local info = debug.getinfo(3)

			local prefix = ("%s %s %8s %s:%d "):format(
				os.date('%Y-%m-%d %H:%M:%S'),
				_levelMsg[lvl],
				this.tag,
				shortPath(info.short_src, info.source),
				info.currentline)
			msg = prefix..formatted
		end
		this.sink(lvl, this.tag, msg)
	end,
	---
	--- @field push function this can be used by reporters to
	--- to open sections in a logger. It is up to the log sink to
	--- implement a push handler
	--- @param utag string a tag used to indentify the start of a section
	push = function(this, utag)
		if this.sink.push then this.sink:push(utag) end
	end,
	---
	--- @field pop function this is used inline with push to pop a tag off the
	--- the logger
	pop = function(this)
		if this.sink.pop then this.sink:pop() end
	end
}, {
	__call = function(this, attrs)
		attrs = attrs or {}
		applyDefaults(attrs, {
			level = this.DEBUG,
			sink  = Console,
			tag   = 'global'
		})
		return setmetatable(attrs, {
			__index = Logger,
			
			--
			-- logs infomation to the log sink
			__call = function(this, fmt, ...)
				this:inf(fmt, ...)
			end
		})
	end,
	__newindex = constify('Logger')
})

return function()
	return Logger, {
		Console = Console,
		File = File,
		Fanout = Fanout
	}
end