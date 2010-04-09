$MODULE_NAME = "Prefs Functions Test";
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
	unload => "plugin_unload", 
	prefs_info => "foo"
); 

	# These names must already exist
	my $GROUP		= "UIUC Buddies";
	my $USERNAME 		= "johnhkelm2";
	
	# We will create these on load then destroy them on unload
	my $TEST_GROUP		= "perlTestGroup";
	my $TEST_NAME	 	= "perlTestName";
	my $TEST_ALIAS	 	= "perlTestAlias";
	my $PROTOCOL_ID 	= "prpl-aim";

sub foo {
	$frame = Purple::PluginPref::Frame->new();

	$ppref = Purple::PluginPref->new_with_label("boolean");
	$frame->add($ppref);
	
	$ppref = Purple::PluginPref->new_with_name_and_label(
	    "/plugins/core/perl_test/bool", "Boolean Preference");
	$frame->add($ppref);

		
	$ppref = Purple::PluginPref->new_with_name_and_label(
	    "/plugins/core/perl_test/choice", "Choice Preference");
	$ppref->set_type(1);
	$ppref->add_choice("ch0", "ch0-val");
	$ppref->add_choice("ch1", "ch1-val");
	$frame->add($ppref);
	
	$ppref = Purple::PluginPref->new_with_name_and_label(
	    "/plugins/core/perl_test/text", "Text Box Preference");
	$ppref->set_max_length(16);
	$frame->add($ppref);
	
	return $frame;
}

sub pref_cb {
	my ($pref, $type, $value, $data) = @_;
	
	print "pref changed: [$pref]($type)=$value data=$data\n";
}

sub plugin_init { 
	
	return %PLUGIN_INFO; 
} 

# This is the sub defined in %PLUGIN_INFO to be called when the plugin is loaded
#	Note: The plugin has a reference to itself on top of the argument stack.
sub plugin_load { 
	my $plugin = shift; 
	print "#" x 80 . "\n\n";


	#########  TEST CODE HERE  ##########

	Purple::Prefs::add_none("/plugins/core/perl_test");
	Purple::Prefs::add_bool("/plugins/core/perl_test/bool", 1);	
	Purple::Prefs::add_string("/plugins/core/perl_test/choice", "ch1");	
	Purple::Prefs::add_string("/plugins/core/perl_test/text", "Foobar");	

	Purple::Prefs::connect_callback($plugin, "/plugins/core/perl_test", \&pref_cb, "none");
	Purple::Prefs::connect_callback($plugin, "/plugins/core/perl_test/bool", \&pref_cb, "bool");
	Purple::Prefs::connect_callback($plugin, "/plugins/core/perl_test/choice", \&pref_cb, "choice");
	Purple::Prefs::connect_callback($plugin, "/plugins/core/perl_test/text", \&pref_cb, "text");

	print "\n\n" . "#" x 80 . "\n\n";
} 

sub plugin_unload { 
	my $plugin = shift; 

	print "#" x 80 . "\n\n";


	#########  TEST CODE HERE  ##########


	print "\n\n" . "#" x 80 . "\n\n";
}

