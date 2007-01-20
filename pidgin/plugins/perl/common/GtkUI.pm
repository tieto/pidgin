package Gaim::GtkUI;

use 5.008;
use strict;
use warnings;
use Carp;

our $VERSION = '0.01';

use Gaim;

require XSLoader;
XSLoader::load('Gaim::GtkUI', $VERSION);

1;
__END__

=head1 NAME

Gaim::GtkUI - Perl extension for the Gaim instant messenger.

=head1 SYNOPSIS

    use Gaim::GtkUI;

=head1 ABSTRACT

    This module provides the interface for using perl scripts as plugins in
    Gaim, with access to the Gaim Gtk interface functions.

=head1 DESCRIPTION

This module provides the interface for using perl scripts as plugins in Gaim,
with access to the Gaim Gtk interface functions. With this, developers can
write perl scripts that can be loaded in Gaim as plugins. The script can
interact with IMs, chats, accounts, the buddy list, gaim signals, and more.

The API for the perl interface is very similar to that of the Gaim C API,
which can be viewed at http://gaim.sourceforge.net/api/ or in the header files
in the Gaim source tree.

=head1 FUNCTIONS

=over

=back

=head1 SEE ALSO
Gaim C API documentation - http://gaim.sourceforge.net/api/

The Gaim perl module.

Gaim website - http://gaim.sourceforge.net/

=head1 AUTHOR

Etan Reisner, E<lt>deryni@gmail.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006 by Etan Reisner
