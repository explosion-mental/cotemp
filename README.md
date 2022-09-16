cotemp - set the Color Temperature
==================================
cotemp is a simple program that uses Xrandr to apply a color temperature and/or
brightness on every screen, or by selecting one with `-s` flag, every 60
seconds (by default).

Depending on the hour and minute defined in a profile, it will switch to that
profile using it's respective temperature and brightness.

Profiles are located in config.h.


Usage
-----
With no arguments cotemp will start to run (forever).

    cotemp [-dlv] [-s screen] [-c crtc] [-t temperature] [-b brightness] [-p profile]

For a more detail information read the man page.

Installation
------------
Edit config.mk to match your local setup (cotemp is installed into
the /usr/local namespace by default).

Afterwards enter the following command to build and install cotemp
(if necessary as root):

    make clean install

Credits
=======
- This is a fork of [sct](https://github.com/faf0/sct) which implemented a lot
  of things like:
	* [Aproximation of the table of resfhit](https://github.com/faf0/sct/pull/10)
	* iterate over all screens
	* clean up code

- Original code was published by Ted Unangst in the public domain:
  https://www.tedunangst.com/flak/post/sct-set-color-temperature

- The following website by Mitchell Charity provides a table for the conversion
  between black-body temperatures and color pixel values:
  http://www.vendian.org/mncharity/dir3/blackbody/

- [Redshift](https://github.com/jonls/redshift) and
  [flux](https://justgetflux.com).

