#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif 

#include "STUNClient.h"
#include "Message.h"
#include "types.h"
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

#define WIN32_SOCK_MAJ_VER 2
#define WIN32_SOCK_MIN_VER 2

#define PDU_SEND_BUFFER_SIZE 128
#define PDU_RECEIVE_BUFFER_SIZE 1024

#define INITIAL_RTO 500
#define RCOUNT 7

static addrinfo aiHints, * aiResult;
static SOCKET theSocket;

static bool bWSAInited = false;
static bool bAddrInfoCalled = false;
static bool bSocketCreated = false;

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

static void reset_cleanup() {
	if (bSocketCreated) {
		shutdown(theSocket, SD_BOTH);
		closesocket(theSocket);
		bSocketCreated = false;
	}

	if (bAddrInfoCalled) {
		freeaddrinfo(aiResult);
		bAddrInfoCalled = false;
	}
}

bool start(std::string& result) {
	if (bWSAInited) stop();

	std::stringstream result_ss;
	int iResult;
	WSADATA wsaData;

	iResult = WSAStartup(MAKEWORD(WIN32_SOCK_MAJ_VER, WIN32_SOCK_MIN_VER), &wsaData);
	bWSAInited = true;

	if (iResult != 0) {
		stop();

		result_ss << "WSAStartup failed: " << getWSALastErrorText(iResult);
		result = result_ss.str();
		return FALSE;
	}

	result_ss << "WSAStartup successful";
	result = result_ss.str();
	return TRUE;
}
bool fetch(const char* hostAddress, const unsigned short hostPort, std::string& result)
{
	if (bWSAInited) reset_cleanup();

	std::stringstream result_ss;
	int iResult;

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
		reset_cleanup();
		return FALSE;
	}

	theSocket = socket(aiResult->ai_family, aiResult->ai_socktype, aiResult->ai_protocol);
	bSocketCreated = true;

	if (theSocket == INVALID_SOCKET) {
		result_ss << "Error at socket(): " << getWSALastErrorText(WSAGetLastError());
		result = result_ss.str();
		reset_cleanup();
		return FALSE;
	}

	std::string software = "Chakmeshma STUN Client";
	MessageAttribute softwareAttr(software.size(), MessageAttributeType::SOFTWARE, reinterpret_cast<const uint8*>(software.c_str()));
	Message requestMessage = Message(MessageMethod::Binding, MessageClass::Request, std::vector<MessageAttribute> {softwareAttr});

	uint32 requestTransactionID[3];
	memcpy(requestTransactionID, requestMessage.transactionID, sizeof(uint32) * 3);

	uint8 requestPDU[PDU_SEND_BUFFER_SIZE];
	uint16 sizePDU = requestMessage.encodeMessage(requestPDU); //TODO handle exceptions

	uint8 transmissions = 0;

	if (sendto(theSocket, (const char*)requestPDU, sizePDU, 0, aiResult->ai_addr, sizeof(*aiResult->ai_addr)) == SOCKET_ERROR) {
		result_ss << "sendto failed with error: " << getWSALastErrorText(WSAGetLastError());
		result = result_ss.str();
		reset_cleanup();
		return FALSE;
	}
	transmissions++;

	uint32 RTO = INITIAL_RTO;

	iResult = setsockopt(theSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&RTO, sizeof(RTO));
	if (iResult == SOCKET_ERROR) {
		result_ss << "setsockopt for SO_RCVTIMEO failed with error: " << getWSALastErrorText(WSAGetLastError());
		result = result_ss.str();
		reset_cleanup();
		return FALSE;
	}

	uint8 bufferReceive[PDU_RECEIVE_BUFFER_SIZE];
	uint32 sizeReceived = 0;

	while (true) {

		if ((sizeReceived = recvfrom(theSocket, (char*)bufferReceive, PDU_RECEIVE_BUFFER_SIZE, 0, nullptr, nullptr)) == SOCKET_ERROR) {
			result_ss << "recvfrom failed with error: " << getWSALastErrorText(WSAGetLastError());
			result = result_ss.str();
			reset_cleanup();
			return FALSE;
		}
		else {
			if (sizeReceived == 0)
			{
				if (transmissions >= RCOUNT)
				{
					result_ss << "Transaction failed due to timeout";
					result = result_ss.str();
					reset_cleanup();
					return FALSE;
				}

				if (sendto(theSocket, (const char*)requestPDU, sizePDU, 0, aiResult->ai_addr, sizeof(*aiResult->ai_addr)) == SOCKET_ERROR) {
					result_ss << "sendto failed with error: " << getWSALastErrorText(WSAGetLastError());
					result = result_ss.str();
					reset_cleanup();
					return FALSE;
				}

				transmissions++;
				RTO *= 2;

				iResult = setsockopt(theSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&RTO, sizeof(RTO));
				if (iResult == SOCKET_ERROR) {
					result_ss << "setsockopt for SO_RCVTIMEO failed with error: " << getWSALastErrorText(WSAGetLastError());
					result = result_ss.str();
					reset_cleanup();
					return FALSE;
				}

				continue;
			}

			try {
				Message responseMessage = Message::fromPacket(bufferReceive, sizeReceived);

				if (responseMessage.method != MessageMethod::Binding)
					continue;

				if (responseMessage.messageClass != MessageClass::SuccessResponse && responseMessage.messageClass != MessageClass::ErrorResponse)
					continue;

				if (memcmp(requestTransactionID, responseMessage.transactionID, sizeof(uint32) * 3) != 0)
					continue;

				if (responseMessage.messageClass == MessageClass::ErrorResponse) {
					std::string errorReasonPhrase;
					uint8 errorCode;

					try {
						if (responseMessage.getProcessErrorAttribute(errorCode, errorReasonPhrase))
							result_ss << "Error response received: " << errorReasonPhrase << " (" << errorCode << ")";
						else
							result_ss << "Error response received";
					}
					catch (const MessageProcessingException& e) {
						result_ss << "Error response received";
					}

					result = result_ss.str();
					reset_cleanup();
					return FALSE;
				}

				uint32 mappedIPv4;
				uint16 mappedPort;

				responseMessage.getMappedAddress(&mappedIPv4, &mappedPort);

				result_ss << "Mapped Address: " \
					<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[3] << "." \
					<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[2] << "." \
					<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[1] << "." \
					<< (int)reinterpret_cast<uint8*>(&mappedIPv4)[0] << ":" \
					<< mappedPort;

				break;
			}
			catch (const MessageProcessingException& e) {
				continue;
			}
		}
	}

	result = result_ss.str();
	reset_cleanup();
	return TRUE;
}
void stop()
{
	reset_cleanup();

	if (bWSAInited) {
		WSACleanup();
		bWSAInited = false;
	}
}