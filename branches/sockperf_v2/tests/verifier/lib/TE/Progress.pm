##
# @file Progress.pm
#
# @brief TE module for Progress object.
#
#

## @class
# Container for Progress object.
package TE::Progress; 

use strict;
use warnings;

# External modules

# Own modules


our $object = undef;

sub init 
{
    # Set default values
    $object->{_fh} = \*STDERR,
    $object->{_counter} = 0;
    $object->{_icons} = ['-', '\\', '|', '/'];
    $object->{_line} = undef;
    $object->{_fields} = ();
    $object->{_width} = 80;
}

sub update 
{ 	
	# Check if it is initialized
	return if (not defined($object));
	
    my $status_line = undef;
    my ($node_field, $status_field) = @_;
    my $icon_field = $object->{_icons}->[$object->{_counter} % scalar(@{$object->{_icons}})];
        
    $status_field = (defined($object->{_fields}->[2]) ? $object->{_fields}->[2] : '') if (not defined($status_field));
    $status_field = $1 if ($status_field =~ /([^\n\r]+)/); 
    $status_field = (defined($object->{_fields}->[2]) ? $object->{_fields}->[2] : '') if ($status_field =~ /^([\n\r]+)/);
    $status_field = (defined($object->{_fields}->[2]) ? $object->{_fields}->[2] : '') if ($status_field eq '');     

    $node_field = (defined($object->{_fields}->[1]) ? $object->{_fields}->[1] : '') if (not defined($node_field));
        
    $status_line = sprintf("%s(%-1.1s)%s %s[%-30.30s]%s %s%-40.40s%s\r",
                           ($^O =~ /Win/ ? "" : "\e[33m"),
                           $icon_field, 
                           ($^O =~ /Win/ ? "" : "\e[0m"),
                           ($^O =~ /Win/ ? "" : "\e[32m"),
                           $node_field,
                           ($^O =~ /Win/ ? "" : "\e[0m"),
                           ($^O =~ /Win/ ? "" : "\e[0m"),
                           $status_field,
                           ($^O =~ /Win/ ? "" : "\e[0m"));

    $object->{_counter} ++;
    $object->{_line} = $status_line;
    $object->{_fields}->[0] = $icon_field;
    $object->{_fields}->[1] = $node_field;
    $object->{_fields}->[2] = $status_field;
    
    printf ${$object->{_fh}} ("%s\r", $status_line) if defined($status_line);
}


sub clean 
{   
    # Check if it is initialized
    return if (not defined($object));
    
    # Clean status line
    my $status_line = (" " x $object->{_width}) . ("\b" x $object->{_width}) if defined($object->{_line});
        
    $object->{_line} = $status_line;
    
    printf ${$object->{_fh}} ("%s\r", $status_line) if defined($status_line);
}


sub end 
{
    # Check if it is initialized
    return if (not defined($object));
    
    # Clean status line
    my $status_line = (" " x $object->{_width}) . ("\b" x $object->{_width}) if defined($object->{_line});
    
    printf ${$object->{_fh}} ("%s\r", $status_line) if defined($status_line);
    
    $object = ();
    $object = undef;
}


1;
