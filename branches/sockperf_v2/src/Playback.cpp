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
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include "Defs.h"
#include "Playback.h"
#include "Ticks.h"

// How to build unit test: g++ Playback.cpp Ticks.cpp -lrt -Wall -Dplayback_test=main -o playback

using std::cout;

typedef std::vector<std::string>  StringVector;

void readFile(StringVector &sv, const char *filename)
{
	std::string line;
	std::ifstream infile (filename, std::ios_base::in);
	if (infile.fail()) {
		log_msg("Can't open file: %s\n", filename);
		exit(SOCKPERF_ERR_NOT_EXIST);
	}
	int i = 0;

	while (infile.good() && getline(infile, line))
	{
		sv.push_back (line);
		i++;
	}
	if (!infile.eof()) {
		log_msg("file: %s, error in line: #%d\n", filename, i+1);
		exit(SOCKPERF_ERR_INCORRECT);
	}

}


void parsePlaybackData(PlaybackVector &pv, StringVector &sv)
{
	PlaybackItem   pi;
	
	double curr_time, prev_time = 0;
	TicksDuration duration;

	for (size_t i = 0; i < sv.size(); i++) {
		std::string s = sv[i];
		if (!s.empty() && s[0] != '#')
		{
			//printf("[%lu]: %s\n", i+1, s.c_str());
			if (2 == sscanf(s.c_str(), "%lf, %d", &curr_time, &pi.size))
			{
				if (prev_time > curr_time) {printf("out-of-order timestamp at line #%lu\n", (long unsigned)i+1); exit(SOCKPERF_ERR_INCORRECT);}
				pi.duration.setFromSeconds(curr_time - prev_time);
				if (!pi.isValid()) {printf("illegal time or size at line #%lu\n", (long unsigned)i+1); exit(SOCKPERF_ERR_INCORRECT);}
			
				pv.push_back(pi);
				prev_time = curr_time;
			}
			else
			{
				cout << "can't read time & size, at line #" << i+1 << '\n';
				exit(1);
			}
		}
	}	
	//printf("pv.size()=%d\n", (int)pv.size());
}

void loadPlaybackData(PlaybackVector &pv, const char *filename)
{
	StringVector sv;
	readFile(sv, filename);
	parsePlaybackData(pv, sv);
	sv.clear();
}

void doPlayback(PlaybackVector &pv)
{
	size_t size =  pv.size();
	for (size_t i = 0; i < size; i++)
		printf("item #%lu: duration=%" PRId64 ", size=%d\n", (long unsigned)i, pv[i].duration.toNsec(), pv[i].size);
}

// unit test
int playback_test(int argc, char* argv[])
{
	if (argc < 2) {
	  cout << argv[0] << ": Need a filename for a parameter.\n";
	  exit(1);
	}

	PlaybackVector pv;
	loadPlaybackData(pv, argv[1]);
	doPlayback(pv);
	return 0;
}
