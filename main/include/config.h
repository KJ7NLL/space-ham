#include "sgp4sdp4.h"

typedef struct
{
	geodetic_t observer;
	char username[30];
	float uplink_mhz, downlink_mhz;
	int i2c_freq;
} config_t;

extern config_t config;
