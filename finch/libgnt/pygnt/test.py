#!/usr/bin/python
import gnt

def button_activate(button, tree):
	list = tree.get_selection_text_list()
	str = ""
	for i in list:
		str = str + i
	entry.set_text("clicked!!!" + str)

gnt.gnt_init()

win = gnt.Window()

entry = gnt.Entry("")

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
	tree.add_row_after(i, [str(i), ""], None, i-1)
tree.add_row_after(entry, ["asd"], None, None)
tree.add_row_after("b", ["123", ""], entry, None)

button.connect("activate", button_activate, tree)

win.show()

gnt.gnt_main()

gnt.gnt_quit()

