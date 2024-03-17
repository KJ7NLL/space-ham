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

int http_get(char *file, const char *orig_url)
{
	const struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_STREAM,
	};

	struct addrinfo *res;
	struct in_addr *addr;
	int s, r;
	char recv_buf[64];

	char *request = NULL;
	char *server = NULL;
	char *path = NULL;
	char *port = "80";
	char *url;
	int ret = 0;

	int url_len = strlen(orig_url);

	url = malloc(url_len + 1);
	if (url == NULL)
	{
		ESP_LOGE(TAG, "... failed to allocate URL memory");

		return -1;
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

	/*
	   Read HTTP response 
	 */
	do
	{
		bzero(recv_buf, sizeof(recv_buf));
		r = read(s, recv_buf, sizeof(recv_buf) - 1);
		for (int i = 0; i < r; i++)
		{
			putchar(recv_buf[i]);
		}
	} while (r > 0);

	ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d.", r, errno);
out_req:
	free(request);

out_close:
	close(s);

out_res:
	freeaddrinfo(res);

out_url:
	free(url);

	return ret;
}
