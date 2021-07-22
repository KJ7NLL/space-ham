#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "sat.h"

void sat_detail(struct satellite *s)
{
	char catnr[6] = {0};
	char elset[4] = {0};

	memcpy(catnr, s->sgp.catnr, 5);
	memcpy(elset, s->sgp.elset, 3);

	printf("%s\r\n"
		"  epoch:         %f\r\n"
		"  julian_epoch:  %f\r\n"
		"  xno:           %f\r\n"
		"  bstar:         %f\r\n"
		"  xincl:         %f\r\n"
		"  eo:            %f\r\n"
		"  xmo:           %f\r\n"
		"  omegao:        %f\r\n"
		"  xnodeo:        %f\r\n"
		"  xndt2o:        %f\r\n"
		"  xndd6o:        %f\r\n"
		"  catnr:         %s\r\n"
		"  elset:         %s\r\n"
		"  ideep:         %d\r\n",
			s->name,
			s->sgp.epoch,
			s->sgp.julian_epoch,
			s->sgp.xno,
			s->sgp.bstar,
			s->sgp.xincl,
			s->sgp.eo,
			s->sgp.xmo,
			s->sgp.omegao,
			s->sgp.xnodeo,
			s->sgp.xndt2o,
			s->sgp.xndd6o,
			catnr,
			elset,
			s->sgp.ideep
		);
}

int sat_csum(char *s)
{
	int csum = 0;
	int i = 0;

	for (i = 0; i < strlen(s)-1 && i < 68; i++)
	{
		if (isdigit(s[i]))
		{
			csum += s[i] - '0';
		}

		else if (s[i] == '-')
			csum++;
	}

	return csum % 10;
}

int sat_tle_valid(struct tle_ascii *tle)
{
	return sat_csum(tle->l[1]) == tle->l[1][68] - '0' &&
		sat_csum(tle->l[2]) == tle->l[2][68] - '0';
}
