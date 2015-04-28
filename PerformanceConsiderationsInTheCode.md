# Introduction #

sockperf is designed to work efficiently and reliably under load of millions packets per second.

# Details #

  * **C++ Templates** - for static binding of "virtual" functions (also enables inlining, unlike virtual functions).  This saves many 'if's in the code for decisions that are constant throughout the lifetime of the fast path (for example switches from command line).  In addition to performance gain, this dramatically simplifies the code (and if you don't believe compare sockperf\_v1 to its father udp\_lat).

  * **RDTSC** - for high-resolution, low-overhead way of getting timing information (applies for all modern systems with "Invariant TSC").

  * **minimal overhead in the fast path** - in the fast path we only take txTime & rxTime of packets.  All calculations and statistics are performed after the fast path.  All memory allocations and other preparations are performed before the fast path.

  * **inline the entire fast path** turn on -Winline and -Werror and analyze any place the optimizer decided to avoid inlining a function call in the fast path.  Then improving the code so the optimizer will agree to inline it without compromising its good preferences.

  * **error handling is not inlined** replace blocks of error handling code with function call to error function.  This leaves more place for inlining real code of the fast path.

  * **no locks between threads** The code is written in a way that there is no need to use locks at all between threads.  This is done while still keeping the data of both threads (sender & receiver threads in the client) in same pages for CPU cache purposes.  Both threads write to distinct entries in the global array of PacketTimes, and still these entries are close one to each other.  i.e., 1 entry for send time of a packet, followed by N entries for receive time of this packet's replies (from multiple servers).  After that comes another 1 entry for send, followed by N entries for recv and so on.

  * **and more...**