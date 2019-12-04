#! /usr/bin/perl

# $Header:$
# Author: Paul Fox
# Date: February 2011

use strict;
use warnings;

use File::Basename;
use FileHandle;
use Getopt::Long;
use IO::File;
use POSIX;
use IO::Socket;
use IO::Select;

#######################################################################
#   Command line switches.                                            #
#######################################################################
my %opts = (
        rate => 20,
        );

sub main
{
        Getopt::Long::Configure('no_ignore_case');
        usage() unless GetOptions(\%opts,
		'exit',
                'help',
                'rate=s',
                'size',
                );

        usage() if ($opts{help});

        time_string();
        $| = 1;

        my %old_pids;
        my %pids;
        my %keep;
        my %mon_size;
        my $first = 1;

        my $fh;
        printf time_string() . "             %8s %5s %5s %s\n", 
                "User", "PID", "PPID", "Command";
        while (1) {
		if ($opts{rate}) {
	                select(undef, undef, undef, $opts{rate} / 1000);
		}
                %old_pids = %pids;
                %pids = ();
                my $dir;
                opendir($dir, "/proc");
                foreach my $pid (readdir($dir)) {
                        next if $pid !~ /^\d/;
                        $pid = "/proc/$pid";

                        my $p = basename($pid);
                        if (defined($old_pids{$p})) {
                                $pids{$p} = $old_pids{$p};
                                next;
                        }

                        chdir($pid);
                        my $uid = "";
                        $uid = (stat($pid))[4];
                        my $cmdline = "(noname)";
                        $fh = new FileHandle("$pid/cmdline");
                        if ($fh) {
                                $cmdline = <$fh>;
                                chomp($cmdline) if $cmdline;
                                $uid = (stat($pid))[4];
                                $cmdline ||= "(noname2)";
                                $cmdline =~ s/\0/ /g;
                                $cmdline =~ s/ +$//;
                        }
                        next if $old_pids{$p} && $old_pids{$p} eq $cmdline;

                        if ($opts{size}) {
                                $mon_size{$pid} = 1;
                        }

                        my $ppid = "??";
                        my $fh1 = new FileHandle("$pid/status");
                        if ($fh1) {
                                while (<$fh1>) {
                                        chomp;
                                        if (/^PPid:\t(\d+)/) {
                                                $ppid = $1;
                                                last;
                                        }
                                }
                        }

                        $pids{$p}{cmd} = $cmdline;
			if ($first) {
				$pids{$p}{time} = time();
				$pids{$p}{user} = $uid;
				next;
			}

                        my $user;
                        if (!defined($old_pids{$p})) {
				$pids{$p}{time} = time();
                                $user = getpwuid($uid) if defined($uid);
                                $user ||= "(nouid)";
                                printf time_string() . "New : %8s %5d %5s %s\n", 
                                        $user, $p, $ppid, $cmdline;
				$pids{$p}{user} = $user;
                        } elsif ($old_pids{$p} ne $cmdline) {
                                $user = getpwuid($uid) if defined($uid);
                                $user ||= "(nouid)";
                                printf time_string() . "New*: %8s %5d %5s %s\n", 
                                        $user, $p, $ppid, $cmdline;
                        }

                        ###############################################
                        #   Track proc sizes.                         #
                        ###############################################
                        if ($opts{size}) {
                                my $vss = "-";
                                my $rss = "-";
                                foreach my $pid (sort(keys(%mon_size))) {
                                        $fh = new FileHandle("$pid/status");
                                        if (!$fh) {
                                                delete($mon_size{$pid});
                                                next;
                                        }
                                        while (<$fh>) {
                                                chomp;
                                                if (/^VmSize:\t(.*) kB/) {
                                                        $vss = $1;
                                                        next;
                                                }
                                                if (/^VmRSS:\t(.*) kB/) {
                                                        $rss = $1;
                                                        last;
                                                }
                                        }
                                }
                                if ($opts{size} && "$rss$vss" ne '--') {
                                        print "  $pid: VSS: $vss RSS: $rss\n";
                                }
                        }

			###############################################
			#   Find terminated procs.		      #
			###############################################
			if ($opts{exit}) {
				foreach my $p (keys(%old_pids)) {
					next if defined($pids{$p});
					my $t = time() - $old_pids{$p}{time};
					if ($t > 3600) {
						$t = sprintf("%dm%ds", $t / 60, $t % 60);
					} else {
						$t = $t . "s";
					}
					printf time_string() . "Term: %8s %s %s %s\n",
						$old_pids{$p}{user},
						$p,
						"[$t]",
						$old_pids{$p}{cmd};
				}
			}
                }
                $first = 0;
        }
}

sub time_string
{
	return strftime("%H:%M:%S ", localtime());
}
#######################################################################
#   Print out command line usage.                                     #
#######################################################################
sub usage
{
        print <<EOF;
pidmon.pl -- monitor for new processes being created
Usage: pidmon.pl


Description:
        
        This script keeps polling the /proc filesystem to look for new processes
        and prints out information on the new process. This is designed to find
        short lived processes.

        Some processes may appear so fast and die so quickly that key attributes
        (like owner or cmdline) may not be available.

        The polling rate defaults to $opts{rate}mSec, and pidmon can use a lot
        of cpu, so be careful in leaving it running for long periods of time.

Switches:

  -exit        Show process termination
  -rate NN     Number of milliseconds between polling.
  -size        Show process size.

EOF
        exit(1);
}

main();
0;