package Pidgin;

use 5.008;
use strict;
use warnings;
use Carp;

our $VERSION = '0.01';

use Purple;

require XSLoader;
XSLoader::load('Pidgin', $VERSION);

1;
__END__

=head1 NAME

Pidgin - Perl extension for the Pidgin instant messenger.

=head1 SYNOPSIS

    use Pidgin;

=head1 ABSTRACT

    This module provides the interface for using perl scripts as plugins in
    Pidgin, with access to the Pidgin Gtk interface functions.

=head1 DESCRIPTION

This module provides the interface for using perl scripts as plugins in Pidgin,
with access to the Pidgin Gtk interface functions. With this, developers can
write perl scripts that can be loaded in Pidgin as plugins. The script can
interact with IMs, chats, accounts, the buddy list, pidgin signals, and more.

The API for the perl interface is very similar to that of the Pidgin C API,
which can be viewed at http://developer.pidgin.im/doxygen/ or in the header files
in the Pidgin source tree.

=head1 FUNCTIONS

=over

=back

=head1 SEE ALSO
Pidgin C API documentation - http://developer.pidgin.im/doxygen/

The Pidgin perl module.

Pidgin website - http://pidgin.im/

=head1 AUTHOR

Etan Reisner, E<lt>deryni@gmail.comE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2006 by Etan Reisner
