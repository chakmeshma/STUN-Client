#include "Message.h"

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<uint32> dist(0, ((uint32)(0) - 1));

const uint32 Message::magic_cookie = 0x2112A442;
const uint16 Message::message_type_method_mask = 0b0011111011101111;
const uint16 Message::message_type_class_mask = 0b0000000100010000;

Message::Message(MessageMethod method, MessageClass messageClass, std::vector<MessageAttribute> attributes)
{
	transactionID[0] = dist(gen);
	transactionID[1] = dist(gen);
	transactionID[2] = dist(gen);
	this->method = method;
	this->messageClass = messageClass;
	this->attributes = attributes;
}

Message::Message(MessageMethod method, MessageClass messageClass, uint32 transactionID[3], std::vector<MessageAttribute> attributes) {
	memcpy(this->transactionID, transactionID, sizeof(uint32) * 3);
	this->method = method;
	this->messageClass = messageClass;
	this->attributes = attributes;
}

//TODO: check for unknown attributes or unexpected attributes that might cause the decodation to fail
Message Message::fromPacket(uint8* pdu, uint32 packetSize) {
	if (packetSize < 20 || (packetSize - 20) % 4 != 0)
		throw MessageProcessingException();
	if ((pdu[0] & 0b11000000) != '\0')
		throw MessageProcessingException();
	if (*(reinterpret_cast<uint32*>(pdu + 4)) != htonl(magic_cookie))
		throw MessageProcessingException();

	uint16 hMessageType = ntohs(*reinterpret_cast<uint16*>(pdu));

	uint8 methodBitIndex = 0;
	uint8 classBitIndex = 0;

	uint16 method = 0;
	uint8 messageClass = 0;

	for (uint8 bitIndex = 0; bitIndex < 14; bitIndex++) {
		bool isMethodBit = (message_type_method_mask >> bitIndex) & 1;
		if (isMethodBit) {
			bool bit = (hMessageType >> bitIndex) & 1;

			if (bit)
				method |= 1 << methodBitIndex++;
			else
				method &= ~(1 << methodBitIndex++);
		}

		bool isClassBit = (message_type_class_mask >> bitIndex) & 1;
		if (isClassBit) {
			bool bit = (hMessageType >> bitIndex) & 1;

			if (bit)
				messageClass |= 1 << classBitIndex++;
			else
				messageClass &= ~(1 << classBitIndex++);
		}
	}

	std::cout << chek((uint8*)&messageClass, sizeof(messageClass));
	std::cout << std::endl << std::endl;
	std::cout << chek((uint8*)&method, sizeof(method));

	MessageClass theClass = static_cast<MessageClass>(messageClass);
	MessageMethod theMethod = static_cast<MessageMethod>(method);

	int bbbbbbbbbb = 3434334;


	//uint16 messageType = ntohs(*reinterpret_cast<uint16*>(pdu));

	//std::cout << chek((uint8*)&messageType, 2) << std::endl;

	////ZUM TESTEN BEGINN

	///*unsigned short mask = 0b 00111110 11101111;

	//std::cout << "Mask:" << std::endl;
	//std::cout << chek((uint8*)&mask, 2) << std::endl;
	//std::cout << std::endl;
	//std::cout << "PDU (first two bytes):" << std::endl;
	//std::cout << chek(pdu, 2) << std::endl;*/

	////ZUM TESTEN ENDE

	//messageMethodEncoded = (*(reinterpret_cast<uint16*>(pdu))) & 0b0011111011101111;
	//messageLength = htons(*reinterpret_cast<uint16*>(pdu + 2));
	//messageClassEncoded = (*(reinterpret_cast<uint16*>(pdu))) & 0b0000000100010000;

	//if (messageLength % 4 != 0)
	//	throw MessageProcessingException();

	//transactionID[0] = *(reinterpret_cast<uint32*>(pdu + 8));
	//transactionID[1] = *(reinterpret_cast<uint32*>(pdu + 12));
	//transactionID[2] = *(reinterpret_cast<uint32*>(pdu + 16));

	//switch (messageMethodEncoded) {
	//case 0x0001: //Binding
	//	messageMethod = MessageMethod::Binding;
	//	break;
	//default:
	//	messageMethod = MessageMethod::Unknown;
	//}

	//switch (messageClassEncoded) {
	//case 0x0100: //Success Response
	//	messageClass = MessageClass::SuccessResponse;
	//	break;
	//default:
	//	messageClass = MessageClass::Unknown;
	//}

	////Parsing Attributes START

	//uint8* pAttributes = pdu + 20;

	//do {
	//	uint16 attributeType;
	//	uint16 attributeLength;

	//	attributeType = ntohs(*reinterpret_cast<uint16*>(pAttributes));
	//	pAttributes += 2;
	//	attributeLength = ntohs(*reinterpret_cast<uint16*>(pAttributes));
	//	pAttributes += 2;

	//	MessageAttribute attribute(attributeLength);
	//	attribute.type = static_cast<MessageAttributeType>(attributeType);

	//	memcpy(attribute.data.get(), pAttributes, attributeLength);

	//	pAttributes += (attributeLength % 4 == 0) ? (attributeLength) : (attributeLength + (4 - (attributeLength % 4)));  //Skip over attribute value padding due to 4-byte alignment

	//	attributes.push_back(attribute);

	//} while (pAttributes < pdu + 20 + messageLength);

	////Parsing Attributes END

	//Message message = Message(messageMethod, messageClass, transactionID, attributes);

	//return message;

	return Message(MessageMethod::Binding, MessageClass::Request, std::vector<MessageAttribute>());
}

uint32 Message::encodeMessageWOAttrs(uint8* pdu) {
	if (method == MessageMethod::Unknown)
		throw MessageProcessingException();

	uint16 hMessageType = 0;

	uint8 methodBitIndex = 0;
	uint8 classBitIndex = 0;

	for (uint8 bitIndex = 0; bitIndex < 14; bitIndex++) {
		bool isMethodBit = (message_type_method_mask >> bitIndex) & 1;
		if (isMethodBit) {
			bool bit = (static_cast<uint16>(method) >> methodBitIndex++) & 1;

			if (bit)
				hMessageType |= (1 << bitIndex);
			else
				hMessageType &= ~(1 << bitIndex);
		}

		bool isClassBit = (message_type_class_mask >> bitIndex) & 1;
		if (isClassBit) {
			bool bit = (static_cast<uint8>(messageClass) >> classBitIndex++) & 1;

			if (bit)
				hMessageType |= (1 << bitIndex);
			else
				hMessageType &= ~(1 << bitIndex);
		}
	}

	uint16 nMessageType = htons(hMessageType);
	uint16 nMessageLength = htons(0);
	uint32 nMagicCookie = htonl(magic_cookie);

	memcpy(((uint16*)pdu) + 0, &nMessageType, sizeof(uint16));
	memcpy(((uint16*)pdu) + 1, &nMessageLength, sizeof(uint16));
	memcpy(((uint32*)pdu) + 1, &nMagicCookie, sizeof(uint32));
	memcpy(((uint8*)pdu) + 8, transactionID, sizeof(uint32) * 3);


	return 20;
}

void Message::getMappedAddress(uint32* ipv4, uint16* port)
{
	for (auto attribute : attributes) {
		if (attribute.type == MessageAttributeType::XOR_MAPPED_ADDRESS && attribute.length == 8 && reinterpret_cast<uint8*>(attribute.data.get())[1] == 1) {

			uint16 encodedPort = *reinterpret_cast<uint16*>(attribute.data.get() + 2);
			uint32 encodedIP = *reinterpret_cast<uint32*>(attribute.data.get() + 4);

			encodedPort = ntohs(encodedPort);
			encodedIP = ntohl(encodedIP);

			*port = encodedPort ^ static_cast<const uint16>(magic_cookie >> 16);
			*ipv4 = encodedIP ^ magic_cookie;

			return;
		}
	}

	throw MessageProcessingException();
}

void Message::getTransactionID(uint32 transactionID[3]) {
	transactionID[0] = this->transactionID[0];
	transactionID[1] = this->transactionID[1];
	transactionID[2] = this->transactionID[2];
}

std::vector<MessageAttribute>& Message::getAttributes() {
	return this->attributes;
}