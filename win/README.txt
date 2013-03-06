Sockperf - Windows support
==========================

Not supported features (Windows):
-	Poll/Epoll.
-	RDTSC - Unlike in Linux, there's only one option for time handling.
		It is done using QueryPerformanceFrequency() and QueryPerformanceCounter() methoods.
-	VMA integration.
-	Daemonizing Sockperf process.
-	Message flags: MSG_DONTWAIT, MSG_NOSIGNAL.


Notes/TODO's:
-	Modify type 'int' to type 'SOCK'.
-	Add windows installer.
-	Split os_abstact to os_win and os_linux.
-	Add regular expression check for the feed file.
-	Add colors (drops, errors, etc.)
