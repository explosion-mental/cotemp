/* See LICENSE file for copyright and license details. */

/* Custom user profiles:
 *  - name: a string label to use with -p cli flag
 *  - from: starting time in which this profile will be enabled
 *  - to:   final time in which this profile is enabled
 *  - temperature: screen temperature, default is 6500
 *  - brightness:  screen brightness [0.0 ... 1.0]
 * time format is in 24h
*/
static const Profile profiles[] = {
	/* name          from             to      temperature  brightness */
	{ "day",	"05:00",	"11:45",	6500,	1.0 },
	{ "even",	"17:00",	"18:45",	6000,	0.9 },
	{ "night",	"18:45",	"20:00",	4000,	0.8 },
	{ "latenight",	"20:00",	"24:00",	3000,	0.7 },
	{ "coffee",	NULL   ,	NULL   ,	8000,	1.0 },
	{ "storm",	NULL   ,	NULL   ,	2000,	1.0 },
	{ "campfire",	NULL   ,	NULL   ,	4500,	1.0 },
};

/* seconds to sleep(3) before adjusting the screen */
static const unsigned int interval = 60;
