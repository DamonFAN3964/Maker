#include "http_client.h"
#include "audio_player.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "HTTPClient";
static bool s_streaming = false;
static TaskHandle_t s_stream_task = NULL;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            // ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Direct audio data playback
                if (evt->data_len > 0) {
                    audio_player_play((uint8_t *)evt->data, evt->data_len);
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void http_stream_task(void *pvParameters)
{
    char *url = (char *)pvParameters;
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = _http_event_handler,
        .buffer_size = AUDIO_BUFFER_SIZE,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        s_streaming = false;
        free(url);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "Starting HTTP stream from: %s", url);
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    s_streaming = false;
    free(url);
    
    ESP_LOGI(TAG, "HTTP stream task finished");
    s_stream_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t http_client_init(void)
{
    ESP_LOGI(TAG, "HTTP client initialized");
    return ESP_OK;
}

typedef struct {
    char *buf;
    size_t cap;
    size_t used;
} http_resp_buf_t;

static esp_err_t http_collect_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id != HTTP_EVENT_ON_DATA || evt->data_len <= 0) {
        return ESP_OK;
    }

    http_resp_buf_t *rb = (http_resp_buf_t *)evt->user_data;
    if (!rb || !rb->buf || rb->cap == 0) {
        return ESP_OK;
    }

    size_t avail = (rb->cap - 1) - rb->used;
    if (avail == 0) {
        return ESP_OK;
    }

    size_t to_copy = (size_t)evt->data_len;
    if (to_copy > avail) {
        to_copy = avail;
    }

    memcpy(rb->buf + rb->used, evt->data, to_copy);
    rb->used += to_copy;
    rb->buf[rb->used] = '\0';
    return ESP_OK;
}

esp_err_t http_client_get_json(const char *url, char *out, size_t out_size, int *out_status)
{
    if (!url || url[0] == '\0' || !out || out_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    out[0] = '\0';
    http_resp_buf_t rb = {
        .buf = out,
        .cap = out_size,
        .used = 0,
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .event_handler = http_collect_event_handler,
        .user_data = &rb,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "accept", "application/json");

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (out_status) {
        *out_status = status;
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "GET %s failed: %s", url, esp_err_to_name(err));
        return err;
    }

    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "GET %s http=%d resp=%s", url, status, out);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t http_client_post_temp_humi(const char *url, float temperature, float humidity)
{
    if (!url || url[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    char json[96];
    int written = snprintf(json, sizeof(json),
                           "{\"temperature\":%.2f,\"humidity\":%.2f}",
                           (double)temperature, (double)humidity);
    if (written < 0 || written >= (int)sizeof(json)) {
        return ESP_ERR_INVALID_SIZE;
    }

    char resp[256] = {0};
    http_resp_buf_t rb = {
        .buf = resp,
        .cap = sizeof(resp),
        .used = 0,
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .event_handler = http_collect_event_handler,
        .user_data = &rb,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }

    esp_http_client_set_header(client, "accept", "application/json");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, (int)strlen(json));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "POST %s failed: %s", url, esp_err_to_name(err));
        return err;
    }

    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "POST %s http=%d resp=%s", url, status, resp);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Uploaded temp/humi: %s (http=%d)", json, status);
    if (resp[0]) {
        ESP_LOGI(TAG, "Cloud response: %s", resp);
    }

    return ESP_OK;
}

esp_err_t http_client_stream_audio(const char *url)
{
    if (s_streaming) {
        ESP_LOGW(TAG, "Already streaming audio");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (url == NULL) {
        ESP_LOGE(TAG, "Invalid URL");
        return ESP_ERR_INVALID_ARG;
    }
    
    char *url_copy = strdup(url);
    if (url_copy == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        return ESP_ERR_NO_MEM;
    }
    
    s_streaming = true;
    
    BaseType_t ret = xTaskCreate(http_stream_task, "http_stream", 8192, url_copy, 5, &s_stream_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create HTTP stream task");
        free(url_copy);
        s_streaming = false;
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

void http_client_stop(void)
{
    if (s_stream_task != NULL) {
        vTaskDelete(s_stream_task);
        s_stream_task = NULL;
    }
    s_streaming = false;
    ESP_LOGI(TAG, "HTTP streaming stopped");
}
