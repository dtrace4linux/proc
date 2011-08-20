#! /usr/bin/perl

# $Header:$

use strict;
use warnings;

use File::Basename;
use FileHandle;
use Getopt::Long;
use IO::File;
use POSIX;

my %stacks;
my $stk_no;
my %cmdline;
my $cmd_no;

my $do_syscall = -f "/proc/self/syscall";

#######################################################################
#   Command line switches.                                            #
#######################################################################
my %opts = (
	n => 1,
	);

sub main
{
        Getopt::Long::Configure('require_order');
        Getopt::Long::Configure('no_ignore_case');
        usage() unless GetOptions(\%opts,
                'help',
		'n=s',
                );

        usage() if $opts{help};

        %stacks = ();
        $stk_no = 0;

	for (my $i = 0; $i < $opts{n}; $i++) {
	        my $dh;
	        opendir($dh, "/proc");
	        while (my $name = readdir($dh)) {
	                next if $name !~ /^[0-9]/;
	                my $str = dump_proc("/proc/$name", 1);
			print $str;
	        }
	        closedir($dh);

	        foreach my $s (keys(%cmdline)) {
	                print $s, "\n";
	        }
	        foreach my $s (keys(%stacks)) {
	                print $s, "\n";
	        }
		print "===\n";
		last if $i + 1 >= $opts{n};
		sleep(1);
	}
}
sub dump_proc
{       my $p = shift;
        my $root_dir = shift;

        my $pid = basename($p);

        my $fh;

        print "=$pid\n";
        my $str = '';
        my $str1 = read_file("$p/stat");
	if ($root_dir) {
	        $str .= join(" ", (split(" ", $str1))[
	#               0, #  readone(&pid);
	                1, #  readstr(tcomm);
	                2, #  readchar(&state);
	                3, #  readone(&ppid);
	                4, #  readone(&pgid);
	                5, #  readone(&sid);
	                6, #  readone(&tty_nr);
	                7, #  readone(&tty_pgrp);
	                8, #  readone(&flags);
	                9, #  readone(&min_flt);
	                10, #  readone(&cmin_flt);
	                11, #  readone(&maj_flt);
	                12, #  readone(&cmaj_flt);
	                13, #  readone(&utime);
	                14, #  readone(&stimev);
	                15, #  readone(&cutime);
	                16, #  readone(&cstime);
	                17, #  readone(&priority);
	                18, #  readone(&nicev);
	                19, #  readone(&num_threads);
	                20, #  readone(&it_real_value);
	                21, #  readunsigned(&start_time);
	                22, #  readone(&vsize);
	                23, #  readone(&rss);
	                24, #  readone(&rsslim);
	#               25, #  readone(&start_code);
	#               26, #  readone(&end_code);
	#               27, #  readone(&start_stack);
	                28, #  readone(&esp);
	                29, #  readone(&eip);
	                30, #  readone(&pending);
	                31, #  readone(&blocked);
	                32, #  readone(&sigign);
	                33, #  readone(&sigcatch);
	                34, #  readone(&wchan);
	#               35, #  readone(&zero1);
	#               36, #  readone(&zero2);
	#               37, #  readone(&exit_signal);
	                38, #  readone(&cpu);
	                39, #  readone(&rt_priority);
	                40, #  readone(&policy);
	                ]);
	} else {
	        $str .= join(" ", (split(" ", $str1))[
	#               0, #  readone(&pid);
	#               1, #  readstr(tcomm);
	                2, #  readchar(&state);
	#               3, #  readone(&ppid);
	#               4, #  readone(&pgid);
	#               5, #  readone(&sid);
	#               6, #  readone(&tty_nr);
	#               7, #  readone(&tty_pgrp);
	                8, #  readone(&flags);
	                9, #  readone(&min_flt);
	                10, #  readone(&cmin_flt);
	                11, #  readone(&maj_flt);
	                12, #  readone(&cmaj_flt);
	                13, #  readone(&utime);
	                14, #  readone(&stimev);
	#               15, #  readone(&cutime);
	#               16, #  readone(&cstime);
	                17, #  readone(&priority);
	                18, #  readone(&nicev);
	#               19, #  readone(&num_threads);
	                20, #  readone(&it_real_value);
	                21, #  readunsigned(&start_time);
	#               22, #  readone(&vsize);
	#               23, #  readone(&rss);
	#               24, #  readone(&rsslim);
	#               25, #  readone(&start_code);
	#               26, #  readone(&end_code);
	#               27, #  readone(&start_stack);
	                28, #  readone(&esp);
	                29, #  readone(&eip);
	                30, #  readone(&pending);
	                31, #  readone(&blocked);
	                32, #  readone(&sigign);
	                33, #  readone(&sigcatch);
	                34, #  readone(&wchan);
	#               35, #  readone(&zero1);
	#               36, #  readone(&zero2);
	#               37, #  readone(&exit_signal);
	                38, #  readone(&cpu);
	                39, #  readone(&rt_priority);
	                40, #  readone(&policy);
	                ]);
	}
        $str .= "\n";

#       $str .= read_file("$p/wchan");
        if ($root_dir) {
		$str .= "io: ";
                foreach my $s (split("\n", read_file("$p/io"))) {
			$s =~ s/^.*: //;
			$str .= "$s ";
		}
		$str .= "\n";
                my $cmdline = read_file("$p/cmdline");
                $cmdline =~ s/\0/ /g;
                if (!defined($cmdline{$cmdline})) {
                        $cmdline{$cmdline} = $cmd_no++;
                }
                $str .= "cmd$cmdline{$cmdline}\n";
        }
        if ($do_syscall) {
                $str .= read_file("$p/syscall");
        }
#       my $lnk = readlink("$p/exe") || "";

        $fh = new FileHandle("$p/stack");
        if ($fh) {
                my $s = '';
                while (<$fh>) {
                        last if /] 0xffff/;
                        $s .= $_;
                }
                if (!defined($stacks{$s})) {
                        $stacks{$s} = $stk_no++;
                }
                $str .= "stack=$stacks{$s}\n";
        }
        if ($root_dir) {
	        foreach my $p1 (glob("$p/task/[0-9]*")) {
	                next if basename($p1) == $pid;
	                $str .= dump_proc($p1, 0);
	        }
	}
	return $str;
}
sub read_file
{       my $fname = shift;

        my $fh;
        return "" if !sysopen($fh, $fname, 0);
        my $str;
        sysread($fh, $str, 2048);
       $str =~ s/18446744073709551615/-1/g;
        return $str;
}
#######################################################################
#   Print out command line usage.                                     #
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