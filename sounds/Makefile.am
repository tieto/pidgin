bin_PROGRAMS = au2h

au2h_SOURCES = au2h.c

EXTRA_DIST = BuddyArrive.au BuddyLeave.au Send.au Receive.au

all: BuddyArrive.h BuddyLeave.h Send.h Receive.h

BuddyArrive.h: au2h BuddyArrive.au
	./au2h BuddyArrive.au BuddyArrive.h

BuddyLeave.h: au2h BuddyLeave.au
	./au2h BuddyLeave.au BuddyLeave.h

Send.h: au2h Send.au                              
	./au2h Send.au Send.h                  

Receive.h: au2h Receive.au                              
	./au2h Receive.au Receive.h                  

if GNOMEAPPLET
gaimsoundsdir = $(datadir)/sounds/gaim
gaimsounds_DATA = $(EXTRA_DIST)
endif
