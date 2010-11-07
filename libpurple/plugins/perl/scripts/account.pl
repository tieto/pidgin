$MODULE_NAME = "Account Functions Test";

use Purple;

# All the information Purple gets about our nifty plugin
%PLUGIN_INFO = (
	perl_api_version => 2,
	name => "Perl: $MODULE_NAME",
	version => "0.1",
	summary => "Test plugin for the Perl interpreter.",
	description => "Implements a set of test proccedures to ensure all " .
		       "functions that work in the C API still work in the " .
		       "Perl plugin interface.  As XSUBs are added, this " .
		       "*should* be updated to test the changes.  " .
		       "Furthermore, this will function as the tutorial perl " .
		       "plugin.",
	author => "John H. Kelm <johnhkelm\@gmail.com>",
	url => "http://sourceforge.net/users/johnhkelm/",

	load => "plugin_load",
	unload => "plugin_unload"
);


	# These names must already exist
	my $USERNAME 		= "johnhkelm2";

	# We will create these on load then destroy them on unload
	my $TEST_NAME	 	= "perlTestName";
	my $PROTOCOL_ID 	= "prpl-aim";


sub plugin_init {
	return %PLUGIN_INFO;
}


# This is the sub defined in %PLUGIN_INFO to be called when the plugin is loaded
#	Note: The plugin has a reference to itself on top of the argument stack.
sub plugin_load {
	my $plugin = shift;
	print "#" x 80 . "\n\n";
	Purple::Debug::info($MODULE_NAME, "plugin_load() - Testing $MODULE_NAME Started.");
	print "\n\n";


	#################################
	#				#
	#	Purple::Account::Option	#
	#				#
	#################################

	print "Testing: Purple::Account::Option::new()...\n";
	$acc_opt  = Purple::Account::Option->new(1, "TEXT", "pref_name");
	$acc_opt2 = Purple::Account::Option->bool_new("TeXt", "MYprefName", 1);

	#################################
	#				#
	#	Purple::Account		#
	#				#
	#################################


	print "Testing: Purple::Account::new()... ";
	$account = Purple::Account->new($TEST_NAME, $PROTOCOL_ID);
	if ($account) { print "ok.\n"; } else { print "fail.\n"; }

	print "Testing: Purple::Accounts::add()...";
	Purple::Accounts::add($account);
	print "pending find...\n";

	print "Testing: Purple::Accounts::find()...";
	$account = Purple::Accounts::find($TEST_NAME, $PROTOCOL_ID);
	if ($account) { print "ok.\n"; } else { print "fail.\n"; }

	print "Testing: Purple::Account::get_username()... ";
	$user_name = $account->get_username();
	if ($user_name) {
		print "Success: $user_name.\n";
	} else {
		print "Failed!\n";
	}

	print "Testing: Purple::Account::is_connected()... ";
	if ($account->is_connected()) {
		print " Connected.\n";
	} else {
		print " Disconnected.\n";
	}

	print "Testing: Purple::Accounts::get_active_status()... ";
	if ($account->get_active_status()) {
		print "Okay.\n";
	} else {
		print "Failed!\n";
	}

	$account = Purple::Accounts::find($USERNAME, $PROTOCOL_ID);
	print "Testing: Purple::Accounts::connect()...pending...\n";

	$account->set_status("available", TRUE);
	$account->connect();

	print "\n\n";
	Purple::Debug::info($MODULE_NAME, "plugin_load() - Testing $MODULE_NAME Completed.\n");
	print "\n\n" . "#" x 80 . "\n\n";
}

sub plugin_unload {
	my $plugin = shift;

	print "#" x 80 . "\n\n";
	Purple::Debug::info($MODULE_NAME, "plugin_unload() - Testing $MODULE_NAME Started.\n");
	print "\n\n";

	#########  TEST CODE HERE  ##########

	print "\n\n";
	Purple::Debug::info($MODULE_NAME, "plugin_unload() - Testing $MODULE_NAME Completed.\n");
	print "\n\n" . "#" x 80 . "\n\n";
}

