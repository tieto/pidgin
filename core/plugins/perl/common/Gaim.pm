package Gaim;

use 5.008;
use strict;
use warnings;
use Carp;

require Exporter;
use AutoLoader;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Gaim ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	
);

our $VERSION = '0.01';

sub AUTOLOAD {
	# This AUTOLOAD is used to 'autoload' constants from the constant()
	# XS function.

	my $constname;
	our $AUTOLOAD;
	($constname = $AUTOLOAD) =~ s/.*:://;
	croak "&Gaim::constant not defined" if $constname eq 'constant';
	my ($error, $val) = constant($constname);
	if ($error) { croak $error; }
	{
		no strict 'refs';

		*$AUTOLOAD = sub { $val };
	}

	goto &$AUTOLOAD;
}

require XSLoader;
XSLoader::load('Gaim', $VERSION);

# Preloaded methods go here.

1;
__END__

=head1 NAME

Gaim - Perl extension the Gaim instant messenger.

=head1 SYNOPSIS

  use Gaim;

=head1 ABSTRACT

  This module provides the interface for using perl scripts as plugins
  in Gaim.

=head1 DESCRIPTION

This module provides the interface for using perl scripts as plugins
in Gaim. With this, developers can write perl scripts that can be
loaded in Gaim as plugins. The scripts can interact with IMs, chats,
accounts, the buddy list, gaim signals, and more.

The API for the perl interface is very similar to that of the Gaim C
API, which can be viewed at http://gaim.sourceforge.net/api/ or in
the header files in the Gaim source tree.

=head1 FUNCTIONS

=over

=item @accounts = Gaim::accounts

Returns a list of all accounts, online or offline.

=item @chats = Gaim::chats

Returns a list of all chats currently open.

=item @connections = Gaim::connections

Returns a list of all active connections.

=item @conversations = Gaim::conversations

Returns a list of all conversations, both IM and chat, currently open.

=item @conv_windows = Gaim::conv_windows

Returns a list of all conversation windows currently open.

=item @ims = Gaim::ims

Returns a list of all instant messages currently open.

=back

=head1 SEE ALSO

Gaim C API documentation - http//gaim.sourceforge.net/api/

Gaim website - http://gaim.sourceforge.net/

=head1 AUTHOR

Christian Hammond, E<lt>chipx86@gnupdate.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2003 by Christian Hammond

This library is free software; you can redistribute it and/or modify
it under the terms of the General Public License (GPL).  For
more information, see http://www.fsf.org/licenses/gpl.txt

=cut
