#!/usr/bin/perl -w
# Original by Andy Harrison,
# Rewrite by Decklin Foster,
# Available under the GPL.

package Gaim2Blt;
use strict;
use Getopt::Std;
use vars qw(%opts $in_group);

getopts('s:', \%opts);
die "usage: $0 -s 'screen name' gaim.buddy\n" unless $opts{s};

print <<"EOF";
Config {
  version 1
}
User {
  screenname "$opts{s}"
}
Buddy {
  list {
EOF

while (<>) {
    chomp;
    my ($type, $args) = split ' ', $_, 2;
    next unless $type;

    if ($type eq 'g') {
        print "    }\n" if ($in_group);
        print qq(    "$args" {\n);
        $in_group = 1;
    } elsif ($type eq 'b') {
        my ($buddy, $alias) = split /:/, $args;
        print qq(      "$buddy"\n);
    }
}

print <<"EOF";
    }
  }
}
EOF
