<p align="center"> <img src="https://user-images.githubusercontent.com/7251387/73837645-2d9b8700-4812-11ea-8b5e-7cee08a52aac.png"></p>

[![Build Status](https://dev.azure.com/pibench/pibench-pipelines/_apis/build/status/wangtzh.pibench?branchName=master)](https://dev.azure.com/pibench/pibench-pipelines/_build/latest?definitionId=1&branchName=master)

# What is PiBench?
PiBench is a **p**ersistent **i**ndex **bench**mark tool targeted at data structures running on top of Intel Optane DC Persistent Memory.
The goal is to provide an unified benchmark framework to facilitate comparison across different results and data structures.
While PiBench can be used to benchmark regular DRAM data structures (such as C++ STL containers), it also gathers additional metrics specific to Intel Optane DC Persistent Memory.

# Building
The project comprises an executable binary that dynamically links to a shared library implementing a persistent data structure.

## Dependencies
The project requires C++17 and was tested with gcc 8.1.0 and CMake 3.13.1.

## CMake
CMake supports out-of-source builds, which means that binaries are generated in a different directory than the source files. This not only maintains a clean source directory, but also allows multiple coexisting builds with different configurations.

The typical approach is to create a `build` folder inside the project root folder after cloning it with git:
```bash
$ git clone --recursive https://github.com/wangtzh/pibench.git
$ cd pibench
$ mkdir build
```

The `--recursive` option indicates that submodules should also be cloned. To generate the build files, type:
```bash
$ cd build
$ cmake ..
```

A specific compiler can be specified with:
```bash
$ CC=<path_to_bin> CXX=<path_to_bin> cmake ..
```

Alternatively, a debug version without optimizations is also supported:
```bash
$ cmake -DCMAKE_BUILD_TYPE=Debug ..
```

Finally, to compile:
```bash
$ make
```
# Intel PCM
PiBench relies on [Processor Counter Monitor](https://github.com/opcm/pcm) to collect hardware metrics.
It needs access to model-specific registers (MSRs) that need set up by loading
the `msr` kernel module. On Arch Linux, this is part of the `msr-tools` package
which can be installed through pacman. Then, load the module:
```bash
$ modprobe msr
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

# OpenMP
PiBench uses OpenMP internally for multithreading.
The environment variable `OMP_NESTED=true` must be set to guarantee correctness.
Check [here](https://docs.microsoft.com/en-us/cpp/parallel/openmp/reference/openmp-environment-variables?view=vs-2019#omp-nested) for details.

Other environment variables such as [`OMP_PLACES`](https://gnu.huihoo.org/gcc/gcc-4.9.4/libgomp/OMP_005fPLACES.html#OMP_005fPLACES) and [`OMP_PROC_BIND`](https://gnu.huihoo.org/gcc/gcc-4.9.4/libgomp/OMP_005fPROC_005fBIND.html) can be set to control the multithreaded behavior.

For example:

`$ OMP_PLACES=cores OMP_PROC_BIND=true OMP_NESTED=true ./PiBench [...]`

Note for Clang users: you may need to additionally install OpenMP runtime, on Arch Linux this can be done by installing the package `extra/openmp`.


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
      --skip_load             Skip the load phase
      --latency_sampling arg  Sample latency of requests (default: 0)
      --help              Print help
```
The tree data structure implemented as a shared library must follow the API defined in [`tree_api.hpp`](include/tree_api.hpp).
An example can be found under [`wrappers/stlmap`](wrappers/stlmap)

The results are printed to `stdout`.
You probably want to redirect the output to a file to be later passed as an input parameter to plotting scripts (`1>results.txt`).
Also, PCM prints status messages to `stderr` and you probably want to discard them in the resulting file (`2>/dev/null`).
The output looks like this:
```
Environment:
        Time: Tue Nov  5 14:05:25 2019
        CPU: 96 * Intel(R) Xeon(R) Platinum 8260L CPU @ 2.40GHz
        CPU Cache: 36608 KB
        Kernel: Linux 5.3.4-3-default
Benchmark Options:
        # Records: 10000000
        # Operations: 1000000
        # Threads: 1
        Sampling: 100 ms
        Latency: 0.1
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
Latencies (99935 operations observed):
        min: 395
        50%: 1949
        90%: 2405
        99%: 11248
        99.9%: 14224
        99.99%: 23216
        99.999%: 59100
        max: 385366
```
# Tail Latency
PiBench can collect the latency of percentage of the total amount of request with the option `--latency_sampling=[0.0, 1.0]`.
This is the probability of the time of individual requests being measured.
A higher probability will result in more precise latency measurements, but also higher overhead.
The user is encouraged to try different percentages and compare latency and throughput numbers.
At the end of the execution the percentiles of the collected measurements is printed in nanoseconds (as seen above).

# Skipping Load Phase
The load phase is executed single-threaded to guarantee a deterministic end result of the data structure.
If the load phase takes too long, it might be helpful to preload the data structure and simply run the benchmark on a fresh working copy of the memory pool by skipping the load phase.
For example, this can be achieved with something like:
```bash
# Preload the tree pool
$ ./PiBench fptree.so -n 1000 -p 0 -r 1 --pool_path=/mnt/pmem1/pool --pool_size=4294967296

# Create working copy
cp /mnt/pmem1/pool /mnt/pmem1/tmp_pool

# Skip load and run benchmark on copy
$ ./PiBench fptree.so -n 1000 -p 1000 -r 1 --skip_load=true --pool_path=/mnt/pmem1/tmp_pool --pool_size=4294967296

# Remove working copy
rm /mnt/pmem1/tmp_pool
```
