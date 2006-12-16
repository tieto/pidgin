$MODULE_NAME = "Buddy List Test";

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
	print "#" x 80 . "\n\n";

	print "PERL: Finding account.\n";
	$account = Gaim::Accounts::find($USERNAME, $PROTOCOL_ID);
	
	#########  TEST CODE HERE  ##########
	
	print "Testing: Gaim::Find::buddy()...";
	$buddy = Gaim::Find::buddy($account, $TEST_NAME);
	if ($buddy) { print "ok.\n"; } else { print "fail.\n"; }	
	
	print "Testing: Gaim::BuddyList::get_handle()...";
	$handle = Gaim::BuddyList::get_handle();
	if ($handle) { print "ok.\n"; } else { print "fail.\n"; }	
	
	print "Testing: Gaim::BuddyList::get_blist()...";	
	$blist = Gaim::BuddyList::get_blist();
	if ($blist) { print "ok.\n"; } else { print "fail.\n"; }	
	
	print "Testing: Gaim::Buddy::new...";
	$buddy = Gaim::Buddy::new($account, $TEST_NAME . "TEST", $TEST_ALIAS);
	if ($buddy) { print "ok.\n"; } else { print "fail.\n"; }

	print "Testing: Gaim::Find::group...";
	$group = Gaim::Find::group($TEST_GROUP);
	if ($group) { print "ok.\n"; } else { print "fail.\n"; }
	
	print "Testing: Gaim::BuddyList::add_buddy...";
	Gaim::BuddyList::add_buddy($buddy, undef, $group, $group);
	if ($buddy) { print "ok.\n"; } else { print "fail.\n"; }
	
	print "Testing: Gaim::Find::buddies...\n";	
	@buddy_array = Gaim::Find::buddies($account, $USERNAME);
	if (@buddy_array) { 
		print "Buddies in list (" . @buddy_array . "): \n";
		foreach $bud (@buddy_array) {	
			print Gaim::Buddy::get_name($bud) . "\n";
		}
	}

	print "#" x 80 . "\n\n";
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

