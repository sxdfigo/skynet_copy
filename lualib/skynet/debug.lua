local table = table
local extern_dbgcmd = {}

local function init(skynet, export)
	local internal_info_func

	function skynet.info_func(func)
		internal_info_func = func
	end

	local dbgcmd

	local function init_dbgcmd()
		dbgcmd = {}

		function dbgcmd.MEM()
			local kb, bytes = collectgarbage "count"
			skynet.ret(skynet.pack(kb,bytes))
		end

		function dbgcmd.GC()

			collectgarbage "collect"
		end

		function dbgcmd.STAT()
			local stat = {}
			stat.task = skynet.task()
			stat.mqlen = skynet.stat "mqlen"
			stat.cpu = skynet.stat "cpu"
			stat.message = skynet.stat "message"
			skynet.ret(skynet.pack(stat))
		end

		function dbgcmd.TASK()
			local task = {}
			skynet.task(task)
			skynet.ret(skynet.pack(task))
		end

		function dbgcmd.INFO(...)
			if internal_info_func then
				skynet.ret(skynet.pack(internal_info_func(...)))
			else
				skynet.ret(skynet.pack(nil))
			end
		end

		function dbgcmd.EXIT()
			skynet.exit()
		end

		function dbgcmd.RUN(source, filename)
			local inject = require "skynet.inject"
			local ok, output = inject(skynet, source, filename , export.dispatch, skynet.register_protocol)
			collectgarbage "collect"
			skynet.ret(skynet.pack(ok, table.concat(output, "\n")))
		end

		function dbgcmd.TERM(service)
			skynet.term(service)
		end

		function dbgcmd.REMOTEDEBUG(...)
			local remotedebug = require "skynet.remotedebug"
			remotedebug.start(export, ...)
		end

		function dbgcmd.SUPPORT(pname)
			return skynet.ret(skynet.pack(skynet.dispatch(pname) ~= nil))
		end

		function dbgcmd.PING()
			return skynet.ret()
		end

		function dbgcmd.LINK()
			skynet.response()	-- get response , but not return. raise error when exit
		end
        
        function dbgcmd.TRACELOG(proto, flag)
        	if type(proto) ~= "string" then
        		flag = proto
                proto = "lua"
        	end
        	skynet.error(string.format("Turn trace log %s for %s", flag, proto))
        	skynet.traceproto(proto, flag)
        	skynet.ret()
        end

        local old_snapshot
        local snapshot = require "snapshot"
        local construct_indentation = (require "skynet.snapshot_utils").construct_indentation
        function dbgcmd.SNAPSHOT()
			collectgarbage "collect"
            local new_snapshot = snapshot()
            if not old_snapshot then
                old_snapshot = new_snapshot
                return skynet.ret(skynet.pack({}))
            end
            local diff = {}
            for k,v in pairs(new_snapshot) do
                if not old_snapshot[k] then
                    diff[k] = v
                end
            end
            old_snapshot = new_snapshot
            local ret = construct_indentation(diff)
            skynet.ret(skynet.pack(ret))
        end

		return dbgcmd
	end -- function init_dbgcmd

	local function _debug_dispatch(session, address, cmd, ...)
		dbgcmd = dbgcmd or init_dbgcmd() -- lazy init dbgcmd
		local f = dbgcmd[cmd] or extern_dbgcmd[cmd]
		assert(f, cmd)
		f(...)
	end

	skynet.register_protocol {
		name = "debug",
		id = assert(skynet.PTYPE_DEBUG),
		pack = assert(skynet.pack),
		unpack = assert(skynet.unpack),
		dispatch = _debug_dispatch,
	}
end

local function reg_debugcmd(name, fn)
	extern_dbgcmd[name] = fn
end

return {
	init = init,
	reg_debugcmd = reg_debugcmd,
}
