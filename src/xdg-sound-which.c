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
#include <sys/eventfd.h>

#include <assert.h>
#include <locale.h>
#include <stdarg.h>
#include <strings.h>
#include <regex.h>
#include <wordexp.h>
#include <execinfo.h>
#include <pwd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <canberra.h>

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

Command default_command = CommandWhich;

typedef struct {
	int debug;
	int output;
	int info;
	Command command;
	int play;
	int noplay;
	int all;
	int skipdot;
	int skiptilde;
	int showdot;
	int showtilde;
	int standard;
	int extensions;
	int extended;
	int missing;
	int headers;
	int dereflinks;
	char **eventids;
	char *theme;
	char *profile;
	char *context;
	char *user;
	char *home;
	char *datadirs;
	char *datahome;
} Options;

Options options = {
	.debug = 0,
	.output = 1,
	.info = 0,
	.command = CommandDefault,
	.play = 0,
	.noplay = 0,
	.all = 0,
	.skipdot = 0,
	.skiptilde = 0,
	.showdot = 0,
	.showtilde = 0,
	.standard = 0,
	.extensions = 0,
	.extended = 0,
	.missing = 0,
	.headers = 0,
	.dereflinks = 0,
	.eventids = NULL,
	.theme = NULL,
	.profile = NULL,
	.context = NULL,
	.user = NULL,
	.home = NULL,
	.datadirs = NULL,
	.datahome = NULL,
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

static const char *StandardContexts[] = {
	"Alert",
	"Notification",
	"Action",
	"Support",
	"Game",
	NULL
};

static const struct Sound {
	const char *eventid;
	int context;
	int extended;
	const char *data;
} StandardSounds[] = {
	/* Standard Alert Sounds */
	{ "network-connectivity-lost",			0, 0, "Network Connectivity Lost"		 },
	{ "network-connectivity-error",			0, 0, "Network Connectivity Error"		 },
	{ "dialog-error",				0, 0, "Error Dialog"				 },
	{ "dialog-error-critical",			0, 1, "Critical Error Dialog"			 },
	{ "dialog-error-serious",			0, 1, "Serious Error Dialog"			 },
	{ "battery-low",				0, 0, "Low Battery"				 },
	{ "battery-low-keyboard",			0, 1, "Low Keyboard Battery"			 },
	{ "battery-low-mouse",				0, 1, "Low Mouse Battery"			 },
	{ "suspend-error",				0, 0, "Suspend Error"				 },
	{ "software-update-urgent",			0, 0, "Urgent Software Update"			 },
	{ "power-unplug-battery-low",			0, 0, "Power Cord Unplugged Battery Low"	 },
	{ "desktop-session-restore-failed",		0, 1, "Session Restoration Failed"		 },
	{ "desktop-session-save-failed",		0, 1, "Session Save Failed"			 },
	{ "thermal-hot",				0, 1, "Device Hot"				 },
	{ "thermal-hot-cpu",				0, 1, "CPU Hot"					 },
	{ "thermal-hot-gpu",				0, 1, "GPU HOt"					 },
	/* Standard Notifications Sounds */
	{ "message-new-instant",			1, 0, "New Instant Message"			 },
	{ "message-new-email",				1, 0, "New Email Message"			 },
	{ "message-new-voicemail",			1, 1, "New Voicemail Message"			 },
	{ "message-new",				1, 1, "New Message"				 },
	{ "message",					1, 1, "Message"					 },
	{ "complete-media-burn",			1, 0, "Optical Medium Burning Complete"		 },
	{ "complete-media-rip",				1, 0, "Optical Medium Ripping Complete"		 },
	{ "complete-media",				1, 1, "Optical Medium Processing Complete"	 },
	{ "complete-download",				1, 0, "File Download Complete"			 },
	{ "complete-copy",				1, 0, "File Copy Complete"			 },
	{ "complete-scan",				1, 0, "Scanner Completed Image Acquisition"	 },
	{ "complete-print",				1, 1, "File Print Complete"			 },
	{ "complete",					1, 1, "Action Complete"				 },
	{ "phone-incoming-call",			1, 0, "Incoming Phone Call"			 },
	{ "phone-outgoing-busy",			1, 0, "Outgoing Phone Call Busy"		 },
	{ "phone-hangup",				1, 0, "Phone Hung Up"				 },
	{ "phone-failure",				1, 0, "Phone Call Cancelled Due To Error"	 },
	{ "network-connectivity-established",		1, 0, "Network Connectivity Established"	 },
	{ "system-bootup",				1, 0, "Initiating Startup Sequence"		 },
	{ "system-ready",				1, 0, "Ready for System Log In"			 },
	{ "system-shutdown",				1, 0, "Initiating Shutdown Sequence"		 },
	{ "system-shutdown-reboot",			1, 1, "Initiating Restart Sequence"		 },
	{ "search-results",				1, 0, "Search Results Returned"			 },
	{ "search-results-empty",			1, 0, "No Search Results Returned"		 },
	{ "desktop-login",				1, 0, "Welcome to the X Desktop Environment"	 },
	{ "desktop-logout",				1, 0, "Leaving the X Desktop Environment"	 },
	{ "desktop-screen-lock",			1, 0, "Screen Locked"				 },
	{ "desktop-screen-unlock",			1, 0, "Screen Unlocked"				 },
	{ "desktop-layout-changed",			1, 1, "Desktop Layout Changed"			 },
	{ "desktop-session-create",			1, 1, "Session Created"				 },
	{ "desktop-session-restore",			1, 1, "Session Restored"			 },
	{ "desktop-session-save",			1, 1, "Session Saved"				 },
	{ "service-login",				1, 0, "User Logged Into Service"		 },
	{ "service-logout",				1, 0, "User Logged Out of Service"		 },
	{ "battery-caution-keyboard",			1, 1, "Keyboard Battery Nearing Exhaustion"	 },
	{ "battery-caution-mouse",			1, 1, "Mouse Battery Nearing Exhaustion"	 },
	{ "battery-caution",				1, 0, "Battery Nearing Exhaustion"		 },
	{ "battery-full-keyboard",			1, 1, "Keyboard Battery Is Fully Charged"	 },
	{ "battery-full-mouse",				1, 1, "Mouse Battery Is Fully Charged"		 },
	{ "battery-full",				1, 0, "Battery Is Fully Charged"		 },
	{ "battery-charging-keyboard",			1, 1, "Keyboard Battery Charging"		 },
	{ "battery-charging-mouse",			1, 1, "Mouse Battery Charging"			 },
	{ "battery-charging",				1, 1, "Battery Charging"			 },
	{ "battery-discharging-keyboard",		1, 1, "Keyboard Battery Discharging"		 },
	{ "battery-discharging-mouse",			1, 1, "Mouse Battery Discharging"		 },
	{ "battery-discharging",			1, 1, "Battery Discharging"			 },
	{ "battery-empty-keyboard",			1, 1, "Keyboard Battery Empty"			 },
	{ "battery-empty-mouse",			1, 1, "Mouse Battery Empty"			 },
	{ "battery-empty",				1, 1, "Battery Empty"				 },
	{ "dialog-warning",				1, 0, "Warning Dialog"				 },
	{ "dialog-information",				1, 0, "Information Dialog"			 },
	{ "dialog-question",				1, 0, "Question Dialog"				 },
	{ "software-update-available",			1, 0, "Software Update Available"		 },
	{ "device-added",				1, 0, "Device Added"				 },
	{ "device-removed",				1, 0, "Device Removed"				 },
	{ "window-new",					1, 0, "New Window Opened"			 },
	{ "window-question",				1, 1, "Question Window Opened"			 },
	{ "window-close-kill-process",			1, 1, "Window Killed by Process Identifier"	 },
	{ "window-close-kill-server",			1, 1, "Window Killed by Server"			 },
	{ "window-close-kill",				1, 1, "Window Killed"				 },
	{ "window-ping",				1, 1, "Window Pinged"				 },
	{ "power-plug",					1, 0, "The power cable has been plugged in"	 },
	{ "power-unplug",				1, 0, "The power cable has been unplugged"	 },
	{ "suspend-start",				1, 0, "Machine Suspending"			 },
	{ "suspend-resume",				1, 0, "Machine Returned From Suspended State"	 },
	{ "lid-open",					1, 0, "Lid Opened"				 },
	{ "lid-close",					1, 0, "Lid Closed"				 },
	{ "alarm-clock-elapsed",			1, 0, "Alarm Clock Elapsed"			 },
	{ "window-attention-active",			1, 0, "Active Window Requires Attention"	 },
	{ "window-attention-inactive",			1, 0, "Inactive Window Requires Attention"	 },
	{ "window-attention",				1, 1, "Window Requires Attention"		 },
	{ "window-urgent",				1, 1, "Urgent Window"				 },
	{ "screen-brighter",				1, 1, "Screen Brighter"				 },
	{ "screen-darker",				1, 1, "Screen Darker"				 },
	{ "thermal-caution-cpu",			1, 1, "CPU has Heated Up"			 },
	{ "thermal-caution-gpu",			1, 1, "GPU has Heated Up"			 },
	{ "thermal-caution",				1, 1, "Device has Heated Up"			 },
	{ "thermal-cool-cpu",				1, 1, "CPU has Cooled Down"			 },
	{ "thermal-cool-gpu",				1, 1, "GPU has Cooled Down"			 },
	{ "thermal-cool",				1, 1, "Device has Cooled Down"			 },
	{ "window-manager-start-adwm",			1, 1, "ADWM Start"				 },
	{ "window-manager-start-blackbox",		1, 1, "Blackbox Start"				 },
	{ "window-manager-start",			1, 1, "Window Manager Start"			 },
	{ "window-manager-reload",			1, 1, "Window Manager Reload"			 },
	/* Standard Action Sounds */
	{ "phone-outgoing-calling",			2, 0, "Outgoing Phone Call In Progress"		 },
	{ "message-sent-instant",			2, 0, "Instant Message Sent"			 },
	{ "message-sent-email",				2, 0, "Email Message Sent"			 },
	{ "bell-terminal",				2, 0, "Terminal Bell"				 },
	{ "bell-window-system",				2, 0, "Window System Bell"			 },
	{ "bell",					2, 1, "Bell"					 },
	{ "trash-empty",				2, 0, "Trash Emptied"				 },
	{ "item-deleted",				2, 0, "Item Deleted"				 },
	{ "file-trash",					2, 0, "File/Folder Sent To Trash"		 },
	{ "camera-shutter",				2, 0, "Camera Shutter"				 },
	{ "camera-focus",				2, 0, "Camera Focus"				 },
	{ "screen-capture",				2, 0, "Screen Capture"				 },
	{ "screen-capture-complete",			2, 1, "Screen Capture Complete"			 },
	{ "count-down",					2, 0, "Counting down"				 },
	{ "completion-success",				2, 0, "Text Completion (Success)"		 },
	{ "completion-fail",				2, 0, "Text Completion (Fail)"			 },
	{ "completion-partial",				2, 0, "Text Completion (Partial)"		 },
	{ "completion-rotation",			2, 0, "Text Completion (Restart)"		 },
	{ "audio-volume-change",			2, 0, "Audio Volume Change"			 },
	{ "audio-channel-left",				2, 0, "Audio Channel Left"			 },
	{ "audio-channel-right",			2, 0, "Audio Channel Right"			 },
	{ "audio-channel-front-left",			2, 0, "Audio Channel Front Left"		 },
	{ "audio-channel-front-right",			2, 0, "Audio Channel Front Right"		 },
	{ "audio-channel-front-center",			2, 0, "Audio Channel Front Center"		 },
	{ "audio-channel-rear-left",			2, 0, "Audio Channel Rear Left"			 },
	{ "audio-channel-rear-right",			2, 0, "Audio Channel Rear Right"		 },
	{ "audio-channel-rear-center",			2, 0, "Audio Channel Rear Center"		 },
	{ "audio-channel-lfe",				2, 0, "Audio Channel Subwoofer"			 },
	{ "audio-channel-side-left",			2, 0, "Audio Channel Side Left"			 },
	{ "audio-channel-side-right",			2, 0, "Audio Channel Side Right"		 },
	{ "audio-test-signal",				2, 0, "Audio Test Signal"			 },
	{ "desktop-created",				2, 1, "Desktop Created"				 },
	{ "desktop-destroyed",				2, 1, "Desktop Destroyed"			 },
	{ "desktop-session-checkpoint",			2, 1, "Session Check-Point"			 },
	{ "window-manager-quit-adwm",			2, 1, "ADWM Shutdown"				 },
	{ "window-manager-quit-blackbox",		2, 1, "Blackbox Shutdown"			 },
	{ "window-manager-quit",			2, 1, "Window Manager Quit"			 },
	{ "window-manager-restart",			2, 1, "Window Manager Restart"			 },
	{ "workspace-created",				2, 1, "Workspace Created"			 },
	{ "workspace-destroyed",			2, 1, "Workspace Destroyed"			 },
	/* Standard Input Feedback Sounds */
	{ "window-close",				3, 0, "Window Closed"				 },
	{ "window-slide-out-sendto",			3, 1, "Window Sent to Other Desktop"		 },
	{ "window-slide-out-shade",			3, 1, "Window Shaded"				 },
	{ "window-slide-out",				3, 0, "Window Slided Out"			 },
	{ "window-slide-in-taketo",			3, 1, "Window Taken to Other Desktop"		 },
	{ "window-slide-in-unshade",			3, 1, "Window Unshaded"				 },
	{ "window-slide-in",				3, 0, "Window Slided In"			 },
	{ "window-slide",				3, 1, "Window Slided"				 },
	{ "windows-slide",				3, 1, "Windows Slided"				 },
	{ "window-stick-pinned",			3, 1, "Window Pinned"				 },
	{ "window-stick-unfloat",			3, 1, "Window Tiled"				 },
	{ "window-stick",				3, 1, "Window Stuck"				 },
	{ "window-unstick-unpinned",			3, 1, "Window Unpinned"				 },
	{ "window-unstick-float",			3, 1, "Window Floated"				 },
	{ "window-unstick",				3, 1, "Window Unstuck"				 },
	{ "window-raise-above",				3, 1, "Window Placed Above Other Windows"	 },
	{ "window-raise",				3, 1, "Window Raised"				 },
	{ "window-lower-below",				3, 1, "Window Placed Below Other Windows"	 },
	{ "window-lower",				3, 1, "Window Lowered"				 },
	{ "window-stack-raise-above",			3, 1, "Window Placed Above Other Windows"	 },
	{ "window-stack-raise",				3, 1, "Window raised"				 },
	{ "window-stack-lower-below",			3, 1, "Window Placed Below Other Windows"	 },
	{ "window-stack-lower",				3, 1, "Window Lowered"				 },
	{ "window-stack",				3, 1, "Window Restacked"			 },
	{ "window-minimized-hidden",			3, 1, "Window Hidden"				 },
	{ "window-minimized",				3, 0, "Window Minimized"			 },
	{ "window-unminimized-unhidden",		3, 1, "Window Revealed"				 },
	{ "window-unminimized",				3, 0, "Window Unminimized"			 },
	{ "window-maximized-vertical",			3, 1, "Window Maximized Vertically"		 },
	{ "window-maximized-horizontal",		3, 1, "Window Maximized Horizontally"		 },
	{ "window-maximized-left",			3, 1, "Window Maximized to the Left"		 },
	{ "window-maximized-right",			3, 1, "Window Maximized to the Right"		 },
	{ "window-maximized-fullscreen",		3, 1, "Window Made Fullscreen"			 },
	{ "window-maximized-filled",			3, 1, "Window Will Fill Available Area"		 },
	{ "window-maximized",				3, 0, "Window Maximized"			 },
	{ "window-unmaximized-vertical",		3, 1, "Window Restored Vertically"		 },
	{ "window-unmaximized-horizontal",		3, 1, "Window Restored Horizontally"		 },
	{ "window-unmaximized-left",			3, 1, "Window Restored from Left"		 },
	{ "window-unmaximized-right",			3, 1, "Window Restored from Right"		 },
	{ "window-unmaximized-fullscreen",		3, 1, "Window Restored from Fullscreen"		 },
	{ "window-unmaximized-unfilled",		3, 1, "Window Restored from Filled"		 },
	{ "window-unmaximized",				3, 0, "Window Unmaximized"			 },
	{ "window-decorated",				3, 1, "Window Decorated"			 },
	{ "window-undecorated",				3, 1, "Window Undecorated"			 },
	{ "window-inactive-click",			3, 0, "Mouse Clicked in Inactive Window"	 },
	{ "window-move-start-topleft",			3, 1, "Window Move Started From Top Left"	 },
	{ "window-move-start-top",			3, 1, "Window Move Started From Top"		 },
	{ "window-move-start-topright",			3, 1, "Window Move Started From Top Right"	 },
	{ "window-move-start-right",			3, 1, "Window Move Started From Right"		 },
	{ "window-move-start-bottomright",		3, 1, "Window Move Started From Bottom Right"	 },
	{ "window-move-start-bottom",			3, 1, "Window Move Started From Bottom"		 },
	{ "window-move-start-bottomleft",		3, 1, "Window Move Started From Bottom Left"	 },
	{ "window-move-start-left",			3, 1, "Window Move Started From Left"		 },
	{ "window-move-start-unsnap-unattract",		3, 1, "Stopped Attracting Move"			 },
	{ "window-move-start-unsnap-unrepel",		3, 1, "Stopped Repelling Move"			 },
	{ "window-move-start-unsnap",			3, 1, "Window Move Unsnapped"			 },
	{ "window-move-start",				3, 0, "Window Move Started"			 },
	{ "window-move-end-cancel",			3, 1, "Window Move Cancelled"			 },
	{ "window-move-end-snap-attract",		3, 1, "Attracting Move"				 },
	{ "window-move-end-snap-repel",			3, 1, "Repelling Move"				 },
	{ "window-move-end-snap",			3, 1, "Window Move Snapped"			 },
	{ "window-move-end",				3, 0, "Window Move Ended"			 },
	{ "window-resize-start-topleft",		3, 1, "Window Resize Started From Top Left"	 },
	{ "window-resize-start-top",			3, 1, "Window Resize Started From Top"		 },
	{ "window-resize-start-topright",		3, 1, "Window Resize Started From Top Right"	 },
	{ "window-resize-start-right",			3, 1, "Window Resize Started From Right"	 },
	{ "window-resize-start-bottomright",		3, 1, "Window Resize Started From Bottom Right"	 },
	{ "window-resize-start-bottom",			3, 1, "Window Resize Started From Bottom"	 },
	{ "window-resize-start-bottomleft",		3, 1, "Window Resize Started From Bottom Left"	 },
	{ "window-resize-start-left",			3, 1, "Window Resize Started From Left"		 },
	{ "window-resize-start-unsnap-unattract",	3, 1, "Stopped Attracting Resize"		 },
	{ "window-resize-start-unsnap-unrepel",		3, 1, "Stopped Repelling Resize"		 },
	{ "window-resize-start-unsnap",			3, 1, "Window Resize Unsnapped"			 },
	{ "window-resize-start",			3, 0, "Window Resize Started"			 },
	{ "window-resize-end-cancel",			3, 1, "Window Resize Cancelled"			 },
	{ "window-resize-end-snap-attract",		3, 1, "Attracting Resize"			 },
	{ "window-resize-end-snap-repel",		3, 1, "Repelling Resize"			 },
	{ "window-resize-end-snap",			3, 1, "Window Resize Snapped"			 },
	{ "window-resize-end",				3, 0, "Window Resize Ended"			 },
	{ "desktop-switch-up",				3, 1, "Desktop Above"				 },
	{ "desktop-switch-left-up",			3, 1, "Desktop Above"				 },
	{ "desktop-switch-left",			3, 0, "Desktop to the Left"			 },
	{ "desktop-switch-down",			3, 1, "Desktop Below"				 },
	{ "desktop-switch-right-down",			3, 1, "Desktop Below"				 },
	{ "desktop-switch-right",			3, 0, "Desktop to the Right"			 },
	{ "window-switch-left",				3, 1, "Window Switched Left"			 },
	{ "window-switch-right",			3, 1, "Window Switched Right"			 },
	{ "window-switch-zoom",				3, 1, "Window Zoomed"				 },
	{ "window-switch",				3, 0, "Window Switched"				 },
	{ "notebook-tab-changed",			3, 0, "Notebook Tab Changed"			 },
	{ "scroll-up",					3, 0, "Scrolling Up"				 },
	{ "scroll-down",				3, 0, "Scrolling Down"				 },
	{ "scroll-left",				3, 0, "Scrolling to the Left"			 },
	{ "scroll-right",				3, 0, "Scrolling to the Right"			 },
	{ "scroll-up-end",				3, 0, "Scroll Up Reached End of Scroll-Bar"	 },
	{ "scroll-down-end",				3, 0, "Scroll Down Reached End of Scroll-Bar"	 },
	{ "scroll-left-end",				3, 0, "Scroll Left Reached End of Scroll-Bar"	 },
	{ "scroll-right-end",				3, 0, "Scroll Right Reached End of Scroll-Bar"	 },
	{ "dialog-ok",					3, 0, "Dialog Acknowledged"			 },
	{ "dialog-cancel",				3, 0, "Dialog Cancelled"			 },
	{ "drag-start",					3, 0, "Drag Started"				 },
	{ "drag-accept",				3, 0, "Drag Accepted"				 },
	{ "drag-fail",					3, 0, "Drag Failed"				 },
	{ "link-pressed",				3, 0, "Link Pressed"				 },
	{ "link-released",				3, 0, "Link Released"				 },
	{ "button-pressed",				3, 0, "Button Pressed"				 },
	{ "button-released",				3, 0, "Button Released"				 },
	{ "menu-click",					3, 0, "Menu Clicked"				 },
	{ "button-toggle-on",				3, 0, "Button Toggled On"			 },
	{ "button-toggle-off",				3, 0, "Button Toggled Off"			 },
	{ "expander-toggle-on",				3, 0, "Expander Activated"			 },
	{ "expander-toggle-off",			3, 0, "Expander Deactivated"			 },
	{ "menu-popup",					3, 0, "Menu Popup"				 },
	{ "menu-popdown",				3, 0, "Menu Popdown"				 },
	{ "menu-replace",				3, 0, "Menu Replace"				 },
	{ "tooltip-popup",				3, 0, "Tooltip Popup"				 },
	{ "tooltip-popdown",				3, 0, "Tooltip Popdown"				 },
	{ "item-selected",				3, 0, "Item Selected"				 },
	{ "desktop-mode-decrease-border",		3, 1, "Decrease Window Border"			 },
	{ "desktop-mode-decrease-margin",		3, 1, "Decrease Window Margin"			 },
	{ "desktop-mode-decrease-width-tiled",		3, 1, "Decrease Master Width Factor"		 },
	{ "desktop-mode-decrease-count-grid-col",	3, 1, "Decrease Column Count"			 },
	{ "desktop-mode-decrease-count-grid-row",	3, 1, "Decrease Row Count"			 },
	{ "desktop-mode-decrease-count-grid",		3, 1, "Decrease Grid Count"			 },
	{ "desktop-mode-decrease-count-tiled",		3, 1, "Decrease Master Count"			 },
	{ "desktop-mode-decrease-count",		3, 1, "Decrease View Count"			 },
	{ "desktop-mode-increase-border",		3, 1, "Increase Window Border"			 },
	{ "desktop-mode-increase-margin",		3, 1, "Increase Window Margin"			 },
	{ "desktop-mode-increase-width-tiled",		3, 1, "Increase Master Width Factor"		 },
	{ "desktop-mode-increase-count-grid-col",	3, 1, "Increase Column Count"			 },
	{ "desktop-mode-increase-count-grid-row",	3, 1, "Increase Row Count"			 },
	{ "desktop-mode-increase-count-grid",		3, 1, "Increase Grid Count"			 },
	{ "desktop-mode-increase-count-tiled",		3, 1, "Increase Master Count"			 },
	{ "desktop-mode-increase-count",		3, 1, "Increase View Count"			 },
	{ "desktop-mode-tiled-right",			3, 1, "Arrange Tiled with Right Master"		 },
	{ "desktop-mode-tiled-left",			3, 1, "Arrange Tiled with Left Master"		 },
	{ "desktop-mode-tiled-top",			3, 1, "Arrange Tiled with Top Master"		 },
	{ "desktop-mode-tiled-bottom",			3, 1, "Arrange Tiled with Bottom Master"	 },
	{ "desktop-mode-tiled",				3, 1, "Arrange Tiled"				 },
	{ "desktop-mode-floating",			3, 1, "Arrange Floating"			 },
	{ "desktop-mode-grid",				3, 1, "Arrange Grid"				 },
	{ "desktop-mode-monocle",			3, 1, "Arrange Monocle"				 },
	{ "desktop-mode",				3, 1, "Arrangement Changed"			 },
	{ "desktop-monitor-switch-left-down",		3, 1, "Monitor below"				 },
	{ "desktop-monitor-switch-left",		3, 1, "Monitor to the left"			 },
	{ "desktop-monitor-switch-right-up",		3, 1, "Monitor above"				 },
	{ "desktop-monitor-switch-right",		3, 1, "Monitor to the right"			 },
	{ "desktop-monitor-switch",			3, 1, "Monitor changed"				 },
	{ "desktop-showing-hideall-decorations",	3, 1, "Undecorate Tiled Windows"		 },
	{ "desktop-showing-hideall-struts",		3, 1, "Hide All Docks and Panels"		 },
	{ "desktop-showing-hideall",			3, 1, "Showing Desktop"				 },
	{ "desktop-showing-showall-decorations",	3, 1, "Decorate Tiled Windows"			 },
	{ "desktop-showing-showall-struts",		3, 1, "Show All Docks and Panels"		 },
	{ "desktop-showing-showall",			3, 1, "Not Showing Desktop"			 },
	{ "desktop-showing",				3, 1, "Desktop Showing"				 },
	{ "desktop-view-flip",				3, 1, "View Flipped"				 },
	{ "desktop-view-rotate",			3, 1, "View Rotated Clockwise"			 },
	{ "desktop-view-unrotate",			3, 1, "View Rotated Counter-Clockwise"		 },
	{ "desktop-view",				3, 1, "View Changed"				 },
	{ "desktop-windows-flip",			3, 1, "Windows Flipped"				 },
	{ "desktop-windows-rotate",			3, 1, "Windows Rotated Clockwise"		 },
	{ "desktop-windows-unrotate",			3, 1, "Windows Rotated Counter-Clockwise"	 },
	{ "desktop-windows",				3, 1, "Windows Changed"				 },
	{ "desktop-zone-flip",				3, 1, "Zone Flipped"				 },
	{ "desktop-zone-rotate",			3, 1, "Zone Rotated Clockwise"			 },
	{ "desktop-zone-unrotate",			3, 1, "Zone Rotated Counter-Clockwise"		 },
	{ "desktop-zone",				3, 1, "Zone Changed"				 },
	/* Standard Game Sounds */
	{ "game-over-winner",				4, 0, "Game-Over (Winner)"			 },
	{ "game-over-loser",				4, 0, "Game-Over (Loser)"			 },
	{ "game-card-shuffle",				4, 0, "Cards Shuffled"				 },
	{ "game-human-move",				4, 0, "Human Move"				 },
	{ "game-computer-move",				4, 0, "Computer Move"				 },
	{ NULL,						0, 0, NULL					 }
};

GHashTable *stdevents = NULL;

static ca_context *ca = NULL;
static int efd = -1;

/** @} */

/** @section Playing Sounds
  * @{ */

/* borrowed from canberra-boot program from libcanberra */

static void
finish_cb(ca_context *c, uint32_t id, int error_code, void *userdata) {
        uint64_t u = 1;

	(void) c;
	(void) id;
	(void) error_code;
	(void) userdata;
        for (;;) {
                if (write(efd, &u, sizeof(u)) > 0)
                        break;

                if (errno != EINTR) {
                        fprintf(stderr, "write() failed: %s\n", strerror(errno));
                        exit(EXIT_FAILURE);
                }
        }
}

static void
init_play(void)
{
	int r;

	if (!options.play || options.noplay)
		return;
	if ((efd = eventfd(0, EFD_CLOEXEC)) < 0) {
		EPRINTF("Failed to create event file descriptor: %s\n", strerror(errno));
		goto fail;
	}
	if ((r = ca_context_create(&ca)) < 0) {
		EPRINTF("Failed to create context: %s\n", ca_strerror(r));
		goto fail;
	}
	if ((r = ca_context_change_props(ca,
					CA_PROP_APPLICATION_NAME, "XDG Sound",
					CA_PROP_APPLICATION_ID, "com.unexicon.xdg-sound.which",
					NULL)) < 0) {
		EPRINTF("Failed to change metadata: %s\n", ca_strerror(r));
		goto fail;
	}
#if 0
	/* should probably not do this */
	const char *env;
	if ((env = getenv("DISPLAY")) && (r = ca_context_change_props(ca, CA_PROP_WINDOW_X11_SCREEN, env, NULL)) < 0) {
		EPRINTF("Failed to change metadata: %s\n", ca_strerror(r));
		goto fail;
	}
#endif
	if ((r = ca_context_open(ca)) < 0) {
		EPRINTF("Failed to open device: %s\n", ca_strerror(r));
		goto fail;
	}
	return;
fail:
	exit(EXIT_FAILURE);
}

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
			g_free(e->OutputProfile);
			e->OutputProfile = g_strjoinv(";", profiles);
			g_free(e->Context);
			e->Context = g_strjoinv(";", contexts);
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

static const struct Sound *
is_standard(const char *eventid)
{
	struct Sound *sound;

	if ((sound = g_hash_table_lookup(stdevents, eventid)))
		if (sound->extended && !options.extended)
			sound = NULL;
	return (sound);
}

static void
play_path(const char *eventid, const char *path)
{
	ca_proplist *p = NULL;
	static const char *disabled = ".disabled", *sound = ".sound";
	const char *env;
	size_t len;
	int r;

	if (!options.play || options.noplay)
		return;
	if (!ca || efd == -1)
		return;
	if (!eventid)
		return;
	len = strlen(path);
	if (strstr(path, disabled) == path + len - strlen(disabled))
		return;
	if (strstr(path, sound) == path + len - strlen(sound))
		return;
	if ((r = ca_proplist_create(&p)) < 0) {
		EPRINTF("Failed to create property list: %s\n", ca_strerror(r));
		return;
	}
	if ((r = ca_proplist_sets(p, CA_PROP_MEDIA_FILENAME, path)) < 0) {
		EPRINTF("Failed to set media filename: %s\n", ca_strerror(r));
		ca_proplist_destroy(p);
		return;
	}
	if ((r = ca_proplist_sets(p, CA_PROP_MEDIA_NAME, eventid)) < 0) {
		EPRINTF("Failed to set media name: %s\n", ca_strerror(r));
		ca_proplist_destroy(p);
		return;
	}
	if ((env = getenv("LANG")) && (r = ca_proplist_sets(p, CA_PROP_MEDIA_LANGUAGE, env)) < 0) {
		EPRINTF("Failed to set media language: %s\n", ca_strerror(r));
		ca_proplist_destroy(p);
		return;
	}
	if ((r = ca_context_play_full(ca, 0, p, finish_cb, NULL)) < 0) {
		EPRINTF("Failed to play event sound: %s\n", ca_strerror(r));
		ca_proplist_destroy(p);
		return;
	}
	for (;;) {
		uint64_t u;

		if (read(efd, &u, sizeof(u)) < 0) {
			if (errno == EINTR)
				break;
			EPRINTF("read() failed: %s\n", strerror(errno));
		} else
			break;
	}
}

static int
output_path(const char *eventid, const char *path)
{
	char *p, *line, *cdir, *file;
	struct stat st;

	file = strdup(path);

	if ((options.extensions && eventid && is_standard(eventid)))
		return 0;
	if (options.standard && eventid && options.missing)
		return 1;
	if (!lstat(file, &st) && S_ISLNK(st.st_mode)) {
		DPRINTF(1, "file %s is a symbolic link\n", file);
		if (options.dereflinks) {
			char *link;

			/* use actual file name, not symbolic link */
			link = calloc(PATH_MAX + 1, sizeof(*link));
			if (readlink(file, link, PATH_MAX) != -1 && *link) {
				DPRINTF(1, "the link is %s\n", link);
				if (link[0] != '/') {
					char *read, *p;

					read = calloc(PATH_MAX + 1, sizeof(*read));
					strncpy(read, file, PATH_MAX);
					if ((p = strrchr(read, '/')))
						p[1] = '\0';
					strncat(read, link, PATH_MAX);
					free(link);
					link = read;
					DPRINTF(1, "the link resolved to %s\n", link);
				}
				free(file);
				file = strdup(link);
			}
			free(link);
		} else {
			DPRINTF(1, "not dereferencing link %s\n", file);
		}
	}

	if (options.skipdot && *file == '.')
		return 0;
	if (options.skiptilde && (*file == '~' || ((p = strstr(file, options.home)) && p == file)))
		return 0;
	cdir = calloc(PATH_MAX + 1, sizeof(*cdir));
	if (!getcwd(cdir, PATH_MAX))
		strncpy(cdir, ".", PATH_MAX);
	line = calloc(PATH_MAX + 1, sizeof(*line));
	if (*file == '~' && !options.showtilde) {
		strncpy(line, options.home, PATH_MAX);
		strncat(line, file + 1, PATH_MAX);
	} else if (*file == '.' && !options.showdot) {
		strncpy(line, cdir, PATH_MAX);
		strncat(line, file + 1, PATH_MAX);
	} else if (options.showtilde && ((p = strstr(file, options.home)) && p == file)) {
		strncpy(line, "~", PATH_MAX);
		strncat(line, file + strlen(options.home), PATH_MAX);
	} else if (options.showdot && ((p = strstr(file, cdir)) && p == file)) {
		strncpy(line, ".", PATH_MAX);
		strncat(line, file + strlen(cdir), PATH_MAX);
	} else
		strncpy(line, file, PATH_MAX);
	if (options.output > 0) {
		if (eventid && options.headers)
			fprintf(stdout, "%s: %s\n", eventid, line);
		else
			fprintf(stdout, "%s\n", line);
	}
	play_path(eventid, path);
	free(cdir);
	free(line);
	free(file);
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
	char *path = NULL, *p;
	struct stat st;
	char *list, *dirs;
	Entry *e = NULL;

	if (!theme)
		return (e);

	list = calloc(PATH_MAX + 1, sizeof(*list));
	strncpy(list, options.datahome, PATH_MAX);
	if (options.skiptilde)
		strncpy(list, options.datadirs, PATH_MAX);
	else {
		strncat(list, ":", PATH_MAX);
		strncat(list, options.datadirs, PATH_MAX);
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
#if 0	/* what do we care? */
	if (!strcmp(theme, "freedesktop")) {
		EPRINTF("called with a fallback theme '%s'\n", theme);
		return;
	}
#endif
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

static char *
lookup_all_themes(void)
{
	char *themes, *list, *dirs;

	themes = calloc(PATH_MAX + 1, sizeof(*themes));
	list = calloc(PATH_MAX + 1, sizeof(*list));
	strncpy(list, options.datahome, PATH_MAX);
	if (options.skiptilde) {
		strncpy(list, options.datadirs, PATH_MAX);
	} else {
		strncat(list, ":", PATH_MAX);
		strncat(list, options.datadirs, PATH_MAX);
	}
	for (dirs = list; dirs && *dirs;) {
		struct dirent *d;
		char *path, *p;
		DIR *dir;

		for (dirs = list; dirs && *dirs;) {
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
				dirs += strlen(dirs);
			}
			path = realloc(path, PATH_MAX + 1);
			strncat(path, "/sounds", PATH_MAX);
			if (!(dir = opendir(path))) {
				DPRINTF(1, "%s: %s\n", path, strerror(errno));
				free(path);
				continue;
			}
			while ((d = readdir(dir))) {
				struct stat st;
				char *file;

				if (d->d_name[0] == '.')
					continue;
				file = calloc(PATH_MAX + 1, sizeof(*file));
				strncpy(file, path, PATH_MAX);
				strncat(file, "/", PATH_MAX);
				strncat(file, d->d_name, PATH_MAX);
				if (!stat(file, &st) && S_ISDIR(st.st_mode)) {
					strncat(file, "/index.theme", PATH_MAX);
					if (!stat(file, &st) && S_ISREG(st.st_mode)) {
						if (*themes)
							strncat(themes, ";", PATH_MAX);
						strncat(themes, d->d_name, PATH_MAX);
					}
				}
				free(file);
			}
		}
	}
	free(list);
	return (themes);
}

static void
lookup_subdir(char *subdirs, const char *theme, const char *profile, const char *context)
{
	char *dirs, *dirsaveptr = NULL, *pfls, *pflsaveptr = NULL, *ctxs, *ctxsaveptr;
	char *dir, *pfl, *ctx;

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
		if (!e->Context) {
			EPRINTF("no contexts!\n");
			free_entry(e);
			return;
		}
		if (!(dirs = strdup(e->Directories)) || !(pfls = strdup(e->OutputProfile)) || !(ctxs = strdup(e->Context))) {
			EPRINTF("cannot dup strings!\n");
			free_entry(e);
			return;
		}
		free_entry(e);
	}
	DPRINTF(1, "theme %s dirs '%s'\n", theme, dirs);
	DPRINTF(1, "theme %s pfls '%s'\n", theme, pfls);
	DPRINTF(1, "theme %s ctxs '%s'\n", theme, ctxs);
	/* technically these should be semicolon separated, some sometimes aren't */
	for (dir = strtok_r(dirs, " ,;:", &dirsaveptr),
	     pfl = strtok_r(pfls, " ,;:", &pflsaveptr),
	     ctx = strtok_r(ctxs, " ,;:", &ctxsaveptr);
	     dir;
	     dir = dir ? strtok_r(NULL, " ,;:", &dirsaveptr) : NULL,
	     pfl = pfl ? strtok_r(NULL, " ,;:", &pflsaveptr) : NULL,
	     ctx = ctx ? strtok_r(NULL, " ,;:", &ctxsaveptr) : NULL) {
		DPRINTF(1, "checking profile '%s' and context '%s' for subdir '%s'\n", pfl, ctx, dir);
		if ((!profile || !strcmp(pfl ? : "", profile)) && (!context || !strcmp(ctx ? : "", context))) {
			if (*subdirs)
				strncat(subdirs, ";", PATH_MAX);
			strncat(subdirs, dir, PATH_MAX);
		}
	}
	free(dirs);
	free(pfls);
	free(ctxs);
	return;
}

static char *
lookup_subdirs(const char *theme, const char *profile, const char *context)
{
	char *subdirs;

	subdirs = calloc(PATH_MAX + 1, sizeof(*subdirs));
	lookup_subdir(subdirs, theme, profile, context);
	return (subdirs);
}

/** @brief look up the file from EVENTID
  *
  * We search in the sound theme directory first (i.e.  /usr/share/sounds/THEME)
  * and then in parent theme directories, and then in
  * /usr/share/sounds/freedesktop theme directory, and then in /usr/share/sounds
  * itself.
  */
static int
lookup_file(const char *name, const char *theme_list)
{
	const gchar *const *locales, *const *locale;
	char *path = NULL, *eventid, *p, *tlist, *themes, *slist, *subdirs;
	const char *theme, *profile, *context, *subdir;
	struct stat st;
	int found_one = 0;

	DPRINTF(2, "looking up %s\n", name);
	locales = g_get_language_names();
	tlist = strdup(theme_list);
	DPRINTF(1, "theme list is '%s'\n", tlist);
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
				for (context = options.context ? : ""; context; context = *context ? "" : NULL) {
					if (!theme && *context) {
						DPRINTF(1, "skipping context '%s' for null theme\n", context);
						continue;
					}
					slist = lookup_subdirs(theme, profile, context);
					DPRINTF(1, "subdir list is '%s' for theme '%s', profile '%s', context '%s'\n", slist, theme, profile, context);
					for (subdirs = slist;;) {
						if (!*subdirs) {
							subdir = NULL;
							if (theme) {
								DPRINTF(1, "skipping empty subdir for theme '%s', profile '%s', context '%s'\n", theme, profile, context);
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
								char *list, *dirs;

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
								strncpy(list, options.datahome, PATH_MAX);
								if (options.skiptilde) {
									strncpy(list, options.datadirs, PATH_MAX);
								} else {
									strncat(list, ":", PATH_MAX);
									strncat(list, options.datadirs, PATH_MAX);
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
									if (!stat(path, &st) && (found_one |= output_path(name, path)) && !options.all) {
										free(list);
										free(eventid);
										free(path);
										free(tlist);
										return (found_one);
									}
									strcpy(p, ".oga");
									DPRINTF(1, "checking '%s'\n", path);
									if (!stat(path, &st) && (found_one |= output_path(name, path)) && !options.all) {
										free(list);
										free(eventid);
										free(path);
										free(tlist);
										return (found_one);
									}
									strcpy(p, ".ogg");
									DPRINTF(1, "checking '%s'\n", path);
									if (!stat(path, &st) && (found_one |= output_path(name, path)) && !options.all) {
										free(list);
										free(eventid);
										free(path);
										free(tlist);
										return (found_one);
									}
									strcpy(p, ".wav");
									DPRINTF(1, "checking '%s'\n", path);
									if (!stat(path, &st) && (found_one |= output_path(name, path)) && !options.all) {
										free(list);
										free(eventid);
										free(path);
										free(tlist);
										return (found_one);
									}
#if 0
									strcpy(p, ".sound");
									DPRINTF(1, "checking '%s'\n", path);
									if (!stat(path, &st) && (found_one |= output_path(name, path)) && !options.all) {
										free(list);
										free(eventid);
										free(path);
										free(tlist);
										return (found_one);
									}
#endif
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
			}
			if (!theme)
				break;
		}
		free(tlist);
	} else {
		path = strdup(eventid);
		DPRINTF(1, "checking '%s'\n", path);
		if (!stat(path, &st)) {
			found_one |= output_path("file", path);
		}
		free(path);
		free(tlist);
	}
	free(eventid);
	return (found_one);
}

static void
list_themes(char *tlist)
{
	const gchar *const *locales, *const *locale;
	char *p, *themes, *slist, *subdirs;
	const char *theme, *profile, *context, *subdir;

	locales = g_get_language_names();
	DPRINTF(1, "theme list is '%s'\n", tlist);

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
			for (context = options.context ? : ""; context; context = *context ? "" : NULL) {
				if (!theme && *context) {
					DPRINTF(1, "skipping context '%s' for null theme\n", context);
					continue;
				}
				slist = lookup_subdirs(theme, profile, context);
				DPRINTF(1, "subdir list is '%s' for theme '%s', profile '%s', context '%s'\n", slist, theme, profile, context);
				for (subdirs = slist;;) {
					if (!*subdirs) {
						subdir = NULL;
						if (theme) {
							DPRINTF(1, "skipping empty subdir for theme '%s', profile '%s', context '%s'\n", theme, profile, context);
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
						char *list, *dirs;

						if (!theme && *locale) {
							DPRINTF(1, "skipping locale '%s' for null theme\n", *locale);
							continue;
						}
						list = calloc(PATH_MAX + 1, sizeof(*list));
						strncpy(list, options.datahome, PATH_MAX);
						if (options.skiptilde) {
							strncpy(list, options.datadirs, PATH_MAX);
						} else {
							strncat(list, ":", PATH_MAX);
							strncat(list, options.datadirs, PATH_MAX);
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
								output_path(NULL, path);
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
		}
		if (!theme)
			break;
	}
	free(tlist);
	return;
}

static void
list_themedirs(void)
{
	char *tlist;

	tlist = lookup_all_themes();
	list_themes(tlist);
}

static void
do_listall(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	init_play();
	list_themedirs();
}


static void
list_paths(void)
{
	char *tlist;

	tlist = lookup_themes();
	list_themes(tlist);
}

static void
do_list(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
	init_play();
	list_paths();
}

static void
do_play(int argc, char *argv[])
{
	char **eventid, *theme_list;

	(void) argc;
	(void) argv;
	options.play = 1;
	options.headers = 1;
	init_play();
	theme_list = lookup_themes();
	for (eventid = options.eventids; eventid && *eventid; eventid++)
		lookup_file(*eventid, theme_list);
	free(theme_list);
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

	if (options.context && *options.context) {
		const char **ctx;
		int context;

		/* need to filter by standard context */
		for (context = 0, ctx = StandardContexts; ctx && *ctx; ctx++, context++)
			if (!strcmp(*ctx, options.context))
				break;
		for (event = events; event; event = event->next) {
			const struct Sound *s;
			const char *p;
			size_t len;

			len = strlen(event->key);

			for (s = StandardSounds; s && s->eventid; s++) {
				if (s->extended && !options.extended)
					continue;
				if (!strcmp(event->key, s->eventid) || ((p = strstr(s->eventid, event->key)) == s->eventid && (p[len] == '-' || p[len] == '\0')))
					break;
			}
			if (!s->eventid) {
				DPRINTF(1, "skipping non-standard sound '%s[%d]'\n", event->key, context);
				continue;
			}
			if (s->eventid && s->context != context) {
				DPRINTF(1, "skipping '%s[%d]' which matches '%s[%d]'\n", event->key, context, s->eventid, s->context);
				continue;
			}
			output_path(event->key, event->file);
		}
	} else {
		for (event = events; event; event = event->next)
			output_path(event->key, event->file);
	}
}

static void
get_events(char *path, const char *theme)
{
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
				if (!lstat(file, &st) && S_ISLNK(st.st_mode)) {
					DPRINTF(1, "file %s is a symbolic link\n", file);
					if (options.dereflinks) {
						char *link;
						/* use actual file name, not symbolic link */
						link = calloc(PATH_MAX + 1, sizeof(*link));
						if (readlinkat(dirfd(dir), file, link, PATH_MAX) != -1 && *link) {
							DPRINTF(1, "the link is %s\n", link);
							if (link[0] != '/') {
								char *read, *p;

								read = calloc(PATH_MAX + 1, sizeof(*read));
								strncpy(read, file, PATH_MAX);
								if ((p = strrchr(read, '/')))
									p[1] = '\0';
								strncat(read ,link, PATH_MAX);
								free(link);
								link = read;
								DPRINTF(1, "the link resolved to %s\n", link);
							}
							free(file);
							file = strdup(link);
						}
						free(link);
					} else {
						DPRINTF(1, "not dereferencing link %s\n", file);
					}
				}
				get_sound_event(key, file);
			}
			free(key);
		} else {
			DPRINTF(1, "%s: %s\n", file, "not a file");
		}
		free(file);
	}
	closedir(dir);
}

static void
do_listem(int argc, char *argv[])
{
	const struct Sound *sound;
	int context = -1;
	char *theme_list;

	(void) argc;
	(void) argv;
	init_play();
	if (options.context && *options.context) {
		const char **ctx;
		int n;

		for (n = 0, ctx = StandardContexts; ctx && *ctx; ctx++, n++) {
			if (!strcmp(*ctx, options.context)) {
				context = n;
				break;
			}
		}
	}
	theme_list = lookup_themes(); /* XXX */
	options.headers = 1;
	for (sound = StandardSounds; sound && sound->eventid; sound++) {
		if (sound->extended && !options.extended)
			continue;
		if (context != -1 && sound->context != context)
			continue;
		if (!options.headers)
			fprintf(stdout, "%s: \n", sound->eventid);
		if (!lookup_file(sound->eventid, theme_list) && options.headers)
			fprintf(stdout, "%s: \n", sound->eventid);
	}
	free(theme_list);
}

static void
do_show(int argc, char *argv[])
{
	const gchar *const *locales, *const *locale;
	char *p, *tlist, *themes, *slist, *subdirs;
	const char *theme, *profile, *context, *subdir;

	(void) argc;
	(void) argv;

	init_play();
	locales = g_get_language_names();
	tlist = lookup_themes();
	DPRINTF(1, "theme list is '%s'\n", tlist);

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
			for (context = options.context ? : ""; context; context = *context ? "" : NULL) {
				if (!theme && *context) {
					DPRINTF(1, "skipping context '%s' for null theme\n", context);
					continue;
				}
				slist = lookup_subdirs(theme, profile, context);
				DPRINTF(1, "subdir list is '%s' for theme '%s', profile '%s', context '%s'\n", slist, theme, profile, context);
				for (subdirs = slist;;) {
					if (!*subdirs) {
						subdir = NULL;
						if (theme) {
							DPRINTF(1, "skipping empty subdir for theme '%s', profile '%s', context '%s'\n", theme, profile, context);
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
						char *list, *dirs;

						if (!theme && *locale) {
							DPRINTF(1, "skipping locale '%s' for null theme\n", *locale);
							continue;
						}
						list = calloc(PATH_MAX + 1, sizeof(*list));
						strncpy(list, options.datahome, PATH_MAX);
						if (options.skiptilde) {
							strncpy(list, options.datadirs, PATH_MAX);
						} else {
							strncat(list, ":", PATH_MAX);
							strncat(list, options.datadirs, PATH_MAX);
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
		}
		if (!theme)
			break;
	}
	free(tlist);
	sort_sound_events();
	options.headers = 1;
	list_sound_events();
}

static void
do_whereis(int argc, char *argv[])
{
	char **eventid, *theme_list;

	(void) argc;
	(void) argv;
	init_play();
	theme_list = lookup_all_themes();
	options.all = 1;
	for (eventid = options.eventids; eventid && *eventid; eventid++)
		lookup_file(*eventid, theme_list);
	free(theme_list);
}

static void
do_which(int argc, char *argv[])
{
	char **eventid, *theme_list;

	(void) argc;
	(void) argv;
	init_play();
	theme_list = lookup_themes();
	for (eventid = options.eventids; eventid && *eventid; eventid++)
		lookup_file(*eventid, theme_list);
	free(theme_list);
}

/** @section Main
  * @{ */

static void
set_defaults(int argc, char *argv[])
{
	(void) argc;
	(void) argv;
}

static void
get_default_dirs(void)
{
	const char *env;

	if (!options.user && !options.home) {
		struct passwd *pw;

		if ((pw = getpwuid(getuid()))) {
			free(options.user);
			options.user = strdup(pw->pw_name);
			free(options.home);
			options.home = strdup(pw->pw_dir);
		} else {
			if ((env = getenv("USER"))) {
				free(options.user);
				options.user = strdup(env);
			}
			if ((env = getenv("HOME"))) {
				free(options.home);
				options.home = strdup(env);
			} else {
				if (options.user) {
					if ((pw = getpwnam(options.user))) {
						free(options.home);
						options.home = strdup(pw->pw_dir);
					} else {	/* guess */
						size_t len = strlen(options.user) + 8;
						char *home = calloc(len, sizeof(*home));

						strncpy(home, "/home/", len);
						strncat(home, options.user, len);
						free(options.home);
						options.home = home;
					}
				} else {
					EPRINTF("cannot determine home directory\n");
					exit(EXIT_FAILURE);
				}
			}
		}
	} else {
		unsetenv("XDG_DATA_DIRS");
		unsetenv("XDG_DATA_HOME");
		unsetenv("XDG_CONFIG_DIRS");
		unsetenv("XDG_CONFIG_HOME");
		unsetenv("XDG_RUNTIME_DIR");
		if (options.user) {
			if (!options.home) {
				struct passwd *pw;

				if ((pw = getpwnam(options.user))) {
					free(options.home);
					options.home = strdup(pw->pw_dir);
				} else {	/* guess */
					size_t len = strlen(options.user) + 8;
					char *home = calloc(len, sizeof(*home));

					strncpy(home, "/home/", len);
					strncat(home, options.user, len);
					free(options.home);
					options.home = home;
				}
			}
		} else if (options.home) {
			char *user, *p;

			user = strdup(options.home);
			/* strip trailing slashes */
			for (p = user + strlen(user) - 1; *p == '/'; *p = '\0', p--) ;
			if ((p = strrchr(user, '/'))) {
				char *base;

				base = strdup(p);
				free(user);
				user = base;
			}
			free(options.user);
			options.user = user;
		}
	}
	if (options.datadirs)
		setenv("XDG_DATA_DIRS", options.datadirs, 1);
	if (options.datahome)
		setenv("XDG_DATA_HOME", options.datahome, 1);
	if (!options.datadirs) {
		free(options.datadirs);
		if ((env = getenv("XDG_DATA_DIRS")))
			options.datadirs = strdup(env);
		else
			options.datadirs = strdup("/usr/local/share:/usr/share");
	}
	if (!options.datahome) {
		free(options.datahome);
		if ((env = getenv("XDG_DATA_HOME")))
			options.datahome = strdup(env);
		else {
			char *data = calloc(PATH_MAX + 1, sizeof(*data));

			strncpy(data, options.home, PATH_MAX);
			strncat(data, "/.local/share", PATH_MAX);
			options.datahome = strdup(data);
			free(data);
		}
	}
}

static void
get_default_profile(void)
{
	if (options.profile)
		return;
	free(options.profile);
	options.profile = strdup("stereo");
}

static void
get_default_theme(void)
{
	char *files, *end, *file;
	Entry *entry;
	int n;

	if (options.theme)
		return;
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
get_default_events(void)
{
	const struct Sound *sound;

	if (stdevents)
		g_hash_table_destroy(stdevents);
	stdevents = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	for (sound = StandardSounds; sound->eventid; sound++) {
		char *eventid;

		for (eventid = strdup(sound->eventid); *eventid;) {
			struct Sound *s;
			char *p;

			s = calloc(1, sizeof(*s));
			*s = *sound;
			DPRINTF(1, "Adding standard event '%s'\n", eventid);
			g_hash_table_replace(stdevents, g_strdup(eventid), s);
			if ((p = strrchr(eventid, '-'))) {
				*p = '\0';
			} else {
				*eventid = '\0';
			}
		}
		free(eventid);
	}
}

static void
get_defaults(int argc, char *argv[])
{
	(void) argc;
	(void) argv;

	get_default_dirs();
	get_default_profile();
	get_default_theme();
	get_default_events();
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
   --play, -p\n\
        play the specified sound files\n\
    --list, -l\n\
        print list of effective lookup paths\n\
    --help, -h, -?, --?\n\
        print this usage information and exit\n\
    --version, -V\n\
        print version and exit\n\
    --copying, -C\n\
        print copying permission and exit\n\
General Options:\n\
    --noplay, -n\n\
        do not play sound, just print sound or file path\n\
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
    --theme, -N THEME             [default: %2$s]\n\
        use sound theme named THEME\n\
    --profile, -P PROFILE         [default: %3$s]\n\
        use sound profile named PROFILE\n\
    --deref, -d                   [default: %6$s]\n\
        dereference symbolic links\n\
    --user, -u USER               [default: %7$s]\n\
        pretend to be specified USER\n\
    --home, -m HOME               [default: %8$s]\n\
        use HOME as home directory\n\
    --xdg-data-dirs, -i DATADIRS  [default: %9$s]\n\
        use DATADIRS for XDG_DATA_DIRS\n\
    --xdg-data-home, -I DATAHOME  [default: %10$s]\n\
        use DATAHOME for XDG_DATA_HOME\n\
    --debug, -D [LEVEL]           [default: %4$d]\n\
        increment or set debug LEVEL\n\
    --verbose, -v [LEVEL]         [default: %5$d]\n\
        increment or set output verbosity LEVEL\n\
        this option may be repeated\n\
", argv[0]
, options.theme
, options.profile
, options.debug
, options.output
, options.dereflinks ? "enabled" : "disabled"
, options.user
, options.home
, options.datadirs
, options.datahome
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
			{"play",		no_argument,		NULL,	'p'},
			{"which",		no_argument,		NULL,	'w'},
			{"noplay",		no_argument,		NULL,	'n'},
			{"list",		no_argument,		NULL,	'l'},

			{"all",			no_argument,		NULL,	'a'},
			{"skip-dot",		no_argument,		NULL,	'o'},
			{"skip-tilde",		no_argument,		NULL,	't'},
			{"show-dot",		no_argument,		NULL,	'O'},
			{"show-tilde",		no_argument,		NULL,	'T'},
			{"theme",		required_argument,	NULL,	'N'},
			{"profile",		required_argument,	NULL,	'P'},
			{"standard",		no_argument,		NULL,	'S'},
			{"extensions",		no_argument,		NULL,	'x'},
			{"extended",		no_argument,		NULL,   'X'},
			{"missing",		no_argument,		NULL,	'M'},
			{"headers",		no_argument,		NULL,	'r'},
			{"deref",		no_argument,		NULL,	'd'},
			{"context",		required_argument,	NULL,	'c'},
			{"user",		required_argument,	NULL,	'u'},
			{"home",		required_argument,	NULL,	'm'},
			{"xdg-data-dirs",	required_argument,	NULL,	'i'},
			{"xdg-data-home",	required_argument,	NULL,	'I'},

			{"debug",		optional_argument,	NULL,	'D'},
			{"verbose",		optional_argument,	NULL,	'v'},
			{"help",		no_argument,		NULL,	'h'},
			{"version",		no_argument,		NULL,	'V'},
			{"copying",		no_argument,		NULL,	'C'},
			{"?",			no_argument,		NULL,	'H'},
			{ 0, }
		};
		/* *INDENT-ON* */

		c = getopt_long_only(argc, argv, "wnlaotOTN:P:SxXMrdc:u:m:i:I:D::v::hVCH?", long_options,
				     &option_index);
#else				/* _GNU_SOURCE */
		c = getopt(argc, argv, "wnlaotOTN:P:SxXMrdc:u:m:i:I:DvhVCH?");
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
			options.play = 1;
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
			options.extensions = 0;
			break;
		case 'x':	/* -x, --extensions */
			options.extensions = 1;
			options.standard = 0;
			options.missing = 0;
			break;
		case 'X':	/* -X, --extended */
			options.extended = 1;
			break;
		case 'M':	/* -M, --missing */
			options.missing = 1;
			options.standard = 1;
			options.extensions = 0;
			break;
		case 'r':	/* -r, --headers */
			options.headers = !options.headers;
			break;
		case 'd':	/* -d, --deref */
			options.dereflinks = 1;
			break;
		case 'c':	/* -c, --context CONTEXT */
			free(options.context);
			options.context = strdup(optarg);
			break;
		case 'u':	/* -u, --user USER */
			free(options.user);
			options.user = strdup(optarg);
			break;
		case 'm':	/* -m, --home HOME */
			free(options.home);
			options.home = strdup(optarg);
			break;
		case 'i':	/* -i, --datadirs DATADIRS */
			free(options.datadirs);
			options.datadirs = strdup(optarg);
			break;
		case 'I':	/* -I, --datahome DATAHOME */
			free(options.datahome);
			options.datahome = strdup(optarg);
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
			options.command = default_command;
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
		if (default_command == CommandWhereis) {
			DPRINTF(1, "%s: running listall\n", argv[0]);
			do_listall(argc, argv);
		} else {
			DPRINTF(1, "%s: running list\n", argv[0]);
			do_list(argc, argv);
		}
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
