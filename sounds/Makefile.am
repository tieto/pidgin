noinst_PROGRAMS = au2h

au2h_SOURCES = au2h.c

EXTRA_DIST = BuddyArrive.au BuddyLeave.au Send.au Receive.au

all: BuddyArrive.h BuddyLeave.h Send.h Receive.h

clean distclean:
	rm -f BuddyArrive.h BuddyLeave.h Send.h Receive.h

BuddyArrive.h: BuddyArrive.au au2h
	./au2h $< $@

BuddyLeave.h: BuddyLeave.au au2h
	./au2h $< $@

Send.h: Send.au au2h
	./au2h $< $@

Receive.h: Receive.au au2h
	./au2h $< $@
