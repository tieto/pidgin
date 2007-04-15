#!/bin/sh
FILES="
	gntwidget.h
	gntbindable.h
	gntbox.h
	gntbutton.h
	gntcheckbox.h
	gntclipboard.h
	gntcolors.h
	gntcombobox.h
	gntentry.h
	gntfilesel.h
	gntkeys.h
	gntlabel.h
	gntline.h
	gntmarshal.h
	gntmenu.h
	gntmenuitem.h
	gntmenuitemcheck.h
	gntstyle.h
	gnttextview.h
	gnttree.h
	gntutils.h
	gntwindow.h
	gntwm.h
	gnt.h"

# Generate the def file
rm gnt.def
for file in $FILES
do
	python /usr/share/pygtk/2.0/codegen/h2def.py ../$file >> gnt.def
done

# Remove the definitions about the enums
ENUMS="
GNT_TYPE_ALIGNMENT
GNT_TYPE_COLOR_TYPE
GNT_TYPE_MENU_TYPE
GNT_TYPE_STYLE
GNT_TYPE_KEY_PRESS_MODE
GNT_TYPE_ENTRY_FLAG
GNT_TYPE_TEXT_FORMAT_FLAGS
"

for enum in $ENUMS
do
	sed -ie s/^.*gtype-id\ \"$enum\".*$//g gnt.def
done


