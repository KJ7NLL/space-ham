#include "sgp.h"

#define NUM_SAT_RECORDS 10

struct satellite
{
	char name[25];

	struct sgp_data sgp;
};

void sat_info(struct satellite *s);
void sat_detail(struct satellite *s);
int sat_tle_valid(struct tle_ascii *tle);
int sat_csum(char *s);
