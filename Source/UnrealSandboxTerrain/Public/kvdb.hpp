#pragma once

#include <iostream>
#include <stdio.h>
#include <fstream>
#include <stdint.h>
#include <unordered_map>
#include <array>
#include <vector>
#include <memory>
#include <string>
#include <list>
#include <set>
#include <mutex>
#include <type_traits>
#include <cassert>
#include <cstring> 
#include <functional> 

#define KVDB_KEY_SIZE 12 // 3 x int32 (X, Y, Z)
#define KVDB_RESERVED_TABLE_SIZE 1000
#define KVDB_MIN_DATA_SIZE 256

typedef uint32_t uint32;
typedef unsigned long long ulong64;
typedef unsigned char byte;

typedef std::array<byte, KVDB_KEY_SIZE> TKeyData;
typedef std::vector<byte> TValueData;
typedef std::shared_ptr<TValueData> TValueDataPtr;

//============================================================================
// Key data hash
//============================================================================
namespace std {
	template <>
	struct hash<TKeyData> {
		std::size_t operator()(const TKeyData& keyData) const {
			std::size_t h = 0;
			for (auto elem : keyData) {
				h ^= std::hash<int>{}(elem) + 0x9e3779b9 + (h << 6) + (h >> 2);
			}
			return h;
		}
	};
}

namespace kvdb {

	//============================================================================
	// Base IO
	//============================================================================
	template <typename T>
	void write(std::ostream* os, const T& obj) {
		if (os != nullptr) {
			os->write((char*)&obj, sizeof(obj));
		}
	}

	template <typename T>
	void read(std::istream* is, T& obj) {
		if (is != nullptr) {
			is->read((char*)&obj, sizeof(obj));
		}
	}

	//============================================================================
	// File position
	//============================================================================
	template <typename T>
	class TPosWrapper {

	private:
		T objT;

	public:
		ulong64 pos;

		TPosWrapper() {};
		TPosWrapper(T t, ulong64 p) : objT(t), pos(p) {};
		const T& operator()() const { return objT; }
		T& operator()() { return objT; }
		T* operator->() { return &objT; }
	};

	//============================================================================
	// File header
	//============================================================================
	typedef struct TFileHeader {
		uint32 version = 1;
		uint32 keySize = 0;
		ulong64 timestamp = 0;
		uint32  endOfHeaderOffset = 0;
	} TFileHeader;

	inline std::ostream* operator << (std::ostream* os, const TFileHeader& obj) {
		write(os, obj);
		return os;
	}

	inline std::istream* operator >> (std::istream* is, TFileHeader& obj) {
		read(is, obj);
		return is;
	}

	//============================================================================
	// Table header
	//============================================================================
	typedef struct TTableHeader {
		ulong64 recordCount = 0;
		ulong64 nextTable = 0;
	} TTableHeader;

	typedef TPosWrapper<TTableHeader> TTableHeaderInfo;

	inline std::ostream* operator << (std::ostream* os, const TTableHeader& obj) {
		write(os, obj);
		return os;
	}

	inline std::istream* operator >> (std::istream* is, TTableHeader& obj) {
		read(is, obj);
		return is;
	}

	//============================================================================
	// Key Entry
	//============================================================================
	typedef struct TKeyEntry {
		ulong64 dataPos = 0;
		ulong64 dataLength = 0;
		ulong64 initialDataLength = 0;
		TKeyData freeKeyData;
	} TKeyEntry;

	typedef TPosWrapper<TKeyEntry> TKeyEntryInfo;

	struct TKeyInfoComparatorByInitialLength {
		bool operator() (const TKeyEntryInfo& lhs, const TKeyEntryInfo& rhs) const {
			return (lhs().initialDataLength > rhs().initialDataLength);
		}
	};

	inline std::ostream* operator << (std::ostream* os, const TKeyEntry& obj) {
		write(os, obj);
		return os;
	}

	inline std::ostream* operator << (std::ostream* os, const TKeyEntryInfo& objInfo) {
		os->seekp(objInfo.pos);
		os << objInfo();
		return os;
	}

	inline std::istream* operator >> (std::istream* is, TKeyEntry& obj) {
		read(is, obj);
		return is;
	}

	//============================================================================
	// File db
	//============================================================================
	template <typename K, typename V>
	class KvFile {

	private:

		std::unordered_map<TKeyData, TKeyEntryInfo> dataMap;
		std::fstream* filePtr = nullptr;
		std::list<TKeyEntryInfo> reservedKeyList;
		std::set<TKeyEntryInfo, TKeyInfoComparatorByInitialLength> deletedKeyList;
		std::list<TTableHeaderInfo> tableList;
		mutable std::mutex fileSharedMutex;

		uint32 reservedKeys = KVDB_RESERVED_TABLE_SIZE;
		const uint32 reservedValueSize = 0;

	private:

		std::shared_ptr<V> valueFromData(TValueDataPtr dataPtr) {
			if (dataPtr == nullptr) return nullptr;

			if constexpr (std::is_same<V, TValueData>::value) { //constexpr
				return std::static_pointer_cast<TValueData>(dataPtr);
			} else {
				V* temp = new V();
				std::memcpy(temp, dataPtr->data(), sizeof(V));
				return std::shared_ptr<V>(temp);
			}
		}

		static TKeyData toKeyData(K key) {
			TKeyData kd;
			std::memcpy(&kd[0], &key, sizeof(K));
			return kd;
		}

		static K keyFromKeyData(const TKeyData& kd) {
			K key;
			std::memcpy(&key, &kd, sizeof(K));
			return key;
		}

		static void toValueData(V value, TValueData& valueData) {
			if (valueData.size() < sizeof(value)) valueData.resize(sizeof(value));
			std::memcpy(valueData.data(), &value, sizeof(value));
		}

		void rewritePair(TKeyEntryInfo& keyInfo, const TValueData& valueData) {
			// rewrite value data
			filePtr->seekp(keyInfo().dataPos);
			filePtr->write((char*)valueData.data(), valueData.size());
			// rewrite key data
			keyInfo().dataLength = valueData.size(); // new length
			filePtr << keyInfo;
		}

		void earsePair(TKeyEntryInfo& keyInfo) {
			// rewrite key data
			keyInfo().dataLength = 0; // new length
			filePtr << keyInfo;
			deletedKeyList.insert(keyInfo);
			dataMap.erase(keyInfo().freeKeyData);
		}

		void expandValueData(const TValueData& valueDataSrc, TValueData& valueDataNew, size_t size) {
			valueDataNew.resize(size);
			std::fill(valueDataNew.begin(), valueDataNew.end(), 0);
			std::memcpy(valueDataNew.data(), valueDataSrc.data(), valueDataSrc.size());
		}

		void newPairFromReserved(const TKeyData& keyData, const TValueData& valueData) {
			// has reserved key slots
			TKeyEntryInfo& keyInfo = reservedKeyList.front();
			TValueData valueDataExp;

			if (reservedValueSize > 0) {
				uint32 n = (uint32)std::round((float)valueData.size() / (float)reservedValueSize) + 1;
				expandValueData(valueData, valueDataExp, n * reservedValueSize);
			} else {
				valueDataExp = std::move(valueData);
			}

			filePtr->seekp(0, std::ios::end); // to end-of-file
			ulong64 endFile = (ulong64)(filePtr->tellp());
			filePtr->write((char*)valueDataExp.data(), valueDataExp.size());

			// fill key data
			keyInfo().dataLength = valueData.size(); // length
			keyInfo().initialDataLength = valueDataExp.size(); // length
			keyInfo().dataPos = endFile;
			keyInfo().freeKeyData = keyData;
			filePtr << keyInfo;

			// add new pair to table 
			dataMap.insert({ keyInfo().freeKeyData, keyInfo });
			reservedKeyList.pop_front();
		}

		ulong64 readTable() {
			ulong64 tablePos = (ulong64)filePtr->tellg();

			TTableHeader tableHeader;
			filePtr >> tableHeader;

			for (unsigned int i = 0; i < tableHeader.recordCount; i++) {
				ulong64 pos = (ulong64)filePtr->tellg();
				TKeyEntry keyEntry;
				filePtr >> keyEntry;
				TKeyEntryInfo keyInfo(keyEntry, pos);

				if (keyInfo().dataLength > 0) {
					dataMap.insert({ keyEntry.freeKeyData, keyInfo });
				} else {
					if (keyInfo().initialDataLength == 0) {
						// reserved key slot
						reservedKeyList.push_back(keyInfo);
					} else {
						// marked as deleted pair
						deletedKeyList.insert(keyInfo);
					}
				}
			}

			tableList.push_back(TTableHeaderInfo(tableHeader, tablePos));
			return tableHeader.nextTable;
		}

		void createNewTable() {
			filePtr->seekp(0, std::ios::end); // to end-of-file
			ulong64 newTablePos = (ulong64)filePtr->tellp();

			// write new table
			TTableHeader newTable{reservedKeys, 0};
			filePtr << newTable;

			// write reserved keys
			for (uint32 i = 0; i < reservedKeys; i++) {
				uint32 newReservedKeyPos = (uint32)filePtr->tellp();
				TKeyEntry newReservedKey;
				filePtr << newReservedKey;
				TKeyEntryInfo keyInfo(newReservedKey, newReservedKeyPos);
				reservedKeyList.push_back(keyInfo);
			}

			// read previous last table 
			TTableHeaderInfo& lastTable = tableList.back();

			// add link to new table
			lastTable().nextTable = newTablePos;

			// rewrite previous last table
			filePtr->seekp(lastTable.pos);
			filePtr << lastTable();

			// add new table to internal list
			tableList.push_back(TTableHeaderInfo(newTable, newTablePos));
		}

		bool hasReserved() {
			return reservedKeyList.size() > 0;
		}

		bool tryWriteToSuitableDeletedPair(const TKeyData& keyData, const TValueData& valueData) {
			for (auto itr = deletedKeyList.begin(); itr != deletedKeyList.end();) {
				TKeyEntryInfo keyInfo = *itr;
				if (keyInfo().initialDataLength >= valueData.size()) {
					keyInfo().freeKeyData = keyData;
					rewritePair(keyInfo, valueData);
					dataMap.insert({ keyInfo().freeKeyData, keyInfo });
					itr = deletedKeyList.erase(itr);
					return true;
				} else {
					++itr;
				}
			}
			return false;
		}

		void addNew(const TKeyData& keyData, const TValueData& valueData) {
			if (valueData.size() == 0) {
				return;
			}

			if (!tryWriteToSuitableDeletedPair(keyData, valueData)) {
				if (hasReserved()) {
					newPairFromReserved(keyData, valueData);
				} else {
					createNewTable();
					newPairFromReserved(keyData, valueData);
				}
			}
		}

		void change(const TKeyData& keyData, const TValueData& valueData) {
			TKeyEntryInfo& keyInfo = dataMap[keyData];
			if (valueData.size() > 0) {
				if (keyInfo().initialDataLength >= valueData.size()) {
					rewritePair(keyInfo, valueData);
				} else {
					//remove old and create new
					earsePair(keyInfo);
					addNew(keyData, valueData);
				}
			} else {
				// erase
				earsePair(keyInfo);
			}
		}

	public:

		KvFile() {
			assert(sizeof(K) <= KVDB_KEY_SIZE);
		}

		KvFile(uint32 s) : reservedValueSize(s) {
			assert(sizeof(K) <= KVDB_KEY_SIZE);
		}

		~KvFile() {
			close();
		}

		void close() {
			if (!isOpen()) return;
			filePtr->close();
			dataMap.clear();
			reservedKeyList.clear();
			deletedKeyList.clear();
			tableList.clear();
		}

		bool isOpen() const {
			return filePtr && filePtr->is_open();
		}

		bool open(const std::string& file) {
			filePtr = new std::fstream(file, std::ios::in | std::ios::out | std::ios::binary);

			if (!isOpen()) return false;

			TFileHeader fileHeader;
			filePtr >> fileHeader;

			ulong64 nextTablePos = readTable();
			while (nextTablePos > 0) {
				filePtr->seekg(nextTablePos);
				nextTablePos = readTable();
			}

			return true;
		}

		size_t size() {
			if (!isOpen()) {
				return 0;
			} else {
				return dataMap.size();
			}
		}

		bool isExist(const K& k) {
			TKeyData keyData = toKeyData(k);
			if (!isOpen()) return false;
			std::lock_guard<std::mutex> guard(fileSharedMutex);
			return !(dataMap.find(keyData) == dataMap.end());
		}
		
		void forEachKey(std::function<void(K key)> func) const {
			if (!isOpen()) return;
			std::lock_guard<std::mutex> guard(fileSharedMutex);
			for (const auto& kv : dataMap) { func(keyFromKeyData(kv.first)); }
		}

		TValueDataPtr loadData(const K& k) {
			TKeyData keyData = toKeyData(k);

			if (!isOpen()) return nullptr;
			std::lock_guard<std::mutex> guard(fileSharedMutex);

			auto got = dataMap.find(keyData);
			if (got == dataMap.end()) {
				return nullptr;
			}

			TKeyEntryInfo& i = dataMap[keyData];
			const TKeyEntry& e = i();

			filePtr->seekg(e.dataPos);

			TValueDataPtr dataPtr = TValueDataPtr(new TValueData);
			dataPtr->resize(e.dataLength);

			if (filePtr->read((char*)dataPtr->data(), e.dataLength)) {
				return dataPtr;
			}

			return nullptr;
		}

		std::shared_ptr<V> load(const K& k) {
			return valueFromData(loadData(k));
		}

		std::shared_ptr<V> operator[] (const K& k) {
			return valueFromData(loadData(k));
		}

		void erase(const K& k) {
			TKeyData keyData = toKeyData(k);

			if (!isOpen()) return;
			std::lock_guard<std::mutex> guard(fileSharedMutex);

			auto got = dataMap.find(keyData);
			if (got != dataMap.end()) {
				TKeyEntryInfo keyInfo = dataMap[keyData];
				earsePair(keyInfo);
			}
		}

		void save(const K& k, const V& v) {
			TKeyData keyData = toKeyData(k);
			TValueData valueData;

			if constexpr(std::is_same<V, TValueData>::value) {
				valueData = static_cast<TValueData>(v);
			} else {
				toValueData(v, valueData);
			}

			if (!isOpen()) return;
			std::lock_guard<std::mutex> guard(fileSharedMutex);

			std::unordered_map<TKeyData, TKeyEntryInfo>::const_iterator got = dataMap.find(keyData);
			if (got == dataMap.end()) {
				// pair not found  
				addNew(keyData, valueData);
			} else {
				// pair found 
				change(keyData, valueData);
			}
		}

		static bool create(const std::string& file, const std::unordered_map<K, V>& test) {
			std::ofstream outFile(file, std::ios::out | std::ios::binary);

			if (!outFile) return false;

			std::ofstream* outFilePtr = &outFile;

			static const ulong64 max_key_records = KVDB_RESERVED_TABLE_SIZE;
			const ulong64 keyRecords = (test.size() > max_key_records) ? test.size() : max_key_records;

			// save file header
			TFileHeader fileHeader;
			fileHeader.keySize = KVDB_KEY_SIZE;
			fileHeader.endOfHeaderOffset = sizeof(fileHeader);

			outFilePtr << fileHeader;

			TTableHeader tableHeader{keyRecords, 0};
			outFilePtr << tableHeader;

			ulong64 bodyDataOffset = (ulong64)(outFile.tellp()) + sizeof(TKeyEntry) * keyRecords;
			std::vector<byte> dataBody;
			for (auto& e : test) {
				TKeyEntry entry;
				entry.freeKeyData = toKeyData(e.first);
				entry.dataPos = dataBody.size() + bodyDataOffset;

				TValueData valueData;
				if constexpr (std::is_same<V, TValueData>::value) {
					valueData = static_cast<TValueData>(e.second);
				} else {
					toValueData(e.second, valueData);
				}

				entry.dataLength = valueData.size();
				entry.initialDataLength = valueData.size();
				dataBody.insert(std::end(dataBody), std::begin(valueData), std::end(valueData));
				outFilePtr << entry;
			}

			if (test.size() < max_key_records) {
				// add empty records
				const ulong64 emptyRecords = max_key_records - test.size();
				for (int i = 0; i < emptyRecords; i++) {
					TKeyEntry entry;
					outFilePtr << entry;
				}
			}

			outFilePtr->write((char*)dataBody.data(), dataBody.size());
			outFile.close();
			return true;
		}


		// ====================================================================================
		void test() {

			for (auto& kv : dataMap) {
				auto v = kv.second;
				printf("%ld <- %ld\n", v().dataLength, v().initialDataLength);
			}

			for (auto& r : reservedKeyList) {
				//printf("reserved: %ld\n", r.pos);
			}

			for (auto& r : deletedKeyList) {
				//printf("deleted: %ld\n", r.pos);
			}


		}
		// ====================================================================================

	};
	//-----------------------------------------------------------------------------
}