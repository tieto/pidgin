#!/usr/bin/perl -w

use Gaim;

%PLUGIN_INFO = (
	perl_api_version => 2,
	name             => 'Test Perl Plugin',
	version          => '1.0',
	summary          => 'Provides as a test base for the perl plugin.',
	description      => 'Provides as a test base for the perl plugin.',
	author           => 'Christian Hammond <chipx86@gnupdate.org>',
	url              => 'http://gaim.sf.net/',

	load             => "plugin_load",
	unload           => "plugin_unload"
);

sub account_away_cb {
	Gaim::debug_info("perl test plugin", "In account_away_cb\n");

	my ($account, $state, $message, $data) = @_;

	Gaim::debug_info("perl test plugin", "Account " .
	                 $account->get_username() . " went away.\n");
	Gaim::debug_info("perl test plugin", $data . "\n");
}

sub plugin_init {
	return %PLUGIN_INFO;
}

sub plugin_load {
	Gaim::debug_info("perl test plugin", "plugin_load\n");
	my $plugin = shift;

	Gaim::debug_info("perl test plugin", "Listing accounts.\n");
	foreach $account (Gaim::accounts()) {
		Gaim::debug_info("perl test plugin", $account->get_username() . "\n");
	}

	Gaim::debug_info("perl test plugin", "Listing buddy list.\n");
	foreach $group (Gaim::BuddyList::groups()) {
		Gaim::debug_info("perl test plugin",
		                 $group->get_name() . ":\n");

		foreach $buddy ($group->buddies()) {
			Gaim::debug_info("perl test plugin",
			                 "  " . $buddy->get_name() . "\n");
		}
	}

	Gaim::signal_connect(Gaim::Accounts::handle, "account-away",
	                     $plugin, \&account_away_cb, "test");
}

sub plugin_unload {
	my $plugin = shift;
}
