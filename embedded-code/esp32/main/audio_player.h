#ifndef _AUDIO_PLAYER_H
#define _AUDIO_PLAYER_H

#include "esp_err.h"

// 选择音频输出器件：0=ES8311 编解码器（默认），1=MAX98357 I2S DAC/功放
#ifndef USE_MAX98357
#define USE_MAX98357 1
#endif

// I2C pins for ES8311（仅在 USE_MAX98357=0 时需要）
#define I2C_MASTER_SCL_IO    GPIO_NUM_18   // SCL
#define I2C_MASTER_SDA_IO    GPIO_NUM_44  // SDA
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ   100000

// Optional ES8311 reset pin (active low). Set to GPIO_NUM_NC if the module self-resets via RC.
#define ES8311_RESET_GPIO     GPIO_NUM_NC

// I2S 引脚（共用）：
//  - 对 ES8311：MCLK/BCLK/LRCK/DOUT/DIN
//  - 对 MAX98357：仅需要 BCLK/LRCK/DOUT，MCLK 设为未用
#define I2S_MCLK             GPIO_NUM_16   // MCLK（MAX98357 可设为 I2S_GPIO_UNUSED）
#define I2S_BCLK             GPIO_NUM_9    // SCLK/BCLK
#define I2S_WS               GPIO_NUM_45   // LRCK/WS
#define I2S_DOUT             GPIO_NUM_10   // DAC DIN（MCU->Codec/DAC）
#define I2S_DIN              GPIO_NUM_11   // ADC DOUT（仅 ES8311 录音用，不播放时可不接）

// Audio configuration
#define SAMPLE_RATE          44100
#define BITS_PER_SAMPLE      16
#define I2S_NUM              I2S_NUM_0

esp_err_t audio_player_init(void);
esp_err_t audio_player_play(const uint8_t *data, size_t length);
esp_err_t audio_player_set_volume(uint8_t volume);
void audio_player_deinit(void);

#endif
