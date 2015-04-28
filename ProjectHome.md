**sockperf** is a network benchmarking utility over socket API that was designed for testing performance (latency and throughput) of high-performance systems (it is also good for testing performance of regular networking systems as well).  It covers most of the socket API calls and options.

Specifically, in addition to the standard throughput tests, **sockperf**, does the following:

  * measure latency of each discrete packet at sub-nanosecond resolution (using TSC register that counts CPU ticks with very low overhead).

  * Does the above for both ping-pong mode and for latency under load mode. This means that we measure latency of single packets even under load of millions Packets Per Second (without waiting for reply of packet before sending subsequent packet on time)

  * Enable spike analysis by providing histogram, with various percentiles of the packets’ latencies (for example: median, min, max, 99% percentile, and more), (this is in addition to average and standard deviation). Also, **sockperf** provides full log with all packet’s tx/rx times that can be further analyzed with external tools, such as MS-Excel or matplotlib - All this without affecting the benchmark itself.

  * Support MANY optional settings for good coverage of socket API and network configurations, while still keeping very low overhead in the fast path to allow cleanest results.