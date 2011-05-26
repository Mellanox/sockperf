##
# @file Common.pm
#
# @brief TE module to define common data.
#
#

## @class
# Container for common data.
package TE::Common;

use strict;
use warnings;
use vars qw(@EXPORT);
use base qw(Exporter);
@EXPORT = qw
(
get_error
set_error
TE_ERR_NONE
TE_ERR_FATAL
TE_ERR_CONNECT
TE_ERR_LOGIN
TE_ERR_PROMPT
TE_ERR_PERL
TE_ERR_TIMEOUT
TE_ERR_BREAK
TE_ERR_BAD_ARGUMENT
TE_ERR_CUSTOM
TE_ERR_UNKNOWN
);

our $last_error = {code => 0, msg => ''};
our $alias = 'verifier_te';

use constant 
{
    TE_ERR_NONE => 0,
    TE_ERR_FATAL => 1,
    TE_ERR_CONNECT => 2,
    TE_ERR_LOGIN => 3,
    TE_ERR_PROMPT => 4,
    TE_ERR_PERL => 5,
    TE_ERR_TIMEOUT => 6,
    TE_ERR_BREAK => 7,
    TE_ERR_BAD_ARGUMENT => 8,
    TE_ERR_CUSTOM => 9,
    TE_ERR_UNKNOWN => 10,
};


our $conf = 
    {
    	options =>
    	   {
                'tasks' => undef,    
                'username' => "root",
                'password' => "password",
                'targets' => undef,
                'log' => undef,
                'screen_log' => 1,
                'format_log' => 0,
                'silent' => 0,
                'progress' => 1,
                'out_level' => 2,
                'log_level' => 3,
                'email' => undef,
    	   },
        common =>
           {
                'app' => 'vperf',    
                'app_path' => '',    
                'host' => 'localhost',
                'host_ip' => undef,
                'flog' => undef,
                'fdump' => undef,
           },          
        current =>
           {
                'target' => 'localhost',
           },    	   
    };


sub get_error
{
    return ( @_ ? $last_error->{msg} : $last_error->{code} );
}


sub set_error
{
    my ($code, $msg) = @_;
	   
    if (!$msg)
    {
        $msg = '' if ($code == TE_ERR_NONE);
        $msg = 'fatal operation' if ($code == TE_ERR_FATAL);
        $msg = 'unknown remote host' if ($code == TE_ERR_CONNECT);
        $msg = 'login failed' if ($code == TE_ERR_LOGIN);
        $msg = 'timed-out waiting for command prompt' if ($code == TE_ERR_PROMPT);
        $msg = 'perl script error' if ($code == TE_ERR_PERL);
        $msg = 'pattern match timed-out' if ($code == TE_ERR_TIMEOUT);
        $msg = 'pattern match timed-out' if ($code == TE_ERR_BREAK);
        $msg = 'pattern match timed-out' if ($code == TE_ERR_BAD_ARGUMENT);
        $msg = 'custom error' if ($code == TE_ERR_CUSTOM);
        $msg = 'unknown error' if ($code >= TE_ERR_UNKNOWN);
    }   
    $last_error->{code} = $code;
    $last_error->{msg} = $msg;
}


1;
