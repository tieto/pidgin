# Rules on how to make object files from various sources

%.o: %.c
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDE_PATHS) -o $@ -c $<

%.c: %.xs
	$(PERL) $(EXTUTILS)/xsubpp -typemap $(EXTUTILS)/typemap -typemap $(GAIM_LIB_PERL_TOP)/common/typemap $< > $@

%.o: %.rc
	$(WINDRES) -I$(GAIM_LIB_TOP) -i $< -o $@
