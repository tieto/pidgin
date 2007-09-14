#!/usr/bin/env python

import gnt, sys

def file_selected(widget, path, file, null):
	sys.stderr.write(path + " " + file)
	list = widget.get_selected_multi_files()
	for i in list:
		sys.stderr.write(i)

gnt.gnt_init()

win = gnt.Window()

files = gnt.FileSel()
files.set_multi_select(True)
files.set_title("Files")
files.connect("file_selected", file_selected, None)

files.show()

gnt.gnt_main()

gnt.gnt_quit()
