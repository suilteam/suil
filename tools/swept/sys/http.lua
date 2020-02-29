local sh   = require("sh")
local json = require("json")

local Http

local genid = function()
	local _gen = sh('_head', {c=16}, '/dev/urandom') |
				 sh('_md5sum', {b=true}) |
				 sh('_awk', "'{print $1}'")
	return tostring(_gen()):gsub('[\r\n]+', '')
end
	
local Response = setmetatable({
	json = function(s)
		if not s._json and s.body then
			s._json = json:decode(s.body)
		end
		return s._json
	end
}, {
	__call = function(this,attrs)
		-- wrapps the given table into a response object
		return setmetatable(attrs, {__index = this})
	end,
	__newindex = Constify('Response')
})

local function _Http(this, attrs, url)
	function parse_resp(id)
		local _hdrs = tostring((sh('_cat', id) | sh('grep', "' '"))())
		local hdrs = _hdrs:split("\r\n")
		if #hdrs < 1 then
			error("headers '".._hdrs.."' are not valid")
		end
		
		local resp = { headers = {} }
		local slines = {}
		for k,v in ipairs(hdrs) do
			-- parsing headers
			local r,_,name,value = v:find("([-_%a]+):%s+(.*)")
			if not r then
				-- not a header, try parsing as status line
				if not #resp.headers == 0 then
					-- request headers come after status lines
					error("invalid response output...'"..v.."'")
				end

				local r,_,proto,code,msg = v:find("(HTTP/%d.%d)%s+(%d+)%s+(%a+)")

				if not r then
					-- invalid status line
					error("invalid status line in response '"..v.."'")
				end
				slines[#slines+1] = {proto = proto, code = tonumber(code), msg = msg}
			else
				resp.headers[name] = value
			end
		end
		if #slines == 2 then
			-- response has a connection status line
			resp.status = slines[2]
			resp.conn = slines[1]
		elseif #slines == 1 then
			-- response does not have a connection status line
			resp.status = slines[1]
		else
			-- invalid response
			error("invalid response, no status found")
		end
		-- cleanup request files
		rm(id)
		resp.code = resp.status.code

		return resp
	end

	-- url required
	assert(url, "Http requires a valid URL")
	assert(attrs, "Http requires a table of parameters")
	
	local reqid = genid()
	local args = {
		s = true,
		D = reqid,
		X = attrs.method or 'GET'
	}

	if attrs.headers then
		-- append headers
		args.H = {}
		for k,v in ipairs(attrs.headers) do
			args.H[#args.H+1] = k..': '..v
		end
	end

	if attrs.body then
		-- append request body
		if not args.H then args.H = {[1] = 'Content-Type: text/plain'}
		else args.H[#args.H+1] = 'Content-Type: text/plain' end

		if type(attrs.body) == 'table' then
			args.data = json.encode(attrs.body)
			args.H[#args.H] = 'Content-Type: application/json'
		elseif type(attrs.body) == 'string' then
			args.data = tostring(attrs.body)
		else
			args.data = attrs.body
		end
	elseif attrs.form then
		-- append multipart form
		args.F = {}
		for k,v in pairs(attrs.form) do
			print(k,v)
			args.F[#args.F+1] = k..'='..v
		end
	elseif attrs.params then
		-- append url encoded form
		args.d = {}
		for k,v in ipairs(attrs.form) do
			args.d[#args.d] = k..'='..v
		end
	end
	
	-- perform http request
	local res,ok = _curl(args, url)
	if not ok then
		-- remove id file if it exists and error out
		rm('-rf', reqid)
		error("Http - invoking curl command failed - '"..res.cmd.."'")
	end
	
	-- parse response status
	local r,resp = pcall(parse_resp, reqid)
	if not r then
		-- remove id file if it exists and error out
		rm('-rf', reqid)
		error(resp)
	end
	-- the body of the response is actually res
	resp.body = tostring(res)
	return Response(resp)
end

local Http = setmetatable({
	-- Http status codes
	Ok = 200,
	Created  = 201,
	Accepted = 202,
	NoNAuthoritative = 203,
	NoContent = 204,
	ResetContent = 205,
	PartialContent = 206,
	MultiStatus = 207,
	AlreadyReported = 208,
	MultipleChoices = 300,
	MovedPermenantly = 301,
	Found = 302,
	SeeOther = 303,
	NotModified = 304,
	UseProxy = 305,
	TemporaryRedirect = 307,
	PermenantRedirect = 308,
	BadRequest = 400,
	Unauthorized = 401,
	PaymentRequired = 402,
	Forbidden = 403,
	NotFound = 404,
	MethodNotAllowed = 405,
	NotAcceptable = 406,
	ProxyAuthenticationRequired = 407,
	RequestTimeout = 408,
	Conflict = 409,
	Gone = 410,
	LengthRequired = 411,
	PreconditionFailed = 412,
	PayloadTooLarge = 413,
	URITooLong = 414,
	UnsupportedMediaType = 415,
	RangeNotSatisifiable = 416,
	ExpectationFailed = 417,
	MisdirectedRequest = 421,
	UnprocessableEntity = 422,
	Locked = 423,
	FailedDependency = 424,
	TooEarly = 425,
	UpgradedRequired = 426,
	PreconditionRequired = 428,
	RooManyRequests = 429,
	RequestHeaderFieldsTooLarge = 431,
	UnavailableForLegalReasons = 451,
	InternalServerError = 500,
	NotImplemented = 501,
	BadGateway = 502,
	ServiceUnavailable = 503,
	GatewayTimeout  = 504,
	HttpVersionNotSupported = 505,
	VariantAlsoNegotiates = 506,
	InsufficientStorge = 507,
	LoopDetected = 508,
	NotExtended = 510,
	NetworkAuthenticationRequired = 511
}, {
	__call = function(this, attrs, url)
		return _Http(this, attrs, url)
	end,
	__newindex = Constify('Http')
})

return function() return Http, Response end