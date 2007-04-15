sub description {
        my($a, $b, $c, $d, $e, $f) = @_;
        ("Example", "1.0", "An example Gaim perl script that does nothing particularly useful:\n\t-Show a dialog on load\n\t-Set user idle for 6,000 seconds\n\t-Greets people signing on with \"Hello\"\n\t-Informs you when script has been loaded for one minute.", "Eric Warmenhoven &lt;eric\@warmenhoven.org>", "http://gaim.sf.net", "/dev/null");
}

$handle = GAIM::register("Example", "1.0", "goodbye", "");

GAIM::print("Perl Says", "Handle $handle");
		
$ver = GAIM::get_info(0);
@ids = GAIM::get_info(1);

$msg = "Gaim $ver:";
foreach $id (@ids) {
	$pro = GAIM::get_info(7, $id);
	$nam = GAIM::get_info(3, $id);
	$msg .= "\n$nam using $pro";
}


GAIM::command("idle", 6000);

GAIM::add_event_handler($handle, "event_buddy_signon", "echo_reply");
GAIM::add_timeout_handler($handle, 60, "notify");

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

