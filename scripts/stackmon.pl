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
my %opts = (
	sleep => 5,
	);

sub main
{
	Getopt::Long::Configure('require_order');
	Getopt::Long::Configure('no_ignore_case');
	usage() unless GetOptions(\%opts,
		'a',
		'help',
		'sleep=s',
		);

	usage() if $opts{help};

	while (1) {
		print "\n";
		print strftime("%Y%m%d %H:%M:%S\n", localtime());
		foreach my $f (glob("/proc/*/stack")) {
			next if $f =~ /\/self\//;
			$f =~ /\/([0-9]*)\//;
			my $pid = $1;
			next if $pid == $$;

			my $fh = new FileHandle($f);
			next if !$fh;
			my $s = '';
			while (<$fh>) {
				$s .= $_;
			}
			if (!$opts{a}) {
				next if $s =~ /do_signal_stop/;
				next if $s =~ /poll_schedule_timeout/;
				next if $s =~ /n_tty_read/;
				next if $s =~ /do_wait/;
				next if $s =~ /pipe_wait/;
			}
			
			my $cmd = 'unknown';
			$fh = new FileHandle("/proc/$pid/cmdline");
			$cmd = <$fh> if $fh;
			$cmd =~ s/\0/ /g;
			print "$cmd:\n";
			print $s;
		}

		sleep(5);
	}
}
#######################################################################
#   Print out command line usage.				      #
#######################################################################
sub usage
{
	print <<EOF;
stackmon.pl -- list out stacks of processes.
Usage: stackmon.pl

Switches:

  -a         Show all procs - otherwise we will filter the boring ones.
  -sleep N   Pause for N seconds before refresh. Default $opts{sleep}

EOF

	exit(1);
}

main();
0;

