/**
 * Author:    Jonghyeok Park (akindo19@skku.edu)
 * Created:   02.21.2020
 * 
 * Code adapted from leveldb and bztre
 * (https://github.com/wangtzh/bztree/blob/master/tests/bztree_pibench_wrapper.cc)
 * Current implementation supports key size <= 8 and naive scan 
 * All bugs are mine.
 **/

#pragma once

#include "tree_api.hpp"
#include <string>
#include <vector>
#include <cassert>
#include "leveldb/db.h"


#include <cstdint>
#include <iostream>
#include <type_traits>
#include <cstring>
#include <array>
#include <mutex>
#include <shared_mutex>


class leveldb_wrapper : public tree_api
{
public:
	leveldb_wrapper(const tree_options_t& opt);
	leveldb_wrapper();

	virtual  ~leveldb_wrapper();
	
	virtual bool find(const char* key, size_t key_sz, char* value_out) override;
	virtual bool insert(const char* key, size_t key_sz, const char* value, size_t value_sz) override;
	virtual bool update(const char* key, size_t key_sz, const char* value, size_t value_sz) override;
	virtual bool remove(const char* key, size_t key_sz) override;
	virtual int scan(const char* key, size_t key_sz, int scan_sz, char*& values_out) override;

private:
	leveldb::DB* db;
	leveldb::Options options;
};

leveldb_wrapper::leveldb_wrapper(const tree_options_t& opt) {

	// TODO(jhpark) : Provide specific wrapper options in Pibench command 
	options.create_if_missing = true;
	options.write_buffer_size = 64 * 1024L * 1024L;
	options.compression = leveldb::kNoCompression;

	leveldb::Status status = leveldb::DB::Open(options, opt.pool_path, &db);
	if (!status.ok()) {
    fprintf(stderr, "open error: %s\n", status.ToString().c_str());
		exit(1);
	}
}

leveldb_wrapper::leveldb_wrapper(){
}

leveldb_wrapper::~leveldb_wrapper(){
	delete db;
}

bool leveldb_wrapper::find(const char* key, size_t key_sz, char* value_out)
{
	// TODO(jhpark): Provide positive/false read statistics
	std::string str;
	uint64_t k = __builtin_bswap64(*reinterpret_cast<const uint64_t *>(key));
  leveldb::Status s = db->Get(leveldb::ReadOptions(), reinterpret_cast<const char *>(&k), &str);

	if (s.ok()) {
		std::vector<char> result(str.begin(), str.end());
		result.push_back('\0');
		value_out = &result[0];
	} 

	return true;
}


bool leveldb_wrapper::insert(const char* key, size_t key_sz, const char* value, size_t value_sz)
{
	uint64_t k = __builtin_bswap64(*reinterpret_cast<const uint64_t *>(key));
	leveldb::Status s = db->Put(leveldb::WriteOptions(), reinterpret_cast<const char *>(&k), value);
	
	if (s.ok()) {
		return true;
	} else {
		return false;
	}
}

bool leveldb_wrapper::update(const char* key, size_t key_sz, const char* value, size_t value_sz) {
  return insert(key, key_sz, value, value_sz);
}

bool leveldb_wrapper::remove(const char* key, size_t key_sz) {
	uint64_t k = __builtin_bswap64(*reinterpret_cast<const uint64_t *>(key));
  leveldb::Status s = db->Delete(leveldb::WriteOptions(), reinterpret_cast<const char *>(&k));

	if (s.ok()) {
		return true;
	} else {
		return false;
	}
}

int leveldb_wrapper::scan(const char* key, size_t key_sz, int scan_sz, char*& values_out) {
	leveldb::Iterator* iterator = db->NewIterator(leveldb::ReadOptions());
	// TODO(jhpark): 
	//	- Provide positive/false scan statistics
	//	- Efficient scan algorithms needed

	int scanned = 0;
	uint64_t k = __builtin_bswap64(*reinterpret_cast<const uint64_t *>(key));

 	static thread_local std::array<char, (1 << 20)> results;
	char *dst = results.data();

	for (iterator->SeekToFirst(); (iterator->Valid()); iterator->Next()) {

		uint64_t result_key = reinterpret_cast<const uint64_t>(iterator->key().data());

		if (memcmp(reinterpret_cast<const char *>(&k), 
				iterator->key().data(), sizeof(uint64_t))==0) {

			memcpy(dst, &result_key, sizeof(uint64_t));
			dst += sizeof(uint64_t);
			auto result_value = iterator->value();
			memcpy(dst, &result_value, sizeof(result_value));
			++scanned;
		} 
	}

	delete iterator;
	values_out = results.data();

	// FIXME(jhpark) : the return type of "scan" funcction should be changed as boolean.
	return 1;
}
