##
# @file UTP.pm
#
# @brief Test Suite UDP throughput.
#
#

## @class
# Container for common data.
package UTP;

use strict;
use warnings;


# Own modules
use TE::Common;
use TE::Funclet;
use TE::Utility;


our $test_suite_udp_tp = 
    [
	    {
	        name => 'tc1',
            note => '#1 - throughput w/o arguments',
            pre_proc => \&te_def_pre_proc,
	        server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET()',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
            	client => {
	            	success => ['Test ended', 'Summary: Message Rate', 'Warmup stage \(sending a few dummy packets\)...'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
            	},
            },
            post_proc => \&te_def_post_proc,
	    },
        {
            name => 'tc2',
            note => '#2 - throughput option --dontwarmup',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() --dontwarmup',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down', 'Warmup stage \(sending a few dummy packets\)...'],
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc3',
            note => '#3 - throughput option -b10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -b10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc4',
            note => '#4 - throughput option -b100',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -b100',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc5',
            note => '#5 - throughput option -b1000',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -b1000',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc6',
            note => '#6 - throughput option -t10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -t10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc7',
            note => '#7 - throughput option -t30',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -t30',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc8',
            note => '#8 - throughput option -m32',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -m32',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc9',
            note => '#9 - throughput option -m4096',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -m4096',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc10',
            note => '#10 - throughput option -m65500',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -m65500',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc11',
            note => '#11 - throughput option -r10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -r10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc12',
            note => '#12 - throughput option -r100',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -r100',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc13',
            note => '#13 - throughput option -r1024',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() -r1024',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc14',
            note => '#14 - throughput option -f (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:10)',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:10)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc15',
            note => '#15 - throughput option -f -Fs (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:10) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:10) -F s',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc16',
            note => '#16 - throughput option -f -Fp (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:10) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:10) -F p',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc17',
            note => '#17 - throughput option -f -Fe (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:10) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:10) -F e',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc18',
            note => '#18 - throughput option -f -Fs (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:300) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:300) -F s',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc19',
            note => '#19 - throughput option -f -Fp (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:300) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:300) -F p',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc20',
            note => '#20 - throughput option -f -Fe (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:300) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:300) -F e',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc21',
            note => '#21 - throughput option -f -Fs --timeout 0',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F s --timeout 0',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:3) -F s --timeout 0',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc22',
            note => '#22 - throughput option -f -Fp --timeout 0',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F p --timeout 0',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:3) -F p --timeout 0',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc23',
            note => '#23 - throughput option -f -Fe --timeout 0',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F e --timeout 0',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:3) -F e --timeout 0',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc24',
            note => '#24 - throughput option -f -Fs --timeout=-1',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F s --timeout=-1',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:3) -F s --timeout=-1',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc25',
            note => '#25 - throughput option -f -Fp --timeout=-1',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F p --timeout=-1',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:3) -F p --timeout=-1',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc26',
            note => '#26 - throughput option -f -Fe --timeout=-1',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:3) -F e --timeout=-1',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:3) -F e --timeout=-1',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc27',
            note => '#27 - throughput option -f -Fs --threads-num=2 (one socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:2) -F s --threads-num=2',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:2)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc28',
            note => '#28 - throughput option -f -Fp --threads-num=2 (one socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:2) -F p --threads-num=2',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:2)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc29',
            note => '#29 - throughput option -f -Fe --threads-num=2 (one socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:2) -F e --threads-num=2',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:2)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc30',
            note => '#30 - throughput option -f -Fs --threads-num=10 (two socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:20) -F s --threads-num=10',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:20)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc31',
            note => '#31 - throughput option -f -Fp --threads-num=10 (two socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:20) -F p --threads-num=10',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:20)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc32',
            note => '#32 - throughput option -f -Fe --threads-num=10 (two socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:20) -F e --threads-num=10',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:20)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc33',
            note => '#33 - throughput option -f -Fs --threads-num=100 (on 400 sockets)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:400) -F s --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:400)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc34',
            note => '#34 - throughput option -f -Fp --threads-num=100 (on 400 sockets)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:400) -F p --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:400)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc35',
            note => '#35 - throughput option -f -Fe --threads-num=100 (on 400 sockets)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:TARGET():17000:400) -F e --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:TARGET():17000:400)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc36',
            note => '#36 - throughput multicast option -f --mc-loopback-enable',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:17000:10) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:224.4.0.3:17000:10) -F s --mc-loopback-enable',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate', 'IP = 224.4.'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc37',
            note => '#37 - throughput multicast option -f -Fs (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:17000:10) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:224.4.0.3:17000:10) -F s',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate', 'IP = 224.4.'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc38',
            note => '#38 - throughput multicast option -f -Fp (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:17000:10) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:224.4.0.3:17000:10) -F p',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate', 'IP = 224.4.'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc39',
            note => '#39 - throughput multicast option -f -Fe (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:17000:10) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:224.4.0.3:17000:10) -F e',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
	                success => ['Test ended', 'Summary: Message Rate', 'IP = 224.4.'],
	                failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc40',
            note => '#40 - throughput multicast option -f -Fs (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:17000:300) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:224.4.0.3:17000:300) -F s',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using select()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate', 'IP = 224.4.'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc41',
            note => '#41 - throughput multicast option -f -Fp (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:17000:300) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:224.4.0.3:17000:300) -F p',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using poll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate', 'IP = 224.4.'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc42',
            note => '#42 - throughput multicast option -f -Fe (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(UDP:224.4.0.3:17000:300) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -f FEED(UDP:224.4.0.3:17000:300) -F e',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'using epoll()'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate', 'IP = 224.4.'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc43',
            note => '#43 - throughput option --pps=1000',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'tp -i TARGET() --pps=1000',
            result_proc => \&te_utp_result_proc1,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Message Rate'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
    ];

sub te_utp_result_proc1
{
    my $status = 0;
    @_ = TE::Utility::clear_array(@_);

    if (@_ == 0)
    {
        $status = 1;
    }
    elsif (@_ > 0)
    {
        my ($arg) = @_;
        
        return ($status) if (!defined($arg));
        
        $status = te_def_result_proc(@_);
        
        if ($status == 0)
        {
            my $server_output = (exists($arg->{server}->{output}) && defined($arg->{server}->{output}) ? $arg->{server}->{output} : '');
            my $client_output = (exists($arg->{client}->{output}) && defined($arg->{client}->{output}) ? $arg->{client}->{output} : '');    
    
            if ( $status == 0 &&
                 $client_output ne '' )
            {
                my $sent_msg = 0;
                if ($client_output =~ m/Message Rate is (\d+) \[msg\/sec\]/) {
                    $sent_msg = $1;
                    if ( not (grep($_ == $sent_msg, (1000..1001))) ) {
                        $status = 1;
                    }
                }
                else {
                    $status = 1;
                }
            }
        }
    }
    
    return ($status);
}

1;
