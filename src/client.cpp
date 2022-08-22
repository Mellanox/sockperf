/*
 * Copyright (c) 2011-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
 */

#include "defs.h"
#include "playback.h"
#include "client.h"
#include "iohandlers.h"
#include "packet.h"
#include "switches.h"

#include <math.h>
#include <map>

TicksTime s_startTime, s_endTime;

//==============================================================================
//==============================================================================

//------------------------------------------------------------------------------
void print_average_results(double usecAvarage) {
    if (g_pApp->m_const_params.burst_size == 1) {
        log_msg("Summary: %s is %.3lf usec",
                g_pApp->m_const_params.full_rtt ? "Round trip" : "Latency", usecAvarage);
    } else {
        log_msg("Summary: %s of burst of %d messages is %.3lf usec",
                g_pApp->m_const_params.full_rtt ? "Round trip" : "Latency",
                g_pApp->m_const_params.burst_size, usecAvarage);
    }
}

//------------------------------------------------------------------------------
/* set the timer on client to the [-t sec] parameter given by user */
void set_client_timer(struct itimerval *timer) {
    // extra sec and extra msec will be excluded from results
    timer->it_value.tv_sec =
        (g_pApp->m_const_params.cooldown_msec + g_pApp->m_const_params.warmup_msec) / 1000 +
        g_pApp->m_const_params.sec_test_duration;
    timer->it_value.tv_usec =
        (g_pApp->m_const_params.cooldown_msec + g_pApp->m_const_params.warmup_msec) % 1000;
    timer->it_interval.tv_sec = 0;
    timer->it_interval.tv_usec = 0;
}

//------------------------------------------------------------------------------
/*  set the timer on client to limit waiting only when based on [-n number-of-packets] parameter
    given by user and sampling data */
void set_client_expiration_timer(struct itimerval *timer, TicksTime testStart) {
    /*  Based off packet rate during sampling time, estimate total run time.
        Using warmup as our sample includes a bias for higher latency, because the head of
        the test has less load and receiver has no recent cache. This works to our benefit
        as we want an overestimate on waiting limit. */
    TicksTime endSampling = endSampling.setNowNonInline();
    TicksDuration sampleTime = endSampling - testStart;
    uint64_t warmupNumber = s_user_params.warmup_num;
    uint64_t cooldownNumber = s_user_params.cooldown_num;
    uint64_t numberTarget = g_pApp->m_const_params.number_test_target;
    uint64_t totalCount = warmupNumber + numberTarget + cooldownNumber;
    uint64_t sampleNumber = warmupNumber;

    // Calculate waiting limit                                          // (Units)
    double sampleTimeSeconds = sampleTime.toDecimalUsec() / 1000000;    // secs
    double sampleSecPacketRate = sampleTimeSeconds / sampleNumber;      // secs/packets
    double runTimeEstimate = sampleSecPacketRate * totalCount;          // (secs/packets)*packets = secs
    double waitingCap = 1.5 * runTimeEstimate; // Add leeway to process stats and write to file
    log_msg("RunTime Estimate=%.3lf sec, Time out in %.3lf sec", runTimeEstimate, waitingCap);

    // Build timer
    timer->it_value.tv_sec = static_cast<long>(waitingCap);
    timer->it_value.tv_usec = 0;
    timer->it_interval.tv_sec = 0;
    timer->it_interval.tv_usec = 0;
}

//------------------------------------------------------------------------------
uint32_t getStartOfRightOutlierBin(){
    const uint32_t lowerRange = s_user_params.histogram_lower_range;
    const uint32_t upperRange = s_user_params.histogram_upper_range;
    const uint32_t binSize = s_user_params.histogram_bin_size;
    uint32_t startBinEdge = upperRange;
    /*
        When range is not divisible by bin size, we can either truncate last bin within range to fit in
        or have the bin overflow to keep bins a constant size. We choose to overflow which shifts outlier
        bin.
    */
    uint32_t overflowRemainder = (upperRange - lowerRange) % binSize;

    if (overflowRemainder != 0) {
        startBinEdge += binSize - overflowRemainder;
    }
    return startBinEdge;
}

//------------------------------------------------------------------------------
uint32_t getLeftOutlierBinIndexReserved() {
    return 0;
}

//------------------------------------------------------------------------------
uint32_t getRightOutlierBinIndexReserved() {
    uint32_t lowerRange = s_user_params.histogram_lower_range;
    uint32_t upperRange = s_user_params.histogram_upper_range;
    uint32_t binSize = s_user_params.histogram_bin_size;
    /*
        Normal bin index for value is calculated with: 1 + (value - lower_range)/bin_size
        Right outliers all fall in the same bin which is one more than the last
        bin within range.
    */
    return 2 + (upperRange - lowerRange)/binSize;
}

//------------------------------------------------------------------------------
uint32_t getNumberOfDigits(uint32_t input) {
    return (int)log10(input) + 1;
}

//------------------------------------------------------------------------------
/*  Store frequencies for each non-empty bin. All bins include only start (exclusive) as
    end (inclusive) can be inferred by adding bin size. Outlier bins include both
    start and end of bin as size depends on outliers. */
void storeHistogram(uint32_t binSize, const std::map<uint32_t, uint32_t> &activeBins, uint32_t minValue, uint32_t maxValue) {
    const uint32_t leftOutlierBinIndex = getLeftOutlierBinIndexReserved();
    const uint32_t rightOutlierBinIndex = getRightOutlierBinIndexReserved();
    const uint32_t lowerRange = s_user_params.histogram_lower_range;
    const uint32_t upperRange = s_user_params.histogram_upper_range;
    uint32_t startBinEdge = 0;
    uint32_t frequency = 0;
    // Align columns with the following variables
    const uint32_t whitespaceAfterMaxBinMessage = 20;
    uint32_t upperRangeDigits = getNumberOfDigits(upperRange);
    uint32_t maxValueDigits = getNumberOfDigits(maxValue);
    uint32_t minValueDigits = getNumberOfDigits(minValue);
    uint32_t columnWidth = maxValueDigits + 1 + upperRangeDigits + whitespaceAfterMaxBinMessage;
    uint32_t columnWidthLeftOutlier = columnWidth - 1 - minValueDigits; // - 1 for "-"
    uint32_t columnWidthRightOutlier = columnWidth - 1 - upperRangeDigits; // - 1 for "-"

    FILE *f = g_pApp->m_const_params.fileFullLog;

    fprintf(f, "histogram was built using the following parameters: "
            "--h_bin_size_us=%" PRIu32 " --h_lower_range_us=%" PRIu32 " --h_upper_range_us=%" PRIu32 "\n",
            binSize, lowerRange, upperRange);
    fprintf(f, "------------------------------\n");
    fprintf(f, "bin (usec)            frequency\n");
    for(std::map<uint32_t, uint32_t>::const_iterator itr = activeBins.begin(); itr != activeBins.end(); ++itr) {
        frequency = itr->second;
        startBinEdge = (itr->first - 1) * binSize + lowerRange;
        if (itr->first == leftOutlierBinIndex) {
            fprintf(f, "%d-%-*d%d\n", minValue, columnWidthLeftOutlier, lowerRange, frequency);
        } else if (itr->first == rightOutlierBinIndex) {
            fprintf(f, "%d-%-*d%d\n", getStartOfRightOutlierBin(), columnWidthRightOutlier,
                    maxValue, frequency);
        } else {
            fprintf(f, "%-*d%d\n", columnWidth, startBinEdge, frequency);
        }
    }
    fprintf(f, "------------------------------\n");
}

//------------------------------------------------------------------------------
/*  Display histogram to fit on terminal screen width (frequency rounded up) */
void printHistogram(uint32_t binSize, std::map<uint32_t, uint32_t> &activeBins, uint32_t minValue, uint32_t maxValue) {
    const uint32_t leftOutlierBinIndex = getLeftOutlierBinIndexReserved();
    const uint32_t rightOutlierBinIndex = getRightOutlierBinIndexReserved();
    const uint32_t lowerRange = s_user_params.histogram_lower_range;
    const uint32_t upperRange = s_user_params.histogram_upper_range;
    const std::string prefixToHistogramDisplay ("sockperf: ");
    uint32_t maxFrequency = 0;
    uint32_t currFrequency = 0;
    uint32_t terminalWidth = 0;
    uint32_t scalingUnit = 0;
    uint32_t maxDisplayWidth = 0;
    uint32_t maxStartBinDigits = getNumberOfDigits(getStartOfRightOutlierBin());
    uint32_t maxEndBinDigits = getNumberOfDigits(maxValue);
    uint32_t whitespaceBeforeFrequencies = 2;
    uint32_t outlierMessageWidth = 11;
    uint32_t whitespaceAfterFrequencies = outlierMessageWidth + 1;
    uint32_t columnWidthOfEndBinEdge = maxEndBinDigits + whitespaceBeforeFrequencies;
    uint32_t columnWidthOfStartBinEdge = maxStartBinDigits;
    uint32_t maxBinMessage = getNumberOfDigits(upperRange + binSize) + 1 + maxEndBinDigits; // align after longest message
    std::map<uint32_t, uint32_t>::iterator itr;

    // Scale to terminal
#ifndef __windows__
    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    terminalWidth = size.ws_col;
#else
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),&csbi);
    terminalWidth = csbi.dwSize.X;
#endif
    for(itr = activeBins.begin(); itr != activeBins.end(); ++itr) {
        currFrequency = itr->second;
        if (currFrequency > maxFrequency) {
            maxFrequency = currFrequency;
        }
    }
    if (terminalWidth == 0) {
        // This may happen when run without terminal, but we still want the file output to contain the display
        uint32_t standardTerminalLength = 80;
        scalingUnit = (maxFrequency + standardTerminalLength - 1)/standardTerminalLength;
    } else {
        maxDisplayWidth = static_cast<uint32_t>(terminalWidth - prefixToHistogramDisplay.length() - maxBinMessage - whitespaceAfterFrequencies
            - whitespaceBeforeFrequencies);
        scalingUnit = (maxFrequency + maxDisplayWidth - 1)/maxDisplayWidth; // round up
    }

    const std::string freqString = "frequency";
    const std::string binsString = "bins";
    uint32_t binsHeaderWidth = static_cast<uint32_t>((maxStartBinDigits + maxEndBinDigits + 2)/2 + binsString.length()/2);
    uint32_t frequencyHeaderWidth = static_cast<uint32_t>((maxDisplayWidth)/2 + freqString.length()/2);
    uint32_t startBinEdge = 0;
    uint32_t endBinEdge = 0;
    uint32_t frequency = 0;
    uint32_t frequencyScaledDownCount = 0;

    if (scalingUnit == 1) {
        log_msg("[Histogram] Display to scale");
    } else {
        log_msg("[Histogram] Display scaled to fit on screen (Key: '#' = up to %d samples)", scalingUnit);
    }
    log_msg("%*s  %*s", binsHeaderWidth, binsString.c_str(), frequencyHeaderWidth, freqString.c_str());
    for(itr = activeBins.begin(); itr != activeBins.end(); ++itr) {
        frequency = itr->second;
        frequencyScaledDownCount = (frequency + scalingUnit - 1) / scalingUnit; // round up
        if (itr->first == leftOutlierBinIndex) {
            log_msg("%*d-%-*d" MAGNETA "%s (outliers)" ENDCOLOR, columnWidthOfStartBinEdge, minValue, columnWidthOfEndBinEdge,
                lowerRange, std::string(frequencyScaledDownCount, '#').c_str());
        } else if (itr->first == rightOutlierBinIndex) {
            log_msg("%*d-%-*d" MAGNETA "%s (outliers)" ENDCOLOR, columnWidthOfStartBinEdge, getStartOfRightOutlierBin(),
                columnWidthOfEndBinEdge, maxValue, std::string(frequencyScaledDownCount, '#').c_str());
        } else {
            startBinEdge = (itr->first - 1) * binSize + lowerRange;
            endBinEdge = startBinEdge + binSize;
            log_msg("%*d-%-*d%s", columnWidthOfStartBinEdge, startBinEdge, columnWidthOfEndBinEdge, endBinEdge,
                std::string(frequencyScaledDownCount, '#').c_str());
        }
    }
}

//------------------------------------------------------------------------------
/* Sparse fixed bin histogram with outlier bins outside given range */
void makeHistogram(TicksDuration *sortedpLat, size_t size) {
    const uint32_t leftOutlierBinIndex = getLeftOutlierBinIndexReserved();
    const uint32_t rightOutlierBinIndex = getRightOutlierBinIndexReserved();
    const uint32_t lowerRange = s_user_params.histogram_lower_range;
    const uint32_t upperRange = s_user_params.histogram_upper_range;
    const uint32_t binSize = s_user_params.histogram_bin_size;
    uint32_t binIndex = 0;
    uint32_t minValue = static_cast<uint32_t>(sortedpLat[0].toDecimalUsec());
    uint32_t maxValue = static_cast<uint32_t>(sortedpLat[size - 1].toDecimalUsec());
    double value = 0;
    std::map<uint32_t, uint32_t> activeBins;
    size_t i = 0;

    // build histogram
    for (; i < size; i++) {
        value = sortedpLat[i].toDecimalUsec();
        if (value < lowerRange) {
            activeBins[leftOutlierBinIndex]++;
            continue;
        }
        if (value >= upperRange) {
            activeBins[rightOutlierBinIndex]++;
            continue;
        }
        binIndex = static_cast<uint32_t>(1 + (value - lowerRange) / binSize);
        activeBins[binIndex]++;
    }

    printHistogram(binSize, activeBins, minValue, maxValue);
    if (g_pApp->m_const_params.fileFullLog) {
        storeHistogram(binSize, activeBins, minValue, maxValue);
    }
}

//------------------------------------------------------------------------------
void printPercentiles(FILE *f, TicksDuration *sortedpLat, size_t size) {
    const double percentile[] = { 0.99999, 0.9999, 0.999, 0.99, 0.90, 0.75, 0.50, 0.25 };
    int num = sizeof(percentile) / sizeof(percentile[0]);
    double observationsInPercentile = (double)size / 100;

    log_msg_file2(f, MAGNETA "Total %lu observations" ENDCOLOR
                             "; each percentile contains %.2lf observations",
                  (long unsigned)size, observationsInPercentile);

    log_msg_file2(f, "---> <MAX> observation = %8.3lf", sortedpLat[size - 1].toDecimalUsec());
    for (int i = 0; i < num; i++) {
        int index = (int)(0.5 + percentile[i] * size) - 1;
        if (index >= 0) {
            log_msg_file2(f, "---> percentile %6.3lf = %8.3lf", 100 * percentile[i],
                          sortedpLat[index].toDecimalUsec());
        }
    }
    log_msg_file2(f, "---> <MIN> observation = %8.3lf", sortedpLat[0].toDecimalUsec());
}

//------------------------------------------------------------------------------
typedef TicksTime RecordLog[2];

//------------------------------------------------------------------------------
void dumpFullLog(int serverNo, RecordLog *pFullLog, size_t size) {
    FILE *f = g_pApp->m_const_params.fileFullLog;
    uint32_t denominator = g_pApp->m_const_params.full_rtt ? 1 : 2;
    if (!f || !size) return;

    fprintf(f, "------------------------------\n");
    fprintf(f, "packet, txTime(sec), rxTime(sec), %s(usec)\n",
            round_trip_str[g_pApp->m_const_params.full_rtt]);
    for (size_t i = 0; i < size; i++) {
        double tx = (double)pFullLog[i][0].debugToNsec() / 1000 / 1000 / 1000;
        double rx = (double)pFullLog[i][1].debugToNsec() / 1000 / 1000 / 1000;
        double result = (rx - tx) * (USEC_PER_SEC / denominator);
        fprintf(f, "%zu, %.9lf, %.9lf, %.3lf\n", i, tx, rx, result);
    }
    fprintf(f, "------------------------------\n");
}

//------------------------------------------------------------------------------
double RationalApproximation(double t) {
    // Abramowitz and Stegun formula 26.2.23
    // with constants from here: https://arxiv.org/pdf/1002.0567.pdf, Section 3
    // Absolute value of error should be less than 8 e-5
    const double c[] = {2.653962002601684482, 1.561533700212080345, 0.061146735765196993};
    const double d[] = {1.904875182836498708, 0.454055536444233510, 0.009547745327068945};
    return t - ((c[2]*t + c[1])*t + c[0]) /
        (((d[2]*t + d[1])*t + d[0])*t + 1.0);
}

//------------------------------------------------------------------------------
double NormalCDFInverse(double p) {
    if (p < 0.0 || p > 1.0) {
        log_err("NormalCDFInverse only accepts 0 < p < 1");
        return 0;
    }

    // Approximation function is only valid for (0 < p < .5)
    // Extend to (0 < p < 1) by taking advantage of normal CDF inverse odd symmetry shape
    // Detailed explanation here: https://www.johndcook.com/blog/normal_cdf_inverse/#basic
    if (p < 0.5) {
        // F^-1(p) = - G^-1(p)
        return -RationalApproximation( sqrt(-2.0*log(p)) );
    } else {
        // F^-1(p) = G^-1(1-p)
        return RationalApproximation( sqrt(-2.0*log(1-p)) );
    }
}

//------------------------------------------------------------------------------
void client_statistics(int serverNo, Message *pMsgRequest) {
    const uint64_t receiveCount = g_receiveCount;
    const uint64_t sendCount = pMsgRequest->getSequenceCounter();
    const uint64_t replyEvery = g_pApp->m_const_params.reply_every;
    const size_t SIZE = receiveCount;
    const int SERVER_NO = serverNo;

    FILE *f = g_pApp->m_const_params.fileFullLog;

    if (!receiveCount) {
        log_msg_file2(f, "No messages were received from the server. Is the server down?");
        return;
    }

    /* Print total statistic that is independent on server count */
    if (SERVER_NO == 0) {
        TicksDuration totalRunTime = s_endTime - s_startTime;
        if (g_skipCount) {
            if (g_pApp->m_const_params.measurement == TIME_BASED) {
                log_msg_file2(f, "[Total Run] RunTime=%.3lf sec; Warm up time=%" PRIu32
                                " msec; SentMessages=%" PRIu64 "; ReceivedMessages=%" PRIu64
                                "; SkippedMessages=%" PRIu64 "",
                            totalRunTime.toDecimalUsec() / 1000000,
                            g_pApp->m_const_params.warmup_msec, sendCount, receiveCount, g_skipCount);
            } else {
                log_msg_file2(f, "[Total Run] RunTime=%.3lf sec; Warm up packets=%" PRIu64
                             "; SentMessages=%" PRIu64 "; ReceivedMessages=%" PRIu64
                             "; SkippedMessages=%" PRIu64 "",
                          totalRunTime.toDecimalUsec() / 1000000,
                          g_pApp->m_const_params.warmup_num, sendCount, receiveCount, g_skipCount);
            }
        } else {
            if (g_pApp->m_const_params.measurement == TIME_BASED) {
                log_msg_file2(f, "[Total Run] RunTime=%.3lf sec; Warm up time=%" PRIu32
                                " msec; SentMessages=%" PRIu64 "; ReceivedMessages=%" PRIu64 "",
                            totalRunTime.toDecimalUsec() / 1000000,
                            g_pApp->m_const_params.warmup_msec, sendCount, receiveCount);
            }
            else {
                log_msg_file2(f, "[Total Run] RunTime=%.3lf sec; Warm up packets=%" PRIu64
                                "; SentMessages=%" PRIu64 "; ReceivedMessages=%" PRIu64 "",
                            totalRunTime.toDecimalUsec() / 1000000,
                            g_pApp->m_const_params.warmup_num, sendCount, receiveCount);
            }
        }
    }

    /* Print server related statistic */
    log_msg_file2(f, "========= Printing statistics for Server No: %d", SERVER_NO);

    /*
     * There are few reasons to ignore warmup/cooldown packets:
     *
     * 1. At the head of the test the load is not real, since only few packets were sent so far.
     * 2. At the tail of the test the load is not real since the sender stopped sending; hence,
     *    the receiver accept packets without load
     * 3. The sender thread starts sending packets and generating load, before the receiver
     *    thread has started, and before its code was cached to memory/cpu.
     * 4. There are some packets that were sent close to s_end time; the legitimate replies to
     *    them will arrive after s_end time and may be lost.  Hence, your fix may cause us to
     *    report on those packets as dropped packets.
     */

    TicksTime testStart = g_pPacketTimes->getTxTime(replyEvery); // first pong request packet
    TicksTime testEnd =
        g_pPacketTimes->getTxTime(sendCount); // will be "truncated" to last pong request packet

    if (!g_pApp->m_const_params.pPlaybackVector) { // no warmup in playback mode
        if (g_pApp->m_const_params.measurement == TIME_BASED) {
            testStart += TicksDuration::TICKS1MSEC * TEST_START_WARMUP_MSEC;
            testEnd -= TicksDuration::TICKS1MSEC * TEST_END_COOLDOWN_MSEC;
        }
    }
    log_dbg("testStart: %.9lf sec testEnd: %.9lf sec",
            (double)testStart.debugToNsec() / 1000 / 1000 / 1000,
            (double)testEnd.debugToNsec() / 1000 / 1000 / 1000);
    if (testEnd < testStart) {
        log_msg_file2(f, "Test end before test start. Ending statistics early");
        return;
    }

    TicksDuration *pLat = new TicksDuration[SIZE];
    RecordLog *pFullLog = g_pApp->m_const_params.fileFullLog ? new RecordLog[SIZE] : NULL;

    TicksDuration rtt;
    TicksDuration sumRtt(0);
    size_t counter = 0;
    TicksTime prevRxTime;
    TicksTime startValidTime;
    TicksTime endValidTime;
    uint32_t denominator = g_pApp->m_const_params.full_rtt ? 1 : 2;
    uint64_t startValidSeqNo = 0;
    uint64_t endValidSeqNo = 0;
    uint64_t startSearchHere = 1;
    uint64_t endNumberSearchHere = -1;

    if (g_pApp->m_const_params.measurement == NUMBER_BASED) {
        startSearchHere += g_pApp->m_const_params.warmup_num;
        endNumberSearchHere = startSearchHere + g_pApp->m_const_params.number_test_target;
    }

    for (uint64_t i = startSearchHere; (counter < SIZE); i++) {
        uint64_t seqNo = i * replyEvery;
        const TicksTime &txTime = g_pPacketTimes->getTxTime(seqNo);
        const TicksTime &rxTime = g_pPacketTimes->getRxTimeArray(seqNo)[SERVER_NO];

        if ((txTime > testEnd) || (txTime == TicksTime::TICKS0)) {
            break;
        }

        if (g_pApp->m_const_params.measurement == NUMBER_BASED && seqNo >= endNumberSearchHere) {
            break;
        }

        if (txTime < testStart) {
            continue;
        }

        if (startValidSeqNo == 0) {
            startValidSeqNo = seqNo;
            startValidTime = txTime;
        }

        if (rxTime == TicksTime::TICKS0) {
            g_pPacketTimes->incDroppedCount(SERVER_NO);
            if (endValidTime < txTime) {
                endValidSeqNo = seqNo;
                endValidTime = txTime;
            }
            continue;
        }

        if (rxTime < prevRxTime) {
            g_pPacketTimes->incOooCount(SERVER_NO);
            continue;
        }

        if (g_pApp->m_const_params.fileFullLog) {
            pFullLog[counter][0] = txTime;
            pFullLog[counter][1] = rxTime;
        }

        endValidSeqNo = seqNo;
        endValidTime = rxTime;

        rtt = rxTime - txTime;

        sumRtt += rtt;
        pLat[counter] = rtt / denominator;

        prevRxTime = rxTime;
        counter++;
    }

    if (!counter) {
        log_msg_file2(
            f, "No valid observations found. Try tune parameters: "
               "--time/--number-of-packets/--mps/--reply-every");
    } else {
        TicksDuration validRunTime = endValidTime - startValidTime;
        log_msg_file2(f, "[Valid Duration] RunTime=%.3lf sec; SentMessages=%" PRIu64
                         "; ReceivedMessages=%" PRIu64 "",
                      validRunTime.toDecimalUsec() / 1000000, (endValidSeqNo - startValidSeqNo + 1),
                      (uint64_t)counter);

        TicksDuration::sort(pLat, counter);
        TicksDuration *sortedpLat = &pLat[0]; // alias for pLat after being sorted
        TicksDuration avgRtt = counter ? sumRtt / (int)counter : TicksDuration::TICKS0;
        TicksDuration avgLatency = avgRtt / 2;
        TicksDuration stdDev = TicksDuration::stdDev(pLat, counter);
        TicksDuration mad = TicksDuration::mad(pLat, counter);
        TicksDuration medianad = TicksDuration::medianad(pLat, counter);
        TicksDuration siqr = TicksDuration::siqr(pLat, counter);
        double usecAvarage = g_pApp->m_const_params.full_rtt ? avgRtt.toDecimalUsec() : avgLatency.toDecimalUsec();
        double coefficientOfVariance = stdDev.toDecimalUsec() / usecAvarage;
        double standardError = stdDev.toDecimalUsec() / sqrt(counter);
        double significanceLevel = s_user_params.ci_significance_level;
        double zScore = 1 - (1 - significanceLevel/100) / 2;
        double confidenceLevelValue = NormalCDFInverse(zScore);
        double lowerInterval = usecAvarage - confidenceLevelValue * standardError;
        double upperInterval = usecAvarage + confidenceLevelValue * standardError;
        log_msg_file2(f, MAGNETA "====> avg-%s=%.3lf (std-dev=%.3lf, mean-ad=%.3lf, median-ad=%.3lf, siqr=%.3lf, "
            "cv=%.3lf, std-error=%.3lf, %.1lf%% ci=[%.3lf, %.3lf])" ENDCOLOR,
            round_trip_str[g_pApp->m_const_params.full_rtt], usecAvarage,
            stdDev.toDecimalUsec(), mad.toDecimalUsec(), medianad.toDecimalUsec(), siqr.toDecimalUsec(),
            coefficientOfVariance, standardError, significanceLevel, lowerInterval, upperInterval);

        /* Display ERROR statistic*/

        bool isColor =
            (g_pPacketTimes->getDroppedCount(SERVER_NO) || g_pPacketTimes->getDupCount(SERVER_NO) ||
             g_pPacketTimes->getOooCount(SERVER_NO));
        const char *colorRedStr = isColor ? RED : "";
        const char *colorResetStr = isColor ? ENDCOLOR : "";
        log_msg_file2(f, "%s# dropped messages = %lu; # duplicated messages = %lu; # out-of-order "
                         "messages = %lu%s",
                      colorRedStr, (long unsigned)g_pPacketTimes->getDroppedCount(SERVER_NO),
                      (long unsigned)g_pPacketTimes->getDupCount(SERVER_NO),
                      (long unsigned)g_pPacketTimes->getOooCount(SERVER_NO), colorResetStr);

        if (usecAvarage) print_average_results(usecAvarage);

        printPercentiles(f, sortedpLat, counter);

        dumpFullLog(SERVER_NO, pFullLog, counter);

        if (s_user_params.b_histogram) makeHistogram(sortedpLat, counter);
    }

    delete[] pLat;
    delete[] pFullLog;
}

//------------------------------------------------------------------------------
void stream_statistics(Message *pMsgRequest) {
    TicksDuration totalRunTime = s_endTime - s_startTime;

    if (totalRunTime <= TicksDuration::TICKS0) return;
    if (!g_pApp->m_const_params.b_stream) return;

    const uint64_t sendCount = pMsgRequest->getSequenceCounter();

    // Send only mode!
    if (g_skipCount) {
        log_msg("Total of %" PRIu64 " messages sent in %.3lf sec (%" PRIu64 " messages skipped)\n",
                sendCount, totalRunTime.toDecimalUsec() / 1000000, g_skipCount);
    } else {
        log_msg("Total of %" PRIu64 " messages sent in %.3lf sec\n", sendCount,
                totalRunTime.toDecimalUsec() / 1000000);
    }
    if (g_pApp->m_const_params.mps != MPS_MAX) {
        if (g_pApp->m_const_params.msg_size_range)
            log_msg(MAGNETA "NOTE: test was performed, using average msg-size=%d (+/-%d), mps=%u. "
                            "For getting maximum throughput use --mps=max (and consider "
                            "--msg-size=1472 or --msg-size=4096" ENDCOLOR,
                    g_pApp->m_const_params.msg_size, g_pApp->m_const_params.msg_size_range,
                    g_pApp->m_const_params.mps);
        else
            log_msg(MAGNETA "NOTE: test was performed, using msg-size=%d, mps=%u. For getting "
                            "maximum throughput use --mps=max (and consider --msg-size=1472 or "
                            "--msg-size=4096)" ENDCOLOR,
                    g_pApp->m_const_params.msg_size, g_pApp->m_const_params.mps);
    } else if (g_pApp->m_const_params.msg_size != 1472) {
        if (g_pApp->m_const_params.msg_size_range)
            log_msg(MAGNETA "NOTE: test was performed, using average msg-size=%d (+/-%d). For "
                            "getting maximum throughput consider using --msg-size=1472" ENDCOLOR,
                    g_pApp->m_const_params.msg_size, g_pApp->m_const_params.msg_size_range);
        else
            log_msg(MAGNETA "NOTE: test was performed, using msg-size=%d. For getting maximum "
                            "throughput consider using --msg-size=1472" ENDCOLOR,
                    g_pApp->m_const_params.msg_size);
    }

    int ip_frags_per_msg =
        (g_pApp->m_const_params.msg_size + DEFAULT_IP_PAYLOAD_SZ - 1) / DEFAULT_IP_PAYLOAD_SZ;
    int msgps = (int)(0.5 + ((double)sendCount) * 1000 * 1000 / totalRunTime.toDecimalUsec());

    int pktps = msgps * ip_frags_per_msg;
    int total_line_ip_data = g_pApp->m_const_params.msg_size;
    double MBps = ((double)msgps * total_line_ip_data) / 1024 /
                  1024; /* No including IP + UDP Headers per fragment */
    if (ip_frags_per_msg == 1)
        log_msg("Summary: Message Rate is %d [msg/sec]", msgps);
    else
        log_msg("Summary: Message Rate is %d [msg/sec], Packet Rate is about %d [pkt/sec] (%d ip "
                "frags / msg)",
                msgps, pktps, ip_frags_per_msg);
    if (g_pApp->m_const_params.giga_size) {
        log_msg("Summary: BandWidth is %.3f GBps (%.3f Gbps)", MBps / 1000, MBps * 8 / 1000);
    } else if (g_pApp->m_const_params.increase_output_precision) {
        log_msg("Summary: BandWidth is %.9f MBps (%.9f Mbps)", MBps, MBps * 8);
    } else {
        log_msg("Summary: BandWidth is %.3f MBps (%.3f Mbps)", MBps, MBps * 8);
    }
}

//------------------------------------------------------------------------------
void client_sig_handler(int signum) {
    if (g_b_exit) {
        log_msg("Test end (interrupted by signal %d)", signum);
        return;
    }
    s_endTime.setNowNonInline();
    g_b_exit = true;

    // Just in case not Activity updates where logged add a '\n'
    if (g_pApp->m_const_params.packetrate_stats_print_ratio &&
        !g_pApp->m_const_params.packetrate_stats_print_details)
        printf("\n");

    switch (signum) {
    case SIGALRM:
        if (g_pApp->m_const_params.measurement == TIME_BASED) {
            log_msg("Test end (interrupted by timer)");
        } else {
            exit_with_log("Test ends uncompleted, number of packets requested taking "
                          "too long (interrupted by timer)", SOCKPERF_ERR_TIMEOUT);
        }
        break;
    case SIGINT:
        log_msg("Test end (interrupted by user)");
        break;
    default:
        log_msg("Test end (interrupted by signal %d)", signum);
        break;
    }
}

//==============================================================================
//==============================================================================

//------------------------------------------------------------------------------
ClientBase::ClientBase() {
    m_pMsgReply = new Message();
    m_pMsgReply->setLength(MAX_PAYLOAD_SIZE);

    m_pMsgRequest = new Message();
    m_pMsgRequest->getHeader()->setClient();
    m_pMsgRequest->setLength(g_pApp->m_const_params.msg_size);
}

//------------------------------------------------------------------------------
ClientBase::~ClientBase() {
    delete m_pMsgReply;
    delete m_pMsgRequest;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
       PongModeCare>::Client(int _fd_min, int _fd_max, int _fd_num)
    : ClientBase(), m_ioHandler(_fd_min, _fd_max, _fd_num), m_pongModeCare(m_pMsgRequest) {
    os_thread_init(&m_receiverTid);
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
       PongModeCare>::~Client() {}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
            PongModeCare>::client_receiver_thread() {
    while (!g_b_exit) {
        client_receive();
    }
}

//------------------------------------------------------------------------------
void *client_receiver_thread(void *arg) {
    ClientBase *_this = (ClientBase *)arg;
    _this->client_receiver_thread();
    return 0;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
            PongModeCare>::cleanupAfterLoop() {
    usleep(100 * 1000); // 0.1 sec - wait for rx packets for last sends (in normal flow)
    if (m_receiverTid.tid) {
        os_thread_kill(&m_receiverTid);
        // os_thread_join(&m_receiverTid);
        os_thread_detach(&m_receiverTid); // just for silenting valgrind's "possibly lost: 288
                                          // bytes" in pthread_create
        os_thread_close(&m_receiverTid);
    }

    if (g_b_errorOccured)
        return; // cleanup started in other thread and triggerd termination of this thread

    log_msg("Test ended");

    if (!m_pMsgRequest->getSequenceCounter()) {
        log_msg("No messages were sent");
    } else if (g_pApp->m_const_params.b_stream) {
        stream_statistics(m_pMsgRequest);
    } else {
        FILE *f = g_pApp->m_const_params.fileFullLog;
        if (f) {
            fprintf(f, "------------------------------\n");
            if (g_pApp->m_const_params.measurement == TIME_BASED) {
                fprintf(f, "test was performed using the following parameters: "
                        "--mps=%d --burst=%d --reply-every=%d --msg-size=%d --time=%d",
                        (int)g_pApp->m_const_params.mps, (int)g_pApp->m_const_params.burst_size,
                        (int)g_pApp->m_const_params.reply_every, (int)g_pApp->m_const_params.msg_size,
                        (int)g_pApp->m_const_params.sec_test_duration);
            } else {
            fprintf(f, "test was performed using the following parameters: "
                       "--burst=%d --msg-size=%d --number-of-packets=%" PRIu64 "",
                        (int)g_pApp->m_const_params.burst_size,
                        (int)g_pApp->m_const_params.msg_size, g_pApp->m_const_params.number_test_target);
            }
            if (g_pApp->m_const_params.dummy_mps) {
                fprintf(f, " --dummy-send=%d", g_pApp->m_const_params.dummy_mps);
            }
            if (g_pApp->m_const_params.full_rtt) {
                fprintf(f, " --full-rtt");
            }
            fprintf(f, "\n");

            fprintf(f, "------------------------------\n");
        }

        for (int i = 0; i < g_pApp->m_const_params.client_work_with_srv_num; i++) {
            client_statistics(i, m_pMsgRequest);
        }
    }

    if (g_pApp->m_const_params.fileFullLog) fclose(g_pApp->m_const_params.fileFullLog);

    if (g_pApp->m_const_params.cycleDuration > TicksDuration::TICKS0 && !g_cycle_wait_loop_counter)
        log_msg("Info: The requested message-per-second rate is too high. Try tuning --mps or "
                "--burst arguments");
}

//------------------------------------------------------------------------------
#ifdef USING_VMA_EXTRA_API // For VMA socketxtreme Only
static int _connect_check_vma(int ifd) {
    int rc = SOCKPERF_ERR_SOCKET;
    int ring_fd = 0;
    int poll = 0;
    rc = g_vma_api->get_socket_rings_fds(ifd, &ring_fd, 1);
    if (rc == -1) {
        rc = SOCKPERF_ERR_SOCKET;
        return rc;
    }
    while (!g_b_exit && poll == 0) {
        struct vma_completion_t vma_comps;
        poll = g_vma_api->socketxtreme_poll(ring_fd, &vma_comps, 1, 0);
        if (poll > 0) {
            if (vma_comps.events & EPOLLOUT) {
                rc = SOCKPERF_ERR_NONE;
            }
        }
    }
    return rc;
}
#endif // USING_VMA_EXTRA_API

//------------------------------------------------------------------------------
static int _connect_check(int ifd) {
    int rc = SOCKPERF_ERR_NONE;
    int pollrc;
    bool avail;

#define POLL_TIMEOUT_MS 1

#ifdef __windows__
    fd_set rfds, wfds;
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = POLL_TIMEOUT_MS * 1000;

    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_SET(ifd, &wfds);
    FD_SET(ifd, &rfds);

    pollrc = select(ifd + 1, &rfds, &wfds, NULL, &tv);
    avail = pollrc > 0 && (FD_ISSET(ifd, &wfds) || FD_ISSET(ifd, &rfds));
#else
    struct pollfd fds = { .fd = ifd, .events = POLLIN | POLLOUT, };
    pollrc = poll(&fds, 1, POLL_TIMEOUT_MS);
    avail = pollrc > 0 && (fds.revents & (POLLIN | POLLOUT));
#endif /* __windows__ */

    if (pollrc < 0) {
        log_err("Failed to poll for events during connection establishment");
    }
    if (avail) {
        socklen_t err_len;
        int error;

        err_len = sizeof(error);
        if (getsockopt(ifd, SOL_SOCKET, SO_ERROR, &error, &err_len) < 0 || error != 0) {
            log_err("Can`t connect socket");
            rc = SOCKPERF_ERR_SOCKET;
        }
    }
    return rc;
}

#undef POLL_TIMEOUT_MS

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
int Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
           PongModeCare>::initBeforeLoop() {
    int rc = SOCKPERF_ERR_NONE;
    if (g_b_exit) return rc;

    /* bind/connect socket */
    if (rc == SOCKPERF_ERR_NONE) {
        // cycle through all set fds in the array (with wrap around to beginning)
        for (int ifd = m_ioHandler.m_fd_min; ifd <= m_ioHandler.m_fd_max; ifd++) {
            fds_data *data = g_fds_array[ifd];

            if (!(data && (data->active_fd_list))) continue;

            const sockaddr_store_t *p_client_bind_addr = &g_pApp->m_const_params.client_bind_info;
            if (p_client_bind_addr->ss_family != AF_UNSPEC) {
                socklen_t client_bind_addr_len = g_pApp->m_const_params.client_bind_info_len;
                std::string hostport = sockaddr_to_hostport(p_client_bind_addr);
#ifdef __linux__
                struct sockaddr_store_t unix_addr;
                socklen_t unix_addr_len;
#endif
                if (p_client_bind_addr->ss_family == AF_UNIX && g_pApp->m_const_params.sock_type == SOCK_DGRAM) { // Need to bind localy
#ifdef __windows__
                    log_err("AF_UNIX with DGRAM isn't supported in windows");
                    rc = SOCKPERF_ERR_SOCKET;
                    break;
#else // __windows__
                    if (p_client_bind_addr->addr_un.sun_path[0] == 0) { // no specific addr client_info was provoided
                        std::string sun_path = build_client_socket_name(&s_user_params.addr.addr_un, getpid(), ifd);
                        log_dbg("No client name was provided, setting addr_un.sun_path to %s\n",
                               sun_path.c_str());
                        if (sun_path.length() >= sizeof(unix_addr.addr_un.sun_path)) {
                            log_err("length of client socket name (%s) is greater-equal %zu bytes", sun_path.c_str(), sizeof(unix_addr.addr_un.sun_path));
                            rc = SOCKPERF_ERR_SOCKET;
                        } else {
                            memset(unix_addr.addr_un.sun_path, 0, sizeof(unix_addr.addr_un.sun_path));
                            memcpy(unix_addr.addr_un.sun_path, sun_path.c_str(), sun_path.length());
                            unix_addr_len = sizeof(struct sockaddr_un);
                            unix_addr.ss_family = AF_UNIX;
                            hostport = sun_path;
                        }
                        p_client_bind_addr = &unix_addr;
                        client_bind_addr_len = unix_addr_len;
                    }
#endif //__windows__
                }

                log_dbg("[fd=%d] Binding to: %s...", ifd, hostport.c_str());
                if (bind(ifd, reinterpret_cast<const sockaddr *>(p_client_bind_addr), client_bind_addr_len) < 0) {
                    log_err("[fd=%d] Can`t bind socket %s", ifd, hostport.c_str());
                    rc = SOCKPERF_ERR_SOCKET;
                    break;
                }
            }
            if (data->sock_type == SOCK_STREAM) {
                std::string hostport = sockaddr_to_hostport(data->server_addr);
                log_dbg("[fd=%d] Connecting to: %s...", ifd, hostport.c_str());

                if (connect(ifd, reinterpret_cast<const sockaddr *>(&(data->server_addr)),
                            data->server_addr_len) < 0) {
                    if (os_err_in_progress()) {
#ifdef USING_VMA_EXTRA_API // For VMA socketxtreme Only
                        if (g_pApp->m_const_params.fd_handler_type == SOCKETXTREME && g_vma_api) {
                            rc = _connect_check_vma(ifd);
                        } else
#endif // USING_VMA_EXTRA_API
                        {
                            rc = _connect_check(ifd);
                        }
                        if (rc == SOCKPERF_ERR_SOCKET) {
                            break;
                        }

                    } else {
                        log_err("Can`t connect socket");
                        rc = SOCKPERF_ERR_SOCKET;
                        break;
                    }
                }
#if defined(DEFINED_TLS)
                if (g_pApp->m_const_params.tls) {
                    data->tls_handle = tls_establish(ifd);
                    if (!data->tls_handle) {
                        rc = SOCKPERF_ERR_SOCKET;
                        break;
                    }
                }
#endif /* DEFINED_TLS */
            }
            /*
             * since when using VMA there is no qp until the bind, and vma cannot
             * check that rate-limit is supported this is done here and not
             * with the rest of the setsockopt
             */
            if (s_user_params.rate_limit > 0 &&
                    sock_set_rate_limit(ifd, s_user_params.rate_limit)) {
                std::string hostport = sockaddr_to_hostport(data->server_addr);
                log_err("[fd=%d] failed setting rate limit on address %s\n", ifd,
                        hostport.c_str());
                rc = SOCKPERF_ERR_SOCKET;
                break;
            }
        }
    }

    if (g_b_exit) return rc;

    if (rc == SOCKPERF_ERR_NONE) {

        printf(MODULE_NAME "[CLIENT] send on:");

        if (!g_pApp->m_const_params.b_stream) {
            log_msg("using %s() to block on socket(s)",
                    handler2str(g_pApp->m_const_params.fd_handler_type));
        }

        rc = m_ioHandler.prepareNetwork();
        if (rc == SOCKPERF_ERR_NONE) {
            sleep(g_pApp->m_const_params.pre_warmup_wait);

            m_ioHandler.warmup(m_pMsgRequest);

            sleep(2);

            if (g_b_exit) return rc;

            rc = set_affinity_list(os_getthread(), g_pApp->m_const_params.sender_affinity);
            if (rc == SOCKPERF_ERR_NONE) {
                if (!g_pApp->m_const_params.b_client_ping_pong &&
                    !g_pApp->m_const_params.b_stream) { // latency_under_load
                    if (0 != os_thread_exec(&m_receiverTid, ::client_receiver_thread, this)) {
                        log_err("Creating thread has failed");
                        rc = SOCKPERF_ERR_FATAL;
                    } else {
                        rc = set_affinity_list(m_receiverTid,
                                               g_pApp->m_const_params.receiver_affinity);
                    }
                }

                if (rc == SOCKPERF_ERR_NONE) {
                    log_msg("Starting test...");

                    if (!g_pApp->m_const_params.pPlaybackVector) {
                        struct itimerval timer;
                        if (g_pApp->m_const_params.measurement == TIME_BASED) {
                            set_client_timer(&timer);
                            if (os_set_duration_timer(timer, client_sig_handler)) {
                                log_err("Failed setting test duration timer");
                                rc = SOCKPERF_ERR_FATAL;
                            }
                        }
                    }

                    if (rc == SOCKPERF_ERR_NONE) {
                        s_startTime.setNowNonInline();
                        g_lastTicks = s_startTime;
                        g_cycleStartTime = s_startTime - g_pApp->m_const_params.cycleDuration;
                    }
                }
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
            PongModeCare>::doSendThenReceiveLoop() {
    if (g_pApp->m_const_params.measurement == TIME_BASED) {
        // cycle through all set fds in the array (with wrap around to beginning)
        for (int curr_fds = m_ioHandler.m_fd_min; !g_b_exit; curr_fds = g_fds_array[curr_fds]->next_fd)
            client_send_then_receive(curr_fds);
    } else {
        uint64_t seqNo = 0;
        uint64_t counterValid = 0;
        uint64_t warmupNumber = s_user_params.warmup_num;
        uint64_t cooldownNumber = s_user_params.cooldown_num;
        uint64_t numberTarget = g_pApp->m_const_params.number_test_target;
        uint64_t stopCounting = warmupNumber + numberTarget + cooldownNumber;
        const int SERVER_NO = 0; // TODO: should be one per server (as of 1/27/21 sockperf handles only one server)
        TicksTime testStart;
        TicksTime prevRxTime;
        struct itimerval timer = {};

        // cycle through all set fds in the array (with wrap around to beginning)
        for (int curr_fds = m_ioHandler.m_fd_min; !g_b_exit; curr_fds = g_fds_array[curr_fds]->next_fd) {
            client_send_then_receive(curr_fds);

            // Packet not recorded, nothing to validate
            if (g_receiveCount == seqNo) {
                continue;
            }

            if (seqNo == 0) {
                testStart = g_pPacketTimes->getTxTime(1); // first pong request packet
            }

            seqNo+= 1;

            // Validate packet observation
            const TicksTime &txTime = g_pPacketTimes->getTxTime(seqNo);
            const TicksTime &rxTime = g_pPacketTimes->getRxTimeArray(seqNo)[SERVER_NO];
            if (txTime == TicksTime::TICKS0 || txTime < testStart ||
                rxTime == TicksTime::TICKS0 || rxTime < prevRxTime) {
                continue;
            }
            prevRxTime = rxTime;
            counterValid++;

            // Set timer to limit wait on number of packets based off warmup sample
            if (counterValid == warmupNumber && timer.it_value.tv_sec == 0) {
                set_client_expiration_timer(&timer, testStart);
                if (os_set_duration_timer(timer, client_sig_handler)) {
                    exit_with_log("Failed setting test expiration timer", SOCKPERF_ERR_FATAL);
                }
            }

            if (counterValid == stopCounting) {
                s_endTime.setNowNonInline();
                // Disarm timer
                timer.it_value.tv_sec = 0;
                timer.it_value.tv_usec = 0;
                os_set_disarm_timer(timer);
                log_msg("Test end (reached packet number)");
                g_b_exit = true;
            }
        }
    }
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
            PongModeCare>::doSendLoop() {
    // cycle through all set fds in the array (with wrap around to beginning)
    for (int curr_fds = m_ioHandler.m_fd_min; !g_b_exit; curr_fds = g_fds_array[curr_fds]->next_fd)
        client_send_burst(curr_fds);
}

//------------------------------------------------------------------------------
static inline void playbackCycleDurationWait(const TicksDuration &i_cycleDuration) {
    static TicksTime s_cycleStartTime = TicksTime().setNowNonInline(); // will only be executed once

    TicksTime nextCycleStartTime = s_cycleStartTime + i_cycleDuration;
    while (!g_b_exit) {
        if (TicksTime::now() >= nextCycleStartTime) {
            break;
        }
    }
    s_cycleStartTime = nextCycleStartTime;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
            PongModeCare>::doPlayback() {
    usleep(100 * 1000); // wait for receiver thread to start (since we don't use warmup) //TODO:
                        // configure!
    s_startTime.setNowNonInline(); // reduce code size by calling non inline func from slow path
    const PlaybackVector &pv = *g_pApp->m_const_params.pPlaybackVector;

    size_t i = 0;
    const size_t size = pv.size();

    // cycle through all set fds in the array (with wrap around to beginning)
    for (int ifd = m_ioHandler.m_fd_min; i < size && !g_b_exit;
         ifd = g_fds_array[ifd]->next_fd, ++i) {

        m_pMsgRequest->setLength(pv[i].size);

        // idle
        playbackCycleDurationWait(pv[i].duration);

        // send
        client_send_packet(ifd);

        m_switchActivityInfo.execute(m_pMsgRequest->getSequenceCounter());
    }
    g_cycle_wait_loop_counter++; // for silenting waring at the end
    s_endTime.setNowNonInline(); // reduce code size by calling non inline func from slow path
    usleep(20 * 1000);           // wait for reply of last packet //TODO: configure!
    g_b_exit = true;
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
void Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
            PongModeCare>::doHandler() {
    int rc = SOCKPERF_ERR_NONE;

    rc = initBeforeLoop();

    if (rc == SOCKPERF_ERR_NONE) {
        if (g_pApp->m_const_params.pPlaybackVector)
            doPlayback();
        else if (g_pApp->m_const_params.b_client_ping_pong)
            doSendThenReceiveLoop();
        else
            doSendLoop();

        cleanupAfterLoop();
    }
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize, class PongModeCare>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
    Client<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration, SwitchMsgSize,
           PongModeCare> c(_fd_min, _fd_max, _fd_num);
    c.doHandler();
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration, class SwitchMsgSize>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
    if (g_pApp->m_const_params.b_stream)
        client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration,
                       SwitchMsgSize, PongModeNever>(_fd_min, _fd_max, _fd_num);
    else if (g_pApp->m_const_params.reply_every == 1)
        client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration,
                       SwitchMsgSize, PongModeAlways>(_fd_min, _fd_max, _fd_num);
    else
        client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration,
                       SwitchMsgSize, PongModeNormal>(_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo,
          class SwitchCycleDuration>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
    if (g_pApp->m_const_params.msg_size_range > 0)
        client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration,
                       SwitchOnMsgSize>(_fd_min, _fd_max, _fd_num);
    else
        client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchCycleDuration,
                       SwitchOff>(_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity, class SwitchActivityInfo>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
    if (g_pApp->m_const_params.cycleDuration > TicksDuration::TICKS0) {
        if (g_pApp->m_const_params.dummy_mps) {
            client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchOnDummySend>(
                _fd_min, _fd_max, _fd_num);
        } else {
            client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchOnCycleDuration>(
                _fd_min, _fd_max, _fd_num);
        }
    } else
        client_handler<IoType, SwitchDataIntegrity, SwitchActivityInfo, SwitchOff>(_fd_min, _fd_max,
                                                                                   _fd_num);
}

//------------------------------------------------------------------------------
template <class IoType, class SwitchDataIntegrity>
void client_handler(int _fd_min, int _fd_max, int _fd_num) {
    if (g_pApp->m_const_params.packetrate_stats_print_ratio > 0)
        client_handler<IoType, SwitchDataIntegrity, SwitchOnActivityInfo>(_fd_min, _fd_max,
                                                                          _fd_num);
    else
        client_handler<IoType, SwitchDataIntegrity, SwitchOff>(_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
template <class IoType> void client_handler(int _fd_min, int _fd_max, int _fd_num) {
    if (g_pApp->m_const_params.data_integrity)
        client_handler<IoType, SwitchOnDataIntegrity>(_fd_min, _fd_max, _fd_num);
    else
        client_handler<IoType, SwitchOff>(_fd_min, _fd_max, _fd_num);
}

//------------------------------------------------------------------------------
void client_handler(handler_info *p_info) {
    if (p_info) {
        switch (g_pApp->m_const_params.fd_handler_type) {
        case SELECT: {
            client_handler<IoSelect>(p_info->fd_min, p_info->fd_max, p_info->fd_num);
            break;
        }
        case RECVFROM: {
            client_handler<IoRecvfrom>(p_info->fd_min, p_info->fd_max, p_info->fd_num);
            break;
        }
        case RECVFROMMUX: {
            client_handler<IoRecvfromMUX>(p_info->fd_min, p_info->fd_max, p_info->fd_num);
            break;
        }
#ifndef __windows__
        case POLL: {
            client_handler<IoPoll>(p_info->fd_min, p_info->fd_max, p_info->fd_num);
            break;
        }
#ifndef __FreeBSD__
        case EPOLL: {
            client_handler<IoEpoll>(p_info->fd_min, p_info->fd_max, p_info->fd_num);
            break;
        }
#endif // !__FreeBSD__
#ifdef USING_VMA_EXTRA_API // For VMA socketxtreme Only
        case SOCKETXTREME: {
            client_handler<IoSocketxtreme>(p_info->fd_min, p_info->fd_max, p_info->fd_num);
            break;
        }
#endif // USING_VMA_EXTRA_API
#endif // !WIN32
        default: {
            ERROR_MSG("unknown file handler");
            break;
        }
        }
    }
}
