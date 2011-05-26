#!/usr/bin/env perl
##
# @file verifier.pl
#
# @brief VPERF Verification Tool.
# @author Igor Ivanov (Igor.Ivanov@itseez.com)
#


use strict;
use warnings;
use Cwd;
use File::Basename;

# Add extra directories to Perl's search path as /lib with tool
# additional modules
use lib dirname($0) . "/lib";
use lib dirname($0) . "/lib/TE";

use Data::Dumper;
use File::Basename;
use File::Temp;
use File::Basename;
use File::Spec;
use Sys::Hostname;

use TE::Common;
use TE::Utility;
use TE::Funclet;

our ($VERSION) = "0.1.";

###########################################################
# Set variables
###########################################################
my $module_name=basename($0,".pl");
my $module_script=basename($0);
my $module_path=File::Spec->rel2abs(dirname($0));
my $command_line = join(" ", @ARGV);

###########################################################
# Set Interrupt Handlers
###########################################################
$SIG{'INT'} = \&__abort;  # use signal handling, if press ^C, will interrupt and call sub __abort.


###########################################################
# Main block
###########################################################
use Getopt::Long qw(:config no_ignore_case);

my $opt_help;
my $opt_version;
my $opt_info;

my $opt_app;
my @opt_target_list;
my $opt_username;
my $opt_password;
my @opt_task_list;

my $opt_mailto;
my $opt_silent;
my $opt_out_level;
my $opt_log_level;
my $opt_progress;
my $opt_log;
my $opt_screen_log;
my $opt_format_log;


GetOptions ('help|h' => \$opt_help,
            'version|v' => \$opt_version,
            'info|i' => \$opt_info,

            'app|a=s' => \$opt_app,
            'target|s=s' => \@opt_target_list,
            'username|u=s' => \$opt_username,
            'password|p=s' => \$opt_password,           
            'task|t=s' => \@opt_task_list,

            'email|e|m=s' => \$opt_mailto,
            'quiet|q!' => \$opt_silent,
            'out-level|o=i' => \$opt_out_level,
            'log-level|d=i' => \$opt_log_level,
            'progress|p=i' => \$opt_progress,
            'log|l=s' => \$opt_log,
            'screen-log' => \$opt_screen_log,
            'format-log' => \$opt_format_log,
            );

# Define reference to the common global structure
my $_options = $TE::Common::conf->{options};
my $_common = $TE::Common::conf->{common};
my $_current = $TE::Common::conf->{current};

# Set values passed from command line for some parameters
$_options->{targets} = [split(/\s*,\s*/,join(',',@opt_target_list))] if scalar(@opt_target_list);
$_options->{username} = $opt_username if defined($opt_username);
$_options->{password} = $opt_password if defined($opt_password);
$_options->{tasks} = [split(/\s*,\s*/,join(',',@opt_task_list))] if scalar(@opt_task_list);
$_options->{email} = $opt_mailto if defined($opt_mailto);
$_options->{silent} =  $opt_silent if defined($opt_silent);

$_options->{out_level} = $opt_out_level if defined($opt_out_level);
$_options->{out_level} = 0 if ($_options->{silent} == 1);

$_options->{log_level} = $opt_log_level if defined($opt_log_level);

$_options->{progress} =  $opt_progress if defined($opt_progress);
$_options->{progress} = 0 if ($_options->{silent} == 1);
$_options->{log} = ( defined($opt_log) ? $opt_log : undef );
$_options->{screen_log} = $opt_screen_log if defined($opt_screen_log);
$_options->{format_log} = 1 if defined($opt_format_log);


if ($opt_help)
{
    __help('');
    exit(TE_ERR_NONE);
}

if ($opt_version)
{
    __version($VERSION);
    exit(TE_ERR_NONE);
}

if (defined($opt_app))
{
    # Check if files existed
    __verify_opt_file( $opt_app );	
    $_common->{app} = basename($opt_app);
    $_common->{app_path} = File::Spec->rel2abs(dirname($opt_app));
}
$_common->{host} = hostname();
$_common->{host_ip} = TE::Utility::get_ip($_common->{host});

$_current->{target} = hostname();

# Get command-line rest arguments and make them as external variables
# they are accessable from user script as $ENV{name} or &shell("echo \$name")
foreach (@ARGV) 
{
    if ($_ =~ /([^=:]+)\=(.*)/) 
    {
    	# save arguments inside internal oject
        $_options->{ext}->{$1} = $2;
        
        # set them as environment variables
        $ENV{$1} = $2;
    }
}


TE::Utility::debug("\n");
TE::Utility::dump(Dumper($_options));


# Process command line options
__setup_task();   
if ( defined($opt_info) || (not scalar(@opt_task_list)) )
{
    __info_task();
}
elsif ( scalar(@opt_task_list) )
{
    __exec_task();
}
else
{
    __help('');
}


# Send notification by e-mail
if ( $opt_mailto ) {
	
    TE::Funclet::shell( "cd $module_path; tar -zcf $_options->{log_file}.tgz $_options->{log_file} $_options->{dump_file}" );
	
    my @attachments = ("$_options->{log_file}.tgz");

    TE::Utility::debug("Sending results to $opt_mailto ...\n");

    __send_results_by_mail($_options->{email}, @attachments);
}

# Exit with error status
exit(get_error());



###########################################################
# Define functions
###########################################################


###############################################################################
#
# Show help to tool
#
###############################################################################
sub __help 
{
    my ($action)=@_;

    print ("Usage: $module_script [options...] [arguments...]\n");
    print ("Mellanox SOCKPERF Verification Utility -- Version: $VERSION\n\n");

    print ("$module_script -s [ip|hostname] -a [absolute path to tool] -t [all|udp-pp|udp-pp:tc1] -l [log file name]\n\n");
	
    print ("\nOptions:\n");
    printf (" %-5s %-10s\t%-s\n", '-h,', '--help', "Show the help message and exit.");
    printf (" %-5s %-10s\t%-s\n", '-v,', '--version', "Output version information and exit.");
    printf (" %-5s %-10s\t%-s\n", '-i',  '--info', "Display detailed information about available tasks.");

    printf (" %-5s %-10s\t%-s\n", '-s,', '--target', "List of targets");
    printf (" %-5s %-10s\t%-s\n", '-u,', '--username', "Privileged username to target (default: $_options->{username}).");
    printf (" %-5s %-10s\t%-s\n", '-p,', '--password', "Password to access target (default: $_options->{password})");
    printf (" %-5s %-10s\t%-s\n", '-t,', '--task',     "Test.");

    print ("\nArguments:\n");

    printf (" %-5s %-10s\t%-s\n", '-e,', '--email', "e-mail address to get notification.");
    printf (" %-5s %-10s\t%-s\n", '-o',  '--out-level', "Set terminal info level 0..4  (default: 1).");
    printf (" %-5s %-10s\t%-s\n", '-d',  '--log-level', "Set log-file info level (default: 2).");
    printf (" %-5s %-10s\t%-s\n", '',    '--[no-]quiet', "Don't display output on the terminal (default: --no-quiet).");
    printf (" %-5s %-10s\t%-s\n", '-p,', '--progress', "Enable/Disable status line (default: 1).");
    printf (" %-5s %-10s\t%-s\n", '-l,', '--log', "Logging file name.");
    printf (" %-5s %-10s\t%-s\n", '',    '--split-log', "Separate logging files by targets (default: off).");
    printf (" %-5s %-10s\t%-s\n", '',    '--no-split-log', "Save output in one logging file (default: on).");
    printf (" %-5s %-10s\t%-s\n", '',    '--format-log', "Enable targetname: labels on output lines");
    
    exit(TE_ERR_NONE);
}


###############################################################################
#
# Display tool version info
#
###############################################################################
sub __version 
{
    printf("$module_name %s\n", @_);
    
    exit(TE_ERR_NONE);
}


###############################################################################
#
# Display all available tasks
# Parameters:
#   none - display information about all tasks on a terminal and return one as a string
#   any - return information about all tasks as a string only
#
###############################################################################
sub __info_task 
{
    my $output_str = '';

    {
        $output_str .= sprintf("Available tasks:\n");
                
        # Calculate max length
        my $max_length = 0;
        foreach my $tsuite (@{$_common->{tsuite}}) 
        {
            $max_length = length($tsuite->{name}) if ($max_length < length($tsuite->{name}));
        }
        
        # Enumerate all sections
        foreach my $tsuite (@{$_common->{tsuite}}) 
        {
            next if grep ($_ !~ /$tsuite->{name}/, @{$_options->{tasks}});
            
            $output_str .= sprintf(" %-${max_length}s - %-s\n", 
                                       $tsuite->{name}, 
                                       $tsuite->{note} );
        }
    }

    print $output_str if (@_ == 0) ; 
    
    return $output_str;
}


###############################################################################
#
# Setup tests
#
###############################################################################
sub __setup_task 
{
	use UPP;
    use TPP;
    use UUL;
    use TUL;
    use UTP;
    use TTP;
    use UPB;
    use TPB;

	$_common->{tsuite} =
	[ 
		{
			name => 'udp-pp',
			tcase => $UPP::test_suite_udp_pp,
			note => 'ping-pong for UDP'
		},
        {
            name => 'udp-ul',
            tcase => $UUL::test_suite_udp_ul,
            note => 'under-load for UDP'
        },
        {
            name => 'udp-tp',
            tcase => $UTP::test_suite_udp_tp,
            note => 'throughput for UDP'
        },
        {
            name => 'udp-pb',
            tcase => $UPB::test_suite_udp_pb,
            note => 'playback for UDP'
        },	
        {
            name => 'tcp-pp',
            tcase => $TPP::test_suite_tcp_pp,
            note => 'ping-pong for TCP'
        },
        {
            name => 'tcp-ul',
            tcase => $TUL::test_suite_tcp_ul,
            note => 'under-load for TCP'
        },
        {
            name => 'tcp-tp',
            tcase => $TTP::test_suite_tcp_tp,
            note => 'throughput for TCP'
        },
        {
            name => 'tcp-pb',
            tcase => $TPB::test_suite_tcp_pb,
            note => 'playback for TCP'
        }   
	];
}


###############################################################################
#
# Execute task
#
###############################################################################
sub __exec_task 
{             
    my $temp_log = ${TE::Common::alias};
    $temp_log .= '-' . $_options->{task} if defined($_options->{task});
    $temp_log .= '-' . sprintf("%02d%02d%04d", 
                                (localtime(time()))[3],
                                (localtime(time()))[4] + 1,
                                (localtime(time()))[5] + 1900);
    $temp_log .= '-' . sprintf("%02d%02d%02d", 
                                (localtime(time()))[2],
                                (localtime(time()))[1],
                                (localtime(time()))[0]);
    $temp_log .= '.log';
    $_options->{log} = $temp_log if (not defined($_options->{log}));

    my $log_file = ( exists($_options->{log}) && defined($_options->{log}) ? $_options->{log} : undef );
    my($filename, $directory, $suffix) = fileparse($log_file, qr/\.[^.]*/);
    my $dump_file = ( exists($_options->{screen_log}) && defined($_options->{screen_log}) ? "$filename.dump" : undef );
    
    unlink $log_file if defined($log_file);
    unlink $dump_file if defined($dump_file);
    
    $_options->{log_file} = $log_file if defined($log_file);
    $_options->{dump_file} = $dump_file if defined($dump_file);
    
    # Launch operations on hosts
    foreach (@{$_options->{targets}})
    {   	
        $_current->{target} = $_;
        
        # Create file for logging
        open($_common->{flog}, ">> $log_file") || 
            TE::Utility::fatal("Can't open $log_file: $!\n") if defined($log_file);
        
        # Create file for dump information if debugging is turned on
        open($_common->{fdump}, ">> $dump_file") || 
            TE::Utility::fatal("Can't open $dump_file: $!\n") if defined($dump_file);
            
        # Form heading
        TE::Utility::trace("\nVoltaire VPERF Verification Tool v$VERSION\n");
        TE::Utility::trace("**********************************\n");
        my $info_str = '';
        $info_str .= sprintf("* Options: %s\n", $command_line);
        $info_str .= sprintf("* Log file: %s\n", $log_file);
        $info_str .= sprintf("* Dump file: %s\n", (defined($dump_file) ? $dump_file : 'none'));
        $info_str .= sprintf("* Host: %s\n", $_common->{host});
        $info_str .= sprintf("* Target: %s\n", $_current->{target});
        $info_str .= sprintf("* Output level: %s\n", $_options->{out_level});
        $info_str .= sprintf("* Log level: %s\n", $_options->{log_level});
        TE::Utility::trace($info_str);
        TE::Utility::trace("**********************************\n");
        TE::Utility::trace("\n");

        local $SIG{ALRM} = sub { 
                           TE::Utility::fatal("ping $_current->{target} unreached. You need to interrupt application\n");
                        };
        alarm 30;
        {
	        TE::Utility::dump("\nTest Environment Settings\n");
	        TE::Utility::dump("**********************************\n");
	        TE::Utility::dump("HOST: $_common->{host}\n");
	        $info_str = '';
	        $info_str .= sprintf("* route table: %s\n", TE::Funclet::shell("route"));
	        $info_str .= sprintf("* interfaces: %s\n", TE::Funclet::shell("ifconfig"));
	        TE::Utility::dump($info_str);
	        TE::Utility::dump("TARGET: $_current->{target}\n");
	        $info_str = '';
	        $info_str .= sprintf("* route table: %s\n", TE::Funclet::execute("route"));
	        $info_str .= sprintf("* interfaces: %s\n", TE::Funclet::execute("ifconfig"));
	        TE::Utility::dump($info_str);
	        TE::Utility::dump("**********************************\n");
	        TE::Utility::dump("\n");
        }
        alarm 0;
    
        my $start_time = time();
        my $num_tests = 0;
        my $num_passes = 0;
        TE::Progress::init() if ($_options->{progress});

		# Check if this method can be used
		eval { require threads };
		if ($@)
		{
		    VFA::Utility::fatal("threads can't be loaded.\n");
		}
        use threads;

		# Launch all tasks in case task list is not defined
		if (scalar(@{$_options->{tasks}}) == 1 && $_options->{tasks}->[0] eq 'all')
		{
            foreach (@{$_common->{tsuite}})
            {
            	push(@{$_options->{tasks}}, $_->{name});
            }
		}

	    foreach my $_task (@{$_options->{tasks}})
	    {
	    	my $i = 0;
	    	my $j = 0;
	    	my ($_tsuite, $_tcase) = split(/:/, $_task);
	    	
            $_current->{task} = $_;
	    	for ($i = 0; $i < scalar(@{$_common->{tsuite}}); $i++)
	    	{
	    		next if (!defined($_common->{tsuite}->[$i]->{tcase}));
	    		next if ( (defined($_tsuite)) && 
	    		          ($_common->{tsuite}->[$i]->{name} !~ /$_tsuite/) );
	    		for ($j = 0; $j < scalar(@{$_common->{tsuite}->[$i]->{tcase}}); $j++)
	    		{
                    next if ( (defined($_tcase)) && 
                              ($_common->{tsuite}->[$i]->{tcase}->[$j]->{name} !~ /$_tcase/) );
                    
		            my $server_output = '';
                    my $client_output = '';
		            my $result_code = 0;
                    my $task = $_common->{tsuite}->[$i]->{tcase}->[$j];
                    my $thread = undef;

			        TE::Utility::dump("\n");
                    TE::Utility::dump("**********************************\n");
                    TE::Utility::dump("* TSUITE : $_common->{tsuite}->[$i]->{name}\n");
                    TE::Utility::dump("*        : $_common->{tsuite}->[$i]->{note}\n")
                        if (defined($_common->{tsuite}->[$i]->{note}));
                    TE::Utility::dump("* TCASE  : $_common->{tsuite}->[$i]->{tcase}->[$j]->{name}\n"); 
                    TE::Utility::dump("*        : $_common->{tsuite}->[$i]->{tcase}->[$j]->{note}\n")
                        if (defined($_common->{tsuite}->[$i]->{tcase}->[$j]->{note}));
			        TE::Utility::dump("**********************************\n");
                    
                    # Execute task
                    $task->{pre_proc}() if defined($task->{pre_proc});
                    if (not (defined($result_code) && $result_code))
                    {
                    	$thread = threads->create(
                               $task->{server_proc}, $task->{server_arg})  if defined($task->{server_proc});
                    	# Wait for server wake up
                    	sleep(2);
                    }
                    if (not (defined($result_code) && $result_code))
                    {
                        local $SIG{ALRM} = sub { 
                        	TE::Funclet::shell("pkill -SIGINT $_common->{app}") 
                        };
                        alarm 300;
                        ($result_code, $client_output) = $task->{client_proc}($task->{client_arg}) if defined($task->{client_proc});
                        alarm 0;
                    }
                    
                    # Wait for server thread completion
                    TE::Funclet::execute("pkill -SIGINT $_common->{app}");
                    if (not (defined($result_code) && $result_code))
                    {
                        my $res= $thread->join() if defined($thread);
                        ($result_code, $server_output) = @$res;
                    }
                    else
                    {
                        $thread->join() if defined($thread);                   	
                    }

                    if (not (defined($result_code) && $result_code))
                    {
                    	$task->{result_arg}->{server}->{output} = $server_output;
                        $task->{result_arg}->{client}->{output} = $client_output;
                        $result_code = $task->{result_proc}($task->{result_arg}) if defined($task->{result_proc});
                    }
                    $task->{post_proc}() if defined($task->{post_proc});
                    
                    # Statistic
                    $num_tests++;
                    $num_passes++ if ( not (defined($result_code) && $result_code) );
                    
                    # Print result line
			        $info_str = '';
			        $info_str .= sprintf("%-6.6s %-12.12s %-12.12s %s\n",
                                         (not (defined($result_code) && $result_code) ? 'PASS' : 'FAIL'),
                                         $_common->{tsuite}->[$i]->{name},
			                             $_common->{tsuite}->[$i]->{tcase}->[$j]->{name},
                                         $_common->{tsuite}->[$i]->{tcase}->[$j]->{note});
                    TE::Utility::trace($info_str);
                    TE::Utility::dump("\n**********************************\n");
	    		}
	    	}
        }
        TE::Progress::end();
        my $finish_time = time();
        
        TE::Utility::trace("\n");
        TE::Utility::trace("**********************************\n");
        $info_str = '';
        $info_str .= sprintf("* Passed: %d\n", $num_passes);
        $info_str .= sprintf("* Failed: %d\n", $num_tests - $num_passes);
        $info_str .= sprintf("* Total: %d\n", $num_tests);
        $info_str .= sprintf("* Start time: %02d-%02d-%04d %02d:%02d:%02d\n", 
                                    (localtime($start_time))[3],
                                    (localtime($start_time))[4] + 1,
                                    (localtime($start_time))[5] + 1900,
                                    (localtime($start_time))[2],
                                    (localtime($start_time))[1],
                                    (localtime($start_time))[0]);
        $info_str .= sprintf("* Finish time: %02d-%02d-%04d %02d:%02d:%02d\n", 
                                    (localtime($finish_time))[3],
                                    (localtime($finish_time))[4] + 1,
                                    (localtime($finish_time))[5] + 1900,
                                    (localtime($finish_time))[2],
                                    (localtime($finish_time))[1],
                                    (localtime($finish_time))[0]);
        $info_str .= sprintf("* Duration: %d:%02d:%02d\n", 
                                    ($finish_time - $start_time) / 3600,
                                    (($finish_time - $start_time) / 60) % 60,
                                    ($finish_time - $start_time) % 60);
        TE::Utility::trace($info_str);
        TE::Utility::trace("**********************************\n");
        
        close($_common->{flog})  if defined($_common->{flog});
        $_common->{flog} = undef;
        close($_common->{fdump})  if defined($_common->{fdump});
        $_common->{fdump} = undef;
    }
}
      
      
###############################################################################
#
# Check if files directed in command line exists
#
###############################################################################
sub __verify_opt_file
{
    my (@files)=@_;
    foreach my $file (@files) 
    {
        if( ! -e $file)
        {
            TE::Utility::fatal("$file doesn't exist\n");
        }
    }
}
      
###############################################################################
#
# Proxy-aware LWP creator
#
###############################################################################
sub __get_lwp
{
    my ($url) = @_;

    my $scheme = $url;
    $scheme =~ s/^\s*(http[s]*):\/\/.*$/$1/;
    # Get the proxy corresponding to the scheme
    my $env_proxy = $ENV{"${scheme}_proxy"};
        
    my $ua = LWP::UserAgent->new();
    return undef
        if (!$ua);
        
    if ($env_proxy) {
        # Ensure the env proxy has the scheme at the prefix
        $env_proxy = "$scheme://$env_proxy"
            if ($env_proxy !~ /^\s*http/);
        $ua->proxy($scheme, $env_proxy);
    }

    $ua;
}
      
      
###############################################################################
#
# Send files by e-mail
#
###############################################################################
sub __send_results_by_mail 
{
    my ($mail_to, @files) = @_;

    foreach my $mail_file (@files) 
    {
        system("echo report is attached | /usr/bin/mutt -s 'breport' -a $mail_file $mail_to");
    }
}

      
###############################################################################
#
# CtrlC Interrupt Handler
#
###############################################################################
sub __abort 
{
	system('stty','echo');
    print "\nDo you want to break? Press \'c\' to continue, \'e\' to exit: ";
    while (1) 
    {
        my $input = lc(getc());
        chomp ($input);
        exit(TE_ERR_BREAK) if ($input eq 'e'); 
    }
}
