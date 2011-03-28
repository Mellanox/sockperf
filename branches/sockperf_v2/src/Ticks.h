 /*
 * Copyright (c) 2011 Mellanox Technologies Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the Mellanox Technologies Ltd nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#ifndef TICKS_H_
#define TICKS_H_

/*
 * TicksTime represents a point in time measured by ticks (related to some
 * abstract base point and not necessarily to real clock)
 * TicksDuration represents an interval of time measured by ticks (usually
 * delta between 2 TicksTime objects) that can be converted to nsec

 * The classes hide their internal implementation and therefore free to optimize/change it.
 * The current known internal implementations are based on clock_gettime or on RDTSC register.

 * Both classes are intended to be as efficient as primitive integral data types.
 * The advantage in these classes is because:
 *   1. you can easily switch your application to be based on clock_gettime or
 *   on RDTSC register (or on any other future implementation)
 *   2. the classes contain many utility and conversion functions to/from many 'time' types
 *   3. the classes contain operators for all valid comparison & arithmetic
 *   operations that are *legal* for them
 *   4. any operation that is illegal will be recognized as compilation
 *   error (for example division by TicksTime object)
 *   5, using these classes will make your code cleaner and safer and faster
 *   6. and more...
 *

------------
NOTE #1:
	For efficiency purposes, the classes leave all runtime checks to their users.
	Hence, there are no	overflow/underflow/divizion-by-zero checks.  Also there is no
	semantic for illegal values (for example: zero/negative time is considered legal
	from the class's point of view)
	(still, there are many compile time error checks, based on types and based on valid operations)

NOTE #2:
	any method that is related to getting current time * from CLOCK (not from RDTSC register) * may
	throw Exception (in case of fatal error related to clock)

------------
------------
Inspired by Boost.Date_Time documentation,
this section describes some of basic arithmetic rules that can be performed with TicksTime
and TicksDuration objects:

	TicksTime + TicksDuration  --> TicksTime
	TicksTime - TicksDuration  --> TicksTime
	TicksTime - TicksTime      --> TicksDuration

Unlike regular numeric types, the following operations are undefined and therefore prevented
by the class'es interface:

	TicksTime     + TicksTime  --> Undefined
	TicksDuration + TicksTime  --> Undefined
	TicksDuration - TicksTime  --> Undefined

---
TicksDuration represent a length of time and can have positive and negative values. It is
frequently useful to be able to perform calculations with other TicksDuration objects and
with simple integral values. The following describes these calculations:

	TicksDuration + TicksDuration  --> TicksDuration
	TicksDuration - TicksDuration  --> TicksDuration

	TicksDuration * Integer        --> TicksDuration
	Integer  * TicksDuration       --> TicksDuration
	TicksDuration / Integer        --> TicksDuration  (Integer Division rules)
*/

#include <time.h>		/* clock_gettime()*/
#include <exception>
#include <stdint.h>// for int64_t
#include <stdlib.h>// for qsort

typedef int64_t ticks_t;

// usefull constants
static const int64_t NSEC_IN_SEC  = 1000*1000*1000;
static const int64_t USEC_IN_SEC  = 1000*1000;
static const int64_t NSEC_IN_MSEC = 1000*1000;

//------------------------------------------------------------------------------
// utility functions
inline static int64_t timeval2nsec (const struct timeval  &_val) { return NSEC_IN_SEC*_val.tv_sec + 1000*_val.tv_usec; }
inline static int64_t timespec2nsec(const struct timespec &_val) { return NSEC_IN_SEC*_val.tv_sec + _val.tv_nsec; }
typedef unsigned long long tscval_t;
tscval_t get_tsc_rate_per_second();
static inline tscval_t gettimeoftsc()
{
	register uint32_t upper_32, lower_32;

	// ReaD Time Stamp Counter (RDTCS)
	__asm__ __volatile__("rdtsc" : "=a" (lower_32), "=d" (upper_32));

	// Return to user
	return (((tscval_t)upper_32) << 32) | lower_32;
}


//------------------------------------------------------------------------------
// forward declaration of classes in this file
class TicksImpl;
class TicksImplRdtsc;
class TicksImplClock;
//
class TicksBase;
class TicksTime;
class TicksDuration;

//------------------------------------------------------------------------------
class TicksImpl {
	friend class TicksBase;
protected:
	TicksImpl() {}
	virtual ~TicksImpl() {}
/* commented out because we avoid inheritance for saving time of dynamically invoking virtual function
	virtual ticks_t nsec2ticks(int64_t _val) = 0;
	virtual int64_t ticks2nsec(ticks_t _val) = 0;
	virtual ticks_t getCurrentTicks( ) = 0;
*/
};

//------------------------------------------------------------------------------
class TicksImplRdtsc : public TicksImpl {
	friend class TicksBase;
	friend class TicksTime;
protected:
	TicksImplRdtsc() {}
	virtual ~TicksImplRdtsc() {}
	static ticks_t nsec2ticks(int64_t _val) {if (_val < MAX_MSEC_CONVERT) return nsec2ticks_smallNamubers(_val); else return (ticks_t)_val/NSEC_IN_MSEC*TICKS_PER_MSEC + nsec2ticks_smallNamubers(_val%NSEC_IN_MSEC);}
	static int64_t ticks2nsec(ticks_t _val) {if (_val < MAX_MSEC_CONVERT) return ticks2nsec_smallNamubers(_val); else return (int64_t)_val/TICKS_PER_MSEC*NSEC_IN_MSEC + ticks2nsec_smallNamubers(_val%TICKS_PER_MSEC);}
	static ticks_t getCurrentTicks()  {return (ticks_t)gettimeoftsc() - BASE_TICKS;}

	//these methods may overflow in conversions for values greater than 1 million seconds ( > 10 days)
	inline static ticks_t nsec2ticks_smallNamubers(int64_t _val) {return (ticks_t)_val*TICKS_PER_MSEC/NSEC_IN_MSEC;}
	inline static int64_t ticks2nsec_smallNamubers(ticks_t _val) {return (int64_t)_val*NSEC_IN_MSEC/TICKS_PER_MSEC;}

	static const int64_t TICKS_PER_SEC;
	static const int64_t TICKS_PER_MSEC;
	static const int64_t MAX_MSEC_CONVERT;
	static const ticks_t BASE_TICKS;
};

//------------------------------------------------------------------------------
class TicksImplClock : public TicksImpl {
	friend class TicksBase;
protected:
	TicksImplClock() {}
	virtual ~TicksImplClock() {}
	static ticks_t nsec2ticks(int64_t _val) {return (ticks_t) _val;}
	static int64_t ticks2nsec(ticks_t _val) {return (int64_t) _val;}
	static ticks_t getCurrentTicks()  /* may throw exception in case clock_gettime fail */  {
		struct timespec ts;
		if (clock_gettime(CLOCK_MONOTONIC, &ts)) {
			doThrow("clock_gettime", __FILE__, __LINE__);
		}
		return (ticks_t)timespec2nsec(ts);
	}

	//------------------------------------------------------------------------------
	//utility function for throwing exception related to this class (no need to inline it)
	static void doThrow(const char *_func, const char *_file, int _line) throw(std::exception);
};


//==============================================================================
//==============================================================================
//==============================================================================
class TicksBase{

public:

	enum Mode {RDTSC, CLOCK};

	// initialize our system to rdtsc mode or clock mode
	// getting time from RDTSC takes ~34nsec,  getting time from CLOCK takes ~620nsec
	// (in addition clock accuracy is 1usec)
	static bool init(Mode _mode = RDTSC);

	//------------------------------------------------------------------------------
protected:
	static Mode ms_mode;
	ticks_t m_ticks;

	//------------------------------------------------------------------------------
	// these 3 methods are the sole thing that depends on mode (RDTSC/CLOCK/...)
	inline static ticks_t nsec2ticks(int64_t _val) {return ms_mode == RDTSC ? TicksImplRdtsc::nsec2ticks(_val) : TicksImplClock::nsec2ticks(_val);}
	static ticks_t nsec2ticksNonInline(int64_t _val);
	inline static int64_t ticks2nsec(ticks_t _val) {return ms_mode == RDTSC ? TicksImplRdtsc::ticks2nsec(_val) : TicksImplClock::ticks2nsec(_val);}
	inline static ticks_t getCurrentTicks( ) {return ms_mode == RDTSC ? TicksImplRdtsc::getCurrentTicks() : TicksImplClock::getCurrentTicks();}

	//------------------------------------------------------------------------------
	// CTORs
	inline TicksBase (ticks_t _ticks)          : m_ticks(_ticks){}
	TicksBase (ticks_t _ticks, int); //provide non inline CTOR just to quite compiler
	inline TicksBase (const TicksBase & other) : m_ticks(other.m_ticks){}
};

//==============================================================================
class TicksDuration : public TicksBase{
	friend class TicksTime;
public:
	// CTORs & DTOR
	inline TicksDuration () : TicksBase(0){}
	inline explicit TicksDuration (int64_t _nsec) : TicksBase(nsec2ticks(_nsec)){}
	inline explicit TicksDuration (const struct timeval   & _val) : TicksBase(nsec2ticks(timeval2nsec(_val))){}
	inline explicit TicksDuration (const struct timespec  & _val) : TicksBase(nsec2ticks(timespec2nsec(_val))){}
	inline ~TicksDuration(){}

	//Useful constants
	static const TicksDuration TICKS0;     // = TicksDuration(0);

	static const TicksDuration TICKS1USEC;
	static const TicksDuration TICKS1MSEC;
	static const TicksDuration TICKS1SEC;

	static const TicksDuration TICKS1MIN;  // = TicksDuration(TICKS1SEC*60);
	static const TicksDuration TICKS1HOUR; // = TicksDuration(TICKS1MIN*60);
	static const TicksDuration TICKS1DAY;  // = TicksDuration(TICKS1HOUR*24);
	static const TicksDuration TICKS1WEEK; // = TicksDuration(TICKS1DAY*7);

	//operators
	inline bool operator<  (const TicksDuration & rhs)  const {return this->m_ticks <  rhs.m_ticks;}
	inline bool operator>  (const TicksDuration & rhs)  const {return this->m_ticks >  rhs.m_ticks;}
	inline bool operator== (const TicksDuration & rhs)  const {return this->m_ticks == rhs.m_ticks;}
	inline bool operator!= (const TicksDuration & rhs)  const {return this->m_ticks != rhs.m_ticks;}
	inline bool operator<= (const TicksDuration & rhs)  const {return this->m_ticks <= rhs.m_ticks;}
	inline bool operator>= (const TicksDuration & rhs)  const {return this->m_ticks >= rhs.m_ticks;}

	inline TicksDuration   operator+  (const TicksDuration & rhs)  const {return TicksDuration(this->m_ticks + rhs.m_ticks, true);}
	inline TicksDuration   operator-  (const TicksDuration & rhs)  const {return TicksDuration(this->m_ticks - rhs.m_ticks, true);}
	inline TicksDuration & operator+= (const TicksDuration & rhs)        {this->m_ticks += rhs.m_ticks; return *this;}
	inline TicksDuration & operator-= (const TicksDuration & rhs)        {this->m_ticks -= rhs.m_ticks; return *this;}

	inline TicksDuration   operator*  (int rhs) const {return TicksDuration(this->m_ticks * rhs, true);}
	inline TicksDuration   operator/  (int rhs) const {return TicksDuration(this->m_ticks / rhs, true);}
	inline TicksDuration & operator*= (int rhs)       {this->m_ticks *= rhs; return *this;}
	inline TicksDuration & operator/= (int rhs)       {this->m_ticks /= rhs; return *this;}

	// 'getCurrentTicks' related method - may throw exception if using clock_gettime and call fatally failed
	/*inline*/ void setDurationSince(const TicksTime & start);

	inline void setFromSeconds(double seconds) {
		int64_t nsec = (int64_t)(seconds * NSEC_IN_SEC);
		m_ticks = nsec2ticks(nsec);
	}

	// utility functions
	//------------------------------------------------------------------------------
	inline int64_t       toNsec() const { return ticks2nsec(m_ticks); }
	inline int64_t       toUsec() const { return (toNsec()+500)/1000; }
	inline double toDecimalUsec() const { return (double)toNsec()/1000; }
	//------------------------------------------------------------------------------
	inline void toTimespec(struct timespec &_val) const {
		int64_t nsec = toNsec();
		_val.tv_sec  = nsec / NSEC_IN_SEC;
		_val.tv_nsec = nsec % NSEC_IN_SEC;
	}
	//------------------------------------------------------------------------------
	inline void toTimeval(struct timeval &_val) const {
		int64_t usec  = toUsec();
		_val.tv_sec   = usec / (USEC_IN_SEC);
		_val.tv_usec  = usec % (USEC_IN_SEC);
	}

	//------------------------------------------------------------------------------
	static int compare(const void *arg1, const void *arg2) {
		const TicksDuration & t1 = *(TicksDuration*)arg1;
		const TicksDuration & t2 = *(TicksDuration*)arg2;
		return t1 > t2 ? 1 : t1 < t2 ? -1 : 0; // Note: returning t1-t2 will not be safe because of cast to int
	}
	//------------------------------------------------------------------------------
	static void sort(TicksDuration* pArr, size_t size) {
		qsort(pArr, size, sizeof(TicksDuration), compare);
	}

	//------------------------------------------------------------------------------
	static TicksDuration stdDev(TicksDuration* pArr, size_t size);

private:
	inline explicit TicksDuration(ticks_t _ticks, bool): TicksBase(_ticks){}
	explicit TicksDuration (int64_t _nsec, int); // provide non inline function for reducing code size outside fast path
};

//==============================================================================
class TicksTime : public TicksBase{
	friend class TicksDuration;
public:
	//CTORs & DTOR
	inline TicksTime ()     : TicksBase(0){} // init to 0
	//inline TicksTime (bool) : TicksBase(getCurrentTicks()){} //init to now
	inline ~TicksTime(){}

	static const TicksTime TICKS0; // = TicksTime(0);

	// 'getCurrentTicks' related method - may throw exception if using clock_gettime and call fatally failed
	inline static TicksTime             now() {return TicksTime(getCurrentTicks());}
	inline TicksTime &               setNow() {m_ticks = getCurrentTicks(); return *this;}
	TicksTime &               setNowNonInline(); // provide non inline function for reducing code size outside fast path
	inline TicksDuration    durationTillNow() {return TicksDuration( getCurrentTicks() - m_ticks, true );}

	//operators
	inline bool operator<  (const TicksTime & rhs)  const {return this->m_ticks <  rhs.m_ticks;}
	inline bool operator>  (const TicksTime & rhs)  const {return this->m_ticks >  rhs.m_ticks;}
	inline bool operator== (const TicksTime & rhs)  const {return this->m_ticks == rhs.m_ticks;}
	inline bool operator!= (const TicksTime & rhs)  const {return this->m_ticks != rhs.m_ticks;}
	inline bool operator<= (const TicksTime & rhs)  const {return this->m_ticks <= rhs.m_ticks;}
	inline bool operator>= (const TicksTime & rhs)  const {return this->m_ticks >= rhs.m_ticks;}

	inline TicksTime     operator+  (const TicksDuration & rhs) const {return TicksTime    (this->m_ticks + rhs.m_ticks);}
	inline TicksTime     operator-  (const TicksDuration & rhs) const {return TicksTime    (this->m_ticks - rhs.m_ticks);}
	inline TicksDuration operator-  (const TicksTime     & rhs) const {return TicksDuration(this->m_ticks - rhs.m_ticks, true);}

	inline TicksTime &   operator+= (const TicksDuration & rhs) {this->m_ticks += rhs.m_ticks; return *this;}
	inline TicksTime &   operator-= (const TicksDuration & rhs) {this->m_ticks -= rhs.m_ticks; return *this;}


	// this method is for debug purposes only since it breaks the concept of TicksTime as a point in time
	// and consider it as duration since some base point!!
	inline int64_t debugToNsec() const { return ticks2nsec(m_ticks);}

	//------------------------------------------------------------------------------
private:
	inline TicksTime(ticks_t _ticks): TicksBase(_ticks){}

};
//------------------------------------------------------------------------------

// 'getCurrentTicks' related method - may throw exception if using clock_gettime and call fatally failed
inline void TicksDuration::setDurationSince(const TicksTime & start) {
	m_ticks = getCurrentTicks();
	m_ticks -= start.m_ticks;
}

#endif /* TICKS_H_ */
