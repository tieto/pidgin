GAIM::register("gaim test", "0.0.1", "goodbye", "");

$ver = GAIM::get_info(0);
@ids = GAIM::get_info(1);

$msg = "Gaim $ver:";
foreach $id (@ids) {
	$pro = GAIM::get_info(7, $id);
	$nam = GAIM::get_info(3, $id);
	$msg .= "\n$nam using $pro";
}

GAIM::print("Perl Says", $msg);

GAIM::command("idle", 6000);

GAIM::add_event_handler("event_buddy_signon", "echo_reply");
GAIM::add_timeout_handler(60, "notify");

sub echo_reply {
	$index = $_[0];
	$who = $_[1];
	GAIM::print_to_conv($index, $who, "Hello", 0);
}

sub notify {
	GAIM::print("1 minute", "gaim test has been loaded for 1 minute");
}

sub goodbye {
	GAIM::print("You Bastard!", "You killed Kenny!");
}
