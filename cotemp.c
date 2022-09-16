/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#include "util.h"

/* macros */
#define LENGTH(X)               (sizeof(X) / sizeof(X[0]))
#ifdef DEBUG
#define debug(...)              do { fprintf(stderr, "cotemp(debug): %s:\n", __func__); fprintf(stderr, "\t" __VA_ARGS__); } while (0)
#else
#define debug(...)
#endif /* DEBUG */
#define GAMMA_MULT              65535.0
#define BRIGHTHESS_DIV          65470.988

// Approximation of the `redshift` table from
// https://github.com/jonls/redshift/blob/04760afe31bff5b26cf18fe51606e7bdeac15504/src/colorramp.c#L30-L273
// without limits:
// GAMMA = K0 + K1 * ln(T - T0)
// Red range (T0 = LowestTemp)
// Green color
#define GAMMA_K0GR             -1.47751309139817
#define GAMMA_K1GR              0.28590164772055
// Blue color
#define GAMMA_K0BR             -4.38321650114872
#define GAMMA_K1BR              0.6212158769447
// Blue range  (T0 = DefaultTemp - LowestTemp)
// Red color
#define GAMMA_K0RB              1.75390204039018
#define GAMMA_K1RB             -0.1150805671482
// Green color
#define GAMMA_K0GB              1.49221604915144
#define GAMMA_K1GB             -0.07513509588921

enum { DefaultTemp = 6500, LowestTemp = 700 };

typedef struct {
	const char *name;
	const char *h1;    /* starting hour */
	const char *h2;    /* final hour */
	const int t;    /* temperature */
	const double b; /* brigthness */
} Profile;

/* variables */
static Display *dpy;
static int temp = DefaultTemp;
static double brightness = 1.0;
static int screens = 1, screen_first = 0, crtc_specified = -1, fdelta = 0;

#include "config.h"

static void cleanup(void)
{
	if (dpy)
		XCloseDisplay(dpy);
}

static void usage(void)
{
	die("usage: cotemp [-d] [-s screen] [-c crtc] [-t temperature] [-b brightness]");
}

static void hhmmfromstr(const char *data, int *h, int *m)
{
	char *ep;

	*h = (int)strtol(data, &ep, 10);
	if (!ep || *ep != ':')
		die("cotemp: cannot parse hour: '%s' - wrong format.", data);

	*m = (int)strtol(ep + 1, &ep, 10);
	if (!ep || *ep != '\0')
		die("cotemp: cannot parse minutes: '%s' - wrong format.", data);
}

static double DoubleTrim(double x, double a, double b)
{
	double buff[3] = {a, x, b};
	return buff[ (int)(x > a) + (int)(x > b) ];
}

static void get_sct_for_screen(int screen, int icrtc)
{
	XRRCrtcGamma *cg;
	XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, RootWindow(dpy, screen));
	int n, c;
	double gdelta = 0.0, t = 0.0;
	double gred = 0.0, ggreen = 0.0, gblue = 0.0; /* gamma RGB */

	/* reset values */
	temp = 0;
	brightness = 1.0;

	n = res->ncrtc;

	if (icrtc >= 0 && icrtc < n)
		n = 1;
	else
		icrtc = 0;

	for (c = icrtc; c < (icrtc + n); c++) {
		cg = XRRGetCrtcGamma(dpy, res->crtcs[c]);
		gred   += cg->red[cg->size - 1];
		ggreen += cg->green[cg->size - 1];
		gblue  += cg->blue[cg->size - 1];
		XRRFreeGamma(cg);
	}

	XFree(res);

	brightness = MAX(gred, ggreen);
	brightness = MAX(gblue, brightness);

	if (brightness > 0.0 && n > 0) {
		gred   /= brightness;
		ggreen /= brightness;
		gblue  /= brightness;
		brightness /= n;
		brightness /= BRIGHTHESS_DIV;
		brightness = DoubleTrim(brightness, 0.0, 1.0);
		debug("Gamma: Red: %f | Green %f | Blue %f | Brightness: %f\n", gred, ggreen, gblue, b);
		gdelta = gblue - gred;

		if (gdelta < 0.0) {
			if (gblue > 0.0) {
				t = exp((ggreen + 1.0 + gdelta - (GAMMA_K0GR + GAMMA_K0BR)) / (GAMMA_K1GR + GAMMA_K1BR)) + LowestTemp;
			} else {
				t = (ggreen > 0.0) ? (exp((ggreen - GAMMA_K0GR) / GAMMA_K1GR) + LowestTemp) : LowestTemp;
			}
		} else {
			t = exp((ggreen + 1.0 - gdelta - (GAMMA_K0GB + GAMMA_K0RB)) / (GAMMA_K1GB + GAMMA_K1RB)) + (DefaultTemp - LowestTemp);
		}
	} else
		brightness = DoubleTrim(brightness, 0.0, 1.0);

	temp = (int)(t + 0.5);
}

static void sct_for_screen(int screen, int icrtc)
{
	XRRCrtcGamma *cg;
	XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, RootWindow(dpy, screen));
	int size, i, n, c;
	double g = 0.0, b = DoubleTrim(brightness, 0.0, 1.0);
	double gred = 0.0, ggreen = 0.0, gblue = 0.0; /* gamma RGB */

	if (temp < DefaultTemp) {
		gred = 1.0;
		if (temp < LowestTemp) {
			ggreen = 0.0;
			gblue = 0.0;
		} else {
			g = log((double)temp - LowestTemp);
			ggreen = DoubleTrim(GAMMA_K0GR + GAMMA_K1GR * g, 0.0, 1.0);
			gblue = DoubleTrim(GAMMA_K0BR + GAMMA_K1BR * g, 0.0, 1.0);
		}
	} else {
		g = log((double)temp - (DefaultTemp - LowestTemp));
		gred = DoubleTrim(GAMMA_K0RB + GAMMA_K1RB * g, 0.0, 1.0);
		ggreen = DoubleTrim(GAMMA_K0GB + GAMMA_K1GB * g, 0.0, 1.0);
		gblue = 1.0;
	}

	debug("Gamma: Red: %f | Green %f | Blue %f | Brightness: %f\n", gred, ggreen, gblue, b);

	n = res->ncrtc;

	if (icrtc >= 0 && icrtc < n)
		n = 1;
	else
		icrtc = 0;

	for (c = icrtc; c < (icrtc + n); c++) {
		size = XRRGetCrtcGammaSize(dpy, res->crtcs[c]);
		cg = XRRAllocGamma(size);

		for (i = 0; i < size; i++) {
			g = GAMMA_MULT * b * (double)i / (double)size;
			cg->red[i]   = (unsigned short int)(g * gred + 0.5);
			cg->green[i] = (unsigned short int)(g * ggreen + 0.5);
			cg->blue[i]  = (unsigned short int)(g * gblue + 0.5);
		}

		XRRSetCrtcGamma(dpy, res->crtcs[c], cg);
		XRRFreeGamma(cg);
	}

	XFree(res);
}

static void run(void)
{
	int i;
	const time_t caltime = time(NULL);
	const struct tm *t = localtime(&caltime); /* broken down time */
	int h, min, h2, min2;

	/* get max value */
	const Profile *p;
	for (p = profiles; p < profiles + LENGTH(profiles); p++) {
		hhmmfromstr(p->h1, &h, &min);
		hhmmfromstr(p->h2, &h2, &min2);
		if (BETWEEN(t->tm_hour * 60 + t->tm_min, h * 60 + min, h2 * 60 + min2)) {
			temp = p->t;
			brightness = p->b;
			break;
		}
	}

	// Set temperature to given value or default for a value of 0
	for (i = screen_first; i < screens; i++) {
		if (fdelta) { // Delta mode: Shift temperature of each screen by given value
			get_sct_for_screen(i, crtc_specified);
			temp += temp;
		}
		sct_for_screen(i, crtc_specified);
	}

}


int main(int argc, char *argv[])
{
	int i;

	if (atexit(cleanup) != 0) /* close dpy on exit */
		die("atexit failed:");
	if (!(dpy = XOpenDisplay(NULL)))
		die("cotemp: cannot open display.");
	if (!(screens = XScreenCount(dpy)))
		die("cotemp: couldn't get screen count.");

	for (i = 1; i < argc; i++)
		/* these options take no arguments */
		if (!strcmp(argv[i], "-v") /* prints version information */
		|| !strcmp(argv[i], "--version")) {
			puts("cotemp-"VERSION);
			exit(0);
		} else if (!strcmp(argv[i], "-d") /* shift temperature value */
			|| !strcmp(argv[i], "--delta")) {
			fdelta = 1;
		} else if (i + 1 == argc) {
			usage();
		/* these options take one argument */
		} else if (!strcmp(argv[i], "-s") /* select a screen */
			|| !strcmp(argv[i], "--screen")) {
			screen_first = atoi(argv[++i]);
			if (screen_first >= screens)
				die("Invalid screen index: '%d'\n", screen_first);
			screens = screen_first + 1;
		} else if (!strcmp(argv[i], "-c") /* select a CRTC */
			|| !strcmp(argv[i], "--crtc")) {
			crtc_specified = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-t") /* set temperature */
			|| !strcmp(argv[i], "--temperature")) {
			temp = atoi(argv[++i]);
			if (temp == 0)
				temp = DefaultTemp;
			else if (temp < LowestTemp) {
				fprintf(stderr, "WARNING! Temperatures below %d cannot be displayed, ignoring value '%d'\n", LowestTemp, temp);
				temp = LowestTemp;
			}
		} else if (!strcmp(argv[i], "-b") /* set brightness */
			|| !strcmp(argv[i], "--brightness")) {
			brightness = atof(argv[++i]);
			if (brightness < 0.0)
				brightness = 1.0;
		} else if (!strcmp(argv[i], "-l") /* output stats about screen(s) */
			|| !strcmp(argv[i], "--list")) {
			for (i = screen_first; i < screens; i++) {
				get_sct_for_screen(i, crtc_specified);
				printf("Screen: %d\n\tTemperature: %d\n\tBrightness: %0.1f\n", i, temp, brightness);
			}
			exit(0);
		} else if (!strcmp(argv[i], "-p") /* select a profile */
			|| !strcmp(argv[i], "--profile")) {
			int found = 0;
			char *name = argv[++i];
			for (int j = 0; j < LENGTH(profiles); j++)
				if (!strcmp(name, profiles[j].name)) {
					temp = profiles[j].t;
					brightness = profiles[j].b;
					found = 1;
					break;
				}
			if (!found)
				fprintf(stderr, "Profile '%s' not found.\n", name);
		} else
			usage();

	while (1) {
		run();
		sleep(interval);
	}

	cleanup();
	return EXIT_SUCCESS;
}
