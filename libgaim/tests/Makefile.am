if HAVE_CHECK
TESTS = check_libgaim
check_PROGRAMS = check_libgaim
check_libgaim_SOURCES = check_libgaim.c $(top_builddir)/libgaim/gaim.h
check_libgaim_CFLAGS = @CHECK_CFLAGS@ $(GLIB_CFLAGS)
check_libgaim_LDADD = @CHECK_LIBS@ $(GLIB_LIBS) $(top_builddir)/libgaim/libgaim.la
endif
