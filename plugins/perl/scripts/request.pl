$MODULE_NAME = "Request Functions Test";

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
	unload => "plugin_unload", 
	plugin_action => "plugin_action_test",
	plugin_action_label => "Plugin Action Test Label"
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

sub ok_cb_test{
	$fields = shift;
	print "ok_cb_test: BEGIN\n";
	print "ok_cb_test: Button Click\n";
	print "ok_cb_test: Field Type: " . $fields . "\n";
	$account = Gaim::Request::fields_get_account($fields, "acct_test");
	print "ok_cb_test: Username of selected account: " . Gaim::Account::get_username($account) . "\n";
	$int = Gaim::Request::fields_get_integer($fields, "int_test");
	print "ok_cb_test: Integer Value:" . $int . "\n";
	$choice = Gaim::Request::fields_get_choice($fields, "ch_test");
	print "ok_cb_test: Choice Value:" . $choice . "\n";
	print "ok_cb_test: END\n";
}

sub cancel_cb_test{
	print "cancel_cb_test: Button Click\n";
}

sub plugin_action_test {
	$plugin = shift;
	print "plugin_action_cb_test: BEGIN\n";
	plugin_request($plugin);
	print "plugin_action_cb_test: END\n";
}

sub plugin_load { 
	my $plugin = shift; 
	#########  TEST CODE HERE  ##########
	

}

sub plugin_request {	
	$group = Gaim::Request::field_group_new("Group Name");
	$field = Gaim::Request::field_account_new("acct_test", "Account Text", undef);
	Gaim::Request::field_account_set_show_all($field, 0);
	Gaim::Request::field_group_add_field($group, $field);
	
	$field = Gaim::Request::field_int_new("int_test", "Integer Text", 33);
	Gaim::Request::field_group_add_field($group, $field);
	
	# Test field choice
	$field = Gaim::Request::field_choice_new("ch_test", "Choice Text", 1);
	Gaim::Request::field_choice_add($field, "Choice 0");
	Gaim::Request::field_choice_add($field, "Choice 1");
	Gaim::Request::field_choice_add($field, "Choice 2");
	
	Gaim::Request::field_group_add_field($group, $field);
	
	
	$request = Gaim::Request::fields_new();
	Gaim::Request::fields_add_group($request, $group);
	
	Gaim::Request::fields(
		$plugin, 
		"Request Title!", 
		"Primary Title", 
		"Secondary Title",
		$request,
		"Ok Text", "ok_cb_test",
		"Cancel Text", "cancel_cb_test");
} 

sub plugin_unload { 
	my $plugin = shift; 
	print "#" x 80 . "\n";
	#########  TEST CODE HERE  ##########
	
	

	print "\n" . "#" x 80 . "\n";
}

