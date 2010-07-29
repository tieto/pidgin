# These are mood images that are NOT also used in the smiley theme.
MOODS = \
    afraid.png \
    bathing.png \
    cinema.png \
    disappointed.png \
    embarrassed.png \
    grumpy.png \
    hot.png \
    internet.png \
    invincible.png \
    music.png \
    restroom.png \
    search.png \
    shopping.png \
    studying.png \
    suit.png \
    surfing.png \
    typing.png \
    working.png \
    writing.png

SMILEYS = \
    amorous.png \
    angel.png \
    angry.png \
    beer.png \
    bored.png \
    boy.png \
    camera.png \
    chilli.png \
    cigarette.png \
    coffee.png \
    confused.png \
    console.png \
    cold.png \
    cool.png \
    cross.png \
    crying.png \
    devil.png \
    dont-know.png \
    drool.png \
    excited.png \
    excruciating.png \
    eyeroll.png \
    girl.png \
    grin.png \
    happy.png \
    hug-left.png \
    hug-right.png \
    hungry.png \
    in_love.png \
    kiss.png \
    lamp.png \
    lying.png \
    meeting.png \
    mobile.png \
    mrgreen.png \
    musical-note.png \
    nerdy.png \
    neutral.png \
    party.png \
    phone.png \
    pirate.png \
    pissed-off.png \
    plate.png \
    question.png \
    rose.png \
    sad.png \
    shame.png \
    shocked.png \
    shut-mouth.png  \
    sick.png \
    silent.png \
    sleeping.png \
    sleepy.png \
    star.png \
    stressed.png \
    thinking.png \
    tongue.png \
    tv.png \
    uhm-yeah.png \
    wink.png


pidginsmileypix_in_files = small.theme.in

if INSTALL_PIXMAPS
pidginsmileypixdir = $(datadir)/pixmaps/pidgin/emotes/small
pidginsmileypix_DATA = \
        $(MOODS) \
        $(SMILEYS) \
        theme

theme: small.theme.in
	sed -e 's/^_Name=/Name=/' \
	    -e 's/^_Description=/Description=/' \
	    -e 's/^_Author=/Author=/' \
	$< > $@
endif

EXTRA_DIST = $(MOODS) $(SMILEYS) $(pidginsmileypix_in_files) theme
