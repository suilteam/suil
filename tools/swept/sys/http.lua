local X = include('sys/sh')
local Json = import('sys/json')

local function genid()
    local cmd = X('_head', { c = 16 }, '/dev/urandom') |
                X('_md5sum', { b = true }) |
                X('_awk', "'{print $1}'")
    return cmd():s()
end

local Response = setmetatable({
    json = function(s)
        if not s._json and s.body then
            s._json = Json:decode(s.body)
        end
        return s._json
    end
}, {
    __call = function(this, attrs)
        -- wraps the given table into a response object
        return setmetatable(attrs, { __index = this })
    end
})

local function _Http(url, attrs)
    function parse_resp(id)
        local read = X('_cat', id) | X("_grep", "' '")
        local _hdrs = read():s()
        Log:trc(_hdrs)

        local hdrs = _hdrs:split("\r\n")
        if #hdrs < 1 then
            error("headers '" .. _hdrs .. "' are not valid")
        end

        local _s = 0
        local _ih = false
        local resp = { headers = {} }
        for k, v in ipairs(hdrs) do
            _ih = _ih or v:find("([-_%a]+):%s+(.*)")
            if not _ih then
                -- still parsing status line
                local r, _, proto, code, msg = v:find("(HTTP/%d.*)%s+(%d+)%s+(%a*)")

                if not r then
                    -- invalid status line
                    error("invalid status line in response '" .. v .. "'")
                end

                if not resp._conn then
                    resp._conn = { proto = proto, code = code, msg = msg }
                else
                    resp.status = tonumber(code)
                    resp._status = { proto = proto, code = code, msg = msg }
                end
            else
                -- parsing headers
                local r, _, name, value = v:find("([-_%a]+)%s*:%s*(.*)")
                if not r then
                    -- invalid status line
                    error("invalid header line in response '" .. v .. "'")
                end
                resp.headers[name] = value
            end
        end
        if not resp.status and resp._conn then
            resp._status = resp._conn
            resp.status = tonumber(resp._conn.code)
            resp._conn = nil
        end
        -- cleanup request files
        rm(id)

        return resp
    end

    -- url required
    assert(url ~= nil and type(url) == 'string', "Http requires a valid URL")
    assert(attrs ~= nil and type(attrs) == 'table', "Http requires a table of parameters")

    local reqid = genid()
    reqid = Dirs.DATA and Dirs.DATA..'/'..reqid or reqid

    local args = {
        s = true,
        D = reqid,
        X = attrs.method or 'GET'
    }

    if attrs.headers then
        -- append headers
        args.H = {}
        for k, v in ipairs(attrs.headers) do
            args.H[#args.H + 1] = k .. ': ' .. v
        end
    end

    if attrs.body then
        -- append request body
        if not args.H then
            args.H = { [1] = 'Content-Type: text/plain' }
        else
            args.H[#args.H + 1] = 'Content-Type: text/plain'
        end

        if type(attrs.body) == 'table' then
            args.data = Json:encode(attrs.body)
            args.H[#args.H] = 'Content-Type: application/json'
        elseif type(attrs.body) == 'string' then
            args.data = tostring(attrs.body)
        else
            args.data = attrs.body
        end
    elseif attrs.form then
        -- append multipart form
        args.F = {}
        for k, v in pairs(attrs.form) do
            args.F[#args.F + 1] = k .. '=' .. tostring(v)
        end
    elseif attrs.params then
        -- append url encoded form
        args.d = {}
        args.G = true
        for k, v in pairs(attrs.params) do
            args.d[#args.d + 1] = k .. '=' .. tostring(v)
        end
    end

    -- perform http request
    local res,ok,_ = _curl(args, url)
    if not ok then
        -- remove id file if it exists and error out
        rm('-rf', reqid)
        error("Http - curl exited with failure code "..tostring(res))
    end

    -- parse response status
    local r, resp = pcall(parse_resp, reqid)
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
    Created = 201,
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
    GatewayTimeout = 504,
    HttpVersionNotSupported = 505,
    VariantAlsoNegotiates = 506,
    InsufficientStorge = 507,
    LoopDetected = 508,
    NotExtended = 510,
    NetworkAuthenticationRequired = 511
}, {
    __call = function(_, url, attrs)
        return _Http(url, attrs)
    end
})

local Verifier = setmetatable({
    HasHeader = function(this, name, fmt, ...)
        Test(this.resp.headers[name], fmt, ...)
        return this
    end,

    IsHeader = function(this, name, value, fmt, ...)
        Equal(this.resp.headers[name], value, fmt, ...)
        return this
    end,

    IsStatus = function(this, value, fmt, ...)
        Equal(this.resp.status, value, fmt, ...)
        return this
    end,
    WithFunc = function(this, func)
        if func and type(func) == 'function' then
            func(this.resp)
        end
        return this
    end
}, {
    __call = function(this, resp)
        return setmetatable({ resp = resp}, {
            __index = this
        })
    end
})

local Jwt = setmetatable({
    AnyRole = function(this, ...)
        local roles = {...}
        local claims = this.payload.claims

        if #roles == 0 then return true end
        if not claims.roles or #claims.roles == 0 then return false end
        for _,rx in ipairs(claims.roles) do
            for _,r in ipairs(roles) do if r == rx then return true end end
        end
        return false
    end,
    AllRoles = function(this, ...)
        local roles = {...}
        local claims = this.payload.claims

        if not claims.roles or #claims.roles == 0 then return false end
        local function find(r)
            for _,rx in ipairs(claims.roles) do if r == rx then return true end end
            return false
        end
        for _,r in ipairs(roles) do
            if not find(r) then return false end
        end
        return true
    end
}, {
    __call = function(this, token)
        token = token:gsub('Bearer ', '')
        local parts  = {}
        for p in token:gmatch('[^%.]+') do
            parts[#parts + 1] = p
        end

        if #parts ~= 3 then return false end
        return setmetatable( {
            header  = Json:decode(Base64:decode(parts[1])),
            payload = Json:decode(Base64:decode(parts[2])),
            signature = parts[3]
        }, {
            __index = this
        })
    end
})

return function () return Http, Response, Verifier, Jwt end