### Not supported features: ###
  * Poll/Epoll.
  * RDTSC - Unlike in Linux, there's only one option for time handling. It is done using QueryPerformanceFrequency() and QueryPerformanceCounter() methoods.
  * VMA integration.
  * Daemonizing Sockperf process.
  * Message flags: MSG\_DONTWAIT, MSG\_NOSIGNAL.


### Notes/TODO's: ###
  * Modify type 'int' to type 'SOCK'.
  * Add windows installer.
  * Split os\_abstact to os\_win and os\_linux.
  * Add regular expression check for feed files.
  * Add colors (drops, errors, etc.)