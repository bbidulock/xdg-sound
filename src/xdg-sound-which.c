/*****************************************************************************

 Copyright (c) 2008-2020  Monavacon Limited <http://www.monavacon.com/>
 Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>
 Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>

 All Rights Reserved.

 This program is free software: you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free Software
 Foundation, version 3 of the license.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 details.

 You should have received a copy of the GNU General Public License along with
 this program.  If not, see <http://www.gnu.org/licenses/>, or write to the
 Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 -----------------------------------------------------------------------------

 U.S. GOVERNMENT RESTRICTED RIGHTS.  If you are licensing this Software on
 behalf of the U.S. Government ("Government"), the following provisions apply
 to you.  If the Software is supplied by the Department of Defense ("DoD"), it
 is classified as "Commercial Computer Software" under paragraph 252.227-7014
 of the DoD Supplement to the Federal Acquisition Regulations ("DFARS") (or any
 successor regulations) and the Government is acquiring only the license rights
 granted herein (the license rights customarily provided to non-Government
 users).  If the Software is supplied to any unit or agency of the Government
 other than DoD, it is classified as "Restricted Computer Software" and the
 Government's rights in the Software are defined in paragraph 52.227-19 of the
 Federal Acquisition Regulations ("FAR") (or any successor regulations) or, in
 the cases of NASA, in paragraph 18.52.227-86 of the NASA Supplement to the FAR
 (or any successor regulations).

 -----------------------------------------------------------------------------

 Commercial licensing and support of this software is available from OpenSS7
 Corporation at a fee.  See http://www.openss7.com/

 *****************************************************************************/

#ifdef HAVE_CONFIG_H
#include "autoconf.h"
#endif

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif

/** @section Includes
  * @{ */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <fcntl.h>
#ifdef _GNU_SOURCE
#include <getopt.h>
#endif
#include <time.h>
#include <signal.h>
#include <syslog.h>
#include <sys/utsname.h>

#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <strings.h>
#include <regex.h>
#include <wordexp.h>
#include <execinfo.h>

#include <glib.h>
#include <glib/gi18n.h>

/** @} */

/** @section Debuggin Preamble
  * @{ */

const char *
_timestamp(void)
{
	static struct timeval tv = { 0, 0 };
	static struct tm tm = { 0, };
	static char buf[BUFSIZ];
	size_t len;

	gettimeofday(&tv, NULL);
	len = strftime(buf, sizeof(buf) - 1, "%b %d %T", gmtime_r(&tv.tv_sec, &tm));
	snprintf(buf + len, sizeof(buf) - len - 1, ".%06ld", tv.tv_usec);
	return buf;
}

#define XPRINTF(_args...) do { } while (0)

#define DPRINTF(_num, _args...) do { if (options.debug >= _num) { \
		fprintf(stderr, NAME "[%d]: D: [%s] %12s: +%4d : %s() : ", getpid(), _timestamp(), __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args); fflush(stderr); } } while (0)

#define EPRINTF(_args...) do { \
		fprintf(stderr, NAME "[%d]: E: [%s] %12s +%4d : %s() : ", getpid(), _timestamp(), __FILE__, __LINE__, __func__); \
		fprintf(stderr, _args); fflush(stderr); } while (0)

#define OPRINTF(_num, _args...) do { if (options.debug >= _num || options.output > _num || options.info) { \
		fprintf(stdout, NAME "[%d]: I: [%s] ", getpid(), _timestamp()); \
		fprintf(stdout, _args); fflush(stdout); } } while (0)

#define PTRACE(_num) do { if (options.debug >= _num || options.output >= _num) { \
		fprintf(stderr, NAME "[%d]: T: [%s] %12s +%4d : %s()\n", getpid(), _timestamp(), __FILE__, __LINE__, __func__); \
		fflush(stderr); } } while (0)

#define CPRINTF(_num, c, _args...) do { if (options.output > _num) { \
		fprintf(stdout, NAME "[%d]: C: 0x%08lx client (%c) ", getpid(), c->win, c->managed ?  'M' : 'U'); \
		fprintf(stdout, _args); fflush(stdout); } } while (0)

void
dumpstack(const char *file, const int line, const char *func)
{
	void *buffer[32];
	int nptr;
	char **strings;
	int i;

	if ((nptr = backtrace(buffer, 32)) && (strings = backtrace_symbols(buffer, nptr))) {
		for (i = 0; i < nptr; i++)
			fprintf(stderr, NAME "[%d]: E: [%s] %12s +%4d : %s() : <stack> %s\n",
				getpid(), _timestamp(), file, line, func, strings[i]);
		fflush(stderr);
	}
}

/** @} */

/** @section Definitions of Globals, Enumerations, Structures
  * @{ */

const char *program = NAME;

typedef enum {
	CommandDefault,
	CommandPlay,
	CommandShow,
	CommandWhereis,
	CommandWhich,
	CommandList,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} Command;

typedef struct {
	int debug;
	int output;
	int info;
	Command command;
	int noplay;
	int all;
	int skipdot;
	int skiptilde;
	int showdot;
	int showtilde;
	int standard;
	char **eventids;
	char *theme;
	char *profile;
	char *context;
} Options;

Options options = {
	.debug = 0,
	.output = 1,
	.info = 0,
	.command = CommandDefault,
	.noplay = 0,
	.all = 0,
	.skipdot = 0,
	.skiptilde = 0,
	.showdot = 0,
	.showtilde = 0,
	.standard = 0,
	.eventids = NULL,
	.theme = NULL,
	.profile = NULL,
	.context = NULL,
};

static const char *SoundThemeFields[] = {
	"Name",
	"Comment",
	"Inherits",
	"Directories",
	"Hidden",
	"Example",
	/* only in directory group */
	"Context",
	"OutputProfile",
	"DisplayName",
	NULL
};

typedef struct {
	char *path;
	char *Name;
	char *Comment;
	char *Inherits;
	char *Directories;
	char *Hidden;
	char *Example;
	char *Context;
	char *OutputProfile;
	char *DisplayName;
} Entry;

typedef struct Event {
	struct Event *next;
	char *key;
	char *file;
} Event;

Event *events = NULL;   /* single linked list */
Event **array = NULL;	/* for sorting */

static const char *StandardSoundNames[] = {
	/* Standard Alert Sounds */
	"network-connectivity-lost",
	"network-connectivity-error",
	"dialog-error",
	"battery-low",
	"suspend-error",
	"software-update-urgent",
	"power-unplug-battery-low",
	/* Standard Notifications Sounds */
	"message-new-instant",
	"message-new-email",
	"complete-media-burn",
	"complete-media-rip",
	"complete-download",
	"complete-copy",
	"complete-scan",
	"phone-incoming-call",
	"phone-outgoing-busy",
	"phone-hangup",
	"phone-failure",
	"network-connectivity-established",
	"system-bootup",
	"system-ready",
	"system-shutdown",
	"search-results",
	"search-results-empty",
	"desktop-login",
	"desktop-logout",
	"desktop-screen-lock",
	"desktop-screen-unlock",
	"service-login",
	"service-logout",
	"battery-caution",
	"battery-full",
	"dialog-warning",
	"dialog-information",
	"dialog-question",
	"software-update-available",
	"device-added",
	"device-removed",
	"window-new",
	"power-plug",
	"power-unplug",
	"suspend-start",
	"suspend-resume",
	"lid-open",
	"lid-close",
	"alarm-clock-elapsed",
	"window-attention-active",
	"window-attention-inactive",
	/* Standard Action Sounds */
	"phone-outgoing-calling",
	"message-sent-instant",
	"message-sent-email",
	"bell-terminal",
	"bell-window-system",
	"trash-empty",
	"item-deleted",
	"file-trash",
	"camera-shutter",
	"camera-focus",
	"screen-capture",
	"count-down",
	"completion-success",
	"completion-fail",
	"completion-partial",
	"completion-rotation",
	"audio-volume-change",
	"audio-channel-left",
	"audio-channel-right",
	"audio-channel-front-left",
	"audio-channel-front-right",
	"audio-channel-front-center",
	"audio-channel-rear-left",
	"audio-channel-rear-right",
	"audio-channel-rear-center",
	"audio-channel-lfe",
	"audio-channel-side-left",
	"audio-channel-side-right",
	"audio-test-signal",
	/* Standard Input Feedback Sounds */
	"window-close",
	"window-slide-in",
	"window-slide-out",
	"window-minimized",
	"window-unminimized",
	"window-maximized",
	"window-unmaximized",
	"window-inactive-click",
	"window-move-start",
	"window-move-end",
	"window-resize-start",
	"window-resize-end",
	"desktop-switch-left",
	"desktop-switch-right",
	"window-switch",
	"notebook-tab-changed",
	"scroll-up",
	"scroll-down",
	"scroll-left",
	"scroll-right",
	"scroll-up-end",
	"scroll-down-end",
	"scroll-left-end",
	"scroll-right-end",
	"dialog-ok",
	"dialog-cancel",
	"drag-start",
	"drag-accept",
	"drag-fail",
	"link-pressed",
	"link-released",
	"menu-click",
	"button-toggle-on",
	"button-toggle-off",
	"expander-toggle-on",
	"expander-toggle-off",
	"menu-popup",
	"menu-popdown",
	"menu-replace",
	"tooltip-popup",
	"tooltip-popdown",
	"item-selected",
	/* Standard Game Sounds */
	"game-over-winner",
	"game-over-loser",
	"game-card-shuffle",
	"game-human-move",
	"game-computer-move",
	NULL
};

/** @} */

/** @section Sound Theme File Parsing
  * @{ */

static void
free_entry(Entry *e)
{
	const char **label;
	char **entry;

	if (!e)
		return;
	for (label = SoundThemeFields, entry = &e->Name; *label; label++, entry++) {
		free(*entry);
		*entry = NULL;
	}
	free(e->path);
	e->path = NULL;
	free(e);
}

static Entry *
parse_file(char *path)
{
	FILE *file;
	char buf[4096], *p, *q;
	int outside_entry = 1;
	char *section = NULL;
	char *key, *val, *lang, *llang, *slang;
	int ok = 0, directories_found = 0, in_dir = -1;
	gchar **s, **subdirs = NULL, **contexts = NULL, **profiles = NULL;
	Entry *e;

	if (!(e = calloc(1, sizeof(*e))))
		return (e);
	if (getenv("LANG") && *getenv("LANG"))
		llang = strdup(getenv("LANG"));
	else
		llang = strdup("POSIX");
	DPRINTF(1, "language is '%s'\n", llang);

	q = strchr(llang, '@');
	slang = strdup(llang);
	if ((p = strchr(slang, '_')))
		*p = '\0';
	if ((p = strchr(llang, '.')))
		*p = '\0';
	if (q) {
		strcat(slang, q);
		strcat(llang, q);
	}

	DPRINTF(1, "long  language string is '%s'\n", llang);
	DPRINTF(1, "short language string is '%s'\n", slang);

	if (!(file = fopen(path, "r"))) {
		EPRINTF("cannot open file '%s' for reading\n", path);
		return (NULL);
	}
	free(e->path);
	e->path = strdup(path);
	while ((p = fgets(buf, sizeof(buf), file))) {
		/* watch out for DOS formatted files */
		if ((q = strchr(p, '\r')))
			*q = '\0';
		if ((q = strchr(p, '\n')))
			*q = '\0';
		if (*p == '[' && (q = strchr(p, ']'))) {
			*q = '\0';
			free(section);
			section = strdup(p + 1);
			if ((outside_entry = strcmp(section, "Sound Theme")))
				if ((outside_entry = strcmp(section, "Sound Data"))) {
					for (s = subdirs, in_dir = 0; s && *s; s++, in_dir++) {
						if (!strcmp(section, *s)) {
							outside_entry = 0;
							directories_found++;
							break;
						}
					}
					if (!s || !*s)
						in_dir = -1;
				}
		}
		if (outside_entry)
			continue;
		if (*p == '#' || !(q = strchr(p, '=')))
			continue;
		*q = '\0';
		key = p;
		val = q + 1;

		/* space before and after the equals sign should be ignored */
		for (q = q - 1; q >= key && isspace(*q); *q = '\0', q--) ;
		for (q = val; *q && isspace(*q); q++) ;

		if (strstr(key, "Name") == key) {
			if (strcmp(key, "Name") == 0) {
				free(e->Name);
				e->Name = strdup(val);
				ok = 1;
				continue;
			}
			if ((p = strchr(key, '[')) && (q = strchr(key, ']'))) {
				*q = '\0';
				lang = p + 1;
				if ((q = strchr(lang, '.')))
					*q = '\0';
				if ((q = strchr(lang, '@')))
					*q = '\0';
				if (strcmp(lang, llang) == 0 || strcmp(lang, slang) == 0) {
					free(e->Name);
					e->Name = strdup(val);
					ok = 1;
				}
			}
		} else if (strstr(key, "Comment") == key) {
			if (strcmp(key, "Comment") == 0) {
				free(e->Comment);
				e->Comment = strdup(val);
				ok = 1;
				continue;
			}
			if ((p = strchr(key, '[')) && (q = strchr(key, ']'))) {
				*q = '\0';
				lang = p + 1;
				if ((q = strchr(lang, '.')))
					*q = '\0';
				if ((q = strchr(lang, '@')))
					*q = '\0';
				if (strcmp(lang, llang) == 0 || strcmp(lang, slang) == 0) {
					free(e->Comment);
					e->Comment = strdup(val);
				}
			}
		} else if (strcmp(key, "Inherits") == 0) {
			free(e->Inherits);
			e->Inherits = strdup(val);
			g_strdelimit(e->Inherits, " ,;:", ' ');
			g_strstrip(e->Inherits);
			g_strdelimit(e->Inherits, " ", ';');
			ok = 1;
		} else if (strcmp(key, "Directories") == 0) {
			free(e->Directories);
			e->Directories = strdup(val);
			g_strdelimit(e->Directories, " ,;:", ' ');
			g_strstrip(e->Directories);
			g_strdelimit(e->Directories, " ", ';');
			g_strfreev(subdirs);
			subdirs = g_strsplit(e->Directories, ";", -1);
			g_strfreev(contexts);
			contexts = g_strdupv(subdirs);
			for (s = contexts; s && *s; s++)
				*(*s) = '\0';
			g_strfreev(profiles);
			profiles = g_strdupv(contexts);
			ok = 1;
		} else if (strcmp(key, "Hidden") == 0) {
			free(e->Hidden);
			e->Hidden = strdup(val);
			ok = 1;
		} else if (strcmp(key, "Example") == 0) {
			free(e->Example);
			e->Example = strdup(val);
			ok = 1;
		} else if (strcmp(key, "Context") == 0) {
			if (contexts && in_dir >= 0) {
				free(contexts[in_dir]);
				contexts[in_dir] = strdup(val);
				g_free(e->Context);
				e->Context = g_strjoinv(";", contexts);
				ok = 1;
			} else {
				EPRINTF("found Context=%s but not in directory section (%s [%d])\n", val, section, in_dir);
			}
		} else if (strcmp(key, "OutputProfile") == 0) {
			if (profiles && in_dir >= 0) {
				free(profiles[in_dir]);
				profiles[in_dir] = strdup(val);
				g_free(e->OutputProfile);
				e->OutputProfile = g_strjoinv(";", profiles);
				ok = 1;
			} else {
				EPRINTF("found OutputProfile=%s but not in directory section (%s [%d])\n", val, section, in_dir);
			}
		} else if (strstr(key, "DisplayName") == key) {
			if (strcmp(key, "DisplayName") == 0) {
				free(e->DisplayName);
				e->DisplayName = strdup(val);
				ok = 1;
				continue;
			}
			if ((p = strchr(key, '[')) && (q = strchr(key, ']'))) {
				*q = '\0';
				lang = p + 1;
				if ((q = strchr(lang, '.')))
					*q = '\0';
				if ((q = strchr(lang, '@')))
					*q = '\0';
				if (strcmp(lang, llang) == 0 || strcmp(lang, slang) == 0) {
					free(e->DisplayName);
					e->DisplayName = strdup(val);
				}
			}
		}
	}
	fclose(file);
	g_strfreev(subdirs);
	g_strfreev(contexts);
	g_strfreev(profiles);
	if (ok)
		return (e);
	free_entry(e);
	return (NULL);
}

/** @} */

static int
output_path(const char *home, const char *path)
{
	char *p, *line, *cdir;

	if (options.skipdot && *path == '.')
		return 0;
	if (options.skiptilde && (*path == '~' || ((p = strstr(path, home)) && p == path)))
		return 0;
	cdir = calloc(PATH_MAX + 1, sizeof(*cdir));
	if (!getcwd(cdir, PATH_MAX))
		strncpy(cdir, ".", PATH_MAX);
	line = calloc(PATH_MAX + 1, sizeof(*line));
	if (*path == '~' && !options.showtilde) {
		strncpy(line, home, PATH_MAX);
		strncat(line, path + 1, PATH_MAX);
	} else if (*path == '.' && !options.showdot) {
		strncpy(line, cdir, PATH_MAX);
		strncat(line, path + 1, PATH_MAX);
	} else if (options.showtilde && ((p = strstr(path, home)) && p == path)) {
		strncpy(line, "~", PATH_MAX);
		strncat(line, path + strlen(home), PATH_MAX);
	} else if (options.showdot && ((p = strstr(path, cdir)) && p == path)) {
		strncpy(line, ".", PATH_MAX);
		strncat(line, path + strlen(cdir), PATH_MAX);
	} else
		strncpy(line, path, PATH_MAX);
	if (options.output > 0)
		fprintf(stdout, "%s\n", line);
	free(cdir);
	free(line);
	return 1;
}

/** @brief look up the index.theme file for a theme
  *
  * We search in the the sound theme in XDG directory precedence looking for the
  * index.theme file corresponding to a sound theme.  The index.theme file to
  * use, according to the freedesktop.org specification, is the first one
  * encountered in XDG data directory search order.
  */
Entry *
lookup_index(const char *theme)
{
	char *path = NULL, *home, *p;
	struct stat st;
	char *list, *dirs, *env;
	Entry *e = NULL;

	if (!theme)
		return (e);

	list = calloc(PATH_MAX + 1, sizeof(*list));
	dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
	if ((env = getenv("XDG_DATA_HOME")) && *env)
		strncpy(list, env, PATH_MAX);
	else {
		strncpy(list, home, PATH_MAX);
		strncat(list, "/.local/share", PATH_MAX);
	}
	if (options.skiptilde)
		strncpy(list, dirs, PATH_MAX);
	else {
		strncat(list, ":", PATH_MAX);
		strncat(list, dirs, PATH_MAX);
	}
	for (dirs = list; !path && *dirs;) {
		if (*dirs == '.' && options.skipdot)
			continue;
		if (*dirs == '~' && options.skiptilde)
			continue;
		if ((p = strchr(dirs, ':'))) {
			*p = '\0';
			path = strdup(dirs);
			dirs = p + 1;
		} else {
			path = strdup(dirs);
			*dirs = '\0';
		}
		path = realloc(path, PATH_MAX + 1);
		strncat(path, "/sounds/", PATH_MAX);
		strncat(path, theme, PATH_MAX);
		strncat(path, "/index.theme", PATH_MAX);
		if (!stat(path, &st)) {
			if ((e = parse_file(path))) {
				free(list);
				free(path);
				return (e);
			}
		}
		free(path);
		path = NULL;
	}
	free(list);
	return (e);
}

static int
on_list(char *themes, const char *theme)
{
	const char *current;
	size_t len = strlen(theme);
	char *p;

	for (current = themes; *current; ) {
		if ((p = strchr(current, ';'))) {
			if (!strncmp(current, theme, len))
				return (1);
			current = p + 1;
		} else {
			if (!strncmp(current, theme, len))
				return (1);
			current = current + strlen(current);
		}
	}
	return (0);
}

static void
lookup_theme(char *themes, const char *theme)
{
	char *inherits, *saveptr = NULL;

	if (!theme) {
		EPRINTF("called with a null theme!\n");
		return;
	}
	if (!strcmp(theme, "freedesktop")) {
		EPRINTF("called with a fallback theme '%s'\n", theme);
		return;
	}
	/* break inheritance loop */
	if (on_list(themes, theme)) {
		EPRINTF("theme %s already on list '%s'\n", theme, themes);
		return;
	}
	DPRINTF(1, "adding theme %s to list\n", theme);
	DPRINTF(1, "theme list was '%s'\n", themes);
	if (*themes)
		strncat(themes, ";", PATH_MAX);
	strncat(themes, theme, PATH_MAX);
	DPRINTF(1, "theme list now '%s'\n", themes);
	{
		Entry *e;

		if (!(e = lookup_index(theme)))
			return;
		if (!e->Inherits) {
			free_entry(e);
			return;
		}
		if (!(inherits = strdup(e->Inherits))) {
			free_entry(e);
			return;
		}
		free_entry(e);
	}
	DPRINTF(1, "theme %s inherits '%s'\n", theme, inherits);
	/* technically these should be semicolon separated, some sometimes aren't */
	for (theme = strtok_r(inherits, " ,;:", &saveptr); theme; theme = strtok_r(NULL, " ,;:", &saveptr))
		lookup_theme(themes, theme);
	free(inherits);
	return;
}

static char *
lookup_themes(void)
{
	char *themes, *theme;

	themes = calloc(PATH_MAX + 1, sizeof(*themes));
	if ((theme = options.theme))
		lookup_theme(themes, theme);
	if (*themes)
		strncat(themes, ";", PATH_MAX);
	strncat(themes, "freedesktop", PATH_MAX);
	return (themes);
}

static void
lookup_subdir(char *subdirs, const char *theme, const char *profile)
{
	char *dirs, *dirsaveptr = NULL, *pfls, *pflsaveptr = NULL;
	char *dir, *pfl;

	if (!theme)
		return;
	{
		Entry *e;

		if (!(e = lookup_index(theme))) {
			EPRINTF("could not lookup index file for theme '%s'!\n", theme);
			return;
		}
		if (!e->Directories) {
			EPRINTF("no directories!\n");
			free_entry(e);
			return;
		}
		if (!e->OutputProfile) {
			EPRINTF("no output profiles!\n");
			free_entry(e);
			return;
		}
		if (!(dirs = strdup(e->Directories)) || !(pfls = strdup(e->OutputProfile))) {
			EPRINTF("cannot dup strings!\n");
			free_entry(e);
			return;
		}
		free_entry(e);
	}
	DPRINTF(1, "theme %s dirs '%s'\n", theme, dirs);
	DPRINTF(1, "theme %s pfls '%s'\n", theme, pfls);
	/* technically these should be semicolon separated, some sometimes aren't */
	for (dir = strtok_r(dirs, " ,;:", &dirsaveptr), pfl = strtok_r(pfls, " ,;:", &pflsaveptr);
	     dir && pfl;
	     dir = strtok_r(NULL, " ,;:", &dirsaveptr), pfl = strtok_r(NULL, " ,;:", &pflsaveptr)) {
		DPRINTF(1, "checking profile '%s' for subdir '%s'\n", pfl, dir);
		if (!strcmp(pfl, profile)) {
			if (*subdirs)
				strncat(subdirs, ";", PATH_MAX);
			strncat(subdirs, dir, PATH_MAX);
		}
	}
	free(dirs);
	free(pfls);
	return;
}

static char *
lookup_subdirs(const char *theme, const char *profile)
{
	char *subdirs;

	subdirs = calloc(PATH_MAX + 1, sizeof(*subdirs));
	lookup_subdir(subdirs, theme, profile);
	return (subdirs);
}

/** @brief look up the file from EVENTID
  *
  * We search in the sound theme directory first (i.e.  /usr/share/sounds/THEME)
  * and then in parent theme directories, and then in
  * /usr/share/sounds/freedesktop theme directory, and then in /usr/share/sounds
  * itself.
  */
static void
lookup_file(const char *name)
{
	const gchar *const *locales, *const *locale;
	char *path = NULL, *eventid, *home, *p, *tlist, *themes, *slist, *subdirs;
	const char *theme, *profile, *subdir;
	struct stat st;

	locales = g_get_language_names();
	tlist = lookup_themes();
	DPRINTF(1, "theme list is '%s'\n", tlist);
	home = getenv("HOME") ? : ".";
	eventid = calloc(PATH_MAX + 1, sizeof(*eventid));
	strncpy(eventid, name, PATH_MAX);
	if ((*eventid != '/') && (*eventid != '.')) {
		for (themes = tlist;;) {
			if (!*themes) {
				theme = NULL;
			} else if ((p = strchr(themes, ';'))) {
				*p = '\0';
				theme = themes;
				themes = p + 1;
			} else {
				theme = themes;
				themes = themes + strlen(themes);
			}
			if (theme)
				DPRINTF(1, "searching with theme '%s'\n", theme);
			else
				DPRINTF(1, "searching without a theme\n");
			for (profile = options.profile ? : "stereo"; profile;
			     profile = *profile ? (strcmp(profile, "stereo") ? "stereo" : "") : NULL) {
				if (!theme && *profile) {
					DPRINTF(1, "skipping profile '%s' for null theme\n", profile);
					continue;
				}
				slist = lookup_subdirs(theme, profile);
				DPRINTF(1, "subdir list is '%s' for theme '%s' and profile '%s'\n", slist, theme, profile);
				for (subdirs = slist;;) {
					if (!*subdirs) {
						subdir = NULL;
						if (theme) {
							DPRINTF(1, "skipping empty subdir for theme '%s', profile '%s'\n", theme, profile);
							break;
						}
					} else if ((p = strchr(subdirs, ';'))) {
						*p = '\0';
						subdir = subdirs;
						subdirs = p + 1;
					} else {
						subdir = subdirs;
						subdirs = subdirs + strlen(subdirs);
					}
					if (subdir)
						DPRINTF(1, "searching with subdir '%s'\n", subdir);
					else
						DPRINTF(1, "searching without a subdir\n");
					strncpy(eventid, name, PATH_MAX);
					for (;;) {
						for (locale = locales;; locale++) {
							char *list, *dirs, *env;

							if (!theme && *locale) {
								DPRINTF(1, "skipping locale '%s' for null theme\n", *locale);
								continue;
							}
							/* strip any sound file extension */
							if ((p = strstr(eventid, ".disabled")) == eventid + strlen(eventid) - 9)
								*p = '\0';
							if ((p = strstr(eventid, ".oga")) == eventid + strlen(eventid) - 4)
								*p = '\0';
							if ((p = strstr(eventid, ".ogg")) == eventid + strlen(eventid) - 4)
								*p = '\0';
							if ((p = strstr(eventid, ".wav")) == eventid + strlen(eventid) - 4)
								*p = '\0';
							if ((p = strstr(eventid, ".sound")) == eventid + strlen(eventid) - 6)
								*p = '\0';

							list = calloc(PATH_MAX + 1, sizeof(*list));
							dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
							if ((env = getenv("XDG_DATA_HOME")) && *env)
								strncpy(list, env, PATH_MAX);
							else {
								strncpy(list, home, PATH_MAX);
								strncat(list, "/.local/share", PATH_MAX);
							}
							if (options.skiptilde) {
								strncpy(list, dirs, PATH_MAX);
							} else {
								strncat(list, ":", PATH_MAX);
								strncat(list, dirs, PATH_MAX);
							}
							for (dirs = list; !path && *dirs;) {
								if (*dirs == '.' && options.skipdot)
									continue;
								if (*dirs == '~' && options.skiptilde)
									continue;
								if ((p = strchr(dirs, ':'))) {
									*p = '\0';
									path = strdup(dirs);
									dirs = p + 1;
								} else {
									path = strdup(dirs);
									*dirs = '\0';
								}
								path = realloc(path, PATH_MAX + 1);
								strncat(path, "/sounds/", PATH_MAX);
								if (theme) {
									strncat(path, theme, PATH_MAX);
									strncat(path, "/", PATH_MAX);
									strncat(path, subdir, PATH_MAX);
									strncat(path, "/", PATH_MAX);
									if (*locale) {
										strncat(path, *locale, PATH_MAX);
										strncat(path, "/", PATH_MAX);
									}
								}
								strncat(path, eventid, PATH_MAX);
								p = path + strlen(path);
								strcpy(p, ".disabled");
								DPRINTF(1, "checking '%s'\n", path);
								if (!stat(path, &st) && output_path(home, path) && !options.all) {
									free(list);
									free(eventid);
									free(path);
									free(tlist);
									return;
								}
								strcpy(p, ".oga");
								DPRINTF(1, "checking '%s'\n", path);
								if (!stat(path, &st) && output_path(home, path) && !options.all) {
									free(list);
									free(eventid);
									free(path);
									free(tlist);
									return;
								}
								strcpy(p, ".ogg");
								DPRINTF(1, "checking '%s'\n", path);
								if (!stat(path, &st) && output_path(home, path) && !options.all) {
									free(list);
									free(eventid);
									free(path);
									free(tlist);
									return;
								}
								strcpy(p, ".wav");
								DPRINTF(1, "checking '%s'\n", path);
								if (!stat(path, &st) && output_path(home, path) && !options.all) {
									free(list);
									free(eventid);
									free(path);
									free(tlist);
									return;
								}
								free(path);
								path = NULL;
							}
							free(list);
							if (!*locale)
								break;
						}
						if ((p = strrchr(eventid, '-'))) {
							*p = '\0';
							continue;
						}
						break;
					}
					if (!subdir)
						break;
				}
				free(slist);
			}
			if (!theme)
				break;
		}
		free(tlist);
	} else {
		path = strdup(eventid);
		DPRINTF(1, "checking '%s'\n", path);
		if (!stat(path, &st)) {
			output_path(home, path);
		}
		free(path);
		free(tlist);
	}
	free(eventid);
	return;
}


static void
list_paths(void)
{
	const gchar *const *locales, *const *locale;
	char *home, *p, *tlist, *themes, *slist, *subdirs;
	const char *theme, *profile, *subdir;

	locales = g_get_language_names();
	tlist = lookup_themes();
	DPRINTF(1, "theme list is '%s'\n", tlist);
	home = getenv("HOME") ? : ".";

	for (themes = tlist;;) {
		if (!*themes) {
			theme = NULL;
		} else if ((p = strchr(themes, ';'))) {
			*p = '\0';
			theme = themes;
			themes = p + 1;
		} else {
			theme = themes;
			themes = themes + strlen(themes);
		}
		if (theme)
			DPRINTF(1, "searching with theme '%s'\n", theme);
		else
			DPRINTF(1, "searching without a theme\n");
		for (profile = options.profile ? : "stereo"; profile;
		     profile = *profile ? (strcmp(profile, "stereo") ? "stereo" : "") : NULL) {
			if (!theme && *profile) {
				DPRINTF(1, "skipping profile '%s' for null theme\n", profile);
				continue;
			}
			slist = lookup_subdirs(theme, profile);
			DPRINTF(1, "subdir list is '%s' for theme '%s' and profile '%s'\n", slist,
				theme, profile);
			for (subdirs = slist;;) {
				if (!*subdirs) {
					subdir = NULL;
					if (theme) {
						DPRINTF(1, "skipping empty subdir for theme '%s', profile '%s'\n", theme, profile);
						break;
					}
					DPRINTF(1, "using empty subdir for null theme\n");
				} else if ((p = strchr(subdirs, ';'))) {
					*p = '\0';
					subdir = subdirs;
					subdirs = p + 1;
				} else {
					subdir = subdirs;
					subdirs = subdirs + strlen(subdirs);
				}
				if (subdir)
					DPRINTF(1, "searching with subdir '%s'\n", subdir);
				else
					DPRINTF(1, "searching without a subdir\n");

				for (locale = locales;; locale++) {
					char *list, *dirs, *env;

					if (!theme && *locale) {
						DPRINTF(1, "skipping locale '%s' for null theme\n", *locale);
						continue;
					}
					list = calloc(PATH_MAX + 1, sizeof(*list));
					dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
					if ((env = getenv("XDG_DATA_HOME")) && *env)
						strncpy(list, env, PATH_MAX);
					else {
						strncpy(list, home, PATH_MAX);
						strncat(list, "/.local/share", PATH_MAX);
					}
					if (options.skiptilde) {
						strncpy(list, dirs, PATH_MAX);
					} else {
						strncat(list, ":", PATH_MAX);
						strncat(list, dirs, PATH_MAX);
					}
					for (dirs = list; dirs && *dirs;) {
						char *p, *path;
						struct stat st;

						if (*dirs == '.' && options.skipdot)
							continue;
						if (*dirs == '~' && options.skiptilde)
							continue;
						if ((p = strchr(dirs, ':'))) {
							*p = '\0';
							path = strdup(dirs);
							dirs = p + 1;
						} else {
							path = strdup(dirs);
							*dirs = '\0';
						}
						path = realloc(path, PATH_MAX + 1);
						strncat(path, "/sounds", PATH_MAX);
						if (theme) {
							strncat(path, "/", PATH_MAX);
							strncat(path, theme, PATH_MAX);
							strncat(path, "/", PATH_MAX);
							strncat(path, subdir, PATH_MAX);
							if (*locale) {
								strncat(path, "/", PATH_MAX);
								strncat(path, *locale, PATH_MAX);
							}
						}
						DPRINTF(1, "checking '%s'\n", path);
						if (!stat(path, &st))
							output_path(home, path);
						free(path);
					}
					free(list);
					if (!*locale)
						break;
				}
				if (!subdir)
					break;
			}
			free(slist);
		}
		if (!theme)
			break;
	}
	free(tlist);
	return;
}

static void
do_list(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	list_paths();
}

static void
do_play(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

static void
get_sound_event(char *key, char *file)
{
	Event *event;

	DPRINTF(1, "getting sound event: %s: %s\n", key, file);
	for (event = events; event; event = event->next) {
		if (!strcmp(key, event->key))
			break;
	}
	if (options.all || !event) {
		DPRINTF(1, "allocating event for: %s: %s\n", key, file);
		event = calloc(1, sizeof(*event));
		event->next = events;
		events = event;
		event->key = strdup(key);
		event->file = strdup(file);
	}
}

static int
sort_keys(const void *a, const void *b)
{
	Event *const *A = a;
	Event *const *B = b;

	return strcmp((*A)->key, (*B)->key);
}

static void
sort_sound_events(void)
{
	Event *event;
	int i, n;

	for (n = 0, event = events; event; event = event->next, n++) ;
	array = calloc(n, sizeof(Event *));
	DPRINTF(1, "there are %d entries\n", n);
	for (i = 0, event = events; event; event = event->next, i++)
		array[i] = event;
	qsort(array, n, sizeof(Event *), &sort_keys);
	events = array[0];
	for (i = 0; i < n - 1; i++)
		array[i]->next = array[i + 1];
	array[n - 1]->next = NULL;
	free(array);
}

static void
list_sound_events(void)
{
	Event *event;

	for (event = events; event; event = event->next)
		fprintf(stdout, "%s: %s\n", event->key, event->file);
}

static void
get_events(char *path, const char *theme) {
	DIR *dir;
	struct dirent *d;

	if (!theme && !options.all)
		return;
	DPRINTF(1, "checking directory: %s\n", path);
	if (!(dir = opendir(path))) {
		DPRINTF(1, "%s: %s\n", path, strerror(errno));
		return;
	}
	while ((d = readdir(dir))) {
		const char *suffix;
		char *f, *file;
		struct stat st;
		int len;

		if (d->d_name[0] == '.')
			continue;
		len = strlen(path) + strlen(d->d_name) + 2;
		file = calloc(len, sizeof(*file));
		strcpy(file, path);
		strcat(file, "/");
		strcat(file, d->d_name);
		if (stat(file, &st) == -1) {
			EPRINTF("%s: %s\n", file, strerror(errno));
			free(file);
			continue;
		}
		if (S_ISREG(st.st_mode)) {
			char *key;

			key = strdup(d->d_name);
			if (((f = strstr(key, (suffix = ".disabled"))) && f[strlen(suffix)] == '\0') ||
			    ((f = strstr(key, (suffix = ".oga")))      && f[strlen(suffix)] == '\0') ||
			    ((f = strstr(key, (suffix = ".ogg")))      && f[strlen(suffix)] == '\0') ||
			    ((f = strstr(key, (suffix = ".wav")))      && f[strlen(suffix)] == '\0') ||
			    ((f = strstr(key, (suffix = ".sound")))    && f[strlen(suffix)] == '\0')) {
				*f = '\0';
				get_sound_event(key, file);
			}
			free(key);
			free(file);
			continue;
		} else {
			DPRINTF(1, "%s: %s\n", file, "not a file");
			free(file);
			continue;
		}
	}
	closedir(dir);
}

static void
do_listem(int argc, char *argv[])
{
	const char **eventid;

	(void) argc;
	(void) argv;
	for (eventid = StandardSoundNames; eventid && *eventid; eventid++) {
		fprintf(stdout, "%s: \n", *eventid);
		lookup_file(*eventid);
	}
}

static void
do_show(int argc, char *argv[])
{
	const gchar *const *locales, *const *locale;
	char *home, *p, *tlist, *themes, *slist, *subdirs;
	const char *theme, *profile, *subdir;

	(void) argc;
	(void) argv;

	locales = g_get_language_names();
	tlist = lookup_themes();
	DPRINTF(1, "theme list is '%s'\n", tlist);
	home = getenv("HOME") ? : ".";

	for (themes = tlist;;) {
		if (!*themes) {
			theme = NULL;
		} else if ((p = strchr(themes, ';'))) {
			*p = '\0';
			theme = themes;
			themes = p + 1;
		} else {
			theme = themes;
			themes = themes + strlen(themes);
		}
		if (theme)
			DPRINTF(1, "searching with theme '%s'\n", theme);
		else
			DPRINTF(1, "searching without a theme\n");
		for (profile = options.profile ? : "stereo"; profile;
		     profile = *profile ? (strcmp(profile, "stereo") ? "stereo" : "") : NULL) {
			if (!theme && *profile) {
				DPRINTF(1, "skipping profile '%s' for null theme\n", profile);
				continue;
			}
			slist = lookup_subdirs(theme, profile);
			DPRINTF(1, "subdir list is '%s' for theme '%s' and profile '%s'\n", slist,
				theme, profile);
			for (subdirs = slist;;) {
				if (!*subdirs) {
					subdir = NULL;
					if (theme) {
						DPRINTF(1, "skipping empty subdir for theme '%s', profile '%s'\n", theme, profile);
						break;
					}
				} else if ((p = strchr(subdirs, ';'))) {
					*p = '\0';
					subdir = subdirs;
					subdirs = p + 1;
				} else {
					subdir = subdirs;
					subdirs = subdirs + strlen(subdirs);
				}
				if (subdir)
					DPRINTF(1, "searching with subdir '%s'\n", subdir);
				else
					DPRINTF(1, "searching without a subdir\n");

				for (locale = locales;; locale++) {
					char *list, *dirs, *env;

					if (!theme && *locale) {
						DPRINTF(1, "skipping locale '%s' for null theme\n", *locale);
						continue;
					}
					list = calloc(PATH_MAX + 1, sizeof(*list));

					dirs = getenv("XDG_DATA_DIRS") ? : "/usr/local/share:/usr/share";
					if ((env = getenv("XDG_DATA_HOME")) && *env) {
						strncpy(list, env, PATH_MAX);
					} else {
						strncpy(list, home, PATH_MAX);
						strncat(list, "/.local/share", PATH_MAX);
					}
					if (options.skiptilde) {
						strncpy(list, dirs, PATH_MAX);
					} else {
						strncat(list, ":", PATH_MAX);
						strncat(list, dirs, PATH_MAX);
					}
					for (dirs = list; dirs && *dirs;) {
						char *p, *path;

						if (*dirs == '.' && options.skipdot)
							continue;
						if (*dirs == '~' && options.skiptilde)
							continue;
						if ((p = strchr(dirs, ':'))) {
							*p = '\0';
							path = strdup(dirs);
							dirs = p + 1;
						} else {
							path = strdup(dirs);
							*dirs = '\0';
						}
						path = realloc(path, PATH_MAX + 1);
						strncat(path, "/sounds", PATH_MAX);
						if (theme) {
							strncat(path, "/", PATH_MAX);
							strncat(path, theme, PATH_MAX);
							strncat(path, "/", PATH_MAX);
							strncat(path, subdir, PATH_MAX);
							if (*locale) {
								strncat(path, "/", PATH_MAX);
								strncat(path, *locale, PATH_MAX);
							}
						}
						get_events(path, theme);
						free(path);
					}
					free(list);
					if (!*locale)
						break;
				}
				if (!subdir)
					break;
			}
			free(slist);
		}
		if (!theme)
			break;
	}
	free(tlist);
	sort_sound_events();
	list_sound_events();
}

static void
do_whereis(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

static void
do_which(int argc, char *argv[])
{
	char **eventid;

	(void) argc;
	(void) argv;
	for (eventid = options.eventids; eventid && *eventid; eventid++)
		lookup_file(*eventid);
}

/** @section Main
  * @{ */

static void
set_default_profile(void)
{
	free(options.profile);
	options.profile = strdup("stereo");
}

static void
set_default_theme(void)
{
	char *files, *end, *file;
	Entry *entry;
	int n;

	files = calloc(PATH_MAX, sizeof(*files));
	strcpy(files, getenv("GTK2_RC_FILES") ? : "/usr/share/gtk-2.0/gtkrc");
	if (*files)
		strcat(files, ":");
	strcat(files, getenv("HOME"));
	strcat(files, "/.gtkrc-2.0");
	strcat(files, ":");
	strcat(files, getenv("HOME"));
	strcat(files, "/.gtkrc-2.0.xde");

	end = files + strlen(files);
	for (n = 0, file = files; file < end; n++,
	     *strchrnul(file, ':') = '\0', file += strlen(file) + 1) ;

	for (n = 0, file = files; file < end; n++, file += strlen(file) + 1) {
		struct stat st;
		FILE *f;
		char *buf, *b, *e;

		if (stat(file, &st)) {
			DPRINTF(1, "%s: %s\n", file, strerror(errno));
			continue;
		}
		if (!S_ISREG(st.st_mode)) {
			DPRINTF(1, "%s: not a file\n", file);
			continue;
		}
		if (!(f = fopen(file, "r"))) {
			DPRINTF(1, "%s: %s\n", file, strerror(errno));
			continue;
		}
		DPRINTF(1, "got file %s\n", file);
		buf = calloc(PATH_MAX + 1, sizeof(*buf));
		while (fgets(buf, PATH_MAX, f)) {
			b = buf;
			b += strspn(b, " \t");
			if (*b == '#' || *b == '\n')
				continue;
			if (strncmp(b, "gtk-sound-theme-name", 20))
				continue;
			b += 20;
			b += strspn(b, " \t");
			if (*b != '=')
				continue;
			b += 1;
			b += strspn(b, " \t");
			if (*b != '"')
				continue;
			b += 1;
			e = b;
			while ((e = strchr(e, '"'))) {
				if (*(e - 1) != '\\')
					break;
				memmove(e - 1, e, strlen(e) + 1);
			}
			if (!e || b >= e)
				continue;
			*e = '\0';
			memmove(buf, b, strlen(b) + 1);
			if ((entry = lookup_index(buf))) {
				OPRINTF(1, "found theme from %s %s\n", file, buf);
				free(options.theme);
				options.theme = strdup(buf);
				free_entry(entry);
			}
		}
		fclose(f);
		free(buf);
	}
	free(files);
	if (!options.theme)
		DPRINTF(1, "could not find theme\n");
	else
		DPRINTF(1, "found theme %s\n", options.theme);
	return;
}

static void
set_defaults(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	set_default_theme();
	set_default_profile();
}

static void
get_defaults(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

static void
copying(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
--------------------------------------------------------------------------------\n\
%1$s\n\
--------------------------------------------------------------------------------\n\
Copyright (c) 2008-2020  Monavacon Limited <http://www.monavacon.com/>\n\
Copyright (c) 2001-2008  OpenSS7 Corporation <http://www.openss7.com/>\n\
Copyright (c) 1997-2001  Brian F. G. Bidulock <bidulock@openss7.org>\n\
\n\
All Rights Reserved.\n\
--------------------------------------------------------------------------------\n\
This program is free software: you can  redistribute it  and/or modify  it under\n\
the terms of the  GNU Affero  General  Public  License  as published by the Free\n\
Software Foundation, version 3 of the license.\n\
\n\
This program is distributed in the hope that it will  be useful, but WITHOUT ANY\n\
WARRANTY; without even  the implied warranty of MERCHANTABILITY or FITNESS FOR A\n\
PARTICULAR PURPOSE.  See the GNU Affero General Public License for more details.\n\
\n\
You should have received a copy of the  GNU Affero General Public License  along\n\
with this program.   If not, see <http://www.gnu.org/licenses/>, or write to the\n\
Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n\
--------------------------------------------------------------------------------\n\
U.S. GOVERNMENT RESTRICTED RIGHTS.  If you are licensing this Software on behalf\n\
of the U.S. Government (\"Government\"), the following provisions apply to you. If\n\
the Software is supplied by the Department of Defense (\"DoD\"), it is classified\n\
as \"Commercial  Computer  Software\"  under  paragraph  252.227-7014  of the  DoD\n\
Supplement  to the  Federal Acquisition Regulations  (\"DFARS\") (or any successor\n\
regulations) and the  Government  is acquiring  only the  license rights granted\n\
herein (the license rights customarily provided to non-Government users). If the\n\
Software is supplied to any unit or agency of the Government  other than DoD, it\n\
is  classified as  \"Restricted Computer Software\" and the Government's rights in\n\
the Software  are defined  in  paragraph 52.227-19  of the  Federal  Acquisition\n\
Regulations (\"FAR\")  (or any successor regulations) or, in the cases of NASA, in\n\
paragraph  18.52.227-86 of  the  NASA  Supplement  to the FAR (or any  successor\n\
regulations).\n\
--------------------------------------------------------------------------------\n\
", NAME " " VERSION);
}

static void
version(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
%1$s (OpenSS7 %2$s) %3$s\n\
Written by Brian Bidulock.\n\
\n\
Copyright (c) 2010, 2011, 2012, 2013, 2014, 2015, 2016, 2017, 2018, 2020 Monavacon Limited.\n\
Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008, 2009  OpenSS7 Corporation\n\
Copyright (c) 1997, 1998, 1999, 2000, 2001  Brian F. G. Bidulock\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\
\n\
Distribute by OpenSS7 under GNU Affero General Public License Version 3,\n\
with conditions, incorporated herein by reference.\n\
\n\
See `%1$s --copying' for copying permissions.\n\
", NAME, PACKAGE, VERSION);
}

static void
usage(int argc, char *argv[])
{
	(void) argc;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stderr, "\
Usage:\n\
    %1$s [options] [--] EVENTID [...]\n\
    %1$s [options] {-l|--list}\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copyring}\n\
", argv[0]);
}

static void
help(int argc, char *argv[])
{
	(void) argc;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options] [{-w|--which}] [--] EVENTID [...]\n\
    %1$s [options] {-l|--list}\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    EVENTID\n\
        the event identifier of XDG sound to play or locate\n\
Command Options:\n\
   [--which, -w]\n\
        list which sound file would be played\n\
    --list, -l\n\
        print list of effective lookup paths\n\
    --help, -h, -?, --?\n\
        print this usage information and exit\n\
    --version, -V\n\
        print version and exit\n\
    --copying, -C\n\
        print copying permission and exit\n\
General Options:\n\
    --all, -a\n\
        print all matching sound entries\n\
    --skip-dot, -o\n\
        skip directories that start with a dot\n\
    --skip-tilde, -t\n\
        skip directories that start with a tilde\n\
    --show-dot, -O\n\
        print dots instead of full path\n\
    --show-tilde, -T\n\
        print tildes instead of full path\n\
    --theme, -N THEME          [default: %2$s]\n\
        use sound theme named THEME\n\
    --profile, -P PROFILE      [default: %3$s]\n\
        use sound profile named PROFILE\n\
    --debug, -D [LEVEL]        [default: %4$d]\n\
        increment or set debug LEVEL\n\
    --verbose, -v [LEVEL]      [default: %5$d]\n\
        increment or set output verbosity LEVEL\n\
	this option may be repeated\n\
", argv[0]
, options.theme
, options.profile
, options.debug
, options.output
);
}

int
main(int argc, char *argv[])
{
	Command command = CommandDefault;

	setlocale(LC_ALL, "");

	set_defaults(argc, argv);

	while (1) {
		int c, val;
		char *endptr;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"which",	no_argument,		NULL,	'w'},
			{"noplay",	no_argument,		NULL,	'n'},
			{"list",	no_argument,		NULL,	'l'},

			{"all",		no_argument,		NULL,	'a'},
			{"skip-dot",	no_argument,		NULL,	'o'},
			{"skip-tilde",	no_argument,		NULL,	't'},
			{"show-dot",	no_argument,		NULL,	'O'},
			{"show-tilde",	no_argument,		NULL,	'T'},
			{"theme",	required_argument,	NULL,	'N'},
			{"profile",	required_argument,	NULL,	'P'},
			{"standard",	no_argument,		NULL,	'S'},
			{"context",	required_argument,	NULL,	'c'},

			{"debug",	optional_argument,	NULL,	'D'},
			{"verbose",	optional_argument,	NULL,	'v'},
			{"help",	no_argument,		NULL,	'h'},
			{"version",	no_argument,		NULL,	'V'},
			{"copying",	no_argument,		NULL,	'C'},
			{"?",		no_argument,		NULL,	'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "wnlaotOTN:P:Sc:D::v::hVCH?", long_options,
				     &option_index);
#else				/* _GNU_SOURCE */
		c = getopt(argc, argv, "wnlaotOTN:P:Sc:DvhVCH?");
#endif				/* _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;
		case 'p':	/* -p, --play */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandPlay;
			options.command = CommandPlay;
			break;
		case 's':	/* -s, --show */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandShow;
			options.command = CommandShow;
			break;
		case 'w':	/* -w, --which */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandWhich;
			options.command = CommandWhich;
			break;
		case 'W':	/* -W, --whereis */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandWhereis;
			options.command = CommandWhereis;
			break;

		case 'n':	/* -n, --noplay */
			options.noplay = 1;
			break;
		case 'l':	/* -l, --list */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandList;
			options.command = CommandList;
			break;

		case 'a':	/* -a, --all */
			options.all = 1;
			break;
		case 'o':	/* -o, --skip-dot */
			options.skipdot = 1;
			break;
		case 't':	/* -t, --skip-tilde */
			options.skiptilde = 1;
			break;
		case 'O':	/* -O, --show-dot */
			options.showdot = 1;
			break;
		case 'T':	/* -T, --show-tilde */
			options.showtilde = 1;
			break;
		case 'N':	/* -N, --theme THEME */
			free(options.theme);
			options.theme = strdup(optarg);
			break;
		case 'P':	/* -P, --profile PROFILE */
			free(options.profile);
			options.profile = strdup(optarg);
			break;
		case 'S':	/* -S, --standard */
			options.standard = 1;
			break;
		case 'c':	/* -c, --context CONTEXT */
			free(options.context);
			options.context = strdup(options.context);
			break;

		case 'D':	/* -D, --debug [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing debug verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.debug++;
				break;
			}
			val = strtol(optarg, &endptr, 0);
			if (*endptr || val < 0)
				goto bad_option;
			options.debug = val;
			break;
		case 'v':	/* -v, --verbose [level] */
			if (options.debug)
				fprintf(stderr, "%s: increasing output verbosity\n", argv[0]);
			if (optarg == NULL) {
				options.output++;
				break;
			}
			val = strtol(optarg, &endptr, 0);
			if (*endptr || val < 0)
				goto bad_option;
			options.output = val;
			break;
		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			command = CommandHelp;
			break;
		case 'V':	/* -V, --version */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandVersion;
			options.command = CommandVersion;
			break;
		case 'C':	/* -C, --copying */
			if (options.command != CommandDefault)
				goto bad_command;
			if (command == CommandDefault)
				command = CommandCopying;
			options.command = CommandCopying;
			break;
		case '?':
		default:
		      bad_option:
			optind--;
		      bad_nonopt:
			if (options.output || options.debug) {
				if (optind < argc) {
					fprintf(stderr, "%s: syntax error near '", argv[0]);
					while (optind < argc)
						fprintf(stderr, "%s ", argv[optind++]);
					fprintf(stderr, "\n");
				} else {
					fprintf(stderr, "%s: missing option or argument", argv[0]);
					fprintf(stderr, "\n");
				}
				fflush(stderr);
			      bad_usage:
				usage(argc, argv);
			}
			exit(2);
		      bad_command:
			fprintf(stderr, "%s: only one command option allowed\n", argv[0]);
			goto bad_usage;
		}
	}
	if (options.debug) {
		fprintf(stderr, "%s: option index = %d\n", argv[0], optind);
		fprintf(stderr, "%s: option count = %d\n", argv[0], argc);
	}
	if (command != CommandHelp)
		if (command == CommandDefault)
			options.command = CommandWhich;
	switch (options.command) {
	case CommandPlay:
	case CommandWhereis:
	case CommandWhich:
		if (optind >= argc) {
			fprintf(stderr, "%s: EVENTID must be specified\n", argv[0]);
			goto bad_usage;
		}
		break;
	case CommandList:
	case CommandShow:
	case CommandVersion:
	case CommandCopying:
		if (optind < argc) {
			fprintf(stderr, "%s: excess arguments\n", argv[0]);
			goto bad_nonopt;
		}
		break;
	case CommandDefault:
	case CommandHelp:
		break;
	}
	if (optind < argc) {
		int n = argc - optind, j = 0;

		options.eventids = calloc(n + 1, sizeof(*options.eventids));
		while (optind < argc)
			options.eventids[j++] = strdup(argv[optind++]);
	}

	get_defaults(argc, argv);

	if (command == CommandDefault)
		command = options.command;
	switch (command) {
	case CommandPlay:
		DPRINTF(1, "%s: running play\n", argv[0]);
		do_play(argc, argv);
		break;
	case CommandShow:
		DPRINTF(1, "%s: running show\n", argv[0]);
		if (options.standard)
			do_listem(argc, argv);
		else
			do_show(argc, argv);
		break;
	case CommandWhereis:
		DPRINTF(1, "%s: running whereis\n", argv[0]);
		do_whereis(argc, argv);
		break;
	case CommandWhich:
		DPRINTF(1, "%s: running which\n", argv[0]);
		do_which(argc, argv);
		break;
	case CommandList:
		DPRINTF(1, "%s: running list\n", argv[0]);
		do_list(argc, argv);
		break;
	case CommandHelp:
		DPRINTF(1, "%s: printing help message\n", argv[0]);
		help(argc, argv);
		break;
	case CommandVersion:
		DPRINTF(1, "%s: printing version message\n", argv[0]);
		version(argc, argv);
		break;
	case CommandCopying:
		DPRINTF(1, "%s: printing copying message\n", argv[0]);
		copying(argc, argv);
		break;
	case CommandDefault:
		break;
	}
	exit(EXIT_SUCCESS);
}

/** @} */

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
