#include <string>
#include "isteamhttp.h"
#include "steamhttp.h"
#include "method.h"
#include "lua.h"
#include "threading.h"

using namespace GarrysMod;

void runFailedHandler(Lua::ILuaBase *LUA, int handler, std::string reason) {
	if (!handler)
		return;

	// Push fail handler to stack and free our ref
	LUA->ReferencePush(handler);
	LUA->ReferenceFree(handler);

	// Push the argument
	LUA->PushString(reason.c_str());

	// Call the fail handler with one argument
	LUA->Call(1, 0);
}

void runSuccessHandler(Lua::ILuaBase *LUA, int handler, HTTPResponse response) {
	if (!handler)
		return;

	// Push success handler to stack and free our ref
	LUA->ReferencePush(handler);
	LUA->ReferenceFree(handler);

	// Push the arguments
	LUA->PushNumber(response.code);
	LUA->PushString(response.body.c_str());
	mapToLuaTable(LUA, response.headers);

	// Call the success handler with three arguments
	LUA->Call(3, 0);
}

bool processRequest(HTTPRequest request) {
	HTTPRequestHandle reqhandle;
	bool ret = true;
	HTTPResponse response = HTTPResponse();

	reqhandle = SteamHTTP()->CreateHTTPRequest(request.method, buildUrl(request).c_str());

	if (reqhandle == INVALID_HTTPREQUEST_HANDLE) {
		failed.push({request.failed, "Failed to init request handle!"});
		ret = false;
		goto cleanup;
	}

	if (!SteamHTTP->SendHTTPRequest(reqhandle, SteamAPICall_t *pCallHandle)) {
		failed.push({request.failed, "Failure while sending HTTP request."});
		ret = false;
		goto cleanup;
	}

	success.push({request.success, response});

cleanup:
	if (handle != INVALID_HTTPREQUEST_HANDLE)
		SteamHTTP()->ReleaseHTTPRequest(handle);

	return ret;
}

/*
 * See https://wiki.garrysmod.com/page/Global/HTTP for documentation.
 * The function takes a single table argument, based off the HTTPRequest structure.
 * It returns a boolean whether a request was sent or not.
 */
LUA_FUNCTION(STEAMHTTP) {
	HTTPRequest request = HTTPRequest();
	bool ret;

	if (!LUA->IsType(1, Lua::Type::TABLE)) {
		LOG("No HTTPRequest table set.");
		ret = false;
		goto exit;
	}

	// Fetch failed handler
	LUA->GetField(1, "failed");
	if (LUA->IsType(-1, Lua::Type::FUNCTION)) {
		request.failed = LUA->ReferenceCreate();
	} else {
		LUA->Pop();
	}

	// Fetch method
	LUA->GetField(1, "method");
	if (LUA->IsType(-1, Lua::Type::STRING)) {
		request.method = methodFromString(LUA->GetString(-1));
	} else {
		request.method = k_EHTTPMethodGET;
	}
	if (request.method == k_EHTTPMethodInvalid) {
		runFailedHandler(LUA, request.failed, "Unsupported request method: " + std::string(LUA->GetString(-1)));
		ret = false;
		goto exit;
	}
	LUA->Pop();

	// Fetch url
	LUA->GetField(1, "url");
	if (LUA->IsType(-1, Lua::Type::STRING)) {
		request.url = LUA->GetString(-1);
	} else {
		runFailedHandler(LUA, request.failed, "invalid url");
		ret = false;
		goto exit;
	}
	LUA->Pop();

	// Fetch success handler
	LUA->GetField(1, "success");
	if (LUA->IsType(-1, Lua::Type::FUNCTION)) {
		request.success = LUA->ReferenceCreate();
	} else {
		LUA->Pop();
	}

	// Fetch headers
	LUA->GetField(1, "headers");
	if (LUA->IsType(-1, Lua::Type::TABLE)) {
		request.headers = mapFromLuaTable(LUA, -1);
	}
	LUA->Pop();

	// Fetch parameters
	LUA->GetField(1, "parameters");
	if (LUA->IsType(-1, Lua::Type::TABLE)) {
		request.parameters = mapFromLuaTable(LUA, -1);
	}
	LUA->Pop();

	// Fetch type
	LUA->GetField(1, "type");
	if (LUA->IsType(-1, Lua::Type::STRING)) {
		request.type = LUA->GetString(-1);
	} else {
		request.type = "text/plain; charset=utf-8";
	}
	LUA->Pop();

	// Fetch body
	LUA->GetField(1, "body");
	if (LUA->IsType(-1, Lua::Type::STRING)) {
		request.body = LUA->GetString(-1);
	}
	LUA->Pop();

	ret = scheduleRequest(request);

exit:
	LUA->PushBool(ret); // Push result to the stack
	return 1; // We are returning a single value
}

GMOD_MODULE_OPEN() {
	// We are working on the global table today
	LUA->PushSpecial(Lua::SPECIAL_GLOB);

	// Push the function mapping (first is the key/function name,
	// second is the value/actual function)
	LUA->PushString("STEAMHTTP");
	LUA->PushCFunction(STEAMHTTP);

	// SetTable takes the item at the top of the stack (value) and
	// the second item from the top (key) and adds them to the table
	// at the stack offset mentioned in the parameter (again, -1 is the top)
	LUA->SetTable(-3);


	// Get the hook.Add method
	LUA->GetField(-1, "hook");
	LUA->GetField(-1, "Add");

	// Push the new hook data
	LUA->PushString("Think");
	LUA->PushString("__steamhttpThinkHook");
	LUA->PushCFunction(threadingDoThink);

	// Add the hook
	LUA->Call(3, 0);

	// Pop the "hook" table
	LUA->Pop();

	// Pop the global table from the stack again
	LUA->Pop();

	return 0;
}

GMOD_MODULE_CLOSE() {
	return 0;
}
