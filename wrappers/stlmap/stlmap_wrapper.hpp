#ifndef __STLMAP_WRAPPER_HPP__
#define __STLMAP_WRAPPER_HPP__

#include "tree_api.hpp"

#include <cstdint>
#include <iostream>
#include <type_traits>
#include <map>
#include <cstring>
#include <array>

template<typename Key, typename T>
class stlmap_wrapper : public tree_api
{
public:
    stlmap_wrapper();
    virtual ~stlmap_wrapper();
    
    virtual bool find(const char* key, size_t key_sz, char* value_out) override;
    virtual bool insert(const char* key, size_t key_sz, const char* value, size_t value_sz) override;
    virtual bool update(const char* key, size_t key_sz, const char* value, size_t value_sz) override;
    virtual bool remove(const char* key, size_t key_sz) override;
    virtual int scan(const char* key, size_t key_sz, int scan_sz, char*& values_out) override;

private:
    std::map<Key,T> map_;
};

template<typename Key, typename T>
stlmap_wrapper<Key,T>::stlmap_wrapper()
{
}

template<typename Key, typename T>
stlmap_wrapper<Key,T>::~stlmap_wrapper()
{
}

template<typename Key, typename T>
bool stlmap_wrapper<Key,T>::find(const char* key, size_t key_sz, char* value_out)
{
    if constexpr (std::is_arithmetic<Key>::value)
    {
        auto it = map_.find(*reinterpret_cast<Key*>(const_cast<char*>(key)));
        if (it == map_.end())
            return false;

        if constexpr (std::is_arithmetic<T>::value)
            memcpy(value_out, &it->second, sizeof(T));
        else
            memcpy(value_out, it->second.c_str(), it->second.size());
    }
    else
    {
        auto it = map_.find(std::string(key, key_sz));
        if (it == map_.end())
            return false;

        if constexpr (std::is_arithmetic<T>::value)
            memcpy(value_out, &it->second, sizeof(T));
        else
            memcpy(value_out, it->second.c_str(), it->second.size());
    }
    return true;
}


template<typename Key, typename T>
bool stlmap_wrapper<Key, T>::insert(const char* key, size_t key_sz, const char* value, size_t value_sz)
{
    Key k;
    if constexpr (std::is_arithmetic<Key>::value)
        k = *reinterpret_cast<Key*>(const_cast<char*>(key));
    else
        k = std::string(key, key_sz);

    T v;
    if constexpr (std::is_arithmetic<T>::value)
        v = *reinterpret_cast<T*>(const_cast<char*>(value));
    else
        v = std::string(value, value_sz);


    return map_.insert(std::make_pair(k,v)).second;
}

template<typename Key, typename T>
bool stlmap_wrapper<Key, T>::update(const char* key, size_t key_sz, const char* value, size_t value_sz)
{
    typename std::map<Key,T>::iterator it;
    if constexpr (std::is_arithmetic<Key>::value)
        it = map_.find(*reinterpret_cast<Key*>(const_cast<char*>(key)));
    else
        it = map_.find(std::string(key, key_sz));

    if (it == map_.end())
        return false;
    else
    {
        if constexpr (std::is_arithmetic<T>::value)
            it->second = *reinterpret_cast<T*>(const_cast<char*>(value));
        else
            it->second = std::string(value, value_sz);

        return true;
    }
}

template<typename Key, typename T>
bool stlmap_wrapper<Key,T>::remove(const char* key, size_t key_sz)
{
    if constexpr (std::is_arithmetic<Key>::value)
        return map_.erase(*reinterpret_cast<Key*>(const_cast<char*>(key))) == 1;
    else
        return map_.erase(std::string(key, key_sz)) == 1;
}

template<typename Key, typename T>
int stlmap_wrapper<Key,T>::scan(const char* key, size_t key_sz, int scan_sz, char*& values_out)
{
    constexpr size_t ONE_MB = 1ULL << 20;
    static thread_local std::array<char, ONE_MB> results;

    int scanned;
    char* dst = reinterpret_cast<char*>(results.data());
    if constexpr (std::is_arithmetic<Key>::value)
    {
        auto it = map_.lower_bound(*reinterpret_cast<Key*>(const_cast<char*>(key)));
        if (it == map_.end())
            return 0;

        for(scanned=0; (scanned < scan_sz) && (it != map_.end()); ++scanned,++it)
        {
            memcpy(dst, &it->first, sizeof(Key));
            dst += sizeof(T);

            if constexpr (std::is_arithmetic<T>::value)
            {
                memcpy(dst, &it->second, sizeof(T));
                dst += sizeof(T);
            }
            else
            {
                memcpy(dst, it->second.c_str(), it->second.size());
                dst += it->second.size();
            }
        }
    }
    else
    {
        auto it = map_.lower_bound(std::string(key, key_sz));
        if (it == map_.end())
            return 0;

        for(scanned=0; (scanned < scan_sz) && (it != map_.end()); ++scanned,++it)
        {
            memcpy(dst, it->first.c_str(), it->first.size());
            dst += sizeof(T);

            if constexpr (std::is_arithmetic<T>::value)
            {
                memcpy(dst, &it->second, sizeof(T));
                dst += sizeof(T);
            }
            else
            {
                memcpy(dst, it->second.c_str(), it->second.size());
                dst += it->second.size();
            }
        }
    }
    values_out = results.data();
    return scanned;
}

#endif
