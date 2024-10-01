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

#include <string.h>

#include "platform.h"

#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "config.h"

#define ESP_MAXIMUM_RETRY  10

#define ESP_WIFI_SAE_MODE      WPA3_SAE_PWE_BOTH

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN

/*
   FreeRTOS event group to signal when we are connected
 */
static EventGroupHandle_t s_wifi_event_group;

/*
   The event group allows multiple bits for each event, but we only care
   about two events: - we are connected to the AP with an IP - we failed to
   connect after the maximum amount of retries 
 */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void event_handler(void *arg, esp_event_base_t event_base,
			  int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
	{
		esp_wifi_connect();
	}
	else if (event_base == WIFI_EVENT
		 && event_id == WIFI_EVENT_STA_DISCONNECTED)
	{
		if (s_retry_num < ESP_MAXIMUM_RETRY)
		{
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		}
		else
		{
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG, "connect to the AP fail");
	}
	else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
	{
		ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;

		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}

void wifi_connect(char *ssid, char *pass)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
							    ESP_EVENT_ANY_ID,
							    &event_handler,
							    NULL,
							    &instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
							    IP_EVENT_STA_GOT_IP,
							    &event_handler,
							    NULL,
							    &instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			/*
			   Authmode threshold resets to WPA2 as default if
			   password matches WPA2 standards (pasword len =>
			   8). If you want to connect the device to
			   deprecated WEP/WPA networks, Please set the
			   threshold value to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK 
			   and set the password with length and format
			   matching to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK
			   standards. 
			 */
			.threshold.authmode =
			ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
			.sae_pwe_h2e = ESP_WIFI_SAE_MODE,
			.sae_h2e_identifier = "",
			},
	};

	memcpy(wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
	memcpy(wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));


	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/*
	   Waiting until either the connection is established
	   (WIFI_CONNECTED_BIT) or connection failed for the maximum number
	   of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler()
	   (see above) 
	 */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
					       WIFI_CONNECTED_BIT |
					       WIFI_FAIL_BIT,
					       pdFALSE,
					       pdFALSE,
					       portMAX_DELAY);

	/*
	   xEventGroupWaitBits() returns the bits before the call returned,
	   hence we can test which event actually happened. 
	 */
	if (bits & WIFI_CONNECTED_BIT)
	{
		ESP_LOGI(TAG, "connected to ap SSID:%s", config.wifi_ssid);
	}
	else if (bits & WIFI_FAIL_BIT)
	{
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
			 config.wifi_ssid, config.wifi_pass);
	}
	else
	{
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}
}

void wifi_init()
{
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();

	if (ret == ESP_ERR_NVS_NO_FREE_PAGES
	    || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
}

int is_wifi_up()
{
	wifi_ap_record_t ap_info;
	return (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK);
}

