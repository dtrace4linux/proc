#! /usr/bin/perl

# $Header:$

use strict;
use warnings;

use File::Basename;
use FileHandle;
use Getopt::Long;
use IO::File;
use POSIX;

my $first = 1;
my %files;

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
		'link=s',
		);

	usage() if $opts{help};

	while (1) {
		foreach my $d (@ARGV) {
			do_dir($d);
		}
		$first = 0;
		sleep(1);
	}
}
sub do_dir
{	my $d = shift;

	$files{$_} = 0 foreach keys(%files);
	foreach my $f (glob("$d/*")) {
		if (!defined($files{$f}) && !$first) {
			print time_string() . "New: $f\n";
			if ($opts{link}) {
				if (!link($f, $opts{link} . "/" . basename($f))) {
					print "Failed to link - $!\n";
				}	
			}
		}
		$files{$f} = 1;
	}

	foreach my $f (sort(keys(%files))) {
		if ($files{$f} == 0 && ! -f $files{$f}) {
			print time_string() . "Deleted: $f\n";
			delete($files{$f});
		}
	}
}
sub time_string
{
	return strftime("%Y%m%d %H:%M:%S ", localtime());
}
#######################################################################
#   Print out command line usage.				      #
#######################################################################
sub usage
{
	print <<EOF;
filemon.pl - monitor for new files.
Usage: filemon.pl [-link <dir>] dir1 [dir2 ...]

Switches:

EOF

	exit(1);
}

main();
0;

