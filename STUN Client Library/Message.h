#pragma once

#include "types.h"
#include <iostream>
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
enum class MessageMethod : uint16 {
	Binding = 0x001,
	Unknown
};
enum class MessageClass : uint8 {
	Request = 0b00,
	Indication = 0b01,
	SuccessResponse = 0b10,
	ErrorResponse = 0b11
};

class MessageProcessingException : public std::exception {
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
	static Message fromPacket(uint8* pdu, uint32 packetSize); // Factory method
	Message(MessageMethod method, MessageClass messageClass, std::vector<MessageAttribute> attributes);
	Message(MessageMethod method, MessageClass messageClass, uint32 transactionID[3], std::vector<MessageAttribute> attributes);
	void getTransactionID(uint32 transactionID[3]);
	std::vector<MessageAttribute>& getAttributes();
	uint32 encodeMessageWOAttrs(uint8* pdu);
	void getMappedAddress(uint32* ipv4, uint16* port);
private:
	static const uint32 magic_cookie;
	static const uint16 message_type_method_mask;
	static const uint16 message_type_class_mask;
	std::vector<MessageAttribute> attributes;
	MessageMethod method;
	MessageClass messageClass;
	uint32 transactionID[3];
};

inline std::string chek(unsigned char* data, unsigned int size, bool reverse = false) {
	std::vector<std::string> allBitSequences;


	for (unsigned int byte = 0; byte < size; byte++) {
		std::string bitSequence("");

		for (int bit = 0; bit < 8; bit++) {
			unsigned char a = (data[byte] >> bit);
			bitSequence.insert(0, 1, a % 2 ? '1' : '0');
		}

		allBitSequences.push_back(bitSequence);
	}

	std::string result("");

	unsigned int sss = allBitSequences.size();

	if (reverse) {
		for (int zz = 0; zz < sss; zz++) {
			result.append(allBitSequences[zz]);
			if (zz < sss - 1)result.append("\n");
		}
	}
	else {
		for (int zz = sss - 1; zz >= 0; zz--) {
			result.append(allBitSequences[zz]);
			if (zz > 0)result.append("\n");
		}
	}

	return result;
}