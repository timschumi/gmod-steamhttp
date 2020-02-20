#include "GarrysMod/Lua/Interface.h"
#include "http.h"

struct QueuedRequestData {
	HTTPRequest request;
	HTTPRequestHandle reqhandle;
	SteamAPICall_t apicall;
};
