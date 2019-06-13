# Tree API

Tree data structures to be benchmarked must be compiled as a shared library and follow the API defined in `tree_api.hpp`.
To create a shared library following the API the developer must implement two main parts.

First, a function that is called by the framework for instantiating a `tree_api` object and returns its pointer:
```c++
extern "C" tree_api* create_tree(const tree_options_t& opt);
```
When instantiating the data structure, the developer can optionally rely on options defined in `tree_options_t` to choose an optimized version of its data structure.
As an example, one might decide to inline keys/values inside tree nodes based on their sizes.

Second, a wrapper class that inherits from the `tree_api` class.
This wrapper class will potentially have the real data structure object as a member and forward all the requests to it.
The wrapper class must override the following methods accordingly:
```c++
virtual void* find(const char* key, size_t sz) = 0;
virtual bool insert(const char* key, size_t key_sz, const char* value, size_t value_sz) = 0;
virtual bool update(const char* key, size_t key_sz, const char* value, size_t value_sz) = 0;
virtual bool remove(const char* key, size_t key_sz) = 0;
```

See the `stlmap` folder for an example of a wrapper class using `std::map` as its underlying data structure.
