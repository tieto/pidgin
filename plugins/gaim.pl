AIM::register("gaim test", "0.0.1", "goodbye", "");

$ver = AIM::get_info(0);
$nam = AIM::get_info(1);
$pro = AIM::get_info(2);

AIM::print("Perl Says", "Gaim $ver, $nam using $pro");

# i should probably do something with these to actually test them, huh
@bud = AIM::buddy_list();
@onl = AIM::online_list();
@den = AIM::deny_list();

AIM::command("idle", "60000") if ($pro ne "Offline");

AIM::add_event_handler("event_buddy_signon", "say_hello");
AIM::add_timeout_handler(600, "notify");

sub echo_reply {
	$args = @_;
	$args =~ s/\"//g;
	AIM::print_to_conv($args, "Hello");
}

sub notify {
	AIM::print("10 minutes", "gaim test has been loaded for 10 minutes");
}

sub goodbye {
	AIM::print("You Bastard!", "You killed Kenny!");
}
