$MODULE_NAME = "Plugin Action Test Plugin";
use Gaim;

sub plugin_init {
	return %PLUGIN_INFO;
}

sub plugin_load {
	my $plugin = shift;
}

sub plugin_unload {
	my $plugin = shift;
}

sub fun1 {
	print "1\n";
}

sub fun2 {
	print "2\n";
}

sub fun3 {
	print "3\n";
}

%plugin_actions = (
	"Action 1" => \&fun1,
	"Action 2" => \&fun2,
	"Action 3" => \&fun3
#	"Action 1" => sub { print "1\n"; },
#	"Action 2" => sub { print "2\n"; },
#	"Action 3" => sub { print "3\n"; }
);

sub plugin_action_names {
	foreach $key (keys %plugin_actions) {
		push @array, $key;
	}

	return @array;
}

# All the information Gaim gets about our nifty plugin
%PLUGIN_INFO = (
	perl_api_version => 2,
	name => "Perl: $MODULE_NAME",
	version => "0.1",
	summary => "Test plugin for the Perl interpreter.",
	description => "Just a basic test plugin template.",
	author => "Etan Reisner <deryni\@gmail.com>",
	url => "http://sourceforge.net/users/deryni9/",

	load => "plugin_load",
	unload => "plugin_unload",
	plugin_action_sub => "plugin_action_names"
);
