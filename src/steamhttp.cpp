#include <string>
#include <vector>
#include "steam_api.h"
#include "steamhttp.h"
#include "lockqueue.h"
#include "lua.h"

using namespace GarrysMod;

std::string HEADERS[] = {
	"Connection",
	"Content-Encoding",
	"Content-Type",
	"Date",
	"Transfer-Encoding",
	"Vary",
	"Cache-Control",
	"Expires",
	"Get-Dictionary",
	"Pragma",
	"Set-Cookie",
	"X-Powered-By",
	"X-UA-Compatible",
	"Allow",
	"Last-Modified",
	"Refresh",
	"Warning",
	"Access-Control-Allow-Origin",
	"Accept-Ranges",
	"Content-Range",
	"Content-Location",
	"Content-MD5",
	"Content-Length",
	"Retry-After",
	"X-RateLimit-Limit",
	"X-RateLimit-Remaining",
	"X-RateLimit-Reset",
	"X-Rate-Limit-Limit",
	"X-Rate-Limit-Remaining",
	"X-Rate-Limit-Reset",
	"ETag",
};

LockableQueue<QueuedRequestData> requests;

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

// Turns a string method into an int
EHTTPMethod methodFromString(std::string method) {
	if (method.compare("GET") == 0)
		return k_EHTTPMethodGET;
	if (method.compare("POST") == 0)
		return k_EHTTPMethodPOST;
	if (method.compare("HEAD") == 0)
		return k_EHTTPMethodHEAD;
	if (method.compare("PUT") == 0)
		return k_EHTTPMethodPUT;
	if (method.compare("DELETE") == 0)
		return k_EHTTPMethodDELETE;
	if (method.compare("PATCH") == 0)
		return k_EHTTPMethodPATCH;
	if (method.compare("OPTIONS") == 0)
		return k_EHTTPMethodOPTIONS;

	return k_EHTTPMethodInvalid;
}

bool createHTTPResponse(HTTPRequestHandle request, SteamAPICall_t apicall, HTTPResponse *response, std::string failreason) {
	bool failed = true;
	HTTPRequestCompleted_t reqcomplete;

	if (!SteamUtils()->GetAPICallResult(apicall, &reqcomplete, sizeof(reqcomplete), reqcomplete.k_iCallback, &failed)) {
		failreason.assign("Could not fetch API Call Result.");
		return false;
	}

	if (failed) {
		failreason.assign("API Call failed.");
		return false;
	}

	if (!reqcomplete.m_bRequestSuccessful) {
		failreason.assign("HTTP Request was unsuccessful");
		return false;
	}

	response->code = reqcomplete.m_eStatusCode;

	// Initialize char* with correct size and copy it over
	std::vector<uint8> buffer(reqcomplete.m_unBodySize);
	SteamHTTP()->GetHTTPResponseBodyData(request, &buffer[0], reqcomplete.m_unBodySize);
	response->body = std::string(buffer.begin(), buffer.end());

	for (std::string header : HEADERS) {
		uint32 headersize;

		if (!SteamHTTP()->GetHTTPResponseHeaderSize(request, header.c_str(), &headersize)
		    || headersize <= 0)
			continue;

		std::vector<uint8> headerbuf(headersize);

		if (!SteamHTTP()->GetHTTPResponseHeaderValue(request, header.c_str(), &headerbuf[0], headersize))
			continue;

		response->headers[header] = std::string(headerbuf.begin(), headerbuf.end());
	}

	return true;
}

void addHeaders(HTTPRequestHandle handle, HTTPRequest request) {
	// Check if we have to append something to the User-Agent
	// The `useragent` parameter overwrites the header
	if (request.useragent.size() != 0)
		SteamHTTP()->SetHTTPRequestUserAgentInfo(handle, request.useragent.c_str());
	else if (request.headers.count("User-Agent") != 0)
		SteamHTTP()->SetHTTPRequestUserAgentInfo(handle, request.headers["User-Agent"].c_str());

	// Add the Content-Type header if not already set
	if (request.headers.count("Content-Type") == 0)
		SteamHTTP()->SetHTTPRequestHeaderValue(handle, "Content-Type", request.type.c_str());

	// Add all the headers from the request struct
	for (auto const& e : request.headers)
		SteamHTTP()->SetHTTPRequestHeaderValue(handle, e.first.c_str(), e.second.c_str());
}

bool processRequest(Lua::ILuaBase *LUA, HTTPRequest request) {
	HTTPRequestHandle reqhandle;
	SteamAPICall_t apicall;

	reqhandle = SteamHTTP()->CreateHTTPRequest(request.method, request.url.c_str());

	if (reqhandle == INVALID_HTTPREQUEST_HANDLE) {
		runFailedHandler(LUA, request.failed, "Failed to init request handle!");
		return false;
	}

	addHeaders(reqhandle, request);

	// Adding body (if available)
	if (request.body.size() != 0)
		SteamHTTP()->SetHTTPRequestRawPostBody(reqhandle, request.type.c_str(), std::vector<uint8>(request.body.begin(), request.body.end()).data(), request.body.size());

	// Adding parameters
	for (auto const& e : request.parameters)
		SteamHTTP()->SetHTTPRequestGetOrPostParameter(reqhandle, e.first.c_str(), e.second.c_str());

	if (!SteamHTTP()->SendHTTPRequest(reqhandle, &apicall)) {
		runFailedHandler(LUA, request.failed, "Failure while sending HTTP request.");
		SteamHTTP()->ReleaseHTTPRequest(reqhandle);
		return false;
	}

	requests.push({request, reqhandle, apicall});

	return true;
}

// At the moment, this is handling one request per tick.
LUA_FUNCTION(callbackHook) {
	if (requests.empty())
		return 0;

	QueuedRequestData queued = requests.pop();
	bool failed = true;

	// If this isn't done, push back onto the queue and stop
	if (!SteamUtils()->IsAPICallCompleted(queued.apicall, &failed)) {
		requests.push(queued);
		return 0;
	}

	if (failed) {
		runFailedHandler(LUA, queued.request.failed,
		                 "API Error: " + SteamUtils()->GetAPICallFailureReason(queued.apicall));
		SteamHTTP()->ReleaseHTTPRequest(queued.reqhandle);
		return 0;
	}

	HTTPResponse response = HTTPResponse();
	std::string failreason = "";

	if (!createHTTPResponse(queued.reqhandle, queued.apicall, &response, failreason)) {
		runFailedHandler(LUA, queued.request.failed, "HTTP Error: " + failreason);
		SteamHTTP()->ReleaseHTTPRequest(queued.reqhandle);
		return 0;
        }

	runSuccessHandler(LUA, queued.request.success, response);

	return 0;
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

	// Fetch useragent
	LUA->GetField(1, "useragent");
	if (LUA->IsType(-1, Lua::Type::STRING)) {
		request.useragent = LUA->GetString(-1);
	}
	LUA->Pop();

	ret = processRequest(LUA, request);

exit:
	LUA->PushBool(ret); // Push result to the stack
	return 1; // We are returning a single value
}

GMOD_MODULE_OPEN() {
	// Initialize the SteamAPI
	if (!SteamAPI_Init())
		LOG("Warning: SteamAPI did not initialize correctly! This might cause issues!");

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
	LUA->PushString("__steamhttpCallback");
	LUA->PushCFunction(callbackHook);

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
