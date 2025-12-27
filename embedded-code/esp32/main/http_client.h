#ifndef _HTTP_CLIENT_H
#define _HTTP_CLIENT_H

#include "esp_err.h"
#include <stddef.h>

// Default cloud API endpoint for uploading temperature/humidity
#define CLOUD_API_LOAD_URL "http://101.126.138.110:8000/api/upload"

// Audio buffer size
#define AUDIO_BUFFER_SIZE  (4 * 1024)

esp_err_t http_client_init(void);
esp_err_t http_client_stream_audio(const char *url);
esp_err_t http_client_post_temp_humi(const char *url, float temperature, float humidity);
esp_err_t http_client_get_json(const char *url, char *out, size_t out_size, int *out_status);
void http_client_stop(void);

#endif
