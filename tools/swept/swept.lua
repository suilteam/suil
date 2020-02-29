---
--- initialize runtime
---
package.path = package.path .. ";sys/?.lua"

-- utils are global functions
require "utils"
-- bring in sh which provides shell function utilities
require "sh"
