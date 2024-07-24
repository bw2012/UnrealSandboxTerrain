#pragma once

#include <stdint.h>
#include <vector>
#include <cstring> 
#include <memory> 

namespace usbt {

	class TFastUnsafeDeserializer {

	private:
		size_t pos = 0;
		const uint8_t* dataPtr = nullptr;

	public:
		TFastUnsafeDeserializer(const uint8_t* dataPtr_) : dataPtr(dataPtr_) { }

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

		void skip(size_t bytes) {
			pos += bytes;
		}

		template <typename T>
		friend TFastUnsafeDeserializer& operator >> (TFastUnsafeDeserializer& in, T& obj) {
			in.readObj(obj);
			return in;
		}
	};


	class TFastUnsafeSerializer {

	private:
		std::shared_ptr<std::vector<uint8>> dataBuffer;

	public:
		TFastUnsafeSerializer() {
			dataBuffer = std::make_shared<std::vector<uint8>>();
		}

		template <typename T>
		void writeObj(const T& obj) {
			auto pos = dataBuffer->size();
			dataBuffer->resize(pos + sizeof(obj));
			memcpy(dataBuffer->data() + pos, &obj, sizeof(obj));
		}

		template <typename T>
		void write(const T* buffer, size_t size) {
			size_t len = sizeof(T) * size;
			auto pos = dataBuffer->size();
			dataBuffer->resize(pos + len);
			memcpy(dataBuffer->data() + pos, buffer, len);
		}

		const std::shared_ptr<std::vector<uint8>> data() {
			return dataBuffer;
		}

		void unsafeCopyTo(uint8_t* target) {
			memcpy(target, dataBuffer->data(), dataBuffer->size());
		}

		template <typename T>
		friend TFastUnsafeSerializer& operator << (TFastUnsafeSerializer& out, const T& obj) {
			out.writeObj(obj);
			return out;
		}
	};

}

