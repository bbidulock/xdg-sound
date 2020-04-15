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

#ifdef GIO_GLIB2_SUPPORT
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>
#endif

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
	CommandWhereis,
	CommandWhich,
	CommandList,
	CommandHelp,
	CommandVersion,
	CommandCopying,
} CommandType;

/** @} */

static void
help(int argc, char *argv[])
{
	(void) argc;
	if (!options.output && !options.debug)
		return;
	(void) fprintf(stdout, "\
Usage:\n\
    %1$s [options] [--] EVENTID [...]\n\
    %1$s [options] {-l|--list}\n\
    %1$s {-h|--help}\n\
    %1$s {-V|--version}\n\
    %1$s {-C|--copying}\n\
Arguments:\n\
    EVENTID\n\
        the event identifier of XDG sound to play or locate\n\
Command Options:\n\
    --which, -w\n\
        print which sound file would be played\n\
    --whereis, -W\n\
        print which sound files could be played\n\
    --list, -l\n\
        print list of effective lookup paths\n\
    --help, -h, -?, --?\n\
        print this usage information and exit\n\
    --version, -V\n\
        print version and exit\n\
    --copying, -C\n\
        print copying permission and exit\n\
General Options:\n\
    --noplay\n\
        do not play sound, just print file path\n\
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
    --theme, -s THEME\n\
        use sound theme named THEME\n\
", argv[0]);
}

int
main(int argc, char *argv[])
{
	Command command = CommandDefault;
	char *buf;

	setlocale(LC_ALL, "");

	buf = basename(argv[0]);
	if (!strcmp(buf, "xdg-sound")) {
		options.noplay = 1;
		command = options.command = CommandPlay;
	} else if (!strcmp(buf, "xdg-sound-list")) {
		options.noplay = 1;
		command = options.command = CommandList;
	} else if (!strcmp(buf, "xdg-sound-whereis")) {
		options.noplay = 1;
		command = options.command = CommandWhereis;
	} else if (!strcmp(buf, "xdg-sound-which")) {
		options.noplay = 1;
		command = options.command = CommandWhich;
	}

	while (1) {
		int c, val;
		char *endptr = NULL;

#ifdef _GNU_SOURCE
		int option_index = 0;
		/* *INDENT-OFF* */
		static struct option long_options[] = {
			{"noplay",	no_argument,		NULL,	'n'},
			{"list",	no_argument,		NULL,	'l'},
			{"which",	no_argument,		NULL,	'w'},
			{"whereis",	no_argument,		NULL,	'W'},
			{"theme",	required_argument,	NULL,	't'},

			{"debug",	optional_argument,	NULL,	'D'},
			{"verbose",	optional_argument,	NULL,	'v'},
			{"help",	no_argument,		NULL,	'h'},
			{"version",	no_argument,		NULL,	'V'},
			{"copying",	no_argument,		NULL,	'C'},
			{"?",		no_argument,		NULL,	'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "nlwWD::v::hVCH?", long_options, &option_index);
#else				/* _GNU_SOURCE */
		c = getopt(argc, argv, "nlwWDvhVCH?");
#endif				/* _GNU_SOURCE */
		if (c == -1) {
			if (options.debug)
				fprintf(stderr, "%s: done options processing\n", argv[0]);
			break;
		}
		switch (c) {
		case 0:
			goto bad_usage;
		case 'n':	/* -n, --noplay */
			options.noplay = 1;
			break;
		case 'l':	/* -l, --list */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandList;
			options.command = CommandList;
			break;
		case 'w':	/* -w, --which */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandWhich;
			options.command = CommandWhich;
			break;
		case 'W':	/* -W, --whereis */
			if (options.command != CommandDefault)
				goto bad_option;
			if (command == CommandDefault)
				command = CommandWhereis;
			options.command = CommandWhereis;
			break;

		case 't':	/* -t, --theme THEME */
			free(options.theme);
			defaults.theme = options.theme = strdup(optarg);
			break;

		case 'h':	/* -h, --help */
		case 'H':	/* -H, --? */
			command = CommandHelp;
			break;
		case 'V':	/* -V, --version */
			command = CommandVersion;
			break;
		case 'C':	/* -C, --copying */
			command = CommandCopying;
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
	if (optind < argc) {
		int n = argc - optind, j = 0;

		options.eventids = calloc(n + 1, sizeof(*options.eventids));
		while (optind < argc)
			options.eventids[j++] = strdup(argv[optind++]);
	} else if (command == CommandDefault || command == CommandPlay || command == CommandWhich
		   || command == CommandWhereis) {
		fprintf(stderr, "%s: EVENTID must be specified\n", argv[0]);
		goto bad_usage;
	}

	switch (command) {
	case CommandDefault:
		command = CommandPlay;
		/* fall thru */
	case CommandPlay:
		DPRINTF(1, "%s: running play\n", argv[0]);
		do_play(argc, argv);
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
	}
	exit(EXIT_SUCCESS);
}

// vim: set sw=8 tw=80 com=srO\:/**,mb\:*,ex\:*/,srO\:/*,mb\:*,ex\:*/,b\:TRANS foldmarker=@{,@} foldmethod=marker:
