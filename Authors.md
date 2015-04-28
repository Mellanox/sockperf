# Details #

sockperf is based on udp\_lat that was developed by the VMA team at Voltaire.
udp\_lat is able to measure **average** latency of **ping-pong** test for UDP traffic.

  * sockperf\_v1 (adds **latency-under-load** and **playback** modes, adds **discrete measuring of times at packet level** using RDTSC and prints histogram with percentiles for **spike analysis**, and uses **entire re-architecture of the code** using C++ templates for keeping high performance with readable and maintainable code) was developed by Avner BenHanoch, based on udp\_lat.

  * sockperf\_v2 (**TCP support**) was mainly developed by Igor Ivanov and others, based on sockperf\_v1.