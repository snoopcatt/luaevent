/* LuaEvent - Copyright (C) 2007 Thomas Harning <harningt@gmail.com>
 * Licensed as LGPL - See doc/COPYING for details */

#include "luaevent.h"
#include "event_callback.h"
#include "event_buffer.h"

#include <lua.h>
#include <lauxlib.h>
#include <assert.h>

#define EVENT_BASE_MT "EVENT_BASE_MT"

le_base* event_base_get(lua_State* L, int idx) {
	return (le_base*)luaL_checkudata(L, idx, EVENT_BASE_MT);
}

int luaevent_newbase(lua_State* L) {
	le_base *base = (le_base*)lua_newuserdata(L, sizeof(le_base));
	base->loop_L = NULL; /* No running loop */
	base->base = event_init();
	luaL_getmetatable(L, EVENT_BASE_MT);
	lua_setmetatable(L, -2);
	return 1;
}

static int luaevent_base_gc(lua_State* L) {
	le_base *base = event_base_get(L, 1);
	if(base->base) {
		event_base_free(base->base);
		base->base = NULL;
	}
	return 0;
}

int getSocketFd(lua_State* L, int idx) {
	int fd;
	luaL_checktype(L, idx, LUA_TUSERDATA);
	lua_getfield(L, idx, "getfd");
	if(lua_isnil(L, -1))
		return luaL_error(L, "Socket type missing 'getfd' method");
	lua_pushvalue(L, idx);
	lua_call(L, 1, 1);
	fd = lua_tointeger(L, -1);
	lua_pop(L, 1);
	return fd;
}

void load_timeval(double time, struct timeval *tv) {
	tv->tv_sec = (int)time;
	tv->tv_usec = (int)(time * 1000000) % 1000000;
}

/* sock, event, callback, timeout */
static int luaevent_addevent(lua_State* L) {
	int fd, event;
	le_callback* arg = event_callback_push(L, 1, 4);
	struct timeval *tv = &arg->timeout;
	if(lua_isnil(L, 2) && lua_isnumber(L, 5)) {
		fd = -1; /* Per event_timer_set.... */
	} else {
		fd = getSocketFd(L, 2);
	}
	event = luaL_checkinteger(L, 3);
	if(lua_isnumber(L, 5)) {
		double time = lua_tonumber(L, 5);
		load_timeval(time, tv);
	} else {
		tv = NULL;
	}

	/* Setup event... */
	event_set(&arg->ev, fd, event | EV_PERSIST, luaevent_callback, arg);
	event_base_set(arg->base->base, &arg->ev);
	event_add(&arg->ev, tv);
	return 1;
}

static int luaevent_loop(lua_State* L) {
	le_base *base = event_base_get(L, 1);
	base->loop_L = L;
	int ret = event_base_loop(base->base, 0);
	lua_pushinteger(L, ret);
	return 1;
}

static luaL_Reg base_funcs[] = {
	{ "addevent", luaevent_addevent },
	{ "loop", luaevent_loop },
	{ NULL, NULL }
};

static luaL_Reg funcs[] = {
	{ "new", luaevent_newbase },
	{ NULL, NULL }
};

typedef struct {
	const char* name;
	int value;
} namedInteger;

static namedInteger consts[] = {
	{"LEAVE", -1},
	{"EV_READ", EV_READ},
	{"EV_WRITE", EV_WRITE},
	{"EV_TIMEOUT", EV_TIMEOUT},
	{NULL, 0}
};

void setNamedIntegers(lua_State* L, namedInteger* p) {
	while(p->name) {
		lua_pushinteger(L, p->value);
		lua_setfield(L, -2, p->name);
		p++;
	}
}

/* Verified ok */
int luaopen_luaevent_core(lua_State* L) {
	/* Register external items */
	event_callback_register(L);
	event_buffer_register(L);
	/* Setup metatable */
	luaL_newmetatable(L, EVENT_BASE_MT);
	lua_newtable(L);
	luaL_register(L, NULL, base_funcs);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, luaevent_base_gc);
	lua_setfield(L, -2, "__gc");
	lua_pop(L, 1);

	luaL_register(L, "luaevent.core", funcs);
	setNamedIntegers(L, consts);
	return 1;
}
