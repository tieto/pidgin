monodir=$(libdir)/gaim
mono_sources = \
	BlistNode.cs \
	BuddyList.cs \
	Buddy.cs \
	Contact.cs \
	Debug.cs \
	Event.cs \
	GaimPlugin.cs \
	Group.cs \
	Signal.cs \
	Status.cs

EXTRA_DIST = $(mono_sources)

mono_SCRIPTS = GaimAPI.dll

mono_build_sources = $(addprefix $(srcdir)/, $(mono_sources))

all: $(mono_SCRIPTS)

$(mono_SCRIPTS): $(mono_build_sources)
	mcs -t:library -out:$(mono_SCRIPTS) $(mono_build_sources)

clean-local:
	rm -rf $(mono_SCRIPTS)
