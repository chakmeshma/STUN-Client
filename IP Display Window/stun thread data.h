#pragma once

#include <string>
#include <mutex>

enum class STUNState : unsigned char {
	Unknown,
	SuccessFetched,
	Fetching,
	Error
};

struct STUNThreadData
{
	std::string targetAddress;
	unsigned short targetPort;
	STUNState state = STUNState::Unknown;
	std::string result;
	std::mutex* dataLock;
};
