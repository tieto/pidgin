$MODULE_NAME = "Account Functions Test";

use Gaim;

# All the information Gaim gets about our nifty plugin
%PLUGIN_INFO = ( 
	perl_api_version => 2, 
	name => " Perl: $MODULE_NAME", 
	version => "0.1", 
	summary => "Test plugin for the Perl interpreter.", 
	description => "Implements a set of test proccedures to ensure all functions that work in the C API still work in the Perl plugin interface.  As XSUBs are added, this *should* be updated to test the changes.  Furthermore, this will function as the tutorial perl plugin.", 
	author => "John H. Kelm <johnhkelm\@gmail.com>", 
	url => "http://sourceforge.net/users/johnhkelm/", 
	
	load => "plugin_load", 
	unload => "plugin_unload" 
); 


	# These names must already exist
	my $GROUP		= "UIUC Buddies";
	my $USERNAME 		= "johnhkelm2";
	
	# We will create these on load then destroy them on unload
	my $TEST_GROUP		= "perlTestGroup";
	my $TEST_NAME	 	= "perlTestName";
	my $TEST_ALIAS	 	= "perlTestAlias";
	my $PROTOCOL_ID 	= "prpl-oscar";


sub plugin_init { 
	return %PLUGIN_INFO; 
} 


# This is the sub defined in %PLUGIN_INFO to be called when the plugin is loaded
#	Note: The plugin has a reference to itself on top of the argument stack.
sub plugin_load { 
	my $plugin = shift; 
	print "#" x 80 . "\n\n";
	Gaim::debug_info("plugin_load()", "Testing $MODULE_NAME Started.");
	print "\n\n";
	

	#################################
	#				#
	#	Gaim::Account::Option	#
	#				#
	#################################

	print "Testing: Gaim::Account::Option::new...\n";
	$account_opt = Gaim::Account::Option::new(1, "TEXT", "pref_name"); 
	Gaim::Account::Option::bool_new("TeXt", "MYprefName", 1);
	
	#################################
	#				#
	#	Gaim::Account		#
	#				#
	#################################


	print "Testing: Gaim::Account::new()...";
	$account = Gaim::Account::new($TEST_NAME, $PROTOCOL_ID);
	if ($account) { print "ok.\n"; } else { print "fail.\n"; }

	print "Testing: Gaim::Account::add()...";
	Gaim::Accounts::add($account);
		print "pending find...\n"; 

	print "Testing: Gaim::Accounts::find()...";
	$account = Gaim::Accounts::find($TEST_NAME, $PROTOCOL_ID);
	if ($account) { print "ok.\n"; } else { print "fail.\n"; }
	
	print "Testing: Gaim::Account::get_username()...";
	$user_name = Gaim::Account::get_username($account);
	if ($user_name) { print $user_name . "...ok.\n"; } else { print "fail.\n"; }


	print "Testing: Gaim::Account::is_connected()";
	$user_connected = Gaim::Account::is_connected($account);
	if (!($user_connected)) { print "...not connected...ok..\n"; } else { print "...connected...ok.\n"; }


	print "Testing: Gaim::Accounts::get_active_status()...";
	$status = Gaim::Account::get_active_status($account);
	if ($status) { print "ok.\n"; } else { print "fail.\n"; }

	$account = Gaim::Accounts::find($USERNAME, $PROTOCOL_ID);
	print "Testing: Gaim::Accounts::connect()...pending...\n";
	
	Gaim::Account::set_status($account, "available", TRUE);
	Gaim::Account::connect($account);

	print "\n\n";
	Gaim::debug_info("plugin_load()", "Testing $MODULE_NAME Completed.");
	print "\n\n" . "#" x 80 . "\n\n";
} 

sub plugin_unload { 
	my $plugin = shift; 

	print "#" x 80 . "\n\n";
	Gaim::debug_info("plugin_unload()", "Testing $MODULE_NAME Started.");
	print "\n\n";

	#########  TEST CODE HERE  ##########

	print "\n\n";
	Gaim::debug_info("plugin_unload()", "Testing $MODULE_NAME Completed.");
	print "\n\n" . "#" x 80 . "\n\n";
}

