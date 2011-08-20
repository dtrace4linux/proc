#! /usr/bin/perl

# $Header:$

use strict;
use warnings;

use File::Basename;
use FileHandle;
use Getopt::Long;
use IO::File;
use POSIX;

use lib "/ms/user/p/pfox/ctw/samples/";
use CTW;
use CTWGraph;

my %graph;

my $ctw;
my $win_width;
my $win_height;

#######################################################################
#   Command line switches.                                            #
#######################################################################
my %opts;

sub main
{
        Getopt::Long::Configure('require_order');
        Getopt::Long::Configure('no_ignore_case');
        usage() unless GetOptions(\%opts,
                'help',
                );

        usage() if ($opts{help});

        $| = 1;

        if (@ARGV) {
                mon_hosts();
        }

        $ctw = new CTW(autoflush => 1);
        print "\033[2J\033[H";

        $SIG{INT} = sub {
                system("stty echo");
                $ctw->ClearChainRefresh();
                $ctw->Refresh();
                _exit(0);
                };

        my $fname = "/tmp/rup";
        my $mtime;
        system("stty -echo");
        while (1) {
                my $mtime1 = (stat($fname))[9];

                ###############################################
                #   Handle window resizing forcing a redraw.  #
                ###############################################
                my ($win_width1, $win_height1) = $ctw->WindowSize();
                if ($win_width ne $win_width1 ||
                    $win_height1 ne $win_height) {
                        print "\033[2J";
                        $win_width = $win_width1;
                        $win_height = $win_height1;
                        $mtime = 0;
                }
                if ($mtime && $mtime == $mtime1) {
                        sleep(1);
                        next;
                }

                $mtime = $mtime1;

                %graph = ();

                my $fh = new FileHandle($fname);
                if (!$fh) {
                        sleep(1);
                        next;
                }
                while (<$fh>) {
                        chomp;
                        my ($host) = (split(" ", $_))[1];
                        $_ =~ s/^.*load average: //;
                        $_ =~ s/,//g;
                        my ($load1, $load5, $load15) = split(" ");
                        push @{$graph{$host}}, $load1;
                }

                display();
        }
}

sub display
{
        my $y = 10;
        $ctw->ClearChain();
        foreach my $h (sort(keys(%graph))) {
                my @arr = @{$graph{$h}};

                $ctw->SetFont(name => "7x13bold");
                $h =~ s/\..*//;
                $ctw->DrawString(x => 12, y => $y + 13, text => $h);
                if (@arr > $win_width - 150) {
                        @arr = @arr[$#arr-($win_width - 150)..$#arr];
                }
                my $g = new CTWGraph(
                        ctw => $ctw,
                        box => 1,
                        width => $win_width - 140,
                        height => 15,
                        border_right => 100,
#                       legend => $h,
#                       legendflags => "yminmax",
                        );
                $g->{opts}{box} = 1;
                my $x = 0;
                foreach my $v (@arr) {
                        $x += 10;
                        $g->AddXY($x, $v);
                }
                $g->Set(x => 100);
                $g->Set(y => $y);
                $y += 35;
                $g->Plot();
                my $t = strftime("%H:%M", localtime());
                my $t0 = strftime("%H:%M", localtime(time() - @arr * 3));
                draw_legend($g, $t0, $t);
                $ctw->Refresh();
        }
}
sub draw_legend
{       my $g = shift;
        my $t0 = shift;
        my $t = shift;

        my %opts = %{$g->{opts}};

        $ctw->SetFont(name => "6x10");
        $ctw->SetBackground(color => 0);
        $ctw->DrawImageString(x => $opts{x}, 
                y => $opts{y} + $opts{height} + 12, text => $t0);
        $ctw->DrawImageString(x => $opts{x} + $opts{width} - 5 * 6, 
                y => $opts{y} + $opts{height} + 12, text => $t);

        $ctw->DrawImageString(x => $opts{x} + $opts{width} + 10, 
                y => $opts{y} + 5, text => $g->{max_y});
        $ctw->DrawImageString(x => $opts{x} + $opts{width} + 10, 
                y => $opts{y} + $opts{height} + 5, text => $g->{min_y});


}

sub mon_hosts
{
        my $fh1 = new FileHandle(">>/tmp/rup");

        while (1) {
                my $cmd = "rup " . join(" ", @ARGV);
                my $t = time();
                my $fh = new FileHandle("$cmd |");
                while (<$fh>) {
                        print $fh1 "$t $_";
                }
                $fh->close();
                $fh1->flush();

                sleep(3);
        }
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
0;