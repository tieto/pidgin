#!/usr/bin/perl 

##  gaim buddy list to aim blt format converter
my $top = 1;
if ($ARGV[0] && $ARGV[1]) {
	open (FILE,$ARGV[0]);
	print "Config {\n version 1\n}\nUser {\n screename $ARGV[1]\n}\nBuddy {\n list {\n";
	foreach $line (<FILE>) {
		chomp ($line);
		$line =~ s/^ +//;			## strip any preceding spaces
		next unless $line =~ /^[bg]/;    	## Ignore everything but the b and g lines
		@buddy = split(//,$line);		## strip off the first 2 chars of the line.
		shift @buddy;				##   crappy way to do it, but I didn't want to
		shift @buddy;				##   mess up screen names with spaces in them.
		foreach $char (@buddy) {
			$buddy .= $char;
		}
		@screenname = split(/:/,$buddy);	## split off the aliases
		
		if ($line =~ /^g/) {
			print "  }\n" unless $top;
			print "  $screenname[0] {\n";
			$top = 0;
		} elsif ($line =~ /^b/) {
			print "   $screenname[0]\n";
		}
	$buddy = undef;
	}
	print "  }\n }\n}\n";
} else {
	print "\n\n\n\tExample:\n\n\t\tgaim2blt.pl gaim.buddy.file YourScreenName\n\n\n\n";
}
