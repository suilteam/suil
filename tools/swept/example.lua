require "swept"

local Http, Response = using("http")

local stop = now() + 10;
local reqs = 0

while now() < stop do
    local resp = Http({},"localhost:1080/hello")
    if resp.code == Http.Ok then
        reqs = reqs + 1
    end
end

print("executed "..reqs.." in 10 seconds")