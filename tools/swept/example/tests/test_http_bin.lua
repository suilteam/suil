--
-- Created by IntelliJ IDEA.
-- User: dc
-- Date: 2020-03-06
-- Time: 12:33 a.m.
-- To change this template use File | Settings | File Templates.
--

local Http,Response, Verfifier = import('sys/http')

local HttpBinFixture = Fixture("HttpBinFixture", "A simple fixture to demostrate useage of Http modile")

HttpBinFixture('HttBinGetTest', "Tests retrieving content from HttpBin")
:before(function(ctx)
    ctx.URL = "https://httpbin.org/get"
end)
:run(function(ctx)
    -- run query
    local resp = Http(ctx.URL, {
        headers = {
            ["Accept"] = "application/json"
        }
    })
    -- verify response
    local V = Verfifier(resp)
    V:IsStatus(Http.Ok, "Request must succeed")
     :WithFunc(function(resp)
        local body = resp:json()
        Test(body and body.headers and body.headers.Host, "HttpBin must return the Host header in its body")
        Equal(body.headers.Host, "httpbin.org", "Host must be httpbin.org")
     end)
     :HasHeader('content-type')
end)

return HttpBinFixture

