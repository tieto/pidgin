$MODULE_NAME = "GTK Frame Test";

use Gaim;

%PLUGIN_INFO = ( 
	perl_api_version => 2, 
	name => " Perl: $MODULE_NAME", 
	version => "0.1", 
	summary => "Test plugin for the Perl interpreter.", 
	description => "Implements a set of test proccedures to ensure all functions that work in the C API still work in the Perl plugin interface.  As XSUBs are added, this *should* be updated to test the changes.  Furthermore, this will function as the tutorial perl plugin.", 
	author => "John H. Kelm <johnhkelm\@gmail.com>", 
	url => "http://gaim.sourceforge.net/", 
	
	GTK_UI => TRUE,
	gtk_prefs_info => "foo",
	load => "plugin_load",
	unload => "plugin_unload",
); 


sub plugin_init { 
	return %PLUGIN_INFO; 
} 

sub button_cb {
	my $widget = shift;
	my $data = shift;
	print "Clicked button with message: " . $data . "\n";
}

sub foo {
	eval '
		use Glib;
		use Gtk2 \'-init\';
				
		$frame = Gtk2::Frame->new(\'Gtk Test Frame\');
		$button = Gtk2::Button->new(\'Print Message\');
		
		$frame->set_border_width(10);
		$button->set_border_width(150);
		$button->signal_connect("clicked" => \&button_cb, "Message Text");
		$frame->add($button);
		
		$button->show();
		$frame->show();
	';
	return $frame;
}

sub plugin_load { 
	my $plugin = shift; 
	print "#" x 80 . "\n";

	
	#########  TEST CODE HERE  ##########

	print "$MODULE_NAME: Loading...\n";
	
	
	Gaim::debug_info("plugin_load()", "Testing $MODULE_NAME Completed.");
	print "#" x 80 . "\n\n";
} 

sub plugin_unload {

}
