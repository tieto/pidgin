$MODULE_NAME = "Buddy List Test";

use Purple;

# All the information Purple gets about our nifty plugin
%PLUGIN_INFO = (
	perl_api_version => 2,
	name => "Perl: $MODULE_NAME",
	version => "0.1",
	summary => "Test plugin for the Perl interpreter.",
	description => "Implements a set of test proccedures to ensure all functions that work in the C API still work in the Perl plugin interface.  As XSUBs are added, this *should* be updated to test the changes.  Furthermore, this will function as the tutorial perl plugin.",
	author => "John H. Kelm <johnhkelm\@gmail.com>",
	url => "http://sourceforge.net/users/johnhkelm/",

	load => "plugin_load",
	unload => "plugin_unload"
);


	# These names must already exist
	my $USERNAME 		= "johnhkelm2";

	# We will create these on load then destroy them on unload
	my $TEST_GROUP		= "UConn Buddies";
	my $TEST_NAME	 	= "johnhkelm";
	my $TEST_ALIAS	 	= "John Kelm";
	my $PROTOCOL_ID 	= "prpl-aim";


sub plugin_init {
	return %PLUGIN_INFO;
}


# This is the sub defined in %PLUGIN_INFO to be called when the plugin is loaded
#	Note: The plugin has a reference to itself on top of the argument stack.
sub plugin_load {
	my $plugin = shift;

	# This is how we get an account to use in the following tests.  You should replace the username
	#  with an existing user
	$account = Purple::Accounts::find($USERNAME, $PROTOCOL_ID);

	# Testing a find function: Note Purple::Find not Purple::Buddy:find!
	#  Furthermore, this should work the same for chats and groups
	Purple::Debug::info($MODULE_NAME, "Testing: Purple::Find::buddy()...");
	$buddy = Purple::Find::buddy($account, $TEST_NAME);
	Purple::Debug::info("", ($buddy ? "ok." : "fail.") . "\n");

	# If you should need the handle for some reason, here is how you do it
	Purple::Debug::info($MODULE_NAME, "Testing: Purple::BuddyList::get_handle()...");
	$handle = Purple::BuddyList::get_handle();
	Purple::Debug::info("", ($handle ? "ok." : "fail.") . "\n");

	# This gets the Purple::BuddyList and references it by $blist
	Purple::Debug::info($MODULE_NAME, "Testing: Purple::get_blist()...");
	$blist = Purple::get_blist();
	Purple::Debug::info("", ($blist ? "ok." : "fail.") . "\n");

	# This is how you would add a buddy named $TEST_NAME" with the alias $TEST_ALIAS
	Purple::Debug::info($MODULE_NAME, "Testing: Purple::BuddyList::Buddy::new...");
	$buddy = Purple::BuddyList::Buddy::new($account, $TEST_NAME, $TEST_ALIAS);
	Purple::Debug::info("", ($buddy ? "ok." : "fail.") . "\n");

	# Here we add the new buddy '$buddy' to the group $TEST_GROUP
	#  so first we must find the group
	Purple::Debug::info($MODULE_NAME, "Testing: Purple::Find::group...");
	$group = Purple::Find::group($TEST_GROUP);
	Purple::Debug::info("", ($group ? "ok." : "fail.") . "\n");

	# To add the buddy we need to have the buddy, contact, group and node for insertion.
	#  For this example we can let contact be undef and set the insertion node as the group
	Purple::Debug::info($MODULE_NAME, "Testing: Purple::BuddyList::add_buddy...\n");
	Purple::BuddyList::add_buddy($buddy, undef, $group, $group);

	# The example that follows gives an indication of how an API call that returns a list is handled.
	#  In this case the buddies of the account found earlier are retrieved and put in an array '@buddy_array'
	#  Further down an accessor method is used, 'get_name()' -- see source for details on the full set of methods
	Purple::Debug::info($MODULE_NAME,  "Testing: Purple::Find::buddies...\n");
	@buddy_array = Purple::Find::buddies($account, undef);
	if (@buddy_array) {
		Purple::Debug::info($MODULE_NAME, "Buddies in list (" . @buddy_array . "): \n");
		foreach $bud (@buddy_array) {
			Purple::Debug::info($MODULE_NAME, Purple::BuddyList::Buddy::get_name($bud) . "\n");
		}
	}
}

sub plugin_unload {
	my $plugin = shift;

	print "#" x 80 . "\n\n";
	#########  TEST CODE HERE  ##########

	print "Testing: Purple::Find::buddy()...";
	$buddy = Purple::Find::buddy($account, $TEST_NAME . TEST);
	if ($buddy) {
		print "ok.\n";
		print "Testing: Purple::BuddyList::remove_buddy()...";
		Purple::BuddyList::remove_buddy($buddy);
		if (Purple::Find::buddy($account, $TEST_NAME . TEST)) { print "fail.\n"; } else { print "ok.\n"; }
	} else { print "fail.\n"; }


	print "\n\n" . "#" x 80 . "\n\n";
}

