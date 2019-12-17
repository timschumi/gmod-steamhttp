#include <string>
#include "method.h"

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

// Turns a method int back into a string
std::string methodToString(EHTTPMethod method) {
	switch (method) {
	case k_EHTTPMethodGET:
		return "GET";
	case k_EHTTPMethodPOST:
		return "POST";
	case k_EHTTPMethodHEAD:
		return "HEAD";
	case k_EHTTPMethodPUT:
		return "PUT";
	case k_EHTTPMethodDELETE:
		return "DELETE";
	case k_EHTTPMethodPATCH:
		return "PATCH";
	case k_EHTTPMethodOPTIONS:
		return "OPTIONS";
	default:
		return "Invalid";
	}
}

bool isLikePost(EHTTPMethod method) {
	switch (method) {
	case k_EHTTPMethodPOST:
	case k_EHTTPMethodPUT:
	case k_EHTTPMethodDELETE:
	case k_EHTTPMethodPATCH:
		return true;
	default:
		return false;
	}
}
