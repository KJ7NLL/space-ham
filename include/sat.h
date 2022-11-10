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
#include "sgp4sdp4.h"

typedef struct {
	tle_t tle;

	double
		// Depth of satellite eclipseg
		eclipse_depth,

		// Satellite's observed position, range, range rate
		sat_az, sat_el, sat_range, sat_range_rate,

		// Satellites geodetic position and velocityg
		sat_lat, sat_long, sat_alt, sat_vel,

		// Solar azmuth and elvationg
		sun_az, sun_el;

	int
		// True if the TLE is valid for tracking
		ready,

		// True if the satellite is in the Earth's shadow
		eclipsed,

		// True if orbital period is more than 225 minutes (SDP4)
		deep_space;
} sat_t;

void tle_info(tle_t *s);
void tle_detail(tle_t *s);
int tle_csum(char *s);

void sat_init(tle_t *tle);
const sat_t *sat_update();
void sat_status();
int sat_tle_line(tle_t *tle, int line, char *tle_set, char *buf);
void sat_reset();
const sat_t *sat_get();
