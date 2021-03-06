#include <WS2tcpip.h>
#include <iostream>
#include <windows.h>
#include "Message.h"

#pragma comment(lib, "Ws2_32.lib")

#define WIN32_SOCK_MAJ_VER 2
#define WIN32_SOCK_MIN_VER 2
#define DEFAULT_TARGET_PORT 19302

//static struct TransactionState {
//	bool firstRequestSent = false;
//	bool firstResponseReceived = false;
//	uint32 numRequestsSent = 0;
//	uint32 numResponsesReceived = 0;
//	uint32 numIndicationsSent = 0;
//	uint32 numIndicationsReceived = 0;
//	uint32 id[3]{ 0,0,0 };
//};
//
//static std::vector<TransactionState> trasnactions;

static WSADATA wsaData;
static const char strUsage[] = "Usage: stunclient <server name or ip> [server port]\n";
static addrinfo aiHints, *aiResult, *aiSelected;
static SOCKET theSocket;
static bool bWSAInited = false;
static bool bAddrInfoCalled = false;
static bool bSocketCreated = false;
static sockaddr_in localAddress;
static bool finishing = false;



static void cleanup() {
	if (bAddrInfoCalled) freeaddrinfo(aiSelected);
	if (bSocketCreated) closesocket(theSocket);
	if (bWSAInited) WSACleanup();
}

static BOOL WINAPI closeHandler(DWORD dwCtrlType) {
	finishing = true;

	cleanup();

	return true;
}

int main(int argc, char **argv)
{
	SetConsoleCtrlHandler(closeHandler, true);
	int iResult;
	bool isIP;

	if (argc != 3 && argc != 2) {
		std::cerr << "Invalid usage.\n";
		std::cout << strUsage;

		return EXIT_FAILURE;
	}

	iResult = WSAStartup(MAKEWORD(WIN32_SOCK_MAJ_VER, WIN32_SOCK_MIN_VER), &wsaData);

	bWSAInited = true;

	if (iResult != 0) {
		std::cerr << "WSAStartup failed: " << iResult << std::endl;

		cleanup();

		return EXIT_FAILURE;
	}

	char strTargetPort[6];

	_itoa_s(DEFAULT_TARGET_PORT, strTargetPort, 10);

	memset(&aiHints, 0, sizeof(aiHints));

	aiHints.ai_family = AF_INET;
	aiHints.ai_socktype = SOCK_DGRAM;
	aiHints.ai_protocol = IPPROTO_UDP;

	iResult = getaddrinfo(argv[1], (argc == 2) ? (strTargetPort) : (argv[2]), &aiHints, &aiResult);

	bAddrInfoCalled = true;

	if (iResult != 0) {
		std::cerr << "Address resolution failed: " << iResult << std::endl;

		cleanup();

		return EXIT_FAILURE;
	}

	theSocket = socket(aiResult->ai_family, aiResult->ai_socktype, aiResult->ai_protocol);

	bSocketCreated = true;

	if (iResult != 0) {
		std::cerr << "Couldn't get local socket name (address): " << WSAGetLastError() << std::endl;

		cleanup();

		return EXIT_FAILURE;
	}

	if (theSocket == INVALID_SOCKET) {
		std::cerr << "Couldn't create socket: " << WSAGetLastError() << std::endl;

		cleanup();

		return EXIT_FAILURE;
	}

	uint32 requestTransactionID[3];

	Message message = Message(MessageMethod::Binding, MessageClass::Request, std::vector<Attribute>());

	message.getTransactionID(requestTransactionID);

	uint8 pdu[1024];
	uint16 sizePDU = message.encodePacket(pdu);

	/*for (int i = 0; i < 20; i++)
	{
		char ch = pdu[i];
		std::cout << toHex((ch & 0xf0) >> 4) << toHex(ch & 0x0f);
	}

	std::cout << std::endl;*/

	if (sendto(theSocket, (const char*)pdu, sizePDU, 0, aiResult->ai_addr, sizeof(*aiResult->ai_addr)) == SOCKET_ERROR) {
		std::cerr << "UDP send failed: " << WSAGetLastError() << std::endl;
	}

	const uint16 sizeReceiveBuffer = 1024;

	uint8 bufferReceive[sizeReceiveBuffer];

	int sizeLocalAddress = sizeof(localAddress);

	iResult = getsockname(theSocket, (sockaddr*)&localAddress, &sizeLocalAddress);

	uint32 sizeReceived = 0;

	while (!finishing) {

		if ((sizeReceived = recvfrom(theSocket, (char*)bufferReceive, sizeReceiveBuffer, 0, (sockaddr*)&localAddress, &sizeLocalAddress)) == SOCKET_ERROR) {
			std::cerr << "UDP receive failed: " << WSAGetLastError() << std::endl;
		}
		else {
			MessageClass messageClass;
			MessageMethod messageMethod;
			uint32 responseTransactionID[3];
			int transactionCompareResult;

			Message message = Message::fromPacket(bufferReceive, sizeReceived);

			message.getTransactionID(responseTransactionID);

			transactionCompareResult = memcmp(requestTransactionID, responseTransactionID, sizeof(uint32) * 3);

			if (transactionCompareResult != 0)
				continue;

			sockaddr_in *targetAddress = reinterpret_cast<sockaddr_in*>(aiResult->ai_addr);

			uint32 mappedIPv4;
			uint16 mappedPort;
			uint16 targetPort = htons(*reinterpret_cast<uint32*>(&targetAddress->sin_port));
			uint32 targetIPv4 = *reinterpret_cast<uint32*>(&targetAddress->sin_addr);

			message.getMappedAddress(targetIPv4, targetPort, &mappedIPv4, &mappedPort);

			std::cout << "Mapped Address: " \
				<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[3] << "." \
				<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[2] << "." \
				<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[1] << "." \
				<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[0] << ":" << mappedPort << std::endl;
		}
	}

	cleanup();

	return EXIT_SUCCESS;
}