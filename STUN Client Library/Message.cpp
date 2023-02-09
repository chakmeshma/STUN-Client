#include "Message.h"

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<uint32> dist(0, ((uint32)(0) - 1));

MessageAttribute::MessageAttribute(uint32 dataSize) : length(dataSize), data(new uint8[dataSize]) {
	this->type = MessageAttributeType::Unknown;
}
MessageAttribute::MessageAttribute(uint32 dataSize, const uint8* attributeData) : length(dataSize), data(new uint8[dataSize]) {
	this->type = MessageAttributeType::Unknown;

	memcpy(this->data, attributeData, dataSize);
}
MessageAttribute::MessageAttribute(uint32 dataSize, MessageAttributeType attributeType) : length(dataSize), data(new uint8[dataSize]) {
	this->type = attributeType;
}
MessageAttribute::MessageAttribute(uint32 dataSize, MessageAttributeType attributeType, const uint8* attributeData) : length(dataSize), data(new uint8[dataSize]) {
	this->type = attributeType;

	memcpy(this->data, attributeData, dataSize);
}

const uint32 Message::magic_cookie = 0x2112A442;
const uint16 Message::message_type_method_mask = 0b0011111011101111;
const uint16 Message::message_type_class_mask = 0b0000000100010000;
Message Message::fromPacket(const uint8* pdu, uint32 packetSize) {
	if (packetSize < 20 || (packetSize - 20) % 4 != 0)
		throw MessageProcessingException();
	if ((pdu[0] & 0b11000000) != '\0')
		throw MessageProcessingException();
	if (*(reinterpret_cast<const uint32*>(pdu + 4)) != htonl(magic_cookie))
		throw MessageProcessingException();

	uint16 hMessageType = ntohs(*reinterpret_cast<const uint16*>(pdu));

	uint8 methodBitIndex = 0;
	uint8 classBitIndex = 0;

	uint16 encMethod = 0;
	uint8 encClass = 0;

	for (uint8 bitIndex = 0; bitIndex < 14; bitIndex++) {
		bool isMethodBit = (message_type_method_mask >> bitIndex) & 1;
		if (isMethodBit) {
			bool bit = (hMessageType >> bitIndex) & 1;

			if (bit)
				encMethod |= 1 << methodBitIndex++;
			else
				encMethod &= ~(1 << methodBitIndex++);
		}

		bool isClassBit = (message_type_class_mask >> bitIndex) & 1;
		if (isClassBit) {
			bool bit = (hMessageType >> bitIndex) & 1;

			if (bit)
				encClass |= 1 << classBitIndex++;
			else
				encClass &= ~(1 << classBitIndex++);
		}
	}

	MessageClass theClass = static_cast<MessageClass>(encClass);
	MessageMethod theMethod = static_cast<MessageMethod>(encMethod);

	uint16 hMessageLength = ntohs(*reinterpret_cast<const uint16*>(pdu + 2));
	if (packetSize != hMessageLength + 20)
		throw MessageProcessingException();
	if (hMessageLength % 4 != 0)
		throw MessageProcessingException();

	uint32 transactionID[3];
	transactionID[0] = *(reinterpret_cast<const uint32*>(pdu) + 2);
	transactionID[1] = *(reinterpret_cast<const uint32*>(pdu) + 3);
	transactionID[2] = *(reinterpret_cast<const uint32*>(pdu) + 4);

	std::vector<MessageAttribute> attributes;

	// ATTRIBUTES PROCESSING BEGIN

	uint8* pAttributes = const_cast<uint8*>(pdu + 20);

	do {
		uint16 hAttributeType;
		uint16 hAttributeLength;

		hAttributeType = ntohs(*reinterpret_cast<uint16*>(pAttributes));
		pAttributes += 2;

		hAttributeLength = ntohs(*reinterpret_cast<uint16*>(pAttributes));
		MessageAttribute attribute(hAttributeLength, static_cast<MessageAttributeType>(hAttributeType));
		pAttributes += 2;

		memcpy(attribute.data, pAttributes, hAttributeLength);

		pAttributes += ((hAttributeLength % 4 == 0) ? (hAttributeLength) : (hAttributeLength + (4 - (hAttributeLength % 4))));  //Skip over attribute value padding due to 4-byte alignment

		attributes.push_back(attribute);

	} while (pAttributes < pdu + 20 + hMessageLength);

	// ATTRIBUTES PROCESSING END

	return Message(theMethod, theClass, transactionID, attributes);
}
Message::Message(MessageMethod method, MessageClass messageClass)
{
	this->method = method;
	this->messageClass = messageClass;

	this->attributes = std::vector<MessageAttribute>();

	populateRandomTransactionID();

}
Message::Message(MessageMethod method, MessageClass messageClass, std::vector<MessageAttribute> attributes)
{
	this->method = method;
	this->messageClass = messageClass;

	this->attributes = attributes;

	populateRandomTransactionID();
}
Message::Message(MessageMethod method, MessageClass messageClass, uint32 transactionID[3])
{
	this->method = method;
	this->messageClass = messageClass;

	this->attributes = std::vector<MessageAttribute>();

	this->transactionID[0] = transactionID[0];
	this->transactionID[1] = transactionID[1];
	this->transactionID[2] = transactionID[2];
}
Message::Message(MessageMethod method, MessageClass messageClass, uint32 transactionID[3], std::vector<MessageAttribute> attributes)
{
	this->method = method;
	this->messageClass = messageClass;

	this->attributes = attributes;

	this->transactionID[0] = transactionID[0];
	this->transactionID[1] = transactionID[1];
	this->transactionID[2] = transactionID[2];
}
uint32 Message::encodeMessage(uint8* pdu)
{
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

	uint16 hMessageLength = 0;

	for (MessageAttribute attribute : attributes) {
		hMessageLength += 4;
		hMessageLength += ((attribute.length % 4 == 0) ? (attribute.length) : (attribute.length + (4 - (attribute.length % 4))));
	}

	uint16 nMessageType = htons(hMessageType);
	uint16 nMessageLength = htons(hMessageLength);
	uint32 nMagicCookie = htonl(magic_cookie);

	memcpy(((uint16*)pdu) + 0, &nMessageType, sizeof(uint16));
	memcpy(((uint16*)pdu) + 1, &nMessageLength, sizeof(uint16));
	memcpy(((uint32*)pdu) + 1, &nMagicCookie, sizeof(uint32));
	memcpy(((uint8*)pdu) + 8, transactionID, sizeof(uint32) * 3);

	uint8* pAttributes = pdu + 20;

	for (MessageAttribute attribute : attributes) {
		if (attribute.type == MessageAttributeType::Unknown)
			throw MessageProcessingException();

		uint16 nAttributeType = htons(static_cast<uint16>(attribute.type));
		uint16 nAttributeLength = htons(attribute.length);

		memcpy(pAttributes, &nAttributeType, 2);
		pAttributes += 2;
		memcpy(pAttributes, &nAttributeLength, 2);
		pAttributes += 2;

		memcpy(pAttributes, attribute.data, attribute.length);
		pAttributes += attribute.length;

		uint8 padding = ((attribute.length % 4 == 0) ? (0) : (4 - (attribute.length % 4)));
		memset(pAttributes, 0, padding);
		pAttributes += padding;
	}

	return 20 + hMessageLength;
}
bool Message::getProcessMappedAddress(uint32* ipv4, uint16* port)
{
	//for (auto attribute : attributes) {
	//	if (attribute.type == MessageAttributeType::XOR_MAPPED_ADDRESS && attribute.length == 8 && reinterpret_cast<uint8*>(attribute.value.get())[1] == 1) {

	//		uint16 encodedPort = *reinterpret_cast<uint16*>(attribute.value.get() + 2);
	//		uint32 encodedIP = *reinterpret_cast<uint32*>(attribute.value.get() + 4);

	//		encodedPort = ntohs(encodedPort);
	//		encodedIP = ntohl(encodedIP);

	//		*port = encodedPort ^ static_cast<const uint16>(magic_cookie >> 16);
	//		*ipv4 = encodedIP ^ magic_cookie;

	//		return;
	//	}
	//}

	//throw MessageProcessingException();

	return true;
}
bool Message::getProcessErrorAttribute(uint8& errorCode, std::string& reasonPhrase)
{
	for (MessageAttribute attribute : attributes) {
		if (attribute.type == MessageAttributeType::ERROR_CODE) {
			if (attribute.length < 4)
				throw MessageProcessingException();

			uint8 errorCodeClass = attribute.data[2] & 0b00000111;

			if (errorCodeClass < 3 || errorCodeClass > 6)
				throw MessageProcessingException();

			uint8 errorCodeNumber = attribute.data[3];

			if (errorCodeNumber > 99)
				throw MessageProcessingException();

			errorCode = errorCodeClass * 100 + errorCodeNumber;
			reasonPhrase = std::string(reinterpret_cast<const char* const>(attribute.data + 4), attribute.length - 4);

			return true;
		}
	}

	return false; //means no error attribute present
}
void Message::populateRandomTransactionID() {
	transactionID[0] = dist(gen);
	transactionID[1] = dist(gen);
	transactionID[2] = dist(gen);
}