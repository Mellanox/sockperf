##
# @file UPB.pm
#
# @brief Test Suite UDP playback.
#
#

## @class
# Container for common data.
package UPB;

use strict;
use warnings;


# Own modules
use TE::Common;
use TE::Funclet;
use TE::Utility;


our $test_suite_udp_pb = 
    [
	    {
	        name => 'tc1',
            note => '#1 - playback w/o arguments',
            pre_proc => \&te_def_pre_proc,
	        server_proc => \&te_def_server_proc,
            server_arg => 'sr -i TARGET()',
            client_proc => \&te_def_client_proc,
            client_arg => 'pb -i TARGET() --data-file=PLAYBACK(1.000000:0.000005:12:1000)',
            result_proc => \&te_def_result_proc,
            result_arg => {
                server => {
                    success => ['Test end', 'interrupted by', 'exit', 'Total 1000 messages received'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR']
                },
                client => {
                    success => ['Test ended', 'Summary: Latency is', 'SentMessages=1000'],
                    failure => ['Segmentation fault', 'Assertion', 'ERROR', 'server down']
                },
            },
            post_proc => \&te_def_post_proc,
	    },
    ];


1;
