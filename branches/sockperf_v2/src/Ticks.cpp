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
#define __STDC_LIMIT_MACROS  // for INT64_MAX in __cplusplus

#include "Ticks.h"

#include <string>
#include <stdio.h>
#include <errno.h>
#include <string.h>	// strerror()
#include <stdint.h> // INT64_MAX
#include <math.h>





#include "clock.h"
#include <unistd.h> // for usleep
/**
 * RDTSC extensions
 */
#define TSCVAL_INITIALIZER	(0)

/**
 * Read RDTSC register
 */


/**
 * Calibrate RDTSC with CPU speed
 * @return number of tsc ticks per second
 */
tscval_t get_tsc_rate_per_second()
{
	static tscval_t tsc_per_second = TSCVAL_INITIALIZER;
	if (!tsc_per_second) {
		uint64_t delta_usec;
		timespec ts_before, ts_after, ts_delta;
		tscval_t tsc_before, tsc_after, tsc_delta;

		// Measure the time actually slept because usleep() is very inaccurate.
		clock_gettime(CLOCK_MONOTONIC, &ts_before);
		tsc_before = gettimeoftsc();
		usleep(100000);//0.1 sec
		clock_gettime(CLOCK_MONOTONIC, &ts_after);
		tsc_after = gettimeoftsc();

		// Calc delta's
		tsc_delta = tsc_after - tsc_before;
		ts_sub(&ts_after, &ts_before, &ts_delta);
		delta_usec = ts_to_usec(&ts_delta);

		// Calc rate
		tsc_per_second = tsc_delta * USEC_PER_SEC / delta_usec;
	}
	return tsc_per_second;
}



// static variables initialization
const int64_t TicksImplRdtsc::TICKS_PER_SEC = get_tsc_rate_per_second();
const int64_t TicksImplRdtsc::TICKS_PER_MSEC = (TicksImplRdtsc::TICKS_PER_SEC+500)/1000;
const int64_t TicksImplRdtsc::MAX_MSEC_CONVERT = TICKS_PER_MSEC > NSEC_IN_MSEC  ? INT64_MAX / TICKS_PER_MSEC : INT64_MAX / NSEC_IN_MSEC;
const ticks_t TicksImplRdtsc::BASE_TICKS = gettimeoftsc();

const TicksDuration TicksDuration::TICKS0(0, 0); //call the non inline CTOR from slow path
const TicksDuration TicksDuration::TICKS1USEC (1000, 0); //call the non inline CTOR from slow path
const TicksDuration TicksDuration::TICKS1MSEC (1000*1000, 0); //call the non inline CTOR from slow path
const TicksDuration TicksDuration::TICKS1SEC  (1000*1000*1000, 0); //call the non inline CTOR from slow path
const TicksDuration TicksDuration::TICKS1MIN  (TICKS1SEC*60);
const TicksDuration TicksDuration::TICKS1HOUR (TICKS1MIN*60);
const TicksDuration TicksDuration::TICKS1DAY  (TICKS1HOUR*24);
const TicksDuration TicksDuration::TICKS1WEEK (TICKS1DAY*7);

const TicksTime TicksTime::TICKS0;

TicksBase::Mode TicksBase::ms_mode = TicksBase::RDTSC;



//------------------------------------------------------------------------------
// provide non inline functions/CTORs for reducing code size outside fast path and for quieting the compiler
TicksBase::TicksBase (ticks_t _ticks, int) : m_ticks(_ticks){}
ticks_t TicksBase::nsec2ticksNonInline(int64_t _val) {return ms_mode == RDTSC ? TicksImplRdtsc::nsec2ticks(_val) : TicksImplClock::nsec2ticks(_val);}
TicksDuration::TicksDuration(int64_t _nsec, int) : TicksBase(nsec2ticks(_nsec), 0){}
TicksTime & TicksTime::setNowNonInline() {m_ticks = getCurrentTicks(); return *this;}

//------------------------------------------------------------------------------
/*static*/ bool TicksBase::init(Mode _mode) { /* inited to rdtsc mode or clock mode */
	static bool isInited = false;
	if (isInited) return false;

	ms_mode = _mode;
	return true;
}

//==============================================================================
class TicksException : public std::exception{
public:
	TicksException(const char *_what) : m_what(_what) {}
	virtual ~TicksException() throw() {}
	virtual const char* what() const throw() {return m_what.c_str();}
	const std::string m_what;
};


//------------------------------------------------------------------------------
/*static*/void TicksImplClock::doThrow(const char *_func, const char *_file, int _line) throw(std::exception){
	const size_t LEN = 256;
	char buf[LEN+1];
	snprintf(buf, LEN, "%s:%d: %s() has failed:  (errno=%d %s)\n", _file, _line, _func, errno, strerror(errno));
	buf[LEN] = '\0';
	throw TicksException(buf);
}

//------------------------------------------------------------------------------
/*static*/ TicksDuration TicksDuration::stdDev(TicksDuration* pArr, size_t size){
	if (size <= 1) return TICKS0;

	double sum = 0;
	double sum_sqr = 0;
	double ticks = 0;

	for (size_t i = 0; i < size; i++) {
		ticks = pArr[i].m_ticks;
		sum += ticks;
		sum_sqr += ticks*ticks;
	}
	double avg = sum/size;
	double variance = (sum_sqr - size*avg*avg)/(size-1);
	double stdDev = sqrt(variance);
	return TicksDuration(ticks_t(stdDev+0.5), true);
}
