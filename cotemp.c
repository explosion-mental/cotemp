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
#define DELTA_MIN           -1000000

struct temp_status
{
    int temp;
    double brightness;
};

static void usage(char * pname);
static double DoubleTrim(double x, double a, double b);
static struct temp_status get_sct_for_screen(Display *dpy, int screen, int icrtc, int fdebug);
static void sct_for_screen(Display *dpy, int screen, int icrtc, struct temp_status temp, int fdebug);


static void usage(char * pname)
{
    printf("cotemp " VERSION "\n"
           "Usage: cotemp [options] [temperature] [brightness]\n"
           "\tIf the argument is 0, cotemp resets the display to the default temperature (6500K)\n"
           "\tIf no arguments are passed, cotemp estimates the current display temperature and brightness\n"
           "Options:\n"
           "\t-h, --help \t cotemp will display this usage information\n"
           "\t-v, --verbose \t cotemp will display debugging information\n"
           "\t-d, --delta\t cotemp will shift temperature by the temperature value\n"
           "\t-s, --screen N\t cotemp will only select screen specified by given zero-based index\n"
           "\t-c, --crtc N\t cotemp will only select CRTC specified by given zero-based index\n");
}

static double DoubleTrim(double x, double a, double b)
{
    double buff[3] = {a, x, b};
    return buff[ (int)(x > a) + (int)(x > b) ];
}

static struct temp_status get_sct_for_screen(Display *dpy, int screen, int icrtc, int fdebug)
{
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    int n, c;
    double t = 0.0;
    double gammar = 0.0, gammag = 0.0, gammab = 0.0, gammad = 0.0;
    struct temp_status temp;
    temp.temp = 0;
    temp.brightness = 1.0;

    n = res->ncrtc;
    if ((icrtc >= 0) && (icrtc < n))
        n = 1;
    else
        icrtc = 0;
    for (c = icrtc; c < (icrtc + n); c++)
    {
        RRCrtc crtcxid;
        int size;
        XRRCrtcGamma *crtc_gamma;
        crtcxid = res->crtcs[c];
        crtc_gamma = XRRGetCrtcGamma(dpy, crtcxid);
        size = crtc_gamma->size;
        gammar += crtc_gamma->red[size - 1];
        gammag += crtc_gamma->green[size - 1];
        gammab += crtc_gamma->blue[size - 1];

        XRRFreeGamma(crtc_gamma);
    }
    XFree(res);
    temp.brightness = (gammar > gammag) ? gammar : gammag;
    temp.brightness = (gammab > temp.brightness) ? gammab : temp.brightness;
    if (temp.brightness > 0.0 && n > 0)
    {
        gammar /= temp.brightness;
        gammag /= temp.brightness;
        gammab /= temp.brightness;
        temp.brightness /= n;
        temp.brightness /= BRIGHTHESS_DIV;
        temp.brightness = DoubleTrim(temp.brightness, 0.0, 1.0);
        if (fdebug > 0) fprintf(stderr, "DEBUG: Gamma: %f, %f, %f, brightness: %f\n", gammar, gammag, gammab, temp.brightness);
        gammad = gammab - gammar;
        if (gammad < 0.0)
        {
            if (gammab > 0.0)
            {
                t = exp((gammag + 1.0 + gammad - (GAMMA_K0GR + GAMMA_K0BR)) / (GAMMA_K1GR + GAMMA_K1BR)) + TEMPERATURE_ZERO;
            }
            else
            {
                t = (gammag > 0.0) ? (exp((gammag - GAMMA_K0GR) / GAMMA_K1GR) + TEMPERATURE_ZERO) : TEMPERATURE_ZERO;
            }
        }
        else
        {
            t = exp((gammag + 1.0 - gammad - (GAMMA_K0GB + GAMMA_K0RB)) / (GAMMA_K1GB + GAMMA_K1RB)) + (TEMPERATURE_NORM - TEMPERATURE_ZERO);
        }
    }
    else
        temp.brightness = DoubleTrim(temp.brightness, 0.0, 1.0);

    temp.temp = (int)(t + 0.5);

    return temp;
}

static void sct_for_screen(Display *dpy, int screen, int icrtc, struct temp_status temp, int fdebug)
{
    double t = 0.0, b = 1.0, g = 0.0, gammar, gammag, gammab;
    int n, c;
    Window root = RootWindow(dpy, screen);
    XRRScreenResources *res = XRRGetScreenResourcesCurrent(dpy, root);

    t = (double)temp.temp;
    b = DoubleTrim(temp.brightness, 0.0, 1.0);
    if (temp.temp < TEMPERATURE_NORM)
    {
        gammar = 1.0;
        if (temp.temp < TEMPERATURE_ZERO)
        {
            gammag = 0.0;
            gammab = 0.0;
        }
        else
        {
            g = log(t - TEMPERATURE_ZERO);
            gammag = DoubleTrim(GAMMA_K0GR + GAMMA_K1GR * g, 0.0, 1.0);
            gammab = DoubleTrim(GAMMA_K0BR + GAMMA_K1BR * g, 0.0, 1.0);
        }
    }
    else
    {
        g = log(t - (TEMPERATURE_NORM - TEMPERATURE_ZERO));
        gammar = DoubleTrim(GAMMA_K0RB + GAMMA_K1RB * g, 0.0, 1.0);
        gammag = DoubleTrim(GAMMA_K0GB + GAMMA_K1GB * g, 0.0, 1.0);
        gammab = 1.0;
    }
    if (fdebug > 0) fprintf(stderr, "DEBUG: Gamma: %f, %f, %f, brightness: %f\n", gammar, gammag, gammab, b);
    n = res->ncrtc;
    if ((icrtc >= 0) && (icrtc < n))
        n = 1;
    else
        icrtc = 0;
    for (c = icrtc; c < (icrtc + n); c++)
    {
        int size, i;
        RRCrtc crtcxid;
        XRRCrtcGamma *crtc_gamma;
        crtcxid = res->crtcs[c];
        size = XRRGetCrtcGammaSize(dpy, crtcxid);

        crtc_gamma = XRRAllocGamma(size);

        for (i = 0; i < size; i++)
        {
            g = GAMMA_MULT * b * (double)i / (double)size;
            crtc_gamma->red[i] = (unsigned short int)(g * gammar + 0.5);
            crtc_gamma->green[i] = (unsigned short int)(g * gammag + 0.5);
            crtc_gamma->blue[i] = (unsigned short int)(g * gammab + 0.5);
        }

        XRRSetCrtcGamma(dpy, crtcxid, crtc_gamma);
        XRRFreeGamma(crtc_gamma);
    }

    XFree(res);
}

int main(int argc, char **argv)
{
    int i, screen, screens;
    int screen_specified, screen_first, screen_last, crtc_specified;
    struct temp_status temp;
    int fdebug = 0, fdelta = 0, fhelp = 0;
    Display *dpy = XOpenDisplay(NULL);

    if (!dpy)
    {
        perror("XOpenDisplay(NULL) failed");
        fprintf(stderr, "ERROR! Ensure DISPLAY is set correctly!\n");
        return EXIT_FAILURE;
    }
    screens = XScreenCount(dpy);
    screen_first = 0;
    screen_last = screens - 1;
    screen_specified = -1;
    crtc_specified = -1;
    temp.temp = DELTA_MIN;
    temp.brightness = -1.0;
    for (i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i],"-h") == 0) || (strcmp(argv[i],"--help") == 0)) fhelp = 1;
        else if ((strcmp(argv[i],"-v") == 0) || (strcmp(argv[i],"--verbose") == 0)) fdebug = 1;
        else if ((strcmp(argv[i],"-d") == 0) || (strcmp(argv[i],"--delta") == 0)) fdelta = 1;
        else if ((strcmp(argv[i],"-s") == 0) || (strcmp(argv[i],"--screen") == 0))
        {
            i++;
            if (i < argc)
            {
                screen_specified = atoi(argv[i]);
            } else {
                fprintf(stderr, "ERROR! Required value for screen not specified!\n");
                fhelp = 1;
            }
        }
        else if ((strcmp(argv[i],"-c") == 0) || (strcmp(argv[i],"--crtc") == 0))
        {
            i++;
            if (i < argc)
            {
                crtc_specified = atoi(argv[i]);
            } else {
                fprintf(stderr, "ERROR! Required value for crtc not specified!\n");
                fhelp = 1;
            }
        }
        else if (temp.temp == DELTA_MIN) temp.temp = atoi(argv[i]);
        else if (temp.brightness < 0.0) temp.brightness = atof(argv[i]);
        else
        {
            fprintf(stderr, "ERROR! Unknown parameter: %s\n!", argv[i]);
            fhelp = 1;
        }
    }

    if (fhelp > 0)
    {
        usage(argv[0]);
    }
    else if (screen_specified >= screens)
    {
        fprintf(stderr, "ERROR! Invalid screen index: %d!\n", screen_specified);
    }
    else
    {
        if (temp.brightness < 0.0) temp.brightness = 1.0;
        if (screen_specified >= 0)
        {
            screen_first = screen_specified;
            screen_last = screen_specified;
        }
        if ((temp.temp < 0) && (fdelta == 0))
        {
            // No arguments, so print estimated temperature for each screen
            for (screen = screen_first; screen <= screen_last; screen++)
            {
                temp = get_sct_for_screen(dpy, screen, crtc_specified, fdebug);
                printf("Screen %d: temperature ~ %d %f\n", screen, temp.temp, temp.brightness);
            }
        }
        else
        {
            if (fdelta == 0)
            {
                // Set temperature to given value or default for a value of 0
                if (temp.temp == 0)
                {
                    temp.temp = TEMPERATURE_NORM;
                }
                else if (temp.temp < TEMPERATURE_ZERO)
                {
                    fprintf(stderr, "WARNING! Temperatures below %d cannot be displayed.\n", TEMPERATURE_ZERO);
                    temp.temp = TEMPERATURE_ZERO;
                }
                for (screen = screen_first; screen <= screen_last; screen++)
                {
                   sct_for_screen(dpy, screen, crtc_specified, temp, fdebug);
                }
            }
            else
            {
                // Delta mode: Shift temperature of each screen by given value
                for (screen = screen_first; screen <= screen_last; screen++)
                {
                    struct temp_status tempd = get_sct_for_screen(dpy, screen, crtc_specified, fdebug);
                    tempd.temp += temp.temp;
                    if (tempd.temp < TEMPERATURE_ZERO)
                    {
                        fprintf(stderr, "WARNING! Temperatures below %d cannot be displayed.\n", TEMPERATURE_ZERO);
                        tempd.temp = TEMPERATURE_ZERO;
                    }
                    sct_for_screen(dpy, screen, crtc_specified, tempd, fdebug);
                }
            }
        }
    }

    XCloseDisplay(dpy);

    return EXIT_SUCCESS;
}

