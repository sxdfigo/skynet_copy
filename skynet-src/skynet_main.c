#include "skynet.h"

#include "skynet_imp.h"
#include "skynet_env.h"
#include "skynet_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <signal.h>
#include <assert.h>

// 详见 optstring() 注释
static int
optint(const char *key, int opt) {
	const char * str = skynet_getenv(key);
	if (str == NULL) {
		char tmp[20];
		sprintf(tmp,"%d",opt);
		skynet_setenv(key, tmp);
		return opt;
	}
	return strtol(str, NULL, 10);
}

/*
static int
optboolean(const char *key, int opt) {
	const char * str = skynet_getenv(key);
	if (str == NULL) {
		skynet_setenv(key, opt ? "true" : "false");
		return opt;
	}
	return strcmp(str,"true")==0;
}
*/

// 根据key设置环境变量, 
// 如果环境变量不存在, 则设置为 opt 的值
// 如果环境变量已经存在, 则返回对应的值
static const char *
optstring(const char *key,const char * opt) {
	const char * str = skynet_getenv(key);
	if (str == NULL) {
		if (opt) {
			skynet_setenv(key, opt);
			opt = skynet_getenv(key);
		}
		return opt;
	}
	return str;
}

static void
_init_env(lua_State *L) {
	lua_pushnil(L);  /* first key */
	while (lua_next(L, -2) != 0) {
		int keyt = lua_type(L, -2);
		if (keyt != LUA_TSTRING) {
			fprintf(stderr, "Invalid config table\n");
			exit(1);
		}
		const char * key = lua_tostring(L,-2);
		if (lua_type(L,-1) == LUA_TBOOLEAN) {
			int b = lua_toboolean(L,-1);
			skynet_setenv(key,b ? "true" : "false" );
		} else {
			const char * value = lua_tostring(L,-1);
			if (value == NULL) {
				fprintf(stderr, "Invalid config table key = %s\n", key);
				exit(1);
			}
			skynet_setenv(key,value);
		}
		lua_pop(L,1);
	}
	lua_pop(L,1);
}

int sigign() {
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction(SIGPIPE, &sa, 0);
	return 0;
}

static const char * load_config = "\
	local config_name = ...\
	local f = assert(io.open(config_name))\
	local code = assert(f:read \'*a\')\
	local function getenv(name) return assert(os.getenv(name), name) end\
	code = string.gsub(code, \'%$([%w_%d]+)\', getenv)\
	print(code)\
	f:close()\
	local result = {}\
	assert(load(code,\'=(load)\',\'t\',result))()\
	return result\
";

int
main(int argc, char *argv[]) {
	const char * config_file = "config";
	if (argc > 1) {
		config_file = argv[1];
	}
	skynet_globalinit(); // 详见skynet_server.c, 初始化 G_NODE
	skynet_env_init(); // 详见skynet_env.c, 初始化 E, E为全部变量, // todo 作用未知

	sigign(); // 忽略SIGPIPE, 详细google SIGPIPE

	struct skynet_config config;

	struct lua_State *L = lua_newstate(skynet_lalloc, NULL); // 创建的一个新的独立的状态机, 使用自定义的内存分配操作skynet_lalloc
	luaL_openlibs(L);	// link lua lib // 加载 lua 通用扩展库

	int err = luaL_loadstring(L, load_config); // luaL_loadstring ,将所有代码读入lua(详见 load_config 定义),并且检查代码是否有语法错误, for 下面的 lua_pcall
	assert(err == LUA_OK);
	lua_pushstring(L, config_file);
	
	err = lua_pcall(L, 1, 1, 0);
	if (err) {
		fprintf(stderr,"%s\n",lua_tostring(L,-1));
		lua_close(L);
		return 1;
	} 
	_init_env(L);

	config.thread =  optint("thread",8);
	config.module_path = optstring("cpath","./cservice/?.so");
	config.harbor = optint("harbor", 1);
	config.bootstrap = optstring("bootstrap","snlua bootstrap");
	config.daemon = optstring("daemon", NULL);
	config.logger = optstring("logger", NULL);

	lua_close(L); // todo 为什么在这里可以 lua_close()? 不需要等到 return 0; 前才 lua_close()?

	skynet_start(&config);
	skynet_globalexit();

	return 0;
}
