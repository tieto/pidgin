# FORTUNE PROFILE
#
# Sets your AIM profile to a fortune (with a header and footer of your
# choice).
#

# By Sean Egan
# bj91704@binghamton.edu
# AIM: SeanEgn
#
# Updated by Nathan Conrad, 31 January 2002
# Changes:
#  * Fortunes have HTML tabs and newlines
# AIM: t98502
# ICQ: 16106363
#
# Updated by Mark Doliner, 15 October 2002
# Changes:
#  * Modified to work with the changed perl interface of gaim 0.60
#  * Fixed a bug where your info would be set to nothing if you had 
#    no pre and no post message
# AIM: lbdash

# Copyright (C) 2001 Sean Egan

# This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA


$handle = GAIM::register("Fortune Profile", "3.3", "", "");

$tab = "&nbsp;";
$tab = $tab . $tab . $tab . $tab;
$nl = "<BR>";

$seconds = 30;                   # Delay before updating away messages.
$max = 1020;                     # Max length of an profile. It should be
                                 # 1024, but I am being safe
$pre_message = "";               # This gets added before the fortune

$post_message ="";

$len = 0;
if ($pre_message ne "") {
  $len += length( $pre_message . "---$nl" );
}
if ($post_message ne "") {
  $len += length("---$nl" . $post_message);
}

$command = "fortune -sn " . ($max - $len);     # Command to get dynamic message from

sub update_away {
  # The fortunes are expanded into HTML (the tabs and newlines) which
  # causes the -s option of fortune to be a little bit meaningless. This
  # will loop until it gets a fortune of a good size (after expansion).
 
  do {
    do {  #It's a while loop because it doesn't always work for some reason
      $fortune =  `$command`;
      if ($? == -1) {
        return;
      }
    } while ($fortune eq "");
    $fortune =~ s/\n/$nl/g;
    $fortune =~ s/\t/$tab/g;
  } while ((length($fortune) + $len ) > $max);

  $message = $fortune;
  if ($pre_message ne "") {
    $message = $pre_message . "---$nl" . $message;
  }
  if ($post_message ne "") {
    $message = $message . "---$nl" . $post_message ;
  }

  foreach $id (GAIM::get_info(1)) {GAIM::command("info", $id, $message);} 
  GAIM::add_timeout_handler($handle, $seconds, "update_away");
}

sub description {
  my($a, $b, $c, $d, $e, $f) = @_;
  ("Fortune Profile", "3.3", "Sets your AIM profile to a fortune (with a header and footer of your choice).",
  "Sean Egan <bj91704\@binghamton.edu>", "http://gaim.sf.net/",
  "/dev/null");
}

# output the first message and start the timers...
# This is done as a timeout to prevent attempts to set the profile before logging in.
GAIM::add_timeout_handler($handle, $seconds, "update_away");
