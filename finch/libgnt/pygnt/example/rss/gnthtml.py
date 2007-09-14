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
This file defines GParser, which is a simple HTML parser to display HTML
in a GntTextView nicely.
"""

import sgmllib
import gnt

class GParser(sgmllib.SGMLParser):
    def __init__(self, view):
        sgmllib.SGMLParser.__init__(self, False)
        self.link = None
        self.view = view
        self.flag = gnt.TEXT_FLAG_NORMAL

    def parse(self, s):
        self.feed(s)
        self.close()

    def unknown_starttag(self, tag, attrs):
        if tag in ["b", "i", "blockquote", "strong"]:
            self.flag = self.flag | gnt.TEXT_FLAG_BOLD
        elif tag in ["p", "hr", "br"]:
            self.view.append_text_with_flags("\n", self.flag)
        else:
            print tag

    def unknown_endtag(self, tag):
        if tag in ["b", "i", "blockquote", "strong"]:
            self.flag = self.flag & ~gnt.TEXT_FLAG_BOLD
        elif tag in ["p", "hr", "br"]:
            self.view.append_text_with_flags("\n", self.flag)
        else:
            print tag

    def start_u(self, attrs):
        self.flag = self.flag | gnt.TEXT_FLAG_UNDERLINE

    def end_u(self):
        self.flag = self.flag & ~gnt.TEXT_FLAG_UNDERLINE

    def start_a(self, attributes):
        for name, value in attributes:
            if name == "href":
                self.link = value

    def do_img(self, attrs):
        for name, value in attrs:
            if name == 'src':
                self.view.append_text_with_flags("[img:" + value + "]", self.flag)

    def end_a(self):
        if not self.link:
            return
        self.view.append_text_with_flags(" (", self.flag)
        self.view.append_text_with_flags(self.link, self.flag | gnt.TEXT_FLAG_UNDERLINE)
        self.view.append_text_with_flags(")", self.flag)
        self.link = None

    def handle_data(self, data):
        if len(data.strip()) == 0:
            return
        self.view.append_text_with_flags(data, self.flag)

