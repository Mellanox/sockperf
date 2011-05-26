##
# @file UPP.pm
#
# @brief Test Suite UDP ping-pong.
#
#

## @class
# Container for common data.
package UPP;

use strict;
use warnings;


# Own modules
use TE::Common;
use TE::Funclet;
use TE::Utility;


our $test_suite_udp_pp = 
    [
	    {
	        name => 'tc1',
            note => '#1 - ping-pong w/o arguments',
            pre_proc => \&te_def_pre_proc,
	        server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET()',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
            	client => {
	            	success => ['Test ended', 'Summary: Latency is', 'Warmup stage \(sending a few dummy packets\)...'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
            	},
            },
            post_proc => \&te_def_post_proc,
	    },
        {
            name => 'tc2',
            note => '#2 - ping-pong option --dontwarmup',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --dontwarmup',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down', 'Warmup stage \(sending a few dummy packets\)...'],
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc3',
            note => '#3 - ping-pong option -b10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -b10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency of burst of 10 packets'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc4',
            note => '#4 - ping-pong option -b100',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -b100',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency of burst of 100 packets'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc5',
            note => '#5 - ping-pong option -b1000',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -b1000',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency of burst of 1000 packets'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc6',
            note => '#6 - ping-pong option -t10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -t10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'RunTime=10'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc7',
            note => '#7 - ping-pong option -t30',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -t30',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'RunTime=30'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc8',
            note => '#8 - ping-pong option -m32',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -m32',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc9',
            note => '#9 - ping-pong option -m4096',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -m4096',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc10',
            note => '#10 - ping-pong option -m65500',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -m65500',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc11',
            note => '#11 - ping-pong option -r10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -r10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc12',
            note => '#12 - ping-pong option -r100',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -r100',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc13',
            note => '#13 - ping-pong option -r1024',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -r1024',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc14',
            note => '#14 - ping-pong option -f (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:10)',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:10)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc15',
            note => '#15 - ping-pong option -f -Fs (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:10) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:10) -F s',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc16',
            note => '#16 - ping-pong option -f -Fp (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:10) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:10) -F p',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc17',
            note => '#17 - ping-pong option -f -Fe (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:10) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:10) -F e',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc18',
            note => '#18 - ping-pong option -f -Fs (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:300) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:300) -F s',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc19',
            note => '#19 - ping-pong option -f -Fp (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:300) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:300) -F p',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using poll()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc20',
            note => '#20 - ping-pong option -f -Fe (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:300) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:300) -F e',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using epoll()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc21',
            note => '#21 - ping-pong option -f -Fs --timeout 0',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F s --timeout 0',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:3) -F s --timeout 0',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc22',
            note => '#22 - ping-pong option -f -Fp --timeout 0',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F p --timeout 0',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:3) -F p --timeout 0',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using poll()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc23',
            note => '#23 - ping-pong option -f -Fe --timeout 0',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F e --timeout 0',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:3) -F e --timeout 0',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using epoll()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc24',
            note => '#24 - ping-pong option -f -Fs --timeout=-1',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F s --timeout=-1',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:3) -F s --timeout=-1',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc25',
            note => '#25 - ping-pong option -f -Fp --timeout=-1',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F p --timeout=-1',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:3) -F p --timeout=-1',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using poll()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc26',
            note => '#26 - ping-pong option -f -Fe --timeout=-1',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F e --timeout=-1',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:3) -F e --timeout=-1',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using epoll()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc27',
            note => '#27 - ping-pong option -f -Fs --threads-num=2 (one socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:2) -F s --threads-num=2',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:2)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc28',
            note => '#28 - ping-pong option -f -Fp --threads-num=2 (one socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:2) -F p --threads-num=2',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:2)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc29',
            note => '#29 - ping-pong option -f -Fe --threads-num=2 (one socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:2) -F e --threads-num=2',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:2)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc30',
            note => '#30 - ping-pong option -f -Fs --threads-num=10 (two socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:20) -F s --threads-num=10',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:20)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc31',
            note => '#31 - ping-pong option -f -Fp --threads-num=10 (two socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:20) -F p --threads-num=10',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:20)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc32',
            note => '#32 - ping-pong option -f -Fe --threads-num=10 (two socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:20) -F e --threads-num=10',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:20)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc33',
            note => '#33 - ping-pong option -f -Fs --threads-num=100 (on 400 sockets)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:400) -F s --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:400)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc34',
            note => '#34 - ping-pong option -f -Fp --threads-num=100 (on 400 sockets)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:400) -F p --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:400)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc35',
            note => '#35 - ping-pong option -f -Fe --threads-num=100 (on 400 sockets)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:400) -F e --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:400)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc36',
            note => '#36 - ping-pong multicast option -f --mc-loopback-enable',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:7000:10) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:224.4.0.3:7000:10) -F s --mc-loopback-enable',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()', 'IP = 224.4.'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc37',
            note => '#37 - ping-pong multicast option -f -Fs (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:7000:10) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:224.4.0.3:7000:10) -F s',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()', 'IP = 224.4.'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc38',
            note => '#38 - ping-pong multicast option -f -Fp (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:7000:10) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:224.4.0.3:7000:10) -F p',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using poll()', 'IP = 224.4.'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc39',
            note => '#39 - ping-pong multicast option -f -Fe (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:7000:10) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:224.4.0.3:7000:10) -F e',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using epoll()', 'IP = 224.4.'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc40',
            note => '#40 - ping-pong multicast option -f -Fs (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:7000:300) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:224.4.0.3:7000:300) -F s',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using select()', 'IP = 224.4.'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc41',
            note => '#41 - ping-pong multicast option -f -Fp (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:7000:300) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:224.4.0.3:7000:300) -F p',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using poll()', 'IP = 224.4.'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc42',
            note => '#42 - ping-pong multicast option -f -Fe (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:7000:300) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:224.4.0.3:7000:300) -F e',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'using epoll()', 'IP = 224.4.'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc43',
            note => '#43 - ping-pong option --pps=10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --pps=10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc44',
            note => '#44 - ping-pong option --pps=1000',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --pps=1000',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc45',
            note => '#45 - ping-pong option --pps=100000',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --pps=100000',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc46',
            note => '#46 - ping-pong unicast option -f -Fs -t20 -m50000 -r512 --threads-num=100 --data-integrity',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:500) -F s --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:TARGET():17000:500) -F s -t20 -m50000 -r512 --data-integrity',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc47',
            note => '#47 - ping-pong multicast option -f -Fs -t20 -m50000 -r512 --threads-num=100 --data-integrity',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:17000:500) -F s --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -f FEED(UDP:224.4.0.3:17000:500) -F s -t20 -m50000 -r512 --data-integrity',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Latency is', 'using select()', 'IP = 224.4.'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
    ];


1;
