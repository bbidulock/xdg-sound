AM_CFLAGS = $(DEBUG_CFLAGS)

bin_PROGRAMS = \
	xdg-play \
	xdg-sound-list \
	xdg-sound-whereis \
	xdg-sound-which

xdg_play_CPPFLAGS = -DNAME=\"xdg-play\"
xdg_play_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS) $(CANBERRA_CFLAGS)
xdg_play_SOURCES = xdg-play.c
xdg_play_LDADD = $(GLIB2_LIBS) $(CANBERRA_LIBS)

xdg_sound_list_CPPFLAGS = -DNAME=\"xdg-sound-list\"
xdg_sound_list_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS) $(CANBERRA_CFLAGS)
xdg_sound_list_SOURCES = xdg-sound-list.c
xdg_sound_list_LDADD = $(GLIB2_LIBS) $(CANBERRA_LIBS)

xdg_sound_whereis_CPPFLAGS = -DNAME=\"xdg-sound-whereis\"
xdg_sound_whereis_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS) $(CANBERRA_CFLAGS)
xdg_sound_whereis_SOURCES = xdg-sound-whereis.c
xdg_sound_whereis_LDADD = $(GLIB2_LIBS) $(CANBERRA_LIBS)

xdg_sound_which_CPPFLAGS = -DNAME=\"xdg-sound-which\"
xdg_sound_which_CFLAGS = $(AM_CFLAGS) $(GLIB2_CFLAGS) $(CANBERRA_CFLAGS)
xdg_sound_which_SOURCES = xdg-sound-which.c
xdg_sound_which_LDADD = $(GLIB2_LIBS) $(CANBERRA_LIBS)

dist_bin_SCRIPTS = \
	xdg-sound

