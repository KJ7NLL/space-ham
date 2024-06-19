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

#include "platform.h"
#include "config.h"

config_t config = {
	// Observer's geodetic co-ordinates.
	// Lat North, Lon East in rads, Alt in km 
	.observer = {45.0*3.141592654/180, -122.0*3.141592654/180, 0.0762, 0.0}, 
	.username = "user",
	.i2c_freq = 10000,
	.lcd_freq = 400000,
	.gnss_debug = false,
	.gnss_passthrough = false,
	.wifi_ssid = "",
	.wifi_pass = "",
};

config_t config_saved = {0};

FRESULT config_save()
{
	config_saved = config;
	return f_write_file("config.bin", &config, sizeof(config));
}

FRESULT config_load()
{
	FRESULT ret;
	ret = f_read_file("config.bin", &config, sizeof(config));

	config_saved = config;

	return ret;
}
