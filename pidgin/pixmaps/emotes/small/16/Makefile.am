SMILEYS = \
    angel.png \
    angry.png \
    beer.png \
    camera.png \
    cigarette.png \
    coffee.png \
    confused.png \
    console.png \
    cool.png \
    cross.png \
    crying.png \
    devil.png \
    dont-know.png \
    grin.png \
    hug-left.png \
    hug-right.png \
    kiss.png \
    love.png \
    meeting.png \
    musical-note.png \
    nerdy.png \
    neutral.png \
    party.png \
    phone.png \
    plate.png \
    question.png \
    sad.png \
    shame.png \
    shock.png \
    sick.png \
    silent.png \
    sleepy.png \
    smile-big.png \
    smile.png \
    thinking.png \
    tongue.png \
    tv.png \
    uhm-yeah.png \
    wink.png \
    yawn.png


pidginsmileypix_in_files = small.theme.in

if INSTALL_PIXMAPS
pidginsmileypixdir = $(datadir)/pixmaps/pidgin/emotes/small
pidginsmileypix_DATA = \
        $(SMILEYS) \
        theme

theme: small.theme.in
	sed -e 's/^_Name=/Name=/' \
	    -e 's/^_Description=/Description=/' \
	    -e 's/^_Author=/Author=/' \
	$< > $@
endif

EXTRA_DIST = $(SMILEYS) $(pidginsmileypix_in_files) theme
