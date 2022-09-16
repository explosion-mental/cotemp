#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/extensions/Xrandr.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TEMPERATURE_NORM    6500
#define TEMPERATURE_ZERO    700
#define GAMMA_MULT          65535.0
// Approximation of the `redshift` table from
// https://github.com/jonls/redshift/blob/04760afe31bff5b26cf18fe51606e7bdeac15504/src/colorramp.c#L30-L273
// without limits:
// GAMMA = K0 + K1 * ln(T - T0)
// Red range (T0 = TEMPERATURE_ZERO)
// Green color
#define GAMMA_K0GR          -1.47751309139817
#define GAMMA_K1GR          0.28590164772055
// Blue color
#define GAMMA_K0BR          -4.38321650114872
#define GAMMA_K1BR          0.6212158769447
// Blue range  (T0 = TEMPERATURE_NORM - TEMPERATURE_ZERO)
// Red color
#define GAMMA_K0RB          1.75390204039018
#define GAMMA_K1RB          -0.1150805671482
// Green color
#define GAMMA_K0GB          1.49221604915144
#define GAMMA_K1GB          -0.07513509588921
#define BRIGHTHESS_DIV      65470.988

#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#ifdef DEBUG
#define debug(...)		do { fprintf(stderr, "cotemp(debug): %s:\n", __func__); fprintf(stderr, "\t" __VA_ARGS__); } while (0)
#else
#define debug(...)
#endif /* DEBUG */

/* variables */
static Display *dpy;
static int temp = TEMPERATURE_NORM;
static double brightness = 1.0;

static void usage(void)
{
	fprintf(stderr, "usage: cotemp [-d] [-s screen] [-c crtc] [-t temperature] [-b brightness]\n");
	exit(0);
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
				t = exp((ggreen + 1.0 + gdelta - (GAMMA_K0GR + GAMMA_K0BR)) / (GAMMA_K1GR + GAMMA_K1BR)) + TEMPERATURE_ZERO;
			} else {
				t = (ggreen > 0.0) ? (exp((ggreen - GAMMA_K0GR) / GAMMA_K1GR) + TEMPERATURE_ZERO) : TEMPERATURE_ZERO;
			}
		} else {
			t = exp((ggreen + 1.0 - gdelta - (GAMMA_K0GB + GAMMA_K0RB)) / (GAMMA_K1GB + GAMMA_K1RB)) + (TEMPERATURE_NORM - TEMPERATURE_ZERO);
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

	if (temp < TEMPERATURE_NORM) {
		gred = 1.0;
		if (temp < TEMPERATURE_ZERO) {
			ggreen = 0.0;
			gblue = 0.0;
		} else {
			g = log((double)temp - TEMPERATURE_ZERO);
			ggreen = DoubleTrim(GAMMA_K0GR + GAMMA_K1GR * g, 0.0, 1.0);
			gblue = DoubleTrim(GAMMA_K0BR + GAMMA_K1BR * g, 0.0, 1.0);
		}
	} else {
		g = log((double)temp - (TEMPERATURE_NORM - TEMPERATURE_ZERO));
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

int main(int argc, char **argv)
{
	int i, screens, screen_first = 0, crtc_specified = -1;
	int fdelta = 0;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "ERROR! Ensure DISPLAY is set correctly!\n");
		return EXIT_FAILURE;
	}
	screens = XScreenCount(dpy);

	if (argc < 2) { /* no args, print info for every screen */
		for (i = screen_first; i < screens; i++) {
			get_sct_for_screen(i, crtc_specified);
			printf("Screen: %d\n\tTemperature: %d\n\tBrightness: %0.1f\n", i, temp, brightness);
		}
		XCloseDisplay(dpy);
		return EXIT_SUCCESS;
	}

	for (i = 1; i < argc; i++)
		/* these options take no arguments */
		if (!strcmp(argv[i], "-v") /* prints version information */
		|| !strcmp(argv[i], "--version")) {
			puts("cotemp-"VERSION);
			exit(0);
		} else if (!strcmp(argv[i], "-d")
			|| !strcmp(argv[i], "--delta")) {
			fdelta = 1;
		} else if (i + 1 == argc) {
			usage();
		/* these options take one argument */
		} else if (!strcmp(argv[i], "-s")
			|| !strcmp(argv[i], "--screen")) {
			screen_first = atoi(argv[++i]);
			if (screen_first >= screens) {
				fprintf(stderr, "Invalid screen index: '%d'\n", screen_first);
				XCloseDisplay(dpy);
				return EXIT_FAILURE;
			}
			screens = screen_first + 1;
		} else if (!strcmp(argv[i], "-c")
			|| !strcmp(argv[i], "--crtc")) {
			crtc_specified = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "-t")
			|| !strcmp(argv[i], "--temperature")) {
			temp = atoi(argv[++i]);
			if (temp == 0)
				temp = TEMPERATURE_NORM;
			else if (temp < TEMPERATURE_ZERO) {
				fprintf(stderr, "WARNING! Temperatures below %d cannot be displayed, ignoring value '%d'\n", TEMPERATURE_ZERO, temp);
				temp = TEMPERATURE_ZERO;
			}
		} else if (!strcmp(argv[i], "-b")
			|| !strcmp(argv[i], "--brightness")) {
			brightness = atof(argv[++i]);
			if (brightness < 0.0)
				brightness = 1.0;
		} else
			usage();

	/* run */

	// Set temperature to given value or default for a value of 0
	for (i = screen_first; i < screens; i++) {
		if (fdelta) { // Delta mode: Shift temperature of each screen by given value
			get_sct_for_screen(i, crtc_specified);
			temp += temp;
		}
		sct_for_screen(i, crtc_specified);
	}

	XCloseDisplay(dpy);
	return EXIT_SUCCESS;
}
