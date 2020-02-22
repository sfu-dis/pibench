/**
 * Author:    Jonghyeok Park (akindo19@skku.edu)
 * Created:   02.21.2020
 * 
 * Code adapted from leveldb and https://github.com/wangtzh/bztree/tests/bztree_pibench_wrapper.h
 * Current implementation supports key size <= 8
 * All bugs are mine.
 **/
 
#include "leveldb_wrapper.h"

extern "C" tree_api* create_tree(const tree_options_t& opt) {
	return new leveldb_wrapper(opt);
}

