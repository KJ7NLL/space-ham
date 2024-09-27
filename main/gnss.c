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

#include <stdio.h>
#include <time.h>

#include "platform.h"

#include "minmea.h"
#include "config.h"

#define GNSS_BAUD_RATE 115200
#define BUF_SIZE 1024
#define GNSS_UART_PORT_NUM 1
#define GNSS_RXD 18
#define GNSS_TXD 19
#define GNSS_RTS (UART_PIN_NO_CHANGE)
#define GNSS_CTS (UART_PIN_NO_CHANGE)

#define INDENT_SPACES "  "

void gnss_print(void *arg);
void gnss_parse(char *line);

void gnss_init()
{
	uart_config_t uart_config = {
		.baud_rate = GNSS_BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	int intr_alloc_flags = 0;

	ESP_ERROR_CHECK(uart_driver_install(GNSS_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
	ESP_ERROR_CHECK(uart_param_config(GNSS_UART_PORT_NUM, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(GNSS_UART_PORT_NUM, GNSS_TXD, GNSS_RXD, GNSS_RTS, GNSS_CTS));

	xTaskCreate(gnss_print, "gnss", 8192, NULL, 1, NULL);
}

void gnss_print(void *arg)
{
	int i = 0;
	char line[MINMEA_MAX_SENTENCE_LENGTH];

	while (1)
	{
		char c;
		int len = uart_read_bytes(GNSS_UART_PORT_NUM, &c, 1, 500 / portTICK_PERIOD_MS);

		if (len)
		{
			line[i] = c;
			if (c == '\n')
			{
				line[i] = '\0';
				if (config.gnss_passthrough == true)
					printf("%s\r\n", line);
				i = 0;
				gnss_parse(line);
			}
			else
				i++;
		}

	}
}

void gnss_parse(char *line)
{
	switch (minmea_sentence_id(line, false))
	{
	case MINMEA_SENTENCE_RMC:
		{
			struct minmea_sentence_rmc frame;

			if (config.gnss_pos && minmea_parse_rmc(&frame, line))
			{
				if (!isnan(minmea_tocoord(&frame.latitude)))
					config.observer.lat = Radians(minmea_tocoord(&frame.latitude));
				if (!isnan(minmea_tocoord(&frame.longitude)))
					config.observer.lon = Radians(minmea_tocoord(&frame.longitude));

				// Update saved poition every 11.1 kilometers or every .1 kilometers of altitude
				if (Degrees(fabs(config_saved.observer.lat - config.observer.lat)) > 0.1 ||
					Degrees(fabs(config_saved.observer.lon - config.observer.lon)) > 0.1 ||
					fabs(config_saved.observer.alt - config.observer.alt) > 0.1)
				{
					config_save();
					printf("GNSS saved updated position\r\n");
				}

				if (config.gnss_debug == true)
				{
					printf(INDENT_SPACES
					       "$xxRMC: raw coordinates and speed: (%ld/%ld,%ld/%ld) %ld/%ld\n",
					       frame.latitude.value,
					       frame.latitude.scale,
					       frame.longitude.value,
					       frame.longitude.scale,
					       frame.speed.value,
					       frame.speed.scale);

					printf(INDENT_SPACES
					       "$xxRMC fixed-point coordinates and speed scaled to three decimal places: (%ld,%ld) %ld\n",
					       minmea_rescale(&frame.latitude,
							      1000),
					       minmea_rescale(&frame.longitude, 1000),
					       minmea_rescale(&frame.speed,
							      1000));
					printf(INDENT_SPACES
					       "$xxRMC floating point degree coordinates and speed: (%f,%f) %f\n",
					       minmea_tocoord(&frame.latitude),
					       minmea_tocoord(&frame.longitude),
					       minmea_tofloat(&frame.speed));
				}
			}
			else
			{
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxRMC sentence is not parsed\n");
			}
		}
		break;

	case MINMEA_SENTENCE_GGA:
		{
			struct minmea_sentence_gga frame;

			if (minmea_parse_gga(&frame, line))
			{
				if (!isnan(minmea_tocoord(&frame.altitude)))
				{
					// config.observer expects kilometers,
					// but frame.altitude gives meters
					config.observer.alt = minmea_tofloat(&frame.altitude)/1000;
				}

				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxGGA: fix quality: %d\n",
					       frame.fix_quality);
			}
			else
			{
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxGGA sentence is not parsed\n");
			}
		}
		break;

	case MINMEA_SENTENCE_GST:
		{
			struct minmea_sentence_gst frame;

			if (minmea_parse_gst(&frame, line))
			{
				if (config.gnss_debug == true)
				{
					printf(INDENT_SPACES
					       "$xxGST: raw latitude,longitude and altitude error deviation: (%ld/%ld,%ld/%ld,%ld/%ld)\n",
					       frame.latitude_error_deviation.value,
					       frame.latitude_error_deviation.scale,
					       frame.longitude_error_deviation.value,
					       frame.longitude_error_deviation.scale,
					       frame.altitude_error_deviation.value,
					       frame.altitude_error_deviation.scale);

					printf(INDENT_SPACES
					       "$xxGST fixed point latitude,longitude and altitude error deviation"
					       " scaled to one decimal place: (%ld,%ld,%ld)\n",
					       minmea_rescale(&frame.latitude_error_deviation,
						10),
					       minmea_rescale(&frame.longitude_error_deviation,
						10),
					       minmea_rescale(&frame.altitude_error_deviation,
						10));

					printf(INDENT_SPACES
					       "$xxGST floating point degree latitude, longitude and altitude error deviation: (%f,%f,%f)",
					       minmea_tofloat(&frame.latitude_error_deviation),
					       minmea_tofloat(&frame.longitude_error_deviation),
					       minmea_tofloat(&frame.altitude_error_deviation));
				}
			}
			else
			{
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxGST sentence is not parsed\n");
			}
		}
		break;

	case MINMEA_SENTENCE_GSV:
		{
			struct minmea_sentence_gsv frame;

			if (minmea_parse_gsv(&frame, line))
			{
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxGSV: message %d of %d\n",
					       frame.msg_nr,
					       frame.total_msgs);

				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxGSV: satellites in view: %d\n",
					       frame.total_sats);

				for (int i = 0; i < 4; i++)
					if (config.gnss_debug == true)
						printf(INDENT_SPACES
						       "$xxGSV: sat nr %d, elevation: %d, azimuth: %d, snr: %d dbm\n",
						       frame.sats[i].nr,
						       frame.sats[i].elevation,
						       frame.sats[i].azimuth,
						       frame.sats[i].snr);
			}
			else
			{
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxGSV sentence is not parsed\n");
			}
		}
		break;

	case MINMEA_SENTENCE_VTG:
		{
			struct minmea_sentence_vtg frame;

			if (minmea_parse_vtg(&frame, line))
			{
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxVTG: true track degrees = %f\n",
					       minmea_tofloat(&frame.true_track_degrees));
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "        magnetic track degrees = %f\n",
					       minmea_tofloat(&frame.magnetic_track_degrees));
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "        speed knots = %f\n",
					       minmea_tofloat(&frame.speed_knots));
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "        speed kph = %f\n",
					       minmea_tofloat(&frame.speed_kph));
			}
			else
			{
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxVTG sentence is not parsed\n");
			}
		}
		break;

	case MINMEA_SENTENCE_ZDA:
		{
			struct minmea_sentence_zda frame;

			if (config.gnss_time && minmea_parse_zda(&frame, line))
			{
				struct tm tm;
				minmea_getdatetime(&tm, &frame.date, &frame.time);

				if (frame.date.year >= 2024)
				{
					struct timeval now = {0};
					now.tv_sec = mktime(&tm);
					settimeofday(&now, NULL);
				}

				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxZDA: %d:%d:%d %02d.%02d.%d UTC%+03d:%02d\n",
					       frame.time.hours,
					       frame.time.minutes,
					       frame.time.seconds,
					       frame.date.day,
					       frame.date.month,
					       frame.date.year,
					       frame.hour_offset,
					       frame.minute_offset);
			}
			else
			{
				if (config.gnss_debug == true)
					printf(INDENT_SPACES
					       "$xxZDA sentence is not parsed\n");
			}
		}
		break;

	case MINMEA_INVALID:
		{
			if (config.gnss_debug == true)
				printf(INDENT_SPACES
				       "$xxxxx sentence is not valid\n");
		}
		break;

	default:
		{
			if (config.gnss_debug == true)
				printf(INDENT_SPACES
				       "$xxxxx sentence is not parsed\n");
		}
		break;
	}
}
