#!/usr/bin/env python
import gobject
import gnt

class MyObject(gobject.GObject):
    __gproperties__ = {
        'mytype': (gobject.TYPE_INT, 'mytype', 'the type of the object',
                0, 10000, 0, gobject.PARAM_READWRITE),
        'string': (gobject.TYPE_STRING, 'string property', 'the string',
                None, gobject.PARAM_READWRITE),
        'gobject': (gobject.TYPE_OBJECT, 'object property', 'the object',
                gobject.PARAM_READWRITE),
    }

    def __init__(self, type = 'string', value = None):
        self.__gobject_init__()
        self.set_property(type, value)

    def __del__(self):
        pass

    def do_set_property(self, pspec, value):
        if pspec.name == 'string':
            self.string = value
            self.type = gobject.TYPE_STRING
        elif pspec.name == 'gobject':
            self.gobject = value
            self.type = gobject.TYPE_OBJECT
        else:
            raise AttributeError, 'unknown property %s' % pspec.name
    def do_get_property(self, pspec):
        if pspec.name == 'string':
            return self.string
        elif pspec.name == 'gobject':
            return self.gobject
        elif pspec.name == 'mytype':
            return self.type
        else:
            raise AttributeError, 'unknown property %s' % pspec.name
gobject.type_register(MyObject)

def button_activate(button, tree):
    list = tree.get_selection_text_list()
    ent = tree.get_selection_data()
    if ent.type == gobject.TYPE_STRING:
        str = ""
        for i in list:
            str = str + i
        entry.set_text("clicked!!!" + str)
    elif ent.type == gobject.TYPE_OBJECT:
        ent.gobject.set_text("mwhahaha!!!")

gnt.gnt_init()

win = gnt.Window()

entry = gnt.Entry("")
obj = MyObject()
obj.set_property('gobject', entry)

win.add_widget(entry)
win.set_title("Entry")

button = gnt.Button("Click!")
win.add_widget(button)

tree = gnt.Tree()
tree.set_property("columns", 1)
win.add_widget(tree)

# so random non-string values can be used as the key for a row in a GntTree!
last = None
for i in range(1, 100):
    key = MyObject('string', str(i))
    tree.add_row_after(key, [str(i)], None, last)
    last = key

tree.add_row_after(MyObject('gobject', entry), ["asd"], None, None)
tree.add_row_after(MyObject('string', "b"), ["123"], MyObject('gobject', entry), None)

button.connect("activate", button_activate, tree)

tv = gnt.TextView()

win.add_widget(tv)
tv.append_text_with_flags("What up!!", gnt.TEXT_FLAG_BOLD)

win.show()

gnt.gnt_main()

gnt.gnt_quit()

