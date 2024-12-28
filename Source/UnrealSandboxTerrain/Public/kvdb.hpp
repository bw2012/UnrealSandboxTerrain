// Small and simple key-value binary file storage.
// initially was designed to store voxel data in UE4 
// September 2017

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
#include <cmath>


#define KVDB_RESERVED_TABLE_SIZE 1000
#define KVDB_MIN_DATA_SIZE 256

#define KVDB_FILE_VERSION 2

#define KVDB_OK 0
#define KVDB_ERROR_OPEN_FILE -1
#define KVDB_ERROR_INCORRECT_FILE_VERSION -2


typedef uint32_t uint32;
typedef uint16_t uint16;
typedef unsigned long long ulong64;
typedef unsigned char byte;

typedef std::vector<byte> TKeyData;
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
				h ^= std::hash<int>{}(elem)+0x9e3779b9 + (h << 6) + (h >> 2);
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
		ulong64 pos = 0;

		TPosWrapper() {};
		TPosWrapper(T t, ulong64 p) : objT(t), pos(p) {};
		const T& operator()() const { return objT; }
		T& operator()() { return objT; }
		T* operator->() { return &objT; }
	};

	//============================================================================
	// File header
	//============================================================================
	#pragma pack(push,1)
	typedef struct TFileHeader {
		char h[4] = {'K', 'V', 'D', 'B'};
		uint32 version = KVDB_FILE_VERSION;
		uint32 keySize = 0;
		ulong64 timestamp = 0;
		uint32  endOfHeaderOffset = (uint32)sizeof(TFileHeader);
		char reverved1[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
		char reverved2[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	} TFileHeader;
	#pragma pack(pop)

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
	#pragma pack(push,1)
	typedef struct TTableHeader {
		ulong64 recordCount = 0;
		ulong64 nextTable = 0;
	} TTableHeader;
	#pragma pack(pop)

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
	#pragma pack(push,1)
	typedef struct TKeyEntryHeader {
		ulong64 dataPos = 0;
		ulong64 dataLength = 0;
		ulong64 initialDataLength = 0;
		ulong64 flags = 0;
		ulong64 payload = 0;
	} TKeyEntryHeader;
	#pragma pack(pop)

	typedef struct TKeyEntry {
		TKeyEntryHeader header;
		TKeyData freeKeyData;
	} TKeyEntry;

	typedef TPosWrapper<TKeyEntry> TKeyEntryInfo;

	struct TKeyInfoComparatorByInitialLength {
		bool operator() (const TKeyEntryInfo& lhs, const TKeyEntryInfo& rhs) const {
			return (lhs().header.initialDataLength > rhs().header.initialDataLength);
		}
	};

	inline std::ostream* operator << (std::ostream* os, const TKeyEntry& obj) {
		write(os, obj.header);
		os->write((char*)obj.freeKeyData.data(), obj.freeKeyData.size());
		return os;
	}

	inline std::ostream* operator << (std::ostream* os, const TKeyEntryInfo& objInfo) {
		os->seekp(objInfo.pos);
		os << objInfo();
		return os;
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

		const uint32 reservedKeys = KVDB_RESERVED_TABLE_SIZE;
		const volatile uint32 expandDataTo = 0; // kek
		uint32 keySize = 0;

	private:

		std::shared_ptr<V> valueFromData(TValueDataPtr dataPtr) const {
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
			TKeyData kd(sizeof(K));
			std::fill(kd.begin(), kd.end(), 0);
			std::memcpy(kd.data(), &key, sizeof(K));
			return kd;
		}

		static K keyFromKeyData(const TKeyData& kd) {
			K key;
			std::memcpy(&key, kd.data(), sizeof(K));
			return key;
		}

		static void toValueData(V value, TValueData& valueData) {
			if (valueData.size() < sizeof(value)) valueData.resize(sizeof(value));
			std::memcpy(valueData.data(), &value, sizeof(value));
		}

		void rewritePair(TKeyEntryInfo& keyInfo, const TValueData& valueData, const ulong64 k_flags) {
			// rewrite value data
			filePtr->seekp(keyInfo().header.dataPos);
			filePtr->write((char*)valueData.data(), valueData.size());
			// rewrite key data
			keyInfo().header.dataLength = valueData.size(); // new length
			keyInfo().header.flags = k_flags;
			filePtr << keyInfo;
		}

		void earsePair(TKeyEntryInfo& keyInfo) {
			// rewrite key data
			keyInfo().header.dataLength = 0; // new length
			keyInfo().header.flags = 0;
			filePtr << keyInfo;
			deletedKeyList.insert(keyInfo);
			dataMap.erase(keyInfo().freeKeyData);
		}

		void expandValueData(const TValueData& valueDataSrc, TValueData& valueDataNew, size_t size) {
			valueDataNew.resize(size);
			std::fill(valueDataNew.begin(), valueDataNew.end(), 0);
			std::memcpy(valueDataNew.data(), valueDataSrc.data(), valueDataSrc.size());
		}

		void newPairFromReserved(const TKeyData& keyData, const TValueData& valueData, const ulong64 k_flags) {
			// has reserved key slots
			TKeyEntryInfo& keyInfo = reservedKeyList.front();
			TValueData valueDataExp;

			if (expandDataTo > 0) {
				uint32 n = (uint32)std::round((float)valueData.size() / (float)expandDataTo) + 1;
				expandValueData(valueData, valueDataExp, (ulong64)n * (ulong64)expandDataTo);
			} else {
				valueDataExp = std::move(valueData);
			}

			filePtr->seekp(0, std::ios::end); // to end-of-file
			ulong64 endFile = (ulong64)(filePtr->tellp());
			filePtr->write((char*)valueDataExp.data(), valueDataExp.size());

			// fill key data
			keyInfo().header.dataLength = valueData.size(); // length
			keyInfo().header.initialDataLength = valueDataExp.size(); // length
			keyInfo().header.dataPos = (valueData.size() > 0) ? endFile : 1; // allow zero length value
			keyInfo().freeKeyData = keyData;
			keyInfo().header.flags = k_flags;
			filePtr << keyInfo;

			// add new pair to table 
			dataMap.insert({ keyInfo().freeKeyData, keyInfo });
			reservedKeyList.pop_front();
		}

		void readKey(std::istream* is, TKeyEntry& ke) {
			read(is, ke.header);
			ke.freeKeyData.resize(sizeof(K));
			is->read((char*)ke.freeKeyData.data(), sizeof(K));
		}

		static void writeKey(std::ostream* os, TKeyEntry& ke) {
			if(ke.freeKeyData.size() == 0){
				TKeyData kd;
				kd.resize(sizeof(K));
				std::fill(kd.begin(), kd.end(), 0);
				ke.freeKeyData = kd;
			}

			os << ke;
		}

		ulong64 readTable() {
			ulong64 tablePos = (ulong64)filePtr->tellg();

			TTableHeader tableHeader;
			filePtr >> tableHeader;

			for (unsigned int i = 0; i < tableHeader.recordCount; i++) {
				ulong64 pos = (ulong64)filePtr->tellg();
				TKeyEntry keyEntry;
				readKey(filePtr, keyEntry);

				TKeyEntryInfo keyInfo(keyEntry, pos);
				if (keyInfo().header.dataLength > 0) { 
					dataMap.insert({ keyEntry.freeKeyData, keyInfo });
				} else {
					if (keyInfo().header.initialDataLength == 0) { 
						if(keyInfo().header.dataPos == 1){ 
							dataMap.insert({ keyEntry.freeKeyData, keyInfo }); // key with zero length data
						} else {
							reservedKeyList.push_back(keyInfo); // reserved key slot
						}
					} else {
						deletedKeyList.insert(keyInfo); // marked as deleted pair
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
				writeKey(filePtr, newReservedKey);
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

		bool hasReserved() const {
			return reservedKeyList.size() > 0;
		}

		bool tryWriteToSuitableDeletedPair(const TKeyData& keyData, const TValueData& valueData, const ulong64 k_flags) {
			for (auto itr = deletedKeyList.begin(); itr != deletedKeyList.end();) {
				TKeyEntryInfo keyInfo = *itr;
				if (keyInfo().header.initialDataLength >= valueData.size()) {
					keyInfo().freeKeyData = keyData;
					keyInfo().header.flags = k_flags;
					rewritePair(keyInfo, valueData, k_flags);
					dataMap.insert({ keyInfo().freeKeyData, keyInfo });
					itr = deletedKeyList.erase(itr);
					return true;
				} else {
					++itr;
				}
			}
			return false;
		}

		void addNew(const TKeyData& keyData, const TValueData& valueData, const ulong64 k_flags) {
			if (!tryWriteToSuitableDeletedPair(keyData, valueData, k_flags)) {
				if (hasReserved()) {
					newPairFromReserved(keyData, valueData, k_flags);
				} else {
					createNewTable();
					newPairFromReserved(keyData, valueData, k_flags);
				}
			}
		}

		void change(const TKeyData& keyData, const TValueData& valueData, const ulong64 k_flags) {
			TKeyEntryInfo& keyInfo = dataMap[keyData];
			if (valueData.size() > 0) {
				if (keyInfo().header.initialDataLength >= valueData.size()) {
					rewritePair(keyInfo, valueData, k_flags);
				} else {
					//remove old and create new
					earsePair(keyInfo);
					addNew(keyData, valueData, k_flags);
				}
			} else {
				// erase
				earsePair(keyInfo);
			}
		}

	public:

		KvFile() {
			keySize = sizeof(K);
		}

		explicit KvFile(uint32 s) : expandDataTo(s) {
			keySize = sizeof(K);
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

		int open(const std::string& file) {
			filePtr = new std::fstream(file, std::ios::in | std::ios::out | std::ios::binary);

			if (!isOpen()) return KVDB_ERROR_OPEN_FILE;

			TFileHeader fileHeader;
			filePtr >> fileHeader;

			if(fileHeader.version != KVDB_FILE_VERSION){
				filePtr->close();
				return KVDB_ERROR_INCORRECT_FILE_VERSION;
			}

			ulong64 nextTablePos = readTable();
			while (nextTablePos > 0) {
				filePtr->seekg(nextTablePos);
				nextTablePos = readTable();
			}

			return KVDB_OK;
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

		ulong64 k_flags(const K& k) const {
			if (!isOpen()) return 0;

			const TKeyData keyData = toKeyData(k);
			std::lock_guard<std::mutex> guard(fileSharedMutex);

			if (auto a = dataMap.find(keyData); a != dataMap.end()) {
				const auto ki = a->second;
				return ki().header.flags;
			}
			
			return 0;
		}

		TValueDataPtr loadData(const K& k) const {
			if (!isOpen()) return nullptr;

			const TKeyData keyData = toKeyData(k);
			std::lock_guard<std::mutex> guard(fileSharedMutex);

			auto got = dataMap.find(keyData);
			if (got == dataMap.end()) return nullptr;

			const TKeyEntryInfo& i = got->second;
			const TKeyEntry& e = i();

			filePtr->seekg(e.header.dataPos);

			TValueDataPtr dataPtr = TValueDataPtr(new TValueData);
			dataPtr->resize(e.header.dataLength);

			if (filePtr->read((char*)dataPtr->data(), e.header.dataLength)) {
				return dataPtr;
			}

			return nullptr;
		}

		std::shared_ptr<V> load(const K& k) const {
			return valueFromData(loadData(k));
		}

		std::shared_ptr<V> operator[] (const K& k) {
			return valueFromData(loadData(k));
		}

		void erase(const K& k) {
			TKeyData keyData = toKeyData(k);

			if (!isOpen()) return;
			std::lock_guard<std::mutex> guard(fileSharedMutex);

			if (auto i = dataMap.find(keyData); i != dataMap.end()) {
				earsePair(i->second);
			}
		}

		void save(const K& k, const V& v, const ulong64 k_flags = 0x0) {
			TKeyData keyData = toKeyData(k);
			TValueData valueData;

			if constexpr(std::is_same<V, TValueData>::value) {
				valueData = static_cast<TValueData>(v);
			} else {
				toValueData(v, valueData);
			}

			if (!isOpen()) return;
			std::lock_guard<std::mutex> guard(fileSharedMutex);

			if (dataMap.find(keyData) == dataMap.end()) {
				// pair not found  
				addNew(keyData, valueData, k_flags);
			} else {
				// pair found 
				change(keyData, valueData, k_flags);
			}
		}

		static bool create(const std::string& file, const std::unordered_map<K, V>& test, ulong64 max_key_records = KVDB_RESERVED_TABLE_SIZE) {
			std::ofstream outFile(file, std::ios::out | std::ios::binary);
			if (!outFile) return false;
			std::ofstream* outFilePtr = &outFile;

			const ulong64 keyRecords = (test.size() > max_key_records) ? test.size() : max_key_records;

			// save file header
			TFileHeader fileHeader{ .keySize = sizeof(K) };
			outFilePtr << fileHeader;

			TTableHeader tableHeader{keyRecords, 0};
			outFilePtr << tableHeader;

			ulong64 bodyDataOffset = (ulong64)(outFile.tellp()) + (sizeof(TKeyEntryHeader) + sizeof(K)) * keyRecords;
			std::vector<byte> dataBody;
			for (auto& e : test) {
				TKeyEntry entry{ .header = TKeyEntryHeader{ .dataPos = dataBody.size() + bodyDataOffset }, .freeKeyData = toKeyData(e.first) };
				TValueData valueData;
				if constexpr (std::is_same<V, TValueData>::value) {
					valueData = static_cast<TValueData>(e.second);
				} else {
					toValueData(e.second, valueData);
				}

				entry.header.dataLength = valueData.size();
				entry.header.initialDataLength = valueData.size();
				dataBody.insert(std::end(dataBody), std::begin(valueData), std::end(valueData));
				outFilePtr << entry;
			}

			if (test.size() < max_key_records) {
				// add empty records
				const ulong64 emptyRecords = max_key_records - test.size();
				for (ulong64 i = 0; i < emptyRecords; i++) {
					TKeyEntry key;
					writeKey(outFilePtr, key);
				}
			}

			outFilePtr->write((char*)dataBody.data(), dataBody.size());
			outFile.close();
			return true;
		}

		static bool create_empty(const std::string& file, ulong64 max_key_records = KVDB_RESERVED_TABLE_SIZE) {
			std::ofstream outFile(file, std::ios::out | std::ios::binary);
			if (!outFile) return false;
			std::ofstream* outFilePtr = &outFile;

			// save file header
			TFileHeader fileHeader{ .keySize = sizeof(K) };
			outFilePtr << fileHeader;

			TTableHeader tableHeader{0, 0};
			outFilePtr << tableHeader;

			// add empty records
			const ulong64 emptyRecords = max_key_records;
			for (ulong64 i = 0; i < emptyRecords; i++) {
				TKeyEntry key;
				writeKey(outFilePtr, key);
			}

			outFile.close();
			return true;
		}


		// ====================================================================================
		
		size_t reserved() const {
			return reservedKeyList.size();
		}		
		
		size_t deleted() const {
			return deletedKeyList.size();
		}	
		
		void info(std::vector<TKeyEntry>& active, std::vector<TKeyEntry>& reserve, std::vector<TKeyEntry>& deleted) {
			if (!isOpen()) return;
			std::lock_guard<std::mutex> guard(fileSharedMutex);
			
			active.clear();
			active.reserve(dataMap.size());
			std::for_each(dataMap.cbegin(), dataMap.cend(), [&](const auto& p){ active.push_back(p.second()); });

			reserve.clear();
			reserve.reserve(reservedKeyList.size());
			std::for_each(reservedKeyList.cbegin(), reservedKeyList.cend(), [&](const auto& p){ reserve.push_back(p); });
            
			deleted.clear();
			deleted.reserve(deletedKeyList.size());
			std::for_each(deletedKeyList.cbegin(), deletedKeyList.cend(), [&](const auto& p){ deleted.push_back(p); });
		}
		// ====================================================================================

	};
	//-----------------------------------------------------------------------------
}
