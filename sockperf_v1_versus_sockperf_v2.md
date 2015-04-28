# Introduction #

sockperf\_v1 is based on Voltaire's udp\_lat and only supports UDP traffic (both UC and MC).

sockperf\_v2 adds TCP support (and some complexity in the code)


# Details #

sockperf\_v1 is based on udp\_lat and adds to it:
  * latency under load mode
  * playback mode
  * discrete measurements at packet level (based on RDTSC) that allows providing better statistics (like histogram).
  * additional switches and new options
  * major design changes for making the code easier for maintenance, while still keeping good performance and even improving it a bit.

sockperf\_v2 is based on sockperf\_v1 and adds to it TCP support (and few more options).  It introduces more complexity in the code, but still provide the good performance.

In addition, sockperf\_v1 was developed internally in Voltaire (acquired by Mellanox).  While sockperf\_v2 was mainly developed by 3rd party contractor.
Both versions are maintained by Mellanox.