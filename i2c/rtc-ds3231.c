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
#include <time.h>

#include "i2c.h"

#include "i2c/rtc-ds3231.h"

void ds3231_write_time(struct tm *rtc)
{
	unsigned char buf[7];
	int month, year, cent;

	// Use mktime to set wday/yday:
	mktime(rtc);

	buf[0] = (rtc->tm_sec-((rtc->tm_sec/10)*10)) | ((rtc->tm_sec/10) << 4);
	buf[1] = (rtc->tm_min-((rtc->tm_min/10)*10)) | ((rtc->tm_min/10) << 4);
	buf[2] = (rtc->tm_hour-((rtc->tm_hour/10)*10)) | ((rtc->tm_hour/10) << 4);
	buf[3] = rtc->tm_wday+1;
	buf[4] = (rtc->tm_mday-((rtc->tm_mday/10)*10)) | ((rtc->tm_mday/10) << 4);

	if (rtc->tm_year + 1900 >= 2100)
		cent = 0x80;
	else
		cent = 0;

	month = rtc->tm_mon+1;
	buf[5] = (month-((month/10)*10)) | ((month/10) << 4) | cent;

	year = rtc->tm_year + 1900;
	year = year - ((year/100)*100);
	buf[6] = (year-((year/10)*10)) | ((year/10) << 4);

	i2c_master_write(0x68 << 1, 0, buf, 7);
}

// Read the current time into `rtc`
void ds3231_read_time(struct tm *rtc)
{
	unsigned char buf[13];
	int cent;

	buf[0] = 0;
	i2c_master_read(0x68 << 1, 0, buf, 12);

	rtc->tm_sec = (buf[0] & 0x0F) + ((buf[0] & 0xF0) >> 4) * 10;
	rtc->tm_min = (buf[1] & 0x0F) + ((buf[1] & 0xF0) >> 4) * 10;
	rtc->tm_hour = (buf[2] & 0x0F) + ((buf[2] & 0x30) >> 4) * 10;
	rtc->tm_mday = (buf[4] & 0x0F) + ((buf[4] & 0x30) >> 4) * 10;
	rtc->tm_mon = ((buf[5] & 0x0F) + ((buf[5] & 0x10) >> 4) * 10) - 1;  // month 0-11

	cent = ((buf[5] & 0x80) >> 7) + 1; // starts at 1900 so add 1 to cent
	rtc->tm_year = (buf[6] & 0x0F) + ((buf[6] & 0xF0) >> 4) * 10 + cent*100;

	// Normalize times if necessary:
	mktime(rtc);
}

