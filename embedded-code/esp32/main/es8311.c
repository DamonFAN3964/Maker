#include "es8311.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ES8311";

static esp_err_t es8311_write_reg(i2c_port_t i2c_num, uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};
    
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ES8311_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, write_buf, 2, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    // ESP_LOGI(TAG,"ret = 0x%02x",ret);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Write register 0x%02x failed", reg_addr);
    }
    return ret;
}

static esp_err_t es8311_read_reg(i2c_port_t i2c_num, uint8_t reg_addr, uint8_t *data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ES8311_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (ES8311_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Read register 0x%02x failed", reg_addr);
    }
    return ret;
}

esp_err_t es8311_init(i2c_port_t i2c_num)
{
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "Initializing ES8311");
    
    // Reset
    ret |= es8311_write_reg(i2c_num, ES8311_RESET_REG00, 0x1F);
    ret |= es8311_write_reg(i2c_num, ES8311_RESET_REG00, 0x00);
    
    // Clock configuration (MCLK = 16.384MHz, LRCK = 44.1kHz)
    ret |= es8311_write_reg(i2c_num, ES8311_CLK_MANAGER_REG01, 0x30);
    ret |= es8311_write_reg(i2c_num, ES8311_CLK_MANAGER_REG02, 0x10);
    ret |= es8311_write_reg(i2c_num, ES8311_CLK_MANAGER_REG03, 0x10);
    ret |= es8311_write_reg(i2c_num, ES8311_CLK_MANAGER_REG04, 0x10);
    ret |= es8311_write_reg(i2c_num, ES8311_CLK_MANAGER_REG05, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_CLK_MANAGER_REG06, 0x18);
    ret |= es8311_write_reg(i2c_num, ES8311_CLK_MANAGER_REG07, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_CLK_MANAGER_REG08, 0xFF);
    
    // Serial Data Port configuration
    ret |= es8311_write_reg(i2c_num, ES8311_SDPIN_REG09, 0x00);
    
    // System configuration
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0A, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0B, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0C, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0D, 0x01);
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0E, 0x02);
    
    // DAC configuration
    ret |= es8311_write_reg(i2c_num, ES8311_DAC_REG31, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_DAC_REG32, 0xBF);
    ret |= es8311_write_reg(i2c_num, ES8311_DAC_REG33, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_DAC_REG34, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_DAC_REG35, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_DAC_REG36, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_DAC_REG37, 0x08);
    
    // GPIO configuration
    ret |= es8311_write_reg(i2c_num, ES8311_GPIO_REG44, 0x08);
    ret |= es8311_write_reg(i2c_num, ES8311_GP_REG45, 0x00);
    
    // Power up
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0D, 0x01);
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0E, 0x02);
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG12, 0x00);
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG13, 0x10);
    ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG14, 0x1A);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "ES8311 initialized successfully");
    } else {
        ESP_LOGE(TAG, "ES8311 initialization failed");
    }
    
    return ret;
}

esp_err_t es8311_config_fmt(i2c_port_t i2c_num, es8311_mode_t mode)
{
    esp_err_t ret = ESP_OK;
    
    if (mode == ES8311_MODE_DECODE) {
        // DAC mode - I2S format, 16-bit
        ret |= es8311_write_reg(i2c_num, ES8311_SDPIN_REG09, 0x00);
        ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0A, 0x00);
    } else {
        // ADC mode
        ret |= es8311_write_reg(i2c_num, ES8311_SDPIN_REG09, 0x00);
        ret |= es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0A, 0x00);
    }
    
    return ret;
}

esp_err_t es8311_set_voice_volume(i2c_port_t i2c_num, uint8_t volume)
{
    esp_err_t ret = ESP_OK;
    
    if (volume > 100) {
        volume = 100;
    }
    
    // Volume mapping: 0-100 -> 0-255
    uint8_t reg_val = (volume * 255) / 100;
    
    ret = es8311_write_reg(i2c_num, ES8311_DAC_REG32, reg_val);
    
    ESP_LOGI(TAG, "Set volume to %d%%", volume);
    
    return ret;
}

esp_err_t es8311_deinit(i2c_port_t i2c_num)
{
    // Power down
    esp_err_t ret = es8311_write_reg(i2c_num, ES8311_SYSTEM_REG0E, 0xFF);
    ESP_LOGI(TAG, "ES8311 deinitialized");
    return ret;
}
