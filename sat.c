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
