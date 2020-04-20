[xdg-sound -- read me first file.  2020-04-20]: #

xdg-sound
===============

Package `xdg-sound-0.9` was released under GPLv3 license 2020-04-20.

This package contains "C"-language programs and utilities for working
with (locating and playing) event sound files from XDG sound themes.
They are useful when developing or modifying sound themes that comply
with the XDG sound theme specifications.

The source is hosted on
[GitHub](https://github.com/bbidulock/xdg-sound).  What it includes is
just the xdg-sound program and manual page, as well as a handfull of
wrapper scripts.


Release
-------

This is the `xdg-sound-0.9` package, released 2020-04-20.  This
release, and the latest version, can be obtained from [GitHub][1], using
a command such as:

    $> git clone https://github.com/bbidulock/xdg-sound.git

Please see the [NEWS][3] file for release notes and history of user
visible changes for the current version, and the [ChangeLog][4] file for
a more detailed history of implementation changes.  The [TODO][5] file
lists features not yet implemented and other outstanding items.

Please see the [INSTALL][7] file for installation instructions.

When working from `git(1)`, please use this file.  An abbreviated
installation procedure that works for most applications appears below.

This release is published under GPLv3.  Please see the license in the
file [COPYING][9].


Quick Start
-----------

The quickest and easiest way to get `xdg-sound` up and running is to run
the following commands:

    $> git clone https://github.com/bbidulock/xdg-sound.git
    $> cd xdg-sound
    $> ./autogen.sh
    $> ./configure
    $> make
    $> make DESTDIR="$pkgdir" install

This will configure, compile and install `xdg-sound` the quickest.  For
those who like to spend the extra 15 seconds reading `./configure
--help`, some compile time options can be turned on and off before the
build.

For general information on GNU's `./configure`, see the file
[INSTALL][7].


Running
-------

Read the manual page after installation:

    man xdg-sound

The following programs are included in `xdg-sound`:

 - [__`xdg-play(1)`__][10] -- This is the primary program.

 - [__`xdg-sound-which(1)`__][11], [__`xdg-sound-whereis(1)`__][12] --
   Two little programs what parallel `which(1)` and `whereis(1)` for
   sound theme event files instead of binaries.

 - [__`xdg-sound-list(1)`__][13] -- lists event sound files.


Issues
------

Report issues on GitHub [here][2].


[1]: https://github.com/bbidulock/xdg-sound
[2]: https://github.com/bbidulock/xdg-sound/issues
[3]: https://github.com/bbidulock/xdg-sound/blob/0.9/NEWS
[4]: https://github.com/bbidulock/xdg-sound/blob/0.9/ChangeLog
[5]: https://github.com/bbidulock/xdg-sound/blob/0.9/TODO
[6]: https://github.com/bbidulock/xdg-sound/blob/0.9/COMPLIANCE
[7]: https://github.com/bbidulock/xdg-sound/blob/0.9/INSTALL
[8]: https://github.com/bbidulock/xdg-sound/blob/0.9/LICENSE
[9]: https://github.com/bbidulock/xdg-sound/blob/0.9/COPYING
[10]: https://github.com/bbidulock/xdg-sound/blob/0.9/man/xdg-play.pod
[11]: https://github.com/bbidulock/xdg-sound/blob/0.9/man/xdg-sound-which.pod
[12]: https://github.com/bbidulock/xdg-sound/blob/0.9/man/xdg-sound-whereis.pod
[13]: https://github.com/bbidulock/xdg-sound/blob/0.9/man/xdg-sound-list.pod

[ vim: set ft=markdown sw=4 tw=72 nocin nosi fo+=tcqlorn spell: ]: #
