//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
// 
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Library General Public License for more details.
// 
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// 
//  Copyright (C) 2022- by Ezekiel Wheeler, KJ7NLL and Eric Wheeler, KJ7LNW.
//  All rights reserved.
//
//  The official website and doumentation for space-ham is available here:
//    https://www.kj7nll.radio/
//

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "sgp4sdp4.h"

#include "sat.h"

// Global sat state structure. We want a pointer in case we wish to support
// tracking multiple satellites. 
static sat_t _sat, *sat = &_sat;

/* Observer's geodetic co-ordinates.      */
/* Lat North, Lon East in rads, Alt in km */
static geodetic_t obs_geodetic =
	{45.0*3.141592654/180, -122.0*3.141592654/180, 0.0762, 0.0};

void tle_info(tle_t *s)
{
	printf("%s (%d)\r\n",
		s->sat_name,
		s->catnr);
}

void tle_detail(tle_t *s)
{
	printf("%s (%s)\r\n"
		"  epoch:         %f\r\n"
		"  xno:           %f\r\n"
		"  bstar:         %f\r\n"
		"  xincl:         %f\r\n"
		"  eo:            %f\r\n"
		"  xmo:           %f\r\n"
		"  omegao:        %f\r\n"
		"  xnodeo:        %f\r\n"
		"  xndt2o:        %f\r\n"
		"  xndd6o:        %f\r\n"
		"  catnr:         %d\r\n"
		"  elset:         %d\r\n"
		"  revnum:        %d\r\n",
			s->sat_name,
			s->idesg, 
			s->epoch,
			s->xno,
			s->bstar,
			s->xincl,
			s->eo,
			s->xmo,
			s->omegao,
			s->xnodeo,
			s->xndt2o,
			s->xndd6o,
			s->catnr,
			s->elset,
			s->revnum
		);
}

int tle_csum(char *s)
{
	int csum = 0;
	unsigned int i = 0;

	for (i = 0; i < strlen(s)-1 && i < 68; i++)
	{
		if (isdigit((int)s[i]))
		{
			csum += s[i] - '0';
		}

		else if (s[i] == '-')
			csum++;
	}

	return csum % 10;
}

void sat_init(tle_t *tle)
{
	// Copy the provided tle into our static private satellite structure
	sat->tle = *tle;

	// Clear all flags:
	//
	// Before calling a different ephemeris
	// or changing the TLE set, flow control
	// flags must be cleared in main().
	ClearFlag(ALL_FLAGS);

	// Select ephemeris type:
	//
	// Will set or clear the DEEP_SPACE_EPHEM_FLAG
	// depending on the TLE parameters of the satellite.
	// It will also pre-process tle members for the
	// ephemeris functions SGP4 or SDP4 so this function
	// must be called each time a new tle set is used
	select_ephemeris(&sat->tle);

	sat->ready = 1;
	sat_update();
}

const sat_t *sat_update()
{
	struct tm utc;
	double
		tsince,            // Time since epoch (in minutes)g
		jul_epoch,         // Julian date of epochg
		jul_utc;           // Julian UTC dateg

	// Satellite's predicted geodetic positiong
	geodetic_t sat_geodetic;

	// Zero vector for initializationsg
	vector_t zero_vector = {0,0,0,0};

	// Satellite position and velocity vectorsg
	vector_t vel = zero_vector;
	vector_t pos = zero_vector;
	// Satellite Az, El, Range, Range rateg
	vector_t obs_set;

	// Solar ECI position vectorg
	vector_t solar_vector = zero_vector;
	// Solar observed azi and ele vectorg
	vector_t solar_set;

	if (! sat->ready)
		return NULL;

	// Get UTC calendar and convert to Juliang
	UTC_Calendar_Now(&utc);
	jul_utc = Julian_Date(&utc);

	// Convert satellite's epoch time to Juliang
	// and calculate time since epoch in minutesg
	jul_epoch = Julian_Date_of_Epoch(sat->tle.epoch);
	tsince = (jul_utc - jul_epoch) * 24*60; // minutes per day

	// Call NORAD routines according to deep-space flagg
	if( isFlagSet(DEEP_SPACE_EPHEM_FLAG) )
	{
		SDP4(tsince, &sat->tle, &pos, &vel);
		sat->deep_space = 1;
	}
	else
	{
		SGP4(tsince, &sat->tle, &pos, &vel);
		sat->deep_space = 0;
	}

	// Scale position and velocity vectors to km and km/secg
	Convert_Sat_State( &pos, &vel );

	// Calculate velocity of satelliteg
	Magnitude( &vel );
	sat->sat_vel = vel.w;

	//* All angles in rads. Distance in km. Velocity in km/s *g
	// Calculate satellite Azi, Ele, Range and Range-rateg
	Calculate_Obs(jul_utc, &pos, &vel, &obs_geodetic, &obs_set);

	// Calculate satellite Lat North, Lon East and Alt.g
	Calculate_LatLonAlt(jul_utc, &pos, &sat_geodetic);

	// Calculate solar position and satellite eclipse depthg
	// Also set or clear the satellite eclipsed flag accordingly
	Calculate_Solar_Position(jul_utc, &solar_vector);
	Calculate_Obs(jul_utc, &solar_vector, &zero_vector, &obs_geodetic, &solar_set);

	if( Sat_Eclipsed(&pos, &solar_vector, &sat->eclipse_depth) )
	{
		SetFlag( SAT_ECLIPSED_FLAG );
		sat->eclipsed = 1;
	}
	else
	{
		ClearFlag( SAT_ECLIPSED_FLAG );
		sat->eclipsed = 0;
	}

	// Convert and print satellite and solar datag
	sat->sat_az = Degrees(obs_set.x);
	sat->sat_el = Degrees(obs_set.y);
	sat->sat_range = obs_set.z;
	sat->sat_range_rate = obs_set.w;
	sat->sat_lat = Degrees(sat_geodetic.lat);
	sat->sat_long = Degrees(sat_geodetic.lon);
	sat->sat_alt = sat_geodetic.alt;

	sat->sun_az = Degrees(solar_set.x);
	sat->sun_el = Degrees(solar_set.y);

	return sat;
}

void sat_status()
{
	if (! sat->ready)
		printf("No satellite is being tracked\r\n");
	else
		printf("\r\nTracking %s (%d): %s\r\n"
			"\r\n Azi=%6.1f Ele=%6.1f Range=%8.1f Range Rate=%6.2f"
			"\r\n Lat=%6.1f Lon=%6.1f  Alt=%8.1f  Vel=%8.3f"
			"\r\n Stellite Status: %s - Depth: %2.3f"
			"\r\n Sun Azi=%6.1f Sun Ele=%6.1f\r\n",
			sat->tle.sat_name, sat->tle.catnr,
				isFlagSet(DEEP_SPACE_EPHEM_FLAG) ? "SDP4" : "SGP4",
			sat->sat_az, sat->sat_el, sat->sat_range, sat->sat_range_rate,
			sat->sat_lat, sat->sat_long, sat->sat_alt, sat->sat_vel,
			sat->eclipsed ? "eclipsed" : "in sunlight",
				sat->eclipse_depth,
			sat->sun_az, sat->sun_el);
}

// tle: the tle object
// line: the tle line number
// tle_set: temp buffer that is filled while loading tle lines, must be >= 139 bytes
// buf: input text line
int sat_tle_line(tle_t *tle, int line, char *tle_set, char *buf)
{
	if (line == 0)
	{
		strncpy(tle->sat_name, buf, sizeof(tle->sat_name));
		tle->sat_name[sizeof(tle->sat_name)-1] = 0;
		line++;
	}
	else if (line == 1)
	{
		strncpy(tle_set, buf, 69);
		tle_set[69] = 0;
		line++;
	}
	else if (line == 2)
	{
		strncpy(tle_set+69, buf, 69);
		tle_set[138] = 0;
		if (Good_Elements(tle_set))
		{
			Convert_Satellite_Data(tle_set, tle);
			tle_info(tle);
			line = 0;
		}
		else
			printf("\r\nInvalid tle: %s\r\n", tle->sat_name);
	}

	return line;
}

void sat_reset()
{
	memset(sat, 0, sizeof(sat_t));
}
