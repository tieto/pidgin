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
This file deals with the rss parsing part (feedparser) of the application
"""

import os
import tempfile, urllib2
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
        try:
            "Apparently some feed items don't have any dates in them"
            self.date = item['date']
            self.date_parsed = item['date_parsed']
        except:
            item['date'] = self.date = time.ctime()
            self.date_parsed = feedparser._parse_date(self.date)

        self.title = item['title'].encode('utf8')
        self.summary = item['summary'].encode('utf8')
        self.link = item['link']
        self.parent = parent
        self.unread = True

    def __del__(self):
        pass

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
    return str(item['title'])

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

    def __init__(self, feed):
        self.__gobject_init__()
        url = feed['link']
        name = feed['name']
        self.url = url           # The url of the feed itself
        self.link = url          # The web page associated with the feed
        self.desc = url
        self.title = (name, url)[not name]
        self.customtitle = name
        self.unread = 0
        self.items = []
        self.hash = {}
        self.pending = False
        self._refresh = {'time' : 30, 'id' : 0}

    def __del__(self):
        pass

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

    def set_result(self, result):
        # XXX Look at result['bozo'] first, and emit some signal that the UI can use
        # to indicate (dim the row?) that the feed has invalid XML format or something

        try:
            channel = result['channel']
            self.set_property('link', channel['link'])
            self.set_property('desc', channel['description'])
            self.set_property('title', channel['title'])
            items = result['items']
        except:
            items = ()

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

        if unread != self.unread:
            self.set_property('unread', unread)

        for hv in tmp:
            self.items.remove(tmp[hv])
            tmp[hv].remove()
            "Also notify the UI about the count change"

        self.pending = False
        return False

    def refresh(self):
        if self.pending:
            return
        self.pending = True
        FeedReader(self).run()
        return True

    def mark_read(self):
        for item in self.items:
            item.mark_unread(False)

    def set_auto_refresh(self, auto):
        if auto:
            if self._refresh['id']:
                return
            if self._refresh['time'] < 1:
                self._refresh['time'] = 1
            self.id = gobject.timeout_add(self._refresh['time'] * 1000 * 60, self.refresh)
        else:
            if not self._refresh['id']:
                return
            gobject.source_remove(self._refresh['id'])
            self._refresh['id'] = 0

gobject.type_register(Feed)

"""
The FeedReader updates a Feed. It fork()s off a child to avoid blocking.
"""
class FeedReader:
    def __init__(self, feed):
        self.feed = feed

    def reap_child(self, pid, status):
        result = feedparser.parse(self.tmpfile.name)
        self.tmpfile.close()
        self.feed.set_result(result)

    def run(self):
        self.tmpfile = tempfile.NamedTemporaryFile()
        self.pid = os.fork()
        if self.pid == 0:
            tmp = urllib2.urlopen(self.feed.url)
            content = tmp.read()
            tmp.close()
            self.tmpfile.write(content)
            self.tmpfile.flush()
            # Do NOT close tmpfile here
            os._exit(os.EX_OK)
        gobject.child_watch_add(self.pid, self.reap_child)

feeds = []
urls = (
    {'name': '/.',
     'link': "http://rss.slashdot.org/Slashdot/slashdot"},
    {'name': 'KernelTrap',
     'link': "http://kerneltrap.org/node/feed"},
    {'name': None,
     'link': "http://pidgin.im/rss.php"},
    {'name': "F1",
     'link': "http://www.formula1.com/rss/news/latest.rss"},
    {'name': "Freshmeat",
     'link': "http://www.pheedo.com/f/freshmeatnet_announcements_unix"},
    {'name': "Cricinfo",
     'link': "http://www.cricinfo.com/rss/livescores.xml"}
)

for url in urls:
    feed = Feed(url)
    feeds.append(feed)

