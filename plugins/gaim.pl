GAIM::register("gaim test", "0.0.1", "goodbye", "");

$ver = GAIM::get_info(0);
$nam = GAIM::get_info(1);
$pro = GAIM::get_info(2, $nam);

GAIM::print("Perl Says", "Gaim $ver, $nam using $pro");

GAIM::command("idle", 60000);

GAIM::add_event_handler("event_buddy_signon", "say_hello");
GAIM::add_timeout_handler(600, "notify");

sub echo_reply {
	$args = @_;
	$args =~ s/\"//g;
	GAIM::print_to_conv($args, "Hello");
}

sub notify {
	GAIM::print("10 minutes", "gaim test has been loaded for 10 minutes");
}

sub goodbye {
	GAIM::print("You Bastard!", "You killed Kenny!");
}
