#!/usr/bin/perl -w
# licq2gaim.pl
#
# Arturo Cisneros, Jr <acjr@hal-pc.org>
# GPL'd

use strict;

my $DIR = "$ENV{HOME}/.licq";
my $GAIM = "$ENV{HOME}/.gaim";
my (@UINS, %USERS) = ();
my $OWNER = "";


get_uins();

foreach my $uin (@UINS) {
	$USERS{$uin} = get_alias($uin);
}

get_owner();
write_list();


sub get_uins {

	opendir(DIR, "$DIR/users") or die "Couldn't open dir $DIR/users/: $!";
	@UINS = grep !/^\./, readdir DIR;
	closedir(DIR);
}

sub get_owner {

	my @foo = ();

	open(FILE, "<$DIR/owner.uin") or die "Couldn't open file $DIR/owner.uin $!";
	while(<FILE>) {
		next unless /^Uin/;
		@foo = split;
		last;
	}
	close(FILE);

	$OWNER = $foo[2];
}

sub get_alias {

	my @foo = ();

	open(FILE, "<$DIR/users/$_[0]") or die "Couldn't open $DIR/users/$_[0]: $!";
	while(<FILE>) {
		next unless /^Alias/;
		@foo = split / /, $_, 3;
		last;
	}
	close(FILE);

	return $foo[2];
}

sub write_list {

	# Backup Original
	if( -e "$GAIM/$OWNER.3.blist") {
		rename("$GAIM/$OWNER.3.blist","$GAIM/$OWNER.3.bak");
	}

	# Write new file
	open(FILE, ">$GAIM/$OWNER.3.blist") or die "Couldn't open file for writing: $!";
	print FILE "m 1\n";
	print FILE "g ICQBuddies\n";
	while(my($key, $value) = each %USERS) {
		$key =~ s/\.uin$//;
		print FILE "b $key:$value";
	}
	close(FILE);
}
