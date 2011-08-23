#! /usr/bin/perl

# $Header:$

use strict;
use warnings;

use File::Basename;
use FileHandle;
use Getopt::Long;
use IO::File;
use POSIX;

#######################################################################
#   Command line switches.					      #
#######################################################################
my %opts;

sub main
{
	Getopt::Long::Configure('require_order');
	Getopt::Long::Configure('no_ignore_case');
	usage() unless GetOptions(\%opts,
		'help',
		);

	usage() if $opts{help};
	while (1) { }
	if (@ARGV == 0) {
		while (1) {
			my $cmd = strftime("p.pl %H:%M:%S", localtime());
			print "$cmd\n";
			system($cmd);
		}
	} else {
		my $t = time();
		while (time() == $t) {}
		$t = time();
		while (time() == $t) {}
	}
}
#######################################################################
#   Print out command line usage.				      #
#######################################################################
sub usage
{
	print <<EOF;
Some help...
Usage:

Switches:

EOF

	exit(1);
}

main();
0;

