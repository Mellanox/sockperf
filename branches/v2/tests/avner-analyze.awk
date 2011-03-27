#!/bin/awk -f

#
# This script will nicely analyze output file that was created using ./avner-test or ./avner-test2
#

/^[ \t]*#/ {print; next} # echo and skip comment line
NF !=3 || $3+0 <= 0 {next} # Skip empty or text lines
{   # handle all other lines
	nsec = 1000 * $3
	sum[$1] += nsec
	num[$1] ++
	sum_sqr[$1] += nsec * nsec
}

END {
	for (test in num) {
		avg[test] = sum[test] / num[test]
		std = (num[test] > 1) ? sqrt ( (sum_sqr[test] - num[test]*avg[test]*avg[test]) / (num[test] - 1) ) : 0
		printf "%s: %12s: avg=%.3fusec (std=%.3f, #Observations=%d)\n", FILENAME, test, avg[test]/1000, std/1000, num[test]
	}

        for (test in num) {
                if (prev) {
                        printf "====> %s: delta_avg (%s_avg - %s_avg) = %.3fusec\n", FILENAME, prev, test, (avg[prev]-avg[test])/1000
                }
		else {
			printf "====> \n"
		}
                prev = test
        }
}
