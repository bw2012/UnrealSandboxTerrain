#pragma once

#include <stdint.h>

class FastUnsafeDeserializer {

private:
	size_t pos = 0;
	const uint8_t* dataPtr = nullptr;

public:
	FastUnsafeDeserializer(const uint8_t* dataPtr_) : dataPtr(dataPtr_) { }

	template <typename T>
	void readObj(T& obj) {
		memcpy(&obj, dataPtr + pos, sizeof(T));
		pos += sizeof(T);
	}

	template <typename T>
	void read(T* buffer, size_t size) {
		size_t len = sizeof(T) * size;
		memcpy(buffer, dataPtr + pos, len);
		pos += len;
	}
};