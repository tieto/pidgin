#!/usr/bin/perl -w

use Gaim;
use vars qw(%PLUGIN_INFO);

%PLUGIN_INFO = (
	perl_api_version => 2,
	name             => 'Test Perl Plugin',
	version          => '1.0',
	summary          => 'Provides as a test base for the perl plugin.',
	description      => 'Provides as a test base for the perl plugin.',
	author           => 'Christian Hammond <chipx86@gnupdate.org>',
	url              => 'http://gaim.sf.net/',

	load             => "plugin_load",
	unload           => "plugin_unload",
);

sub plugin_init {
	return %PLUGIN_INFO;
}

sub plugin_load {
	my $plugin = shift;

	foreach $account (Gaim::accounts()) {
		Gaim::debug("perl test plugin", $account->get_username() . "\n");
	}
}

sub plugin_unload {
	my $plugin = shift;
}
