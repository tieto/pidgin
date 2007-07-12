pidginsmileypixdir = $(datadir)/pixmaps/pidgin/emotes/none
pidginsmileypix_in_files = none.theme.in
pidginsmileypix_DATA = theme

theme: none.theme.in
	sed -e 's/^_Name=/Name=/' \
	    -e 's/^_Description=/Description=/' \
	    -e 's/^_Author=/Author=/' \
	$< > $@

EXTRA_DIST = $(pidginsmileypix_DATA) \
	$(pidginsmileypix_in_files) \
	Makefile.mingw
