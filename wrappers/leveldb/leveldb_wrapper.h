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
#include <sstream>

#include <cstdint>
#include <iostream>
#include <type_traits>
#include <cstring>
#include <array>
#include <mutex>
#include <shared_mutex>

/* Describes all of the sstables that make up the db contents. */
#define LEVELDB_SSTTABLE_STATS "leveldb.sstables"
/* Statistics about the internal operation of the DB. */
#define LEVELDB_STAT "leveldb.stats"
/* Approximate number of bytes of memory in use by the DB. */
#define LEVELDB_MEMORY_USAGE "leveldb.approximate-memory-usage"
/* Number of files at level (append level as number). */
#define LEVELDB_NUM_FILES_AT_LEVEL "leveldb.num-files-at-level"

class leveldb_wrapper : public tree_api
{
public:
	leveldb_wrapper(const tree_options_t& opt);

	virtual  ~leveldb_wrapper();
	
	virtual bool find(const char* key, size_t key_sz, char* value_out) override;
	virtual bool insert(const char* key, size_t key_sz, const char* value, size_t value_sz) override;
	virtual bool update(const char* key, size_t key_sz, const char* value, size_t value_sz) override;
	virtual bool remove(const char* key, size_t key_sz) override;
	virtual int scan(const char* key, size_t key_sz, int scan_sz, char*& values_out) override;

	void print_stat(leveldb::DB* db_, bool print_sst=false);

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

leveldb_wrapper::~leveldb_wrapper(){
	// TODO(jhpark) : pass parameters for leveldb statistics
	print_stat(db);
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

	for (iterator->Seek(key); (iterator->Valid() && scanned < scan_sz); iterator->Next()) {

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

void leveldb_wrapper::print_stat(leveldb::DB* db_, bool print_sst) {

	std::string stats_property;
	std::string sst_property;
	std::string mem_usage_property;
	std::string numfiles_level_property;

	std::cout << std::string(50, '=') << "\n";
	if(print_sst) {
		db_->GetProperty(LEVELDB_SSTTABLE_STATS, &sst_property);
    std::cout << "LevelDB SST Description:"
              << "\n"
              << sst_property << std::endl;
	}

  db_->GetProperty(LEVELDB_STAT, &stats_property);
  std::cout << "LevelDB Stats:"
            << "\n"
            << stats_property << std::endl;

	db_->GetProperty(LEVELDB_MEMORY_USAGE, &mem_usage_property);
  std::cout << "LevelDB Memory Usage:"
            << "\n\t"
            << mem_usage_property << " (B)" << std::endl;

	// print number of files at each levels
	std::cout << "LevelDB Number of files at each levels:" << std::endl;
	uint32_t level = 0;
	uint32_t total_files = 0;
	int num_files = 0;
  bool ret = false;

  while (true) {
    ret = db_->GetProperty(LEVELDB_NUM_FILES_AT_LEVEL + std::to_string(level), &numfiles_level_property);
    if (!ret) {
      break;
    }
    
    std::istringstream(numfiles_level_property) >> num_files;
    total_files += num_files;
    std::cout << "\tLevel " << level << ": " << num_files << std::endl;
	  level++;
  }

	std::cout << "\tTotal: " << total_files << std::endl;
	std::cout << std::string(50, '=') << "\n";
}
