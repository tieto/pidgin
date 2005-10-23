MONOSOURCES = \
	GaimPlugin.cs \
	Debug.cs \
	BuddyList.cs \
	Buddy.cs \
	Signal.cs \
	Event.cs

TARGET=GaimAPI.dll

all: $(TARGET)

$(TARGET): $(MONOSOURCES)
	mcs -t:library -out:$(TARGET) $(MONOSOURCES)

.PHONY: install clean

install: $(TARGET)
	$(INSTALL) -m 644 $(TARGET) $(libdir)/gaim
	
clean:
	rm -rf $(TARGET)
