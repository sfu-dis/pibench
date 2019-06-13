#ifndef __DUMMY_WRAPPER_HPP__
#define __DUMMY_WRAPPER_HPP__

#include "tree_api.hpp"

class dummy_wrapper : public tree_api
{
public:
    dummy_wrapper() { }
    
    virtual ~dummy_wrapper() { }
    
    virtual bool find(const char* key, size_t key_sz, char* value_out) override
    {
        return true;
    }

    virtual bool insert(const char* key, size_t key_sz, const char* value, size_t value_sz) override
    {
        return true;
    }

    virtual bool update(const char* key, size_t key_sz, const char* value, size_t value_sz) override
    {
        return true;
    }

    virtual bool remove(const char* key, size_t key_sz) override
    {
        return true;
    }

    virtual int scan(const char* key, size_t key_sz, int scan_sz, char*& values_out) override
    {
        return scan_sz;
    }
};

#endif