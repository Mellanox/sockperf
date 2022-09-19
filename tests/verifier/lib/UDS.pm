##
# @file UDS.pm
#
# @brief Test Suite Unix Domain Socket.
#
#

## @class
# Container for common data.
package UDS;

use strict;
use warnings;


# Own modules
use TE::Common;
use TE::Funclet;
use TE::Utility;


our $test_suite_uds = 
    [
        {
            name => 'tc1',
            note => '#1 - ping-pong w/o arguments (default is udp)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() ',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() ',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'Warmup stage \(sending a few dummy messages\)...', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                },
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc2',
            note => '#2 - ping-pong option --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'Warmup stage \(sending a few dummy messages\)...', 'ADDR'],
                    failure =>  ['Segmentation fault', 'Assertion', 'ERROR', 'server down'],
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc3',
            note => '#3 - ping-pong option -b10 --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp -b10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency of burst of 10 messages', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc4',
            note => '#4 - ping-pong option -b100 --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp -b100',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency of burst of 100 messages', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc5',
            note => '#5 - ping-pong option -b500 --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp -b500',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency of burst of 500 messages', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc6',
            note => '#6 - ping-pong option -t10 --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp -t10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'RunTime=(9\.|10)', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc7',
            note => '#7 - ping-pong option -t10 (udp) ',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -t30',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'RunTime=(9\.|10)', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc8',
            note => '#8 - ping-pong option -m32 --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp -m32',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc9',
            note => '#9 - ping-pong option -m4096 --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp -m4096',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc10',
            note => '#10 - ping-pong option --m4096 (udp)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -m4096',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc11',
            note => '#11 - ping-pong option -r10 --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp -r10',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc12',
            note => '#12 - ping-pong option -r100 --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp -r100',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc13',
            note => '#13 - ping-pong option -r1024 --tcp',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET() --tcp',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() --tcp -r1024',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
        {
            name => 'tc14',
            note => '#13 - ping-pong option -r1024 (udp)',
            pre_proc => \&te_def_pre_proc,
            server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pp -i TARGET() -r1024',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'ADDR'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                }
            },
            post_proc => \&te_def_post_proc,
        },
    ];


1;
