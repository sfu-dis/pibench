# PiBench
Framework for benchmarking persistent indexes.

# Building
The project comprises an executable binary that dynamically links to a shared library implementing a persistent data structure.

## Dependencies
The project requires c++17 and was tested with gcc 8.1.0 and CMake 3.13.1.

## CMake
CMake supports out-of-source builds, which means that binaries are generated in a different directory than the source files. This not only maintains a clean source directory, but also allows multiple coexisting builds with different configurations.

The typical approach is to create a `build` folder inside the project root folder after cloning it with git:
```
git clone --recursive https://github.com/wangtzh/pibench.git
cd nvm-data-structure
mkdir build
```

The `--recursive` option indicates that submodules should also be cloned. To generate the build files, type:
```
cd build
cmake ..
```

A specific compiler can be specified with:
```
CC=<path_to_bin> CXX=<path_to_bin> cmake ..
```

Alternatively, a debug version without optimizations is also supported:
```
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

Finally, to compile:
```
make
```
# Intel PCM
The tool relies on Processor Counters to collect hardware metrics.
It needs access to model-specific registers (MSRs) that need set up by loading
the `msr` kernel module. On Arch Linux, this is part of the `msr-tools` package
which can be installed through pacman. Then, load the module:
```
modprobe msr
```
It may happen that the following message is displayed during runtime:
```
Error while reading perf data. Result is -1
Check if you run other competing Linux perf clients.
```
If so, you can comment the following line in `pcm/Makefile`:
```
CXXFLAGS += -DPCM_USE_PERF
```

# Running
The `PiBench` executable is generated and supports the following arguments:
```
$ ./PiBench --help
Benchmark framework for persistent indexes.
Usage:
  PiBench [OPTION...] INPUT

      --input arg         Absolute path to library file
  -n, --records arg       Number of records to load (default: 1000000)
  -p, --operations arg    Number of operations to execute (default: 1000000)
  -t, --threads arg       Number of threads to use (default: 1)
  -f, --key_prefix arg    Prefix string prepended to every key (default: )
  -k, --key_size arg      Size of keys in Bytes (without prefix) (default: 4)
  -v, --value_size arg    Size of values in Bytes (default: 4)
  -r, --read_ratio arg    Ratio of read operations (default: 1)
  -i, --insert_ratio arg  Ratio of insert operations (default: 0)
  -u, --update_ratio arg  Ratio of update operations (default: 0)
  -d, --remove_ratio arg  Ratio of remove operations (default: 0)
  -s, --scan_ratio arg    Ratio of scan operations (default: 0)
      --scan_size arg     Number of records to be scanned. (default: 100)
      --sampling_ms arg   Sampling window in milliseconds (default: 1000)
      --distribution arg  Key distribution to use (default: UNIFORM)
      --skew arg          Key distribution skew factor to use (default: 0.2)
      --seed arg          Seed for random generators (default: 1729)
      --pcm               Turn on Intel PCM (default: true)
      --pool_path arg     Path to persistent pool (default: )
      --pool_size arg     Size of persistent pool (in Bytes) (default: 0)
      --help              Print help
```
The tree data structure implemented as a shared library must follow the API defined in [`tree_api.hpp`](include/tree_api.hpp).
An example can be found under [`wrappers/stlmap`](wrappers/stlmap)

The results are printed to `stdout`.
You probably want to redirect the output to a file to be later passed as an input parameter to plotting scripts (`1>results.txt`).
Also, PCM prints status messages to `stderr` and you probably want to discard them in the resulting file (`2>/dev/null`).
The output looks like this:
```
Benchmark Options:
        # Records: 10000000
        # Operations: 1000000
        # Threads: 1
        Sampling: 100 ms
        Key prefix:
        Key size: 4
        Value size: 8
        Random seed: 1729
        Key distribution: SELFSIMILAR(0.200000)
        Scan size: 100
        Operations ratio:
                Read: 0.5
                Insert: 0
                Update: 0.5
                Delete: 0
                Scan: 0
Overview:
        Load time: 13894.3 milliseconds
        Run time: 450.647 milliseconds
PCM Metrics:
        L3 misses: 465456489
        DRAM Reads (bytes): 372072000
        DRAM Writes (bytes): 194785536
        NVM Reads (bytes): 65489456
        NVM Writes (bytes): 465456987
Samples:
        192095
        216949
        205066
        241721
        144168
```

