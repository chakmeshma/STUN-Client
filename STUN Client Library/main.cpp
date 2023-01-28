#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif 


#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>
#include "Message.h"

#pragma comment(lib, "Ws2_32.lib")

#define WIN32_SOCK_MAJ_VER 2
#define WIN32_SOCK_MIN_VER 2

static addrinfo aiHints, * aiResult;
static SOCKET theSocket;

static bool bWSAInited = false;
static bool bAddrInfoCalled = false;
static bool bSocketCreated = false;
static bool bFinished = false;


static std::string getWSALastErrorText(int lastErrorCode) {
	char* s = NULL;

	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, lastErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&s, 0, NULL);

	std::string* strError = new std::string(s);

	LocalFree(s);

	return *strError;
}

static void resetStateBooleans() {
	bWSAInited = false;
	bAddrInfoCalled = false;
	bSocketCreated = false;
	bFinished = false;
}

static void cleanup() {
	if (bSocketCreated) closesocket(theSocket);
	if (bAddrInfoCalled) freeaddrinfo(aiResult);
	if (bWSAInited) WSACleanup();

	resetStateBooleans();
}

extern "C" __declspec(dllexport) int fetch(const char* const hostAddress, const unsigned short hostPort, std::string& result)
{
	resetStateBooleans();

	std::stringstream result_ss;
	int iResult;
	WSADATA wsaData;

	iResult = WSAStartup(MAKEWORD(WIN32_SOCK_MAJ_VER, WIN32_SOCK_MIN_VER), &wsaData);
	bWSAInited = true;

	if (iResult != 0) {
		result_ss << "WSAStartup failed: " << getWSALastErrorText(iResult);

		result = result_ss.str();

		cleanup();

		return EXIT_FAILURE;
	}

	char strTargetPort[6];

	_itoa(hostPort, strTargetPort, 10);

	memset(&aiHints, 0, sizeof(aiHints));
	aiHints.ai_family = AF_INET;
	aiHints.ai_socktype = SOCK_DGRAM;
	aiHints.ai_protocol = IPPROTO_UDP;

	iResult = getaddrinfo(hostAddress, strTargetPort, &aiHints, &aiResult);
	bAddrInfoCalled = true;

	if (iResult != 0) {
		result_ss << "getaddrinfo failed: " << getWSALastErrorText(WSAGetLastError());

		result = result_ss.str();

		cleanup();

		return EXIT_FAILURE;
	}

	theSocket = socket(aiResult->ai_family, aiResult->ai_socktype, aiResult->ai_protocol);
	bSocketCreated = true;

	if (theSocket == INVALID_SOCKET) { //TODO: fix this
		result_ss << "Error at socket(): " << getWSALastErrorText(WSAGetLastError());

		result = result_ss.str();

		cleanup();

		return EXIT_FAILURE;
	}

	uint32 requestTransactionID[3];

	Message message = Message(MessageMethod::Binding, MessageClass::Request, std::vector<MessageAttribute>());

	message.getTransactionID(requestTransactionID);

	uint8 pdu[1024];
	uint16 sizePDU = message.encodePacket(pdu);


	if (sendto(theSocket, (const char*)pdu, sizePDU, 0, aiResult->ai_addr, sizeof(*aiResult->ai_addr)) == SOCKET_ERROR) {
		result_ss << "sendto failed with error: " << getWSALastErrorText(WSAGetLastError());

		result = result_ss.str();

		cleanup();

		return EXIT_FAILURE;
	}

	const uint16 sizeReceiveBuffer = 10240;
	uint8 bufferReceive[sizeReceiveBuffer];
	uint32 sizeReceived = 0;

	std::stringstream result_string_stream;

	while (!bFinished) {

		if ((sizeReceived = recvfrom(theSocket, (char*)bufferReceive, sizeReceiveBuffer, 0, nullptr, nullptr)) == SOCKET_ERROR) {
			result_ss << "recvfrom failed with error: " << getWSALastErrorText(WSAGetLastError());

			result = result_ss.str();

			cleanup();

			return EXIT_FAILURE;
		}
		else {
			uint32 responseTransactionID[3];
			int transactionCompareResult;

			Message message = Message::fromPacket(bufferReceive, sizeReceived);

			message.getTransactionID(responseTransactionID);

			transactionCompareResult = memcmp(requestTransactionID, responseTransactionID, sizeof(uint32) * 3);

			if (transactionCompareResult != 0)
				continue;

			sockaddr_in* targetAddress = reinterpret_cast<sockaddr_in*>(aiResult->ai_addr);

			uint32 mappedIPv4;
			uint16 mappedPort;
			uint16 targetPort = htons(*reinterpret_cast<uint32*>(&targetAddress->sin_port));
			uint32 targetIPv4 = *reinterpret_cast<uint32*>(&targetAddress->sin_addr);

			message.getMappedAddress(targetIPv4, targetPort, &mappedIPv4, &mappedPort);

			result_ss << "Mapped Address: " \
				<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[3] << "." \
				<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[2] << "." \
				<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[1] << "." \
				<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[0] << ":" \
				<< mappedPort;

			bFinished = true;
		}
	}

	result = result_ss.str();

	cleanup();

	return EXIT_SUCCESS;
}