##
# @file TUL.pm
#
# @brief Test Suite TCP under-load.
#
#

## @class
# Container for common data.
package TUL;

use strict;
use warnings;


# Own modules
use TE::Common;
use TE::Funclet;
use TE::Utility;


our $test_suite_tcp_ul = 
    [
        {
            name => 'tc1',
            note => '#1 - under-load w/o arguments',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp',
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
            note => '#2 - under-load option --dontwarmup',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp --dontwarmup',
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
            note => '#3 - under-load option -b10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -b10',
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
            note => '#4 - under-load option -b100',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -b100',
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
            note => '#5 - under-load option -b1000',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -b1000',
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
            note => '#6 - under-load option -t10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -t10',
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
            note => '#7 - under-load option -t30',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -t30',
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
            note => '#8 - under-load option -m32',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -m32',
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
            note => '#9 - under-load option -m4096',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -m4096',
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
            note => '#10 - under-load option -m65500',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -m65500',
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
            note => '#11 - under-load option -r10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -r10',
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
            note => '#12 - under-load option -r100',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -r100',
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
            note => '#13 - under-load option -r1024',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp -r1024',
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
            note => '#14 - under-load option -f (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:10)',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:10)',
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
            note => '#15 - under-load option -f -Fs (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:10) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:10) -F s',
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
            note => '#16 - under-load option -f -Fp (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:10) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:10) -F p',
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
            note => '#17 - under-load option -f -Fe (10 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:10) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:10) -F e',
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
            note => '#18 - under-load option -f -Fs (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:300) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:300) -F s',
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
            note => '#19 - under-load option -f -Fp (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:300) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:300) -F p',
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
            note => '#20 - under-load option -f -Fe (300 records)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:300) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:300) -F e',
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
            note => '#21 - under-load option -f -Fs --timeout 0',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:3) -F s --timeout 0',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:3) -F s --timeout 0',
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
            note => '#22 - under-load option -f -Fp --timeout 0',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:3) -F p --timeout 0',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:3) -F p --timeout 0',
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
            note => '#23 - under-load option -f -Fe --timeout 0',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:3) -F e --timeout 0',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:3) -F e --timeout 0',
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
            note => '#24 - under-load option -f -Fs --timeout=-1',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:3) -F s --timeout=-1',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:3) -F s --timeout=-1',
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
            note => '#25 - under-load option -f -Fp --timeout=-1',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:3) -F p --timeout=-1',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:3) -F p --timeout=-1',
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
            note => '#26 - under-load option -f -Fe --timeout=-1',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:3) -F e --timeout=-1',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:3) -F e --timeout=-1',
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
            note => '#27 - under-load option -f -Fs --threads-num=2 (one socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:2) -F s --threads-num=2',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:2)',
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
            note => '#28 - under-load option -f -Fp --threads-num=2 (one socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:2) -F p --threads-num=2',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:2)',
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
            note => '#29 - under-load option -f -Fe --threads-num=2 (one socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:2) -F e --threads-num=2',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:2)',
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
            note => '#30 - under-load option -f -Fs --threads-num=10 (two socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:20) -F s --threads-num=10',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:20)',
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
            note => '#31 - under-load option -f -Fp --threads-num=10 (two socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:20) -F p --threads-num=10',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:20)',
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
            note => '#32 - under-load option -f -Fe --threads-num=10 (two socket per thread)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:20) -F e --threads-num=10',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:20)',
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
            note => '#33 - under-load option -f -Fs --threads-num=100 (on 400 sockets)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:400) -F s --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:400)',
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
            note => '#34 - under-load option -f -Fp --threads-num=100 (on 400 sockets)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:400) -F p --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:400)',
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
            note => '#35 - under-load option -f -Fe --threads-num=100 (on 400 sockets)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():17000:400) -F e --threads-num=100',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():17000:400)',
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
            note => '#36 - under-load -f -Fs (10 records the same addr:port)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():0:10) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():0:10) -F s',
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
            name => 'tc37',
            note => '#37 - under-load -f -Fp (10 records the same addr:port)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():0:10) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():0:10) -F p',
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
            name => 'tc38',
            note => '#38 - under-load -f -Fe (10 records the same addr:port)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():0:10) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():0:10) -F e',
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
            name => 'tc39',
            note => '#39 - under-load -f -Fs (300 records the same addr:port)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():0:300) -F s',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():0:300) -F s',
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
            name => 'tc40',
            note => '#40 - under-load -f -Fp (300 records the same addr:port)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():0:300) -F p',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():0:300) -F p',
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
            name => 'tc41',
            note => '#41 - under-load -f -Fe (300 records the same addr:port)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -f FEED(TCP:TARGET():0:300) -F e',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -f FEED(TCP:TARGET():0:300) -F e',
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
            name => 'tc42',
            note => '#42 - under-load option --pps=10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp --pps=10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'No messages were received from the server'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc43',
            note => '#43 - under-load option --pps=1000',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp --pps=1000',
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
            note => '#44 - under-load option --pps=100000',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp --pps=100000',
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
            note => '#45 - under-load option --pps=100 --reply-every=100',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp --pps=100 --reply-every=100',
            result_proc => \&te_tul_result_proc1,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'No valid observations found'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc46',
            note => '#46 - under-load option --pps=1000 --reply-every=10',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'ul -i TARGET() --tcp --pps=1000 --reply-every=10',
            result_proc => \&te_tul_result_proc2,
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
    ];

sub te_tul_result_proc1
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
                my $recv_msg = 0;
                if ($client_output =~ m/SentMessages=(\d+)/) {
                    $sent_msg = $1;
                    if ($client_output =~ m/ReceivedMessages=(\d+)/) {
                        $recv_msg = $1;
                        if ( not (grep($_ == $sent_msg, (90..120)) && grep($_ == $recv_msg, (1..2))) ) {
                            $status = 1;
                        }
                    }
                    else {
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

sub te_tul_result_proc2
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
                my $recv_msg = 0;
                if ($client_output =~ m/SentMessages=(\d+)/) {
                    $sent_msg = $1;
                    if ($client_output =~ m/ReceivedMessages=(\d+)/) {
                        $recv_msg = $1;
                        if ( not (grep($_ == $sent_msg, (990..1200)) && grep($_ == $recv_msg, (98..120))) ) {
                            $status = 1;
                        }
                    }
                    else {
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
