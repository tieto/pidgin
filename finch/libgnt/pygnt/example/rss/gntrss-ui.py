#!/usr/bin/env python

"""
gr - An RSS-reader built using libgnt and feedparser.

Copyright (C) 2007 Sadrul Habib Chowdhury <sadrul@pidgin.im>

This application is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This application is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this application; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301
USA
"""

"""
This file deals with the UI part (gnt) of the application

TODO:
    - Allow showing feeds of only selected 'category' and/or 'priority'. A different
      window should be used to change such filtering.
    - Display details of each item in its own window.
    - Add search capability, and allow searching only in title/body. Also allow
      filtering in the search results.
    - Show the data and time for feed items (probably in a separate column .. perhaps not)
    - Have a simple way to add a feed.
    - Allow renaming a feed.
"""

import gntrss
import gnthtml
import gnt
import gobject
import sys

__version__ = "0.0.1alpha"
__author__ = "Sadrul Habib Chowdhury (sadrul@pidgin.im)"
__copyright__ = "Copyright 2007, Sadrul Habib Chowdhury"
__license__ = "GPL" # see full license statement above

gnt.gnt_init()

class RssTree(gnt.Tree):
    __gsignals__ = {
        'active_changed' : (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (gobject.TYPE_OBJECT,))
    }

    __gntbindings__ = {
        'jump-next-unread' : ('jump_next_unread', 'n')
    }

    def jump_next_unread(self, null):
        first = None
        next = None
        all = self.get_rows()
        for item in all:
            if item.unread:
                if next:
                    first = item
                    break
                elif not first and self.active != item:
                    first = item
            if self.active == item:
                next = item
        if first:
            self.set_active(first)
            self.set_selected(first)
        return True

    def __init__(self):
        self.active = None
        gnt.Tree.__init__(self)
        gnt.set_flag(self, 8)    # remove borders
        self.connect('key_pressed', self.do_key_pressed)

    def set_active(self, active):
        if self.active == active:
            return
        if self.active:
            flag = gnt.TEXT_FLAG_NORMAL
            if self.active.unread:
                flag = flag | gnt.TEXT_FLAG_BOLD
            self.set_row_flags(self.active, flag)
        old = self.active
        self.active = active
        flag = gnt.TEXT_FLAG_UNDERLINE
        if self.active.unread:
            flag = flag | gnt.TEXT_FLAG_BOLD
        self.set_row_flags(self.active, flag)
        self.emit('active_changed', old)

    def do_key_pressed(self, null, text):
        if text == '\r':
            now = self.get_selection_data()
            self.set_active(now)
            return True
        return False

gobject.type_register(RssTree)
gnt.register_bindings(RssTree)

win = gnt.Box(homo = False, vert = True)
win.set_toplevel(True)
win.set_title("GntRss")
win.set_pad(0)

#
# [[[ Generic feed/item callbacks
#
def feed_item_added(feed, item):
    add_feed_item(item)

def add_feed(feed):
    if not feed.get_data('gntrss-connected'):
        feed.connect('added', feed_item_added)
        feed.connect('notify', update_feed_title)
        feed.set_data('gntrss-connected', True)
    feeds.add_row_after(feed, [feed.title, str(feed.unread)], None, None)

def remove_item(item, feed):
    items.remove(item)

def update_feed_item(item, property):
    if property.name == 'unread':
        if feeds.active == item.parent:
            flag = 0
            if item == items.active:
                flag = gnt.TEXT_FLAG_UNDERLINE
            if item.unread:
                flag = flag | gnt.TEXT_FLAG_BOLD
            else:
                flag = flag | gnt.TEXT_FLAG_NORMAL
            items.set_row_flags(item, flag)

        unread = item.parent.unread
        if item.unread:
            unread = unread + 1
        else:
            unread = unread - 1
        item.parent.set_property('unread', unread)

def add_feed_item(item):
    currentfeed = feeds.active
    if item.parent != currentfeed:
        return
    months = ["", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
    dt = str(item.date_parsed[2]) + "." + months[item.date_parsed[1]] + "." + str(item.date_parsed[0])
    items.add_row_after(item, [str(item.title), dt], None, None)
    if item.unread:
        items.set_row_flags(item, gnt.TEXT_FLAG_BOLD)
    if not item.get_data('gntrss-connected'):
        item.set_data('gntrss-connected', True)
        # this needs to happen *without* having to add the item in the tree
        item.connect('notify', update_feed_item)
        item.connect('delete', remove_item)

#
# ]]] Generic feed/item callbacks
#


####
# [[[ The list of feeds
###

# 'Add Feed' dialog
add_feed_win = None
def add_feed_win_closed(win):
    global add_feed_win
    add_feed_win = None

def add_new_feed():
    global add_feed_win

    if add_feed_win:
        gnt.gnt_window_present(add_feed_win)
        return
    win = gnt.Window()
    win.set_title("New Feed")

    box = gnt.Box(False, False)
    label = gnt.Label("Link")
    box.add_widget(label)
    entry = gnt.Entry("")
    entry.set_size(40, 1)
    box.add_widget(entry)

    win.add_widget(box)
    win.show()
    add_feed_win = win
    add_feed_win.connect("destroy", add_feed_win_closed)

#
# The active row in the feed-list has changed. Update the feed-item table.
def feed_active_changed(tree, old):
    items.remove_all()
    if not tree.active:
        return
    update_items_title()
    for item in tree.active.items:
        add_feed_item(item)
    win.give_focus_to_child(items)

#
# Check for the action keys and decide how to deal with them.
def feed_key_pressed(tree, text):
    if tree.is_searching():
        return
    if text == 'r':
        feed = tree.get_selection_data()
        tree.perform_action_key('j')
        #tree.perform_action('move-down')
        feed.refresh()
    elif text == 'R':
        feeds = tree.get_rows()
        for feed in feeds:
            feed.refresh()
    elif text == 'm':
        feed = tree.get_selection_data()
        if feed:
            feed.mark_read()
            feed.set_property('unread', 0)
    elif text == 'a':
        add_new_feed()
    else:
        return False
    return True

feeds = RssTree()
feeds.set_property('columns', 2)
feeds.set_col_width(0, 20)
feeds.set_col_width(1, 6)
feeds.set_column_resizable(0, False)
feeds.set_column_resizable(1, False)
feeds.set_column_is_right_aligned(1, True)
feeds.set_show_separator(False)
feeds.set_column_title(0, "Feeds")
feeds.set_show_title(True)

feeds.connect('active_changed', feed_active_changed)
feeds.connect('key_pressed', feed_key_pressed)
gnt.unset_flag(feeds, 256)   # Fix the width

####
# ]]] The list of feeds
###

####
# [[[ The list of items in the feed
####

#
# The active item in the feed-item list has changed. Update the
# summary content.
def item_active_changed(tree, old):
    details.clear()
    if not tree.active:
        return
    item = tree.active
    details.append_text_with_flags(str(item.title) + "\n", gnt.TEXT_FLAG_BOLD)
    details.append_text_with_flags("Link: ", gnt.TEXT_FLAG_BOLD)
    details.append_text_with_flags(str(item.link) + "\n", gnt.TEXT_FLAG_UNDERLINE)
    details.append_text_with_flags("Date: ", gnt.TEXT_FLAG_BOLD)
    details.append_text_with_flags(str(item.date) + "\n", gnt.TEXT_FLAG_NORMAL)
    details.append_text_with_flags("\n", gnt.TEXT_FLAG_NORMAL)
    parser = gnthtml.GParser(details)
    parser.parse(str(item.summary))
    item.mark_unread(False)

    if old and old.unread:   # If the last selected item is marked 'unread', then make sure it's bold
        items.set_row_flags(old, gnt.TEXT_FLAG_BOLD)

#
# Look for action keys in the feed-item list.
def item_key_pressed(tree, text):
    if tree.is_searching():
        return
    current = tree.get_selection_data()
    if text == 'M':     # Mark all of the items 'read'
        feed = feeds.active
        if feed:
            feed.mark_read()
    elif text == 'm':     # Mark the current item 'read'
        current.mark_unread(False)
        tree.perform_action_key('j')
    elif text == 'U':     # Mark the current item 'unread'
        current.mark_unread(True)
    elif text == 'd':
        current.remove()
        tree.perform_action_key('j')
    else:
        return False
    return True

items = RssTree()
items.set_property('columns', 2)
items.set_col_width(0, 40)
items.set_col_width(1, 11)
items.set_column_resizable(1, False)
items.set_column_title(0, "Items")
items.set_column_title(1, "Date")
items.set_show_title(True)
items.connect('key_pressed', item_key_pressed)
items.connect('active_changed', item_active_changed)

####
# ]]] The list of items in the feed
####

#
# Update the title of the items list depending on the selection in the feed list
def update_items_title():
    feed = feeds.active
    if feed:
        items.set_column_title(0, str(feed.title) + ": " + str(feed.unread) + "(" + str(len(feed.items)) + ")")
    else:
        items.set_column_title(0, "Items")
    items.draw()

# The container on the top
line = gnt.Line(vertical = False)

# The textview to show the details of a feed
details = gnt.TextView()
details.set_take_focus(True)
details.set_flag(gnt.TEXT_VIEW_TOP_ALIGN)
details.attach_scroll_widget(details)

# Make it look nice
s = feeds.get_size()
size = gnt.screen_size()
size[0] = size[0] - s[0]
items.set_size(size[0], size[1] / 2)
details.set_size(size[0], size[1] / 2)

# Category tree
cat = gnt.Tree()
cat.set_property('columns', 1)
cat.set_column_title(0, 'Category')
cat.set_show_title(True)
gnt.set_flag(cat, 8)    # remove borders

box = gnt.Box(homo = False, vert = False)
box.set_pad(0)

vbox = gnt.Box(homo = False, vert = True)
vbox.set_pad(0)
vbox.add_widget(feeds)
vbox.add_widget(gnt.Line(False))
vbox.add_widget(cat)
box.add_widget(vbox)

box.add_widget(gnt.Line(True))

vbox = gnt.Box(homo = False, vert = True)
vbox.set_pad(0)
vbox.add_widget(items)
vbox.add_widget(gnt.Line(False))
vbox.add_widget(details)
box.add_widget(vbox)

win.add_widget(box)
win.show()

def update_feed_title(feed, property):
    if property.name == 'title':
        if feed.customtitle:
            title = feed.customtitle
        else:
            title = feed.title
        feeds.change_text(feed, 0, title)
    elif property.name == 'unread':
        feeds.change_text(feed, 1, str(feed.unread) + "(" + str(len(feed.items)) + ")")
        flag = 0
        if feeds.active == feed:
            flag = gnt.TEXT_FLAG_UNDERLINE
            update_items_title()
        if feed.unread > 0:
            flag = flag | gnt.TEXT_FLAG_BOLD
        feeds.set_row_flags(feed, flag)

# populate everything
for feed in gntrss.feeds:
    feed.refresh()
    feed.set_auto_refresh(True)
    add_feed(feed)

gnt.gnt_register_action("Stuff", add_new_feed)
gnt.gnt_main()

gnt.gnt_quit()

