#pragma once

#include "types.h"
#include <vector>
#include <random>
#include <winsock2.h>

enum class MessageAttributeType : uint16 {
	MAPPED_ADDRESS = 0x0001,
	USERNAME = 0x0006,
	MESSAGE_INTEGRITY = 0x0008,
	ERROR_CODE = 0x0009,
	UNKNOWN_ATTRIBUTES = 0x000A,
	REALM = 0x0014,
	NONCE = 0x0015,
	XOR_MAPPED_ADDRESS = 0x0020,
	SOFTWARE = 0x8022,
	ALTERNATE_SERVER = 0x8023,
	FINGERPRINT = 0x8028,
	UKNOWN = 0,
};

enum class MessageTypeLZ : uint16 {
	Binding_Request = 0x0001,
	Binding_Indication = 0x0011,
	Binding_SuccessResponse = 0x0101,
	Binding_ErrorResponse = 0x0111
};

enum class MessageMethod {
	Binding,
	Unknown
};

enum class MessageClass {
	Request,
	Indication,
	SuccessResponse,
	ErrorResponse,
	Unknown
};

enum class ProcessingExceptionType {
	InvalidPacket,
	InvalidPacketParam,
	NAMappedAddress
};

class ProcessingException : public std::exception {
public:
	ProcessingException(ProcessingExceptionType exceptionType) : type(exceptionType) {}
private:
	ProcessingExceptionType type;
};

class MessageAttribute {
public:
	MessageAttribute(uint32 dataSize) : type(MessageAttributeType::UNKNOWN_ATTRIBUTES), length(dataSize), data(new uint8[dataSize]) {}

	MessageAttributeType type;
	uint32 length;
	std::shared_ptr<uint8[]> data;
};

class Message {
public:
	static Message fromPacket(uint8* pdu, unsigned int packetSize); // Factory method
	Message(MessageMethod method, MessageClass messageClass, std::vector<MessageAttribute> attributes);
	Message(MessageMethod method, MessageClass messageClass, uint32 transactionID[3], std::vector<MessageAttribute> attributes);
	void getTransactionID(uint32 transactionID[3]);
	std::vector<MessageAttribute>& getAttributes();
	uint32 encodePacket(uint8* pdu);
	void getMappedAddress(uint32 targetIPv4, uint16 targetPort, uint32* ipv4, uint16* port);
	const static uint32 magic_cookie;
private:
	static bool isHostBigEndian();
	static bool isHostNetworkDifferentEndianness();
	std::vector<MessageAttribute> attributes;
	MessageMethod method;
	MessageClass messageClass;
	uint32 transactionID[3];
};