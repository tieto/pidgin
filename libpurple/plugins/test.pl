use Purple;

%PLUGIN_INFO = (
	perl_api_version => 2,
	name             => 'Test Perl Plugin',
	version          => '1.0',
	summary          => 'Provides as a test base for the perl plugin.',
	description      => 'Provides as a test base for the perl plugin.',
	author           => 'Etan Reisner <deryni\@pidgin.im>',
	url              => 'https://pidgin.im',

	load             => "plugin_load"
);

sub plugin_init {
	return %PLUGIN_INFO;
}

sub account_status_cb {
	my ($account, $old, $new, $data) = @_;

	Purple::Debug::info("perl test plugin", "In account_status_cb\n");

	Purple::Debug::info("perl test plugin", "Account " .
	                    $account->get_username() . " changed status.\n");
	Purple::Debug::info("perl test plugin", $data . "\n");
}

sub plugin_load {
	my $plugin = shift;

	Purple::Debug::info("perl test plugin", "plugin_load\n");

	Purple::Debug::info("perl test plugin", "Listing accounts.\n");
	foreach $account (Purple::Accounts::get_all()) {
		Purple::Debug::info("perl test plugin", $account->get_username() . "\n");
	}

	Purple::Signal::connect(Purple::Accounts::get_handle(),
	                        "account-status-changed", $plugin,
	                        \&account_status_cb, "test");
}
