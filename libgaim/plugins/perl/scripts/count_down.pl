use Gaim;

%PLUGIN_INFO = ( 
	perl_api_version => 2, 
	name => "Countdown Info Timer", 
	version => "0.1", 
	summary => "Makes a countdown in days from today.", 
	description => "Long description coming....", 
	author => "John H. Kelm <johnhkelm\@gmail.com>", 
	url => "http://gaim.sourceforge.net/", 
	
	load => "plugin_load", 
	unload => "plugin_unload" 
); 

	$GLOBAL_TEST_VAR = "STUFF!";

sub plugin_unload { 
	my $plugin = shift; 
}

sub plugin_init { 
	return %PLUGIN_INFO; 
} 


sub plugin_load { 
	my $plugin = shift; 
	
	# Retrieve all the accounts
	@accounts = Gaim::Accounts::get_all();
	
	print "NUM OF ACCS: " . $accounts . "\n";
	# Search each account's user info for our tag
	foreach $acc (@accounts) {
		print "IN ACCOUNTS\n";
		$user_info = $acc->get_user_info();
		print "USER INFO 1: " . $user_info . "\n";
		# Find <countdown> and replace
		$user_info =~ /countdown([0-9]+).([0-9]+).([0-9]+)/;
		print "Found: " .$1 . " " . $2 . " " . $3 . "\n";
		$days = count_days($1, $2, $3);
		$user_info =~ s/countdown(\d\d\d\d).(\d\d).(\d\d)/$days/;
		print "USER INFO 2: " . $user_info . "\n";
	#	$acc->set_user_info($user_info);
	
	}
	
	eval '
		use Gtk2 \'-init\';
		use Glib;
		$window = Gtk2::Window->new(\'toplevel\');
		$window->set_border_width(10);
		$button = Gtk2::Button->new("Hello World");
		$button->signal_connect(clicked => \&hello, $window);
		
		$window->add($button);
		$button->show;
		$window->show;
	#	Gtk2->main;
		
		0;
		
	'; warn $@ if $@;
}

sub hello {
	my ($widget, $window) = @_;
	print "Called from sub hello!\n ";
	print "Test var: " . $GLOBAL_TEST_VAR . " \n";
	@accounts = Gaim::Accounts::get_all();
	$acc = $accounts[0];
	$user_info = $acc->get_user_info();
	print "USER INFO from sub hello: " . $user_info . "\n";
	$window->destroy;
}

sub count_days {
	($year, $month, $day) = @_;
	
	
	eval '
		use Time::Local;
		$future = timelocal(0,0,0,$day,$month-1,$year); 
	'; warn $@ if $@;
	$today = time();
	$days = int(($future - $today)/(60*60*24));
	return $days;
}
