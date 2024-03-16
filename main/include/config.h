#include "sgp4sdp4.h"

typedef struct
{
	geodetic_t observer;
	char username[30];
	float uplink_mhz, downlink_mhz;
	int i2c_freq;
	int lcd_freq;
	int gnss_debug;
	int gnss_passthrough;
	char wifi_ssid[33];
	char wifi_pass[64];
} config_t;

extern config_t config;
