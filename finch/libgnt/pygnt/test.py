#!/usr/bin/python
import gnt

def button_activate(button, entry):
	entry.set_text("clicked!!!")

gnt.gnt_init()

win = gnt.Window()

entry = gnt.Entry("")

win.add_widget(entry)
win.set_title("Entry")

button = gnt.Button("Click!")
win.add_widget(button)

button.connect("activate", button_activate, entry)

win.show()

gnt.gnt_main()

gnt.gnt_quit()

