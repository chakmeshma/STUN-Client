#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "STUNClient.h"
#include <Windows.h>
#include <iostream>


BOOL WINAPI closeHandler(DWORD CtrlType) {
	stop();

	return false;
}

int main(int argc, char* argv[])
{
	if (argc != 3) {
		std::cout << "Usage: IP Display Console.exe <target STUN server name> <port>" << std::endl;

		return EXIT_FAILURE;
	}

	SetConsoleCtrlHandler(closeHandler, true);

	std::string result("");

	bool successInit = start(result);

	if (!successInit)
	{
		std::cout << result << std::endl;
		return EXIT_FAILURE;
	}

	std::cout << "Contacting " << argv[1] << ":" << argv[2] << "..." << std::endl;

	bool successFetch = fetch(argv[1], atoi(argv[2]), result);

	std::cout << ((successFetch) ? "Successed" : "Failed") << std::endl;
	std::cout << result << std::endl;

	stop();

	return ((successFetch) ? EXIT_SUCCESS : EXIT_FAILURE);
}
