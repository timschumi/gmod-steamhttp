#include "isteamhttp.h"

EHTTPMethod methodFromString(std::string method);
std::string methodToString(EHTTPMethod method);
bool isLikePost(EHTTPMethod method);
