
themetemplatedir = $(datadir)/pidgin/theme
themetemplate_DATA = Template.html
themecontentsdir = $(themetemplatedir)/Contents
themecontents_DATA = Contents/Info.plist
themeresourcesdir = $(themecontentsdir)/Resources
themeresources_DATA = Contents/Resources/Content.html \
                      Contents/Resources/Status.html \
                      Contents/Resources/main.css
themevariantsdir = $(themeresourcesdir)/Variants
themevariants_DATA = Contents/Resources/Variants/Default.css \
                     Contents/Resources/Variants/No-Timestamps.css

EXTRA_DIST = \
	$(themetemplate_DATA) \
	$(themecontents_DATA) \
	$(themeresources_DATA) \
	$(themevariants_DATA)

