AM_CFLAGS = $(DEBUG_CFLAGS)

bin_PROGRAMS = \
	xdg-play

xdg_play_CPPFLAGS = -DNAME=\"xdg-play\"
xdg_play_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS) $(CANBERRA_CFLAGS)
xdg_play_SOURCES = xdg-play.c
xdg_play_LDADD = $(GLIB2_LIBS) $(CANBERRA_LIBS)

dist_bin_SCRIPTS = \
	xdg-sound \
	xdg-sound-list \
	xdg-sound-whereis \
	xdg-sound-which

