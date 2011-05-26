##
# @file Funclet.pm
#
# @brief TE module to define funclets.
#
#

## @class
# Container for funclets.
package TE::Funclet;

use strict;
use warnings;


# External modules
#use Net::Telnet;
use File::Basename;
use Cwd 'abs_path';


# Own modules
use TE::Common;
use TE::Utility;
use TE::Progress;


# Define reference to the common global structure
my $_options = $TE::Common::conf->{options};
my $_common = $TE::Common::conf->{common};
my $_current = $TE::Common::conf->{current};

# Set of predefined variables that are available inside user script
our $HOST = \$_common->{host};
our $HOST_IP = \$_common->{host_ip};
our $TARGET = \$_current->{target};
my $cwd = Cwd::getcwd();

# Common variables and constants
my $_timeout = 30;


sub __error
{
	my $status = TE_ERR_NONE;
	
	$_ = "@_";

	$status = TE_ERR_CONNECT if (/^unknown remote host/i);
    $status = TE_ERR_LOGIN if (/^login failed/i);
    $status = TE_ERR_PROMPT if (/^timed-out waiting for command prompt/i);
    $status = TE_ERR_TIMEOUT if (/^pattern match timed-out/i);
    $status = TE_ERR_UNKNOWN if (!$status);
    
    # Set error code
    set_error($status, "Telnet: $_");
    
    # Display message 
    TE::Utility::error();
    
    # Follow special error behaviour
    TE::Utility::force_error();
}


sub __waitfor
{
    my ($telnet, $node, $timeout, $prompt, $prompt_reg) = @_;
    my @outputs = ();
	
    my $timeout_counter = 0;

    while( !$telnet->eof() && !get_error())
    { 
        my ($prematch, $match) = $telnet->waitfor(Timeout  => 1,
                                                  Match => '/[\n\r]/',
                                                  Match => $prompt_reg,
                                                  Errmode => sub { return; });

        # Refresh information in status line in case last output
        # contains printed symbols
        TE::Progress::update($node, $prematch);
                    
        push @outputs, $prematch . "\n" if defined($prematch );
        if (defined($match)) 
        {
            $timeout_counter = 0;
        }
        else
        {
            $timeout_counter ++;
        }
                    
        if( (defined($match) && ($match =~ m/$prompt/)) ||
            (defined($timeout) && ($timeout_counter > $timeout)))
        {
            # Finish command processing
            last;
        }
    } 
	
	return @outputs;
}

###############################################################################
#
# Return last error code or error message.
# Valid error numbers are all non-zero integer values. This function returns 
# error number if no arguments are passed and error message in case error code 
# is passed.
#
###############################################################################
sub errno 
{
    return get_error( @_ );
}


###############################################################################
#
# Execute Perl script on a host and return array or scalar
#
###############################################################################
sub perl 
{
    my $status = TE_ERR_NONE;
    my $result = undef;

    @_ = TE::Utility::clear_array(@_);
    
    TE::Utility::debug((caller(0))[3] . ":\n{\n" . join("\n", @_) . "\n}\n");
    set_error(TE_ERR_NONE);
    
    TE::Progress::update($_common->{host}, 'Processing perl script ...');

    my $cmd = join("\n", @_);
    
    # Execute script
    no strict;
    no warnings;
    my @outputs = eval($cmd) if (!get_error());
    # Save evaluation error
    my $eval_err = $@;
    use strict;
    use warnings;

    if ( !get_error() && $eval_err )
    {
            set_error(TE_ERR_PERL, "Perl syntax or runtime error: $eval_err");
            TE::Utility::error();
            TE::Utility::force_error();   	
    }
       
    # Form result
    $result .= join("", @outputs);
    
    $status = get_error();
    TE::Utility::debug((caller(0))[3] . ": return = $status\n");

    return $result;
}


###############################################################################
#
# Execute Shell script on host and return standard output
#
###############################################################################
sub shell 
{
    my $status = TE_ERR_NONE;
    my $result = '';

    @_ = TE::Utility::clear_array(@_);
    
    TE::Utility::debug((caller(0))[3] . ":\n{\n" . join("\n", @_) . "\n}\n");
    set_error(TE_ERR_NONE);
    
    TE::Progress::update($_common->{host}, 'Executing shell command ...');
    
    my (@commands) = @_;

    foreach (@commands)
    {
        next if ($_ eq '');
        last if (get_error());
        
        my $cmd = $_;
        my @outputs = ();

        TE::Progress::update(undef, $cmd);
        
        # Execute script
        # which means $cmd will 'pipe' its output to SHELL. That means SHELL can be used 
        # to read the output of the command (specifically the stuff the command sends to STDOUT). 
        eval { open SHELL, "$cmd 2>&1 |"  or die $! };
        my $eval_err = $@;
        if ( !get_error() && $eval_err )
        {  
            set_error(TE_ERR_UNKNOWN, "Current command \'$cmd\': $eval_err");
            TE::Utility::error();
            TE::Utility::force_error();
       }

        # Form result
        if (!get_error())
        {
            while (<SHELL>) 
            {
                last if (get_error());
		        
                push @outputs, $_ if defined($_);
		        
                TE::Progress::update(undef, $_);
            }
        }
        
        close SHELL;
	    
        # Form result
        $result .= join("", @outputs);
            
        TE::Utility::dump("IN:\n$cmd\nOUT:\n@{[join(\"\", @outputs)]}\n");
    }

    $status = get_error();
    TE::Utility::debug((caller(0))[3] . ": return = $status\n");

    return $result;
}


###############################################################################
#
# Execute Command Shell procedure on a target and return standard output
#
###############################################################################
sub execute 
{
    my $status = TE_ERR_NONE;
    my $result = '';

    @_ = TE::Utility::clear_array(@_);
    
    TE::Utility::debug((caller(0))[3] . ":\n{\n" . join("\n", @_) . "\n}\n");
    set_error(TE_ERR_NONE);
    
    TE::Progress::update($_common->{host}, 'Executing shell command ...');
    
    my (@commands) = @_;

    foreach (@commands)
    {
        next if ($_ eq '');
        last if (get_error());
        
        my $cmd = $_;
        my @outputs = ();

        TE::Progress::update(undef, $cmd);
        
        # Execute script
        # which means $cmd will 'pipe' its output to SHELL. That means SHELL can be used 
        # to read the output of the command (specifically the stuff the command sends to STDOUT).
        $cmd = "ssh $$TARGET " . $cmd; 
        eval { open SHELL, "$cmd 2>&1 |"  or die $! };
        my $eval_err = $@;
        if ( !get_error() && $eval_err )
        {  
            set_error(TE_ERR_UNKNOWN, "Current command \'$cmd\': $eval_err");
            TE::Utility::error();
            TE::Utility::force_error();
       }

        # Form result
        if (!get_error())
        {
            while (<SHELL>) 
            {
                last if (get_error());
                
                push @outputs, $_ if defined($_);
                
                TE::Progress::update(undef, $_);
            }
        }
        
        close SHELL;
        
        # Form result
        $result .= join("", @outputs);
            
        TE::Utility::dump("IN:\n$cmd\nOUT:\n@{[join(\"\", @outputs)]}\n");
    }

    $status = get_error();
    TE::Utility::debug((caller(0))[3] . ": return = $status\n");

    return $result;
}

    
###############################################################################
#
# Send message to output
# display string on the terminal and in log-file. There are several
# supported special categories such as 'fatal', 'error', 'trace' and 'debug'. 
# Category 'fatal' supports  setting customer error code.
#
###############################################################################
sub message 
{
    @_ = TE::Utility::clear_array(@_);

    if (@_ == 0)
    {   # Do nothing
        return;
    }
    elsif (@_ == 1)
    {   # Output message string as is
        TE::Utility::outlog( -terminal=>1, -file=>1, $_[0] )  if (defined($_[0]));
    }
    else
    {   # Process by category
        if ( $_[0] =~ /^fatal/i )
        {
            # Pass message and user defined return code
            if ( defined($_[2]) ) 
            {
                TE::Utility::fatal( $_[1], $_[2] );
            }
            else 
            {
                TE::Utility::fatal( $_[1] );
            }
        }
        elsif ( $_[0] =~ /^error/i )
        {
            # Set error code
            set_error( TE_ERR_CUSTOM, $_[1] );

            # Display message 
        	TE::Utility::error( $_[1] );

            # Follow special error behaviour
            #TE::Utility::force_error();
        }
        elsif ( $_[0] =~ /^trace/i )
        {
        	TE::Utility::trace( $_[1] );
        }
        elsif ( $_[0] =~ /^debug/i )
        {
        	TE::Utility::debug( $_[1] );
        }
        elsif ( $_[0] =~ /^dump/i )
        {
            TE::Utility::dump( $_[1] );
        }
        else
        {
        	TE::Utility::outlog( -terminal=>1, -file=>1, @_ );
        }
	}
}


###########################################################
# Services (following subroutine are based on primary funclets)
###########################################################
sub progress
{
    TE::Progress::update($_common->{host}, @_);
    sleep(1);
}

sub fail
{
	message("fatal", @_);
}

sub error
{
    message("error", @_);
}

sub die_error
{
    message("error", @_);
    
    # Clean status line to prevent log corruption
    TE::Progress::clean();
        
    # Do die to use capacity of eval operation and switch to process next target
    # pressure die message to display internal errors
    die "\n";
}


sub execute_nowait
{
	execute(-timeout => -1, @_);
}


1;
