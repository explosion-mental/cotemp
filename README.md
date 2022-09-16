cotemp - set the Color Temperature
==================================
cotemp is a simple program that uses Xrandr to apply a color temperature and/or
brightness on every screen, or by selecting one with `-s` flag.

Usage
-----
With no arguments cotemp will print information about the screens, outputing
temperature and brightness. If the argument of `-t` or `--temperature` is
**0**, the temperature will be set to default (no temperature).

    cotemp [-d] [-s screen] [-c crtc] [-t temperature] [-b brightness]

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
- This is a fork of [sct](https://github.com/faf0/sct) in which I'll be adding
  some nice things that I think make the program even easier and simple to use
  but it will go out of the way of the original program.

- Original code was published by Ted Unangst in the public domain:
  https://www.tedunangst.com/flak/post/sct-set-color-temperature

- The following website by Mitchell Charity provides a table for the conversion
  between black-body temperatures and color pixel values:
  http://www.vendian.org/mncharity/dir3/blackbody/

- [Redshift](https://github.com/jonls/redshift) and
  [flux](https://justgetflux.com).

