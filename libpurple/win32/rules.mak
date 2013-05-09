# Rules on how to make object files from various sources

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDE_PATHS) -o $@ -c $<

%.c: %.xs
	$(TAG) "PERL" $(PERL) -MExtUtils::ParseXS -e 'ExtUtils::ParseXS::process_file(filename => "$<", output => "$@", typemap => "$(PURPLE_PERL_TOP)/common/typemap");'

%.o: %.rc
	$(WINDRES) -I$(PURPLE_TOP) -i $< -o $@

%.desktop: %.desktop.in $(wildcard $(PIDGIN_TREE_TOP)/po/*.po)
	LC_ALL=C $(PERL) $(INTLTOOL_MERGE) -d -u -c $(PIDGIN_TREE_TOP)/po/.intltool-merge-cache $(PIDGIN_TREE_TOP)/po $< $@

%.html.h: %.html
	@echo -e "  GEN\t$@"
	@echo "static const char $*_html[] = {" > $@
	@sed -e 's/^[ 	]\+//g' -e 's/[ 	]\+/ /g' $< | xxd -i | sed -e 's/\(0x[0-9a-f][0-9a-f]\)$$/\1, 0x00/' >> $@
	@echo "};" >> $@
