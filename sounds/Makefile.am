noinst_PROGRAMS = au2h

au2h_SOURCES = au2h.c

EXTRA_DIST = BuddyArrive.au BuddyLeave.au Send.au Receive.au RedAlert.au

all: BuddyArrive.h BuddyLeave.h Send.h Receive.h RedAlert.h

clean distclean:
	rm -f BuddyArrive.h BuddyLeave.h Send.h Receive.h RedAlert.h

SUFFIXES = .au .h

BuddyArrive.h BuddyLeave.h Send.h Receive.h RedAlert.h: au2h

.au.h: au2h
	./au2h $< $@
