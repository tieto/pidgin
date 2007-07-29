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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
USA
"""

"""
This file deals with the rss parsing part (feedparser) of the application
"""

import threading
import feedparser
import gobject
import sys
import time

##
# The FeedItem class. It will update emit 'delete' signal when it's
# destroyed.
##
class FeedItem(gobject.GObject):
    __gproperties__ = {
        'unread' : (gobject.TYPE_BOOLEAN, 'read',
            'The unread state of the item.',
            False, gobject.PARAM_READWRITE)
    }
    __gsignals__ = {
        'delete' : (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (gobject.TYPE_OBJECT,))
    }
    def __init__(self, item, parent):
        self.__gobject_init__()
        self.date = item['date']
        self.date_parsed = item['date_parsed']
        self.title = item['title']
        self.summary = item['summary']
        self.link = item['link']
        self.parent = parent
        self.unread = True

    def remove(self):
        self.emit('delete', self.parent)
        if self.unread:
            self.parent.set_property('unread', self.parent.unread - 1)

    def do_set_property(self, property, value):
        if property.name == 'unread':
            self.unread = value

    def mark_unread(self, unread):
        if self.unread == unread:
            return
        self.set_property('unread', unread)

gobject.type_register(FeedItem)

def item_hash(item):
    return str(item['date'] + item['title'])

"""
The Feed class. It will update the 'link', 'title', 'desc' and 'items'
attributes if/when they are updated (triggering 'notify::<attr>' signal)

TODO:
    - Add a 'count' attribute
    - Each feed will have a 'uidata', which will be its display window
    - Look into 'category'. Is it something that feed defines, or the user?
    - Have separate refresh times for each feed.
    - Have 'priority' for each feed. (somewhat like category, perhaps?)
"""
class Feed(gobject.GObject):
    __gproperties__ = {
        'link' : (gobject.TYPE_STRING, 'link',
            'The web page this feed is associated with.',
            '...', gobject.PARAM_READWRITE),
        'title' : (gobject.TYPE_STRING, 'title',
            'The title of the feed.',
            '...', gobject.PARAM_READWRITE),
        'desc' : (gobject.TYPE_STRING, 'description',
            'The description for the feed.',
            '...', gobject.PARAM_READWRITE),
        'items' : (gobject.TYPE_POINTER, 'items',
            'The items in the feed.', gobject.PARAM_READWRITE),
        'unread' : (gobject.TYPE_INT, 'unread',
            'Number of unread items in the feed.', 0, 10000, 0, gobject.PARAM_READWRITE)
    }
    __gsignals__ = {
        'added' : (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (gobject.TYPE_OBJECT,))
    }

    def __init__(self, url):
        self.__gobject_init__()
        self.url = url           # The url of the feed itself
        self.link = url          # The web page associated with the feed
        self.desc = url
        self.title = url
        self.unread = 0
        self.timer = 0
        self.items = []
        self.hash = {}

    def do_set_property(self, property, value):
        if property.name == 'link':
            self.link = value
        elif property.name == 'desc':
            self.desc = value
        elif property.name == 'title':
            self.title = value
        elif property.name == 'unread':
            self.unread = value
        pass

    def check_thread_for_death(self):
        #if self.thread.running:
        #    sys.stderr.write(time.ctime() + "continue")
        #    return True
        # The thread has ended!!
        #result = self.thread.result
        #self.thread = None
        result = feedparser.parse(self.url)
        # XXX Look at result['bozo'] first, and emit some signal that the UI can use
        # to indicate (dim the row?) that the feed has invalid XML format or something

        channel = result['channel']
        self.set_property('link', channel['link'])
        self.set_property('desc', channel['description'])
        self.set_property('title', channel['title'])
        self.timer = 0
        items = result['items']
        tmp = {}
        for item in self.items:
            tmp[hash(item)] = item

        unread = self.unread
        for item in items:
            try:
                exist = self.hash[item_hash(item)]
                del tmp[hash(exist)]
            except:
                itm = FeedItem(item, self)
                self.items.append(itm)
                self.emit('added', itm)
                self.hash[item_hash(item)] = itm
                unread = unread + 1

        for hv in tmp:
            tmp[hv].remove()

        if unread != self.unread:
            self.set_property('unread', unread)
        return False

    def refresh(self):
        #if self.thread == 0:
        #   self.thread = FeedReader(self)
        #   self.thread.start()
        if self.timer == 0:
            self.timer = gobject.timeout_add(1000, self.check_thread_for_death)

gobject.type_register(Feed)

##
# A FeedReader class, which is threaded to make sure it doesn't freeze the ui
# (this thing doesn't quite work ... yet)
##
class FeedReader(threading.Thread):
    def __init__(self, feed):
        self.feed = feed
        self.running = True
        threading.Thread.__init__(self)

    def run(self):
        sys.stderr.write(str(time.ctime()) + " STARTED!!!\n\n\n")
        self.running = True
        self.result = feedparser.parse(self.feed.url)
        self.running = False
        sys.stderr.write(str(time.ctime()) + " DONE!!!\n\n\n")

feeds = []
urls = ("http://rss.slashdot.org/Slashdot/slashdot",
        "http://www.python.org/channews.rdf",
        "http://pidgin.im/rss.php",
        "http://kerneltrap.org/node/feed"
        )

for url in urls:
    feed = Feed(url)
    feeds.append(feed)

