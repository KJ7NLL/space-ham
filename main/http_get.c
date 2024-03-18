/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
   https://github.com/espressif/esp-idf/blob/master/examples/protocols/http_request/main/http_request_example_main.c
*/

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "ff.h"
#include "fatfs-util.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "sdkconfig.h"

static const char *TAG = "http_get";

void http_parse_url(char *url, char **server, char **path, char **port)
{
	int url_len;

	url_len = strlen(url);

	for (int i = 0; i < url_len; i++)
	{
		if (url[i] == ':' && url[i+1] == '/' && url[i+2] == '/')
		{
			*server = &url[i+3];
			
			// Clobber the slashes so the last else if dosen't run
			url[i+1] = '\0';
			url[i+2] = '\0';

		}
		else if (url[i] == ':')
		{
			url[i] = '\0';
			*port = &url[i+1];
		}
		else if (url[i] == '/')
		{
			url[i] = '\0';

			// The '/' at the beginning of path is missing
			*path = &url[i+1];
			
			break;
		}
	}

	// if path is NULL the it tries to read from a NULL pointer
	if (*path == NULL)
		*path = "";
	if (*port == NULL)
		*port = "80";
}

int http_get(char *file_name, const char *orig_url)
{
	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *res;
	struct in_addr *addr;
	int s;
	char recv_buf[128];

	char *request = NULL;
	char *server = NULL;
	char *path = NULL;
	char *port = "80";
	char *url;
	int ret = 0;

	FIL fil;              /* File object */
	FRESULT f_res = FR_OK;  /* API result code */
	UINT bw;          /* Bytes written */

	int url_len = strlen(orig_url);

	url = malloc(url_len + 1);
	if (url == NULL)
	{
		ESP_LOGE(TAG, "... failed to allocate URL memory");

		ret = -1;
		goto out;
	}
		
	strcpy(url, orig_url);

	http_parse_url(url, &server, &path, &port);

	printf("server: %s\r\n"
		"path: %s\r\n"
		"port: %s\r\n",
		server,
		path,
		port);

	int err = getaddrinfo(server, port, &hints, &res);
	if (err != 0 || res == NULL)
	{
		ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, res);

		ret = -1;
		goto out_url;
	}

	/*
	   Code to print the resolved IP.

	   Note: inet_ntoa is non-reentrant, look at ipaddr_ntoa_r
	   for "real" code 
	 */
	addr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
	ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

	s = socket(res->ai_family, res->ai_socktype, 0);
	if (s < 0)
	{
		ESP_LOGE(TAG, "... Failed to allocate socket.");

		ret = -1;
		goto out_res;
	}
	ESP_LOGI(TAG, "... allocated socket");

	if (connect(s, res->ai_addr, res->ai_addrlen) != 0)
	{
		ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);

		ret = -1;
		goto out_close;
	}

	ESP_LOGI(TAG, "... connected");

	request = malloc(url_len + 128);
	if (request == NULL)
	{
		ESP_LOGE(TAG, "... failed to allocate request memory");

		ret = -1;
		goto out_close;
	}

	snprintf(request, url_len + 128, "GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n",
		path, server);
	if (write(s, request, strlen(request)) < 0)
	{
		ESP_LOGE(TAG, "... socket send failed");

		ret = -1;
		goto out_req;
	}

	ESP_LOGI(TAG, "... socket send success");

	struct timeval receiving_timeout;

	receiving_timeout.tv_sec = 5;
	receiving_timeout.tv_usec = 0;
	if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout,
		       sizeof(receiving_timeout)) < 0)
	{
		ESP_LOGE(TAG, "... failed to set socket receiving timeout");

		ret = -1;
		goto out_req;
	}
	ESP_LOGI(TAG, "... set socket receiving timeout success");

	FILE *fp = fdopen(s, "r");

	if (fp == NULL)
	{
		ESP_LOGE(TAG, "... failed to open socket as a FILE handle");

		ret = -1;
		goto out_req;
	}

	f_res = f_open(&fil, file_name, FA_CREATE_ALWAYS | FA_WRITE);
	if (f_res != FR_OK)
	{
		ESP_LOGE(TAG, "... failed to open %s: %s", file_name, ff_strerror(f_res));

		ret = -1;
		goto out_req;
	}

	bool header_done = false;

	while (fgets(recv_buf, sizeof(recv_buf)-1, fp) != NULL)
	{
		if (!header_done && strcmp(recv_buf, "\r\n") == 0)
		{
			header_done = true;

			continue;
		}

		if (header_done == true)
		{
			f_write(&fil, recv_buf, strlen(recv_buf), &bw);
		}
		else
			printf("%s", recv_buf);
	}

	f_close(&fil);

out_req:
	free(request);

out_close:
	close(s);

out_res:
	freeaddrinfo(res);

out_url:
	free(url);

out:
	return ret;
}
