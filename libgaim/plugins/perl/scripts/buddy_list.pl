$MODULE_NAME = "Buddy List Test";

use Gaim;

# All the information Gaim gets about our nifty plugin
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
	my $PROTOCOL_ID 	= "prpl-oscar";


sub plugin_init {
	return %PLUGIN_INFO;
}


# This is the sub defined in %PLUGIN_INFO to be called when the plugin is loaded
#	Note: The plugin has a reference to itself on top of the argument stack.
sub plugin_load {
	my $plugin = shift;

	# This is how we get an account to use in the following tests.  You should replace the username
	#  with an existing user
	$account = Gaim::Accounts::find($USERNAME, $PROTOCOL_ID);

	# Testing a find function: Note Gaim::Find not Gaim::Buddy:find!
	#  Furthermore, this should work the same for chats and groups
	Gaim::Debug::info($MODULE_NAME, "Testing: Gaim::Find::buddy()...");
	$buddy = Gaim::Find::buddy($account, $TEST_NAME);
	Gaim::Debug::info("", ($buddy ? "ok." : "fail.") . "\n");

	# If you should need the handle for some reason, here is how you do it
	Gaim::Debug::info($MODULE_NAME, "Testing: Gaim::BuddyList::get_handle()...");
	$handle = Gaim::BuddyList::get_handle();
	Gaim::Debug::info("", ($handle ? "ok." : "fail.") . "\n");

	# This gets the Gaim::BuddyList and references it by $blist
	Gaim::Debug::info($MODULE_NAME, "Testing: Gaim::get_blist()...");
	$blist = Gaim::get_blist();
	Gaim::Debug::info("", ($blist ? "ok." : "fail.") . "\n");

	# This is how you would add a buddy named $TEST_NAME" with the alias $TEST_ALIAS
	Gaim::Debug::info($MODULE_NAME, "Testing: Gaim::BuddyList::Buddy::new...");
	$buddy = Gaim::BuddyList::Buddy::new($account, $TEST_NAME, $TEST_ALIAS);
	Gaim::Debug::info("", ($buddy ? "ok." : "fail.") . "\n");

	# Here we add the new buddy '$buddy' to the group $TEST_GROUP
	#  so first we must find the group
	Gaim::Debug::info($MODULE_NAME, "Testing: Gaim::Find::group...");
	$group = Gaim::Find::group($TEST_GROUP);
	Gaim::Debug::info("", ($group ? "ok." : "fail.") . "\n");

	# To add the buddy we need to have the buddy, contact, group and node for insertion.
	#  For this example we can let contact be undef and set the insertion node as the group
	Gaim::Debug::info($MODULE_NAME, "Testing: Gaim::BuddyList::add_buddy...\n");
	Gaim::BuddyList::add_buddy($buddy, undef, $group, $group);

	# The example that follows gives an indication of how an API call that returns a list is handled.
	#  In this case the buddies of the account found earlier are retrieved and put in an array '@buddy_array'
	#  Further down an accessor method is used, 'get_name()' -- see source for details on the full set of methods
	Gaim::Debug::info($MODULE_NAME,  "Testing: Gaim::Find::buddies...\n");
	@buddy_array = Gaim::Find::buddies($account, undef);
	if (@buddy_array) {
		Gaim::Debug::info($MODULE_NAME, "Buddies in list (" . @buddy_array . "): \n");
		foreach $bud (@buddy_array) {
			Gaim::Debug::info($MODULE_NAME, Gaim::BuddyList::Buddy::get_name($bud) . "\n");
		}
	}
}

sub plugin_unload {
	my $plugin = shift;

	print "#" x 80 . "\n\n";
	#########  TEST CODE HERE  ##########

	print "Testing: Gaim::Find::buddy()...";
	$buddy = Gaim::Find::buddy($account, $TEST_NAME . TEST);
	if ($buddy) {
		print "ok.\n";
		print "Testing: Gaim::BuddyList::remove_buddy()...";
		Gaim::BuddyList::remove_buddy($buddy);
		if (Gaim::Find::buddy($account, $TEST_NAME . TEST)) { print "fail.\n"; } else { print "ok.\n"; }
	} else { print "fail.\n"; }


	print "\n\n" . "#" x 80 . "\n\n";
}

