/* See LICENSE file for copyright and license details. */

/* Custom user profiles:
 * name: a string label to use with -p cli flag
 * from: starting time in which this profile will be enabled
 * to:   final time in which this profile is enabled
 * temperature: screen temperature, default is 6500
 * brightness:  screen brightness [0.0 ... 1.0]
*/
static const Profile profiles[] = {
	/* name          from             to      temperature  brightness */
	{ "day",	"05:00",	"11:45",	6500,	1.0 },
	{ "night",	"18:00",	"24:00",	3000,	0.7 },
};
