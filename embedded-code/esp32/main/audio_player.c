#include "audio_player.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

#if !USE_MAX98357
#include "es8311.h"
#include "driver/i2c.h"
#endif

static const char *TAG = "AudioPlayer";
static i2s_chan_handle_t tx_chan = NULL;

#if !USE_MAX98357
static void es8311_reset_sequence(void)
{
    if (ES8311_RESET_GPIO == GPIO_NUM_NC) {
        return; // Module handles reset internally
    }

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << ES8311_RESET_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);

    // ES8311 reset is active low
    gpio_set_level(ES8311_RESET_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(ES8311_RESET_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

static esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed");
        return err;
    }
    
    err = i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed");
        return err;
    }
    
    ESP_LOGI(TAG, "I2C initialized successfully");
    return ESP_OK;
}

// Quick bus probe to confirm codec is responding on the expected address
static void i2c_scan_bus(i2c_port_t i2c_num)
{
    ESP_LOGI(TAG, "Scanning I2C bus...");
    uint8_t dummy = 0;
    for (uint8_t addr = 1; addr < 0x7F; addr++) {
        esp_err_t res = i2c_master_write_to_device(i2c_num, addr, &dummy, 1, pdMS_TO_TICKS(20));
        if (res == ESP_OK) {
            ESP_LOGI(TAG, "Found device at 0x%02X", addr);
        }
    }
}
#endif

static esp_err_t i2s_init(void)
{
    esp_err_t ret = ESP_OK;
    
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM, I2S_ROLE_MASTER);
    ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S new channel failed");
        return ret;
    }
    
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = SAMPLE_RATE,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = USE_MAX98357 ? I2S_GPIO_UNUSED : I2S_MCLK,
            .bclk = I2S_BCLK,
            .ws = I2S_WS,
            .dout = I2S_DOUT,
            .din = I2S_DIN,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ret = i2s_channel_init_std_mode(tx_chan, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel init failed");
        return ret;
    }
    
    ret = i2s_channel_enable(tx_chan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel enable failed");
        return ret;
    }
    
    ESP_LOGI(TAG, "I2S initialized successfully");
    return ESP_OK;
}

esp_err_t audio_player_init(void)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Initializing audio player");
    
    // ES8311 模式下才需要 I2C + 编解码器初始化
#if !USE_MAX98357
    // Hardware reset for ES8311 if the pin is wired out
    es8311_reset_sequence();

    // Initialize I2C
    ret = i2c_master_init();
    if (ret != ESP_OK) {
        return ret;
    }

    // Optional: scan to see if ES8311 (0x18) responds
    i2c_scan_bus(I2C_MASTER_NUM);
#endif

    // Initialize I2S
    ret = i2s_init();
    if (ret != ESP_OK) {
        return ret;
    }

#if !USE_MAX98357
    // Initialize ES8311
    vTaskDelay(pdMS_TO_TICKS(100));
    ret = es8311_init(I2C_MASTER_NUM);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Configure ES8311 for DAC mode
    ret = es8311_config_fmt(I2C_MASTER_NUM, ES8311_MODE_DECODE);
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Set default volume to 80%
    ret = es8311_set_voice_volume(I2C_MASTER_NUM, 80);
    ESP_LOGI(TAG, "Audio player initialized (ES8311)");
    return ret;
#else
    ESP_LOGI(TAG, "Audio player initialized (MAX98357, I2S only)");
    return ESP_OK;
#endif
}

esp_err_t audio_player_play(const uint8_t *data, size_t length)
{
    if (tx_chan == NULL || data == NULL || length == 0) {
        ESP_LOGE(TAG, "Invalid parameters for audio playback");
        return ESP_ERR_INVALID_ARG;
    }
    
    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(tx_chan, data, length, &bytes_written, portMAX_DELAY);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed");
        return ret;
    }
    
    if (bytes_written != length) {
        ESP_LOGW(TAG, "Not all bytes written: %d/%d", bytes_written, length);
    }
    
    return ESP_OK;
}

esp_err_t audio_player_set_volume(uint8_t volume)
{
#if !USE_MAX98357
    return es8311_set_voice_volume(I2C_MASTER_NUM, volume);
#else
    // MAX98357 无数字音量控制，返回成功即可
    (void)volume;
    return ESP_OK;
#endif
}

void audio_player_deinit(void)
{
    if (tx_chan != NULL) {
        i2s_channel_disable(tx_chan);
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
    }

#if !USE_MAX98357
    es8311_deinit(I2C_MASTER_NUM);
    i2c_driver_delete(I2C_MASTER_NUM);
#endif

    ESP_LOGI(TAG, "Audio player deinitialized");
}
