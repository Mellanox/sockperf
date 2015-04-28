In under-load mode we measure performance for messages when the wire is loaded with many packets as it usually happen in real life.

# Introduction #

In this mode messages are sent to the other side on time for generating the desired load, without waiting for reply of the previous packet.



# Details #
Unlike PingPong mode, this mode is similar to real life, and reflect situation in which the network is loaded with many packets in parallel to the packet that we are sending.

Important parameters for configuring the test in this mode are:

  * packets-per-second (or messages-per-second) - defines the load that we want to generate in this test.

  * reply-every - defines the portion of the packets for which we want to measure latency.

For example you can use MPS of 1,000,000 and ask for reply-every 100. This means that the test will generate load of 1,000,000 message per second (assuming your system can carry this load) and will measure the latency of 1/100 of them (using a reply-packet from the server to the client and dividing the round-trip by 2).  The reply-every=100 is neglectable comparing to the load of 1,000,000.  Still it will allow 10,000 samples per seconds for latency under the above load, which is quite enough for reliable statistics.