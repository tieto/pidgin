#!/usr/bin/perl -w

#  GNOME po update utility.
#  (C) 2000 The Free Software Foundation
#
#  Author(s): Kenneth Christiansen
#  Patches:   Björn Voigt <bjoern@cs.tu-berlin.de>


$VERSION = "1.2.5 beta 2";
$LANG    = $ARGV[0];
$PACKAGE  = "gaim";

if (! $LANG){
    print "update.pl:  missing file arguments\n";
    print "Try `update.pl --help' for more information.\n";
    exit;
}

if ($LANG=~/^-(.)*/){

    if ("$LANG" eq "--version" || "$LANG" eq "-V"){
        print "GNOME PO Updater $VERSION\n";
        print "Written by Kenneth Christiansen <kenneth\@gnome.org>, 2000.\n\n";
        print "Copyright (C) 2000 Free Software Foundation, Inc.\n";
        print "This is free software; see the source for copying conditions.  There is NO\n";
        print "warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n";
	exit;
    }


    elsif ($LANG eq "--help" || "$LANG" eq "-H"){
        print "Usage: ./update.pl [OPTIONS] ...LANGCODE\n";
        print "Updates pot files and merge them with the translations.\n\n";
        print "  -V, --version                shows the version\n";
        print "  -H, --help                   shows this help page\n";
        print "  -P, --pot                    only generates the potfile\n";
        print "  -M, --maintain               search for missing files in POTFILES.in\n";
	print "\nExamples of use:\n";
        print "update.sh --pot    just creates a new pot file from the source\n";
	print "update.sh da       created new pot file and updated the da.po file\n\n";
        print "Report bugs to <kenneth\@gnome.org>.\n";
	exit;
    }

    elsif($LANG eq "--pot" || "$LANG" eq "-P"){

        print "Building the $PACKAGE.pot ...";

        $b="xgettext --default-domain\=$PACKAGE --directory\=\.\."
          ." --add-comments --keyword\=\_ --keyword\=N\_"
          ." --files-from\=\.\/POTFILES\.in ";
        $b1="test \! -f $PACKAGE\.po \|\| \( rm -f \.\/$PACKAGE\.pot "
           ."&& mv $PACKAGE\.po \.\/$PACKAGE\.pot \)";
	if(($ret=system($b . " && " . $b1))==0) {
	    print "...done\n";
	}
	else {
	    print "...failed\n";
	}

        exit $ret;
    }

    elsif ($LANG eq "--maintain" || "$LANG" eq "-M"){

        $a="find ../ -path ../intl -prune -o -print | egrep '.*\\.(c|y|cc|c++|h|gob)\$' ";

        open(BUF2, "POTFILES.in") || die "update.pl:  there's not POTFILES.in!!!\n";
        print "Searching for missing _(\" \") entries and for deleted files...\n";
        open(BUF1, "$a|");


        @buf2 = <BUF2>;
        @buf1 = <BUF1>;

        if (-s "POTFILES.ignore") {
            open FILE, "POTFILES.ignore";
            while (<FILE>) {
                if ($_=~/^[^#]/o) {
                    push @bup, $_;
                }
            }
            print "POTFILES.ignore found! Ignoring files...\n";
	    @buf2 = (@bup, @buf2);
	    }

        foreach my $file (@buf1) {
	    $cmd="xgettext -o - --omit-header --keyword=_ " .
		"--keyword=N_ " . $file . " |";
	    open XGET, $cmd;
	    if(<XGET>) {
		$file = unpack("x3 A*",$file) . "\n";
		push @buff1, $file;
	    }
	    close XGET;
#	    open FILE, "<$file";
#            while (<FILE>) {
#                if ($_=~/_\(\"/o || $_=~/ngettext\(\"/o){
#                    $file = unpack("x3 A*",$file) . "\n";
#                    push @buff1, $file;
#                    last;
#                }
#            }
	}

        @bufff1 = sort (@buff1);

        @bufff2 = sort (@buf2);

        my %in2;
        foreach (@bufff2) {
	    chomp;
            $in2{$_} = 1;
        }

        my %in1;
        foreach (@bufff1) {
	    chomp;
            $in1{$_} = 1;
        }

        foreach (@bufff1) {
	    chomp;
            if (!exists($in2{$_})) {
                push @result, $_ . "\n";
	    }
	}

        foreach (@bufff2) {
	    chomp;
            if (! -f "../" . $_) {
                push @deletedfiles, $_ . "\n"; 
	    }
	}
	
	foreach (@bufff2) {
	    if (!exists($in1{$_})) {
		push @noi18nfiles, $_ . "\n";
	    }
	}

        if(@result){
            open OUT, ">POTFILES.in.missing";
            print OUT @result;
            print "\nHere are the missing files:\n\n", @result, "\n";
            print "File POTFILES.in.missing is being placed in directory...\n";
            print "Please add the files that should be ignored in POTFILES.ignore\n";
        }
        else{
	    unlink("POTFILES.in.missing");
        }
	    
        if(@deletedfiles){
            open OUT, ">POTFILES.in.deleted";
            print OUT @deletedfiles;
            print "\nHere are the deleted files:\n\n", @deletedfiles, "\n";
            print "File POTFILES.in.deleted is being placed in directory...\n";
            print "Please delete the files from POTFILES.in or POTFILES.ignore\n";
        }
        else{
	    unlink("POTFILES.in.deleted");
        }

        if(@noi18nfiles){
            open OUT, ">POTFILES.in.noi18n";
            print OUT @noi18nfiles;
            print "\nHere are the files which currently have no i18n strings:\n\n", 
	    @noi18nfiles, "\n";
            print "File POTFILES.in.noi18n is being placed in directory...\n";
            print "Please delete the files from POTFILES.in or POTFILES.ignore\n";
	    print "or ignore the files.\n";
        }
        else{
	    unlink("POTFILES.in.noi18n");
        }

        if( ! @result && ! @deletedfiles ) {
            print "\nWell, it's all perfect! Congratulation!\n";
        }
    }


    else{
        print "update.pl: invalid option -- $LANG\n";
        print "Try `update.pl --help' for more information.\n";
    }
    exit;
    }

elsif(-s "$LANG.po"){

    print "Building the $PACKAGE.pot ...";

    $c="xgettext --default-domain\=$PACKAGE --directory\=\.\."
      ." --add-comments --keyword\=\_ --keyword\=N\_"
      ." --files-from\=\.\/POTFILES\.in ";  
    $c1="test \! -f $PACKAGE\.po \|\| \( rm -f \.\/$PACKAGE\.pot "
       ."&& mv $PACKAGE\.po \.\/$PACKAGE\.pot \)";

    if(($ret=system($c . " && " . $c1))==0) {
	print "...done\n";
    }
    else {
	print "...failed\n";
    }

    if($ret==0) {
	print "\nNow merging $LANG.po with $PACKAGE.pot, and creating an updated $LANG.po ...\n";

    
	$d="if msgmerge $LANG.po $PACKAGE.pot -o $LANG.new.po; then " .
	    "  mv -f $LANG.new.po $LANG.po; " .
	    "  msgfmt --statistics  -c -v -o /dev/null $LANG.po; " .
	    "else " .
	    "  echo \"msgmerge failed!\"; " .
	    "  rm -f $LANG.new.po; ".
	    "fi";

	$ret=system($d);

	exit $ret;
    }
}

else{
    print "update.pl:  sorry $LANG.po does not exist!\n";
    print "Try `update.pl --help' for more information.\n";    
    exit;
}
