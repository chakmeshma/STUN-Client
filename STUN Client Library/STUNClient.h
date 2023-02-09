#pragma once

#ifdef STUNCLIENTLIBRARY_EXPORTS
#define STUNCLIENTLIBRARY_API __declspec(dllexport)
#else
#define STUNCLIENTLIBRARY_API __declspec(dllimport)
#endif

#include <string>

STUNCLIENTLIBRARY_API bool start(std::string& result);
STUNCLIENTLIBRARY_API bool fetch(const char* hostAddress, const unsigned short hostPort, std::string& result);
STUNCLIENTLIBRARY_API void stop();