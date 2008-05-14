pidginsmileypix_in_files = none.theme.in

if INSTALL_PIXMAPS
pidginsmileypixdir = $(datadir)/pixmaps/pidgin/emotes/none
pidginsmileypix_DATA = theme

theme: none.theme.in
	sed -e 's/^_Name=/Name=/' \
	    -e 's/^_Description=/Description=/' \
	    -e 's/^_Author=/Author=/' \
	$< > $@
endif

EXTRA_DIST = \
	$(pidginsmileypix_in_files) \
	Makefile.mingw \
	theme
