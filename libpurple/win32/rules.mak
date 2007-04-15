# Rules on how to make object files from various sources

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDE_PATHS) -o $@ -c $<

%.c: %.xs
	$(PERL) $(EXTUTILS)/xsubpp -typemap $(EXTUTILS)/typemap -typemap $(PURPLE_PERL_TOP)/common/typemap $< > $@

%.o: %.rc
	$(WINDRES) -I$(PURPLE_TOP) -i $< -o $@
