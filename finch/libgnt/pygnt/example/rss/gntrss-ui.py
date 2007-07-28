#!/usr/bin/env python

# A very simple and stupid RSS reader
#
# Uses the Universal Feed Parser
#

import gntrss
import gnt
import gobject
import sys

__version__ = "0.0.1alpha"
__author__ = "Sadrul Habib Chowdhury (sadrul@pidgin.im)"
__copyright__ = "Copyright 2007, Sadrul Habib Chowdhury"
__license__ = "GPL" # see full license statement below

gnt.gnt_init()

class RssTree(gnt.Tree):
    __gsignals__ = {
        'active_changed' : (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (gobject.TYPE_OBJECT,)),
        'key_pressed' : 'override'
    }

    def __init__(self):
        self.active = None
        gnt.Tree.__init__(self)

    def set_active(self, active):
        if self.active == active:
            return
        if self.active:
            self.set_row_flags(self.active, gnt.TEXT_FLAG_NORMAL)
        old = self.active
        self.active = active
        self.set_row_flags(self.active, gnt.TEXT_FLAG_UNDERLINE)
        self.emit('active_changed', old)

    def do_key_pressed(self, text):
        if text == '\r':
            now = self.get_selection_data()
            self.set_active(now)
            return True
        return False

gobject.type_register(RssTree)

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
        if feeds.active != item.parent:
            return
        if item.unread:
            item.parent.unread = item.parent.unread + 1
            items.set_row_flags(item, gnt.TEXT_FLAG_BOLD)
        else:
            item.parent.unread = item.parent.unread - 1
            items.set_row_flags(item, gnt.TEXT_FLAG_NORMAL)
        item.parent.notify('unread')

def add_feed_item(item):
    currentfeed = feeds.active
    if item.parent != currentfeed:
        return
    items.add_row_after(item, [str(item.title)], None, None)
    if item.unread:
        items.set_row_flags(item, gnt.TEXT_FLAG_BOLD)
    if not item.get_data('gntrss-connected'):
        item.set_data('gntrss-connected', True)
        item.connect('notify', update_feed_item)
        item.connect('delete', remove_item)

#
# ]]] Generic feed/item callbacks
#


####
# [[[ The list of feeds
###

#
# The active row in the feed-list has changed. Update the feed-item table.
def feed_active_changed(tree, old):
    items.remove_all()
    if not tree.active:
        return
    for item in tree.active.items:
        add_feed_item(item)

#
# Check for the action keys and decide how to deal with them.
def feed_key_pressed(tree, text):
    if text == 'r':
        feed = tree.get_selection_data()
        tree.perform_action_key('j')
        #tree.perform_action('move-down')
        feed.refresh()
    elif text == 'R':
        feeds = tree.get_rows()
        for feed in feeds:
            feed.refresh()
    else:
        return False
    return True

feeds = RssTree()
feeds.set_property('columns', 2)
feeds.set_col_width(0, 20)
feeds.set_col_width(1, 4)
feeds.set_column_resizable(0, False)
feeds.set_column_resizable(1, False)
feeds.set_column_is_right_aligned(1, True)
feeds.set_show_separator(False)

feeds.connect('active_changed', feed_active_changed)
feeds.connect('key_pressed', feed_key_pressed)

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
    details.append_text_with_flags(str(item.summary), gnt.TEXT_FLAG_NORMAL)
    details.scroll(0)
    if item.unread:
        item.set_property('unread', False)
    win.give_focus_to_child(browser)

#
# Look for action keys in the feed-item list.
def item_key_pressed(tree, text):
    current = tree.get_selection_data()
    if text == 'M':     # Mark all of the items 'read'
        all = tree.get_rows()
        for item in all:
            item.unread = False
    elif text == 'm':     # Mark the current item 'read'
        if current.unread:
            current.set_property('unread', False)
    elif text == 'U':     # Mark the current item 'unread'
        if not current.unread:
            current.set_property('unread', True)
    else:
        return False
    return True

items = RssTree()
items.set_property('columns', 1)
items.set_col_width(0, 40)
items.connect('key_pressed', item_key_pressed)
items.connect('active_changed', item_active_changed)

####
# ]]] The list of items in the feed
####

# The container on the top
box = gnt.Box(homo = False, vert = False)
box.set_pad(0)
box.add_widget(feeds)
box.add_widget(items)

win.add_widget(box)

win.add_widget(gnt.Line(vertical = False))

# The textview to show the details of a feed
details = gnt.TextView()

win.add_widget(details)

browser = gnt.Button("Open in Browser")
win.add_widget(browser)
details.attach_scroll_widget(browser)

win.show()

def update_feed_title(feed, property):
    if property.name == 'title':
        feeds.change_text(feed, 0, feed.title)
    elif property.name == 'unread':
        feeds.change_text(feed, 1, str(feed.unread))

# populate everything
for feed in gntrss.feeds:
    add_feed(feed)

gnt.gnt_main()

gnt.gnt_quit()

