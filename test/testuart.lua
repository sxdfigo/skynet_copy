local skynet = require "skynet"
local uart = require "uart"

local fd

skynet.start(function()
	local cb = function(id,data)
		print("recv data:" .. data)
		if string.sub(data,1,-2) == "quit" then
			uart.close(fd)
			skynet.exit()
		end
		uart.send(fd,"ok "..data)
	end
	fd = uart.open (cb, "/dev/ttyS1")
	uart.set (fd, 115200, 0, 8, 1, 'N')
end)