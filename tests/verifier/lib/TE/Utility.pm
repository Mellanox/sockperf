##
# @file Utility.pm
#
# @brief TE module for different auxiliary functions.
#
#

## @class
# Container for auxiliary functions.
package TE::Utility;

use strict;
use warnings;
use vars qw(@EXPORT);
use base qw(Exporter);
@EXPORT = qw
(
te_def_pre_proc
te_def_server_proc
te_def_client_proc
te_def_result_proc
te_def_post_proc
te_create_feed_file
);


# External modules
use Sys::Hostname;

# Own modules
use TE::Common;
use TE::Funclet;


# Define reference to the common global structure
my $_options = $TE::Common::conf->{options};
my $_common = $TE::Common::conf->{common};
my $_current = $TE::Common::conf->{current};

my @passwords;

sub add_password
{
    my ($pw) = @_;
    push @passwords, $pw;
}


sub get_ip
{
    my $host = ( @_ > 0 ? "@_" : '');

    my @raw_ip = gethostbyname($host);
    my @ip = unpack("C4", $raw_ip[4]);
    my $dotted_ip = join(".",@ip);

    return $dotted_ip;
}

sub clear_array
{
	return grep(defined, @_);
}


sub force_error
{
    my $code = get_error();

    if ( $code != TE_ERR_NONE )
    {   
        # Clean status line to prevent log corruption
        TE::Progress::clean();
	    
        # Do die to use capacity of eval operation and switch to process next target
        # pressure die message to display internal errors
        die "\n";
    }
}


sub outlog 
{
    my $str = '';
    my $file_log = $_common->{flog};
    my $file_scr = $_common->{fdump};

    my @arg = ();
    my %args = (scalar(@_) % 2 ? (@_,undef) : @_) ;
    my $argv = join(' , ', @_);

    my $opt_prefix = '';
    my $opt_terminal = 0;
    my $opt_file = 0;
    if (exists($args{'-prefix'}))
    {
        $opt_prefix = $args{'-prefix'};
        $argv =~ s/\-prefix\s,\s$opt_prefix\s?\,?\s?//;
    }
    if (exists($args{'-terminal'}))
    {
        $opt_terminal = $args{'-terminal'};
        $argv =~ s/\-terminal\s,\s$opt_terminal\s?\,?\s?//;
    }
    if (exists($args{'-file'}))
    {
        $opt_file = $args{'-file'};
        $argv =~ s/\-file\s,\s$opt_file\s?\,?\s?//;
    }
    if (!scalar(@arg))
    {
        @arg = split(' , ', $argv);
    }
    
    $str = "@arg";
               
    if ($_options->{format_log})
    {
        my $temp = ($_options->{format_log} == 1 ? $_current->{target} : $TE::Common::alias );
        $str = "${temp}: ${opt_prefix}$str";
        $str =~ s/[\n\r]$//;
        $str =~ s/[\n\r]/\n${temp}\: ${opt_prefix}/g;
        $str.="\n";
    }
	
    # Clean status line to prevent log corruption
    TE::Progress::clean();
	
    # Save message into log-file
    if (defined($file_log) && $opt_file) 
    {
        my $line = $str;
        if (scalar(@passwords)) 
        {
            foreach my $pw (@passwords) 
            {
                $line =~ s/$pw/topsecret/g;
            }
        }
        print $file_log $line;
    }

    # Save message into dump-file
    if (defined($file_scr) && $opt_terminal) 
    {
        my $line = $str;
        if (scalar(@passwords)) 
        {
            foreach my $pw (@passwords) 
            {
                $line =~ s/$pw/topsecret/g;
            }
        }
        print $file_scr $line;
    }
    
    # Display message on terminal
    print $str if (($_options->{silent} == 0)  && $opt_terminal);

    # Restore status line
    TE::Progress::update(undef, "@arg");
}


sub fatal 
{
    my $out_level = ($_options->{out_level} >= 0 ? 1 : 0);
    my $log_level = ($_options->{log_level} >= 0 ? 1 : 0);
	
    if ($out_level || $log_level)
    {
        my $str = "@_";
       
        outlog(-prefix=>'fatal: ', -terminal=>$out_level, -file=>$log_level, $str);

        # Clean status line in case fatal error only
        TE::Progress::clean();
        
        # default fatal code
        my $rc = TE_ERR_FATAL;
        
        # user defined code
        $rc = $_[1] if (defined($_[1]));
        
        # last error code
        $rc = get_error() if (get_error());
               
        exit($rc);
    }
}


sub error 
{
    my $out_level = ($_options->{out_level} >= 1 ? 1 : 0);
    my $log_level = ($_options->{log_level} >= 1 ? 1 : 0);
    
    if ($out_level || $log_level)
    {
    	my $code = get_error();
        my $str = ( @_ == 0 ? get_error($code) : "@_" );
    
        outlog(-prefix=>'error: ', -terminal=>$out_level, -file=>$log_level, $str);
    }
}


sub trace 
{
    my $out_level = ($_options->{out_level} >= 2 ? 1 : 0);
    my $log_level = ($_options->{log_level} >= 2 ? 1 : 0);
    
    if ($out_level || $log_level)
    {
        my $str = "@_";
    
        outlog(-prefix=>'trace: ', -terminal=>$out_level, -file=>$log_level, $str);
    }
}


sub dump 
{
    my $out_level = ($_options->{out_level} >= 3 ? 1 : 0);
    my $log_level = ($_options->{log_level} >= 3 ? 1 : 0);
    
    if ($out_level || $log_level)
    {
        my $str = "@_";
        
        outlog(-prefix=>'dump: ', -terminal=>$out_level, -file=>$log_level, $str);
    }
}


sub debug 
{
    my $out_level = ($_options->{out_level} >= 4 ? 1 : 0);
    my $log_level = ($_options->{log_level} >= 4 ? 1 : 0);
    
    if ($out_level || $log_level)
    {
        my $str = "@_";
        
        outlog(-prefix=>'debug: ', -terminal=>$out_level, -file=>$log_level, $str);
    }
}


sub te_create_playback_file
{
    my @arg = ();
    my %args = (scalar(@_) % 2 ? (@_,undef) : @_) ;
    my $argv = join(' , ', @_);

    my $opt_base = 1.000000;
    my $opt_size = 0;
    my $opt_step = 0.000005;
    my $opt_msg = 12;
    if (exists($args{'-base'}))
    {
        $opt_base = $args{'-base'};
        $argv =~ s/\-base\s,\s$opt_base\s?\,?\s?//;
    }
    if (exists($args{'-size'}))
    {
        $opt_size = $args{'-size'};
        $argv =~ s/\-size\s,\s$opt_size\s?\,?\s?//;
    }
    if (exists($args{'-step'}))
    {
        $opt_step = $args{'-step'};
        $argv =~ s/\-step\s,\s$opt_step\s?\,?\s?//;
    }
    if (exists($args{'-msg'}))
    {
        $opt_msg = $args{'-msg'};
        $argv =~ s/\-msg\s,\s$opt_msg\s?\,?\s?//;
    }
    if (!scalar(@arg))
    {
        @arg = split(' , ', $argv);
    }

    TE::Utility::debug((caller(0))[3] . ":\n{\n" . join("\n", @_) . "\n}\n");
    TE::Utility::debug((caller(0))[3] . ":\nBase: $opt_base Size: $opt_size Step: $opt_step Msg: $opt_msg\n");

    my $line = '';
    my $fpb = undef;
    my $pb_file = File::Spec->rel2abs('.');
    $pb_file .= '/' . ${TE::Common::alias} . '_pb_file_' . $$;
    
    # Create feed file
    open($fpb, "> $pb_file") || 
        TE::Utility::fatal("Can't open $pb_file: $!\n") if defined($pb_file);
    
    TE::Utility::dump("PLAYBACK FILE : $pb_file\n");
    TE::Utility::dump("-" x 20 . "\n");
    for (my $i = 0; $i < $opt_size; $i++)
    {
        $line = "";
        $line .= sprintf("%0.9f, %d\n", $opt_base + $i * $opt_step, $opt_msg);
        
        print $fpb $line;
        TE::Utility::dump($line);
    }
    TE::Utility::dump("-" x 20 . "\n");
    
    close($fpb);
    
    return $pb_file;
}


sub te_create_feed_file
{
    my @arg = ();
    my %args = (scalar(@_) % 2 ? (@_,undef) : @_) ;
    my $argv = join(' , ', @_);

    my $opt_protocol = 'UDP';
    my $opt_size = 0;
    my $opt_addr = get_ip($_current->{target});
    my $opt_port = 7000;
    my $opt_step = 1; # step of port number
    if (exists($args{'-protocol'}))
    {
        $opt_protocol = $args{'-protocol'};
        $argv =~ s/\-protocol\s,\s$opt_protocol\s?\,?\s?//;
    }
    if (exists($args{'-size'}))
    {
        $opt_size = $args{'-size'};
        $argv =~ s/\-size\s,\s$opt_size\s?\,?\s?//;
    }
    if (exists($args{'-addr'}))
    {
        $opt_addr = get_ip($args{'-addr'});
        $argv =~ s/\-addr\s,\s$opt_addr\s?\,?\s?//;
    }
    if (exists($args{'-port'}))
    {
        $opt_port = $args{'-port'};
        $argv =~ s/\-port\s,\s$opt_port\s?\,?\s?//;
    }
    if (!scalar(@arg))
    {
        @arg = split(' , ', $argv);
    }

    TE::Utility::debug((caller(0))[3] . ":\n{\n" . join("\n", @_) . "\n}\n");
    TE::Utility::debug((caller(0))[3] . ":\nProtocol: $opt_protocol Addr: $opt_addr Port: $opt_port Size: $opt_size\n");

    my $line = '';
    my $ffeed = undef;
    my $feed_file = File::Spec->rel2abs('.');
    $feed_file .= '/' . ${TE::Common::alias} . '_feed_file_' . $$;
	
    # Create feed file
    open($ffeed, "> $feed_file") || 
        TE::Utility::fatal("Can't open $feed_file: $!\n") if defined($feed_file);

	my ($group1,$group2,$group3,$group4) = ( $opt_addr =~ /^(\d+).(\d+).(\d+).(\d+)$/i);
    $opt_step = 0 if (!$opt_port);
    $opt_port = 17000 if (!$opt_port);
	my $port = $opt_port;
	
    TE::Utility::dump("FEED FILE : $feed_file\n");
    TE::Utility::dump("-" x 20 . "\n");
	for (my $i = $opt_port; $i < ($opt_port + $opt_size); $i++)
	{
		$line = "";
        $line .= 'T:' if ($opt_protocol eq 'TCP');
        $line .= "$group1";
        $line .= ".$group2";
        if ($group1 == 224 && $group4 > 254)
        {
        	$group3++;
        	$group4 = 3;
        }
        $line .= ".$group3";
        if ($group1 == 224)
        {
            $group4++;
        }
        $line .= ".$group4";

        $line .= ":$port\n";
        $port+=$opt_step;
		
        print $ffeed $line;
        TE::Utility::dump($line);
	}
    TE::Utility::dump("-" x 20 . "\n");
	
	close($ffeed);
	
	return $feed_file;
}


sub te_def_server_proc
{
    my $status = 0;
    my $result = undef;
    @_ = TE::Utility::clear_array(@_);

    if (@_ > 0)
    {
        my $cmd = "$_common->{app_path}\/$_common->{app} "; 
        $cmd .= join(" ", @_);
        $cmd =~ s/TARGET\(\)/$_current->{target}/;
        if ($cmd =~ /FEED\(.*\)/)
        {
            my ($group1,$group2,$group3,$group4) = ( $cmd =~ /FEED\((TCP|UDP):(.*):(\d+):(\d+)\)/);
        	my $feed_file = te_create_feed_file(-protocol=>$group1, -addr=>$group2, -port=>$group3, -size=>$group4);
            $cmd =~ s/FEED\(.*\)/$feed_file/;
        }
           
        $result = TE::Funclet::execute($cmd);
    }
    
    return [$status, $result];
}


sub te_def_client_proc
{
    my $status = 0;
    my $result = undef;
    @_ = TE::Utility::clear_array(@_);

    if (@_ > 0)
    {
        my $cmd = "$_common->{app_path}\/$_common->{app} "; 
        $cmd .= join(" ", @_);
        $cmd =~ s/TARGET\(\)/$_current->{target}/;
        if ($cmd =~ /FEED\(.*\)/)
        {
            my ($group1,$group2,$group3,$group4) = ( $cmd =~ /FEED\((TCP|UDP):(.*):(\d+):(\d+)\)/);
            my $feed_file = te_create_feed_file(-protocol=>$group1, -addr=>$group2, -port=>$group3, -size=>$group4);
            $cmd =~ s/FEED\(.*\)/$feed_file/;
        }
        if ($cmd =~ /PLAYBACK\(.*\)/)
        {
            my ($group1,$group2,$group3,$group4) = ( $cmd =~ /PLAYBACK\(([\d\.]+):([\d\.]+):(\d+):(\d+)\)/);
            my $pb_file = te_create_playback_file(-base=>$group1, -step=>$group2, -msg=>$group3, -size=>$group4);
            $cmd =~ s/PLAYBACK\(.*\)/$pb_file/;
        }
           
        $result = TE::Funclet::shell($cmd);
    }
    
    return ($status, $result);
}


sub te_def_result_proc
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
        
        my $server_output = (exists($arg->{server}->{output}) && defined($arg->{server}->{output}) ? $arg->{server}->{output} : '');
        my $client_output = (exists($arg->{client}->{output}) && defined($arg->{client}->{output}) ? $arg->{client}->{output} : '');


        if ( $status == 0 &&
             $server_output ne '' &&
             (exists($arg->{server}->{success}) && defined($arg->{server}->{success})) )
        {
            my @match_list = @{$arg->{server}->{success}};
            foreach my $match (@match_list)
            {
                if ( $server_output !~ /$match/)
                {
                    $status = 1;
                    last;
                }
            }
        }

        if ( $status == 0 &&
             $server_output ne '' &&
             (exists($arg->{server}->{failure}) && defined($arg->{server}->{failure})) )
        {
            my @match_list = @{$arg->{server}->{failure}};
            foreach my $match (@match_list)
            {
                if ( $server_output =~ /$match/)
                {
                    $status = 1;
                    last;
                }
            }
        }

        if ( $status == 0 &&
             $client_output ne '' &&
             (exists($arg->{client}->{success}) && defined($arg->{client}->{success})) )
        {
            my @match_list = @{$arg->{client}->{success}};
            foreach my $match (@match_list)
            {
                if ( $client_output !~ /$match/)
                {
                    $status = 1;
                    last;
                }
            }
        }

        if ( $status == 0 &&
             $client_output ne '' &&
             (exists($arg->{client}->{failure}) && defined($arg->{client}->{failure})) )
        {
            my @match_list = @{$arg->{client}->{failure}};
            foreach my $match (@match_list)
            {
                if ( $client_output =~ /$match/)
                {
                    $status = 1;
                    last;
                }
            }
        }
    }
    
    return ($status);
}


sub te_def_pre_proc
{
    TE::Funclet::shell("killall $_common->{app}");
    TE::Funclet::execute("killall $_common->{app}");
}


sub te_def_post_proc
{
    TE::Funclet::shell("pkill -SIGINT $_common->{app}");
    TE::Funclet::execute("pkill -SIGINT $_common->{app}");
}


1;
