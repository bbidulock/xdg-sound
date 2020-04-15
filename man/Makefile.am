
xdg_man1_files = \
	xdg-play.man \
	xdg-sound.man \
	xdg-sound-which.man \
	xdg-sound-whereis.man \
	xdg-sound-list.man 

nodist_man1_MANS = $(xdg_man1_files)
CLEANFILES = $(xdg_man1_files)

EXTRA_DIST = \
	xdg-play.pod \
	xdg-sound.pod \
	xdg-sound-which.pod \
	xdg-sound-whereis.pod \
	xdg-sound-list.pod 

$(xdg_man1_files): %.man: %.pod
	$(AM_V_GEN)pod2man --center="User Commands" \
		--date="$(DATE)" \
		--errors="none" \
		--fixed="CW" \
		--fixedbold="CB" \
		--fixeditalic="CI" \
		--fixedbolditalic="CX" \
		--nourls \
		--release="$(PACKAGE) $(VERSION)" \
		--section="1" \
		$< >$@

