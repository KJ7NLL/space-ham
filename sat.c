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

#include "sat.h"

void sat_info(tle_t *s)
{
	printf("%s (%d)\r\n",
		s->sat_name,
		s->catnr);
}

void sat_detail(tle_t *s)
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

int sat_csum(char *s)
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
