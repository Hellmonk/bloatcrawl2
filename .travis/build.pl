#!/usr/bin/env perl
use strict;
use warnings;

chdir "crawl-ref/source"
    or die "couldn't chdir: $!";

open my $fh, '>', "util/release_ver"
    or die "couldn't open util/release_ver: $!";
$fh->print("v0.0-a0");
$fh->close;

$ENV{TRAVIS} = 1;

# can't set these in .travis.yml because env vars are set before compiler
# selection
$ENV{FORCE_CC} = $ENV{CC};
$ENV{FORCE_CXX} = $ENV{CXX};

if ($ENV{MONSTER}) {
    try("make -j2 monster");
    # pick something more exciting here possibly?
    # or use a variety to touch all the code paths?
    try("util/monster/monster Orb Guardian");
}
elsif ($ENV{FULLDEBUG}) {
    try("make -j2 debug");
}
else {
    try("make -j2");
}

if (!$ENV{TILES}) {
    if ($ENV{FULLDEBUG}) {
        try("make test");
    }
    elsif (!$ENV{MONSTER}) {
        try("make nondebugtest");
    }
}

sub try {
    my ($cmd) = @_;
    print "$cmd\n";
    if (system $cmd) {
        if ($? == -1) {
            print "failed to execute '$cmd': $!\n";
            error(1);
        }
        elsif ($? & 127) {
            printf "'$cmd' died with signal %d\n", ($? & 127);
            error(1);
        }
        elsif ($?) {
            my $exit = $? >> 8;
            printf "'$cmd' returned exit value %d\n", $exit;
            error($exit);
        }
    }
}

sub error {
    my ($exitcode) = @_;
    system "cat morgue/crash-*.txt";
    printf "mon-spell.out:\n";
    system "cat mon-spell.out";
    printf "job-data.out:\n";
    system "cat job-data.out";
    exit $exitcode;
}
