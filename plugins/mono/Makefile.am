SUBDIRS = api loader

TARGETS = MPlugin.dll GetBuddyBack.dll

all: $(TARGETS)
	
install-exec-am: $(TARGETS)
	$(INSTALL) -m 644 $(TARGETS) $(libdir)/gaim

clean-generic:
	rm -rf $(TARGETS)

SUFFIXES = .cs .dll
.cs.dll: api/GaimAPI.dll
	mcs -t:library -lib:./api -out:$*.dll -r:GaimAPI.dll $*.cs
