$MODULE_NAME = "Conversation Test";

use Gaim;

# All the information Gaim gets about our nifty plugin
%PLUGIN_INFO = ( 
	perl_api_version => 2, 
	name => " Perl: $MODULE_NAME", 
	version => "0.1", 
	summary => "Test plugin for the Perl interpreter.", 
	description => "Implements a set of test proccedures to ensure all functions that work in the C API still work in the Perl plugin interface.  As XSUBs are added, this *should* be updated to test the changes.  Furthermore, this will function as the tutorial perl plugin.", 
	author => "John H. Kelm <johnhkelm\@gmail.com", 
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
	print "Testing Gaim::Conv::new()...";
	$conv1 = Gaim::Conv::new(1, $account, "Test Conv. 1");
	if ($conv) { print "ok.\n"; } else { print "fail.\n"; }

	print "Testing Gaim::Conv::new()...";
	$conv2 = Gaim::Conv::new(1, $account, "Test Conv. 2");
	if ($conv) { print "ok.\n"; } else { print "fail.\n"; }
	
	print "Testing Gaim::Conv::Window::new()...\n";
	$win = Gaim::Conv::Window::new();	
	
	print "Testing Gaim::Conv::Window::add_conversation()...";
	$conv_count = Gaim::Conv::Window::add_conversation($win, $conv1);
	if ($conv_count) { print "ok..." . $conv_count . " conversations...\n"; } else { print "fail.\n"; }

	print "Testing Gaim::Conv::Window::add_conversation()...";
	$conv_count = Gaim::Conv::Window::add_conversation($win, $conv2);
	if ($conv_count) { print "ok..." . $conv_count . " conversations...\n"; } else { print "fail.\n"; }
	
	print "Testing Gaim::Conv::Window::show()...\n";
	Gaim::Conv::Window::show($win);
	
	print "Testing Gaim::Conv::get_im_data()...\n";
	$im = Gaim::Conv::get_im_data($conv1);	
	if ($im) { print "ok.\n"; } else { print "fail.\n"; }
	
	print "Testing Gaim::Conv::IM::send()...\n";
	Gaim::Conv::IM::send($im, "Message Test.");
	
	print "Testing Gaim::Conv::IM::write()...\n";
	Gaim::Conv::IM::write($im, "sendingUser", "<b>Message</b> Test.", 0, 0);
	
	print "#" x 80 . "\n\n";
} 

sub plugin_unload { 
	my $plugin = shift; 

	print "#" x 80 . "\n\n";
	#########  TEST CODE HERE  ##########


	
	print "Testing Gaim::Conv::Window::get_conversation_count()...\n";
	$conv_count = Gaim::Conv::Window::get_conversation_count($win);
	print $conv_count;
	if ($conv_count > 0) {
		print "Testing Gaim::Conv::Window::destroy()...\n";
		Gaim::Conv::Window::destroy($win);
	}
	
	print "\n\n" . "#" x 80 . "\n\n";
}

