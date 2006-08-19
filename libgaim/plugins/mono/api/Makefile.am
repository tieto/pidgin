monodir=$(libdir)/gaim
mono_sources = \
	GaimPlugin.cs \
	Debug.cs \
	BuddyList.cs \
	Buddy.cs \
	Signal.cs \
	Event.cs \
	Status.cs

EXTRA_DIST = $(mono_sources)

mono_SCRIPTS = GaimAPI.dll

mono_build_sources = $(addprefix $(srcdir)/, $(mono_sources))

all: $(mono_SCRIPTS)

$(mono_SCRIPTS): $(mono_build_sources)
	mcs -t:library -out:$(mono_SCRIPTS) $(mono_build_sources)

clean-local:
	rm -rf $(mono_SCRIPTS)
