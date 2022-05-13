## Introduction

**sockperf** is a network benchmarking utility over socket API that was designed for testing performance (latency and throughput) of high-performance systems (it is also good for testing performance of regular networking systems). It covers most of the socket API calls and options.

Specifically, in addition to the standard throughput tests, **sockperf** does the following:

  * Measure latency of each discrete packet at sub-nanosecond resolution (using TSC register that counts CPU ticks with very low overhead).
  
  * Does the above for both ping-pong mode and latency under load mode. This means that we measure latency of single packets even under load of millions of Packets Per Second (without waiting for reply of packet before sending subsequent packet on time)
  
  * Enable spike analysis by providing histogram, with various percentiles of the packets’ latencies (for example: median, min, max, 99% percentile, and more), (this is in addition to average and standard deviation). Also, **sockperf** provides a full log with all packet’s tx/rx times that can be further analyzed with external tools, such as MS-Excel or matplotlib - All this without affecting the benchmark itself.
  
  * Support MANY optional settings for good coverage of socket API and network configurations, while still keeping very low overhead in the fast path to allow cleanest results.
  
## Prereqs: What you will need to compile sockperf on Unix systems

   * Perl 5.8+ (used by the automake tools)

   * GNU make tools: automake 1.7+, autoconf 2.57+, m4 1.4+ and libtool 1.4+

   * A C++11 Compiler, among those tested are:
    
     * GCC
     * Clang
     * icc

   `sudo apt install perl make automake autoconf m4 libtool-bin g++`

## How to install

  The sockperf package uses the GNU autotools compilation and installation
  framework.
```  
./autogen.sh  (only when cloning from repository)
./configure --prefix=<path to install>
make
make install
 ```
### Configuration

   Type `./configure --help` for a list of all the configure
   options. Some of the options are generic autoconf options, while the sockperf
   specific options are prefixed with "SOCKPERF:" in the help text.
   
 * To enable TLS support
   * `./configure --prefix=<path to install> --with-tls=<path to OpenSSL install>`
   * Use OpenSSL 3.0.0 or higher

 * To enable unit tests
   * `./configure --prefix=<path to install> --enable-test`

 * To enable the documentation
   * `./configure --prefix=<path to install> --enable-doc`

 * To enable the special scripts
   * `./configure --prefix=<path to install> --enable-tool`

 * To compile with debug symbols and information:
   * `./configure --prefix=<path to install> --enable-debug`
   * This will define the DEBUG variable at compile time.

### To build for ARM

1) Define CROSS_COMPILE in the environment to point to the cross compilation tools, e.g.
set `CROSS_COMPILE=/opt/gcc-linaro-arm-linux-gnueabihf-4.7-2012.11-20121123_linux/bin/arm-linux-gnueabihf-`
2) Use `./autogen.sh` to create the configure script.
3) Invoke `./configure` with the following options:
`./configure CXX=${CROSS_COMPILE}g++ STRIP=${CROSS_COMPILE}strip
LD=${CROSS_COMPILE}ld CC=${CROSS_COMPILE}gcc --host i386`
4) Invoke `make`

### To build for FreeBSD

* Make sure automake tools are installed.

## Licensing

   [View Here](https://github.com/Mellanox/sockperf/blob/sockperf_v2/copying)

~Good luck!

