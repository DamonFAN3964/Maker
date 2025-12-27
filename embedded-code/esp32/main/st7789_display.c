#include "st7789_display.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// TODO: adjust these pins to match your actual wiring
#define ST7789_PIN_MOSI   GPIO_NUM_14
#define ST7789_PIN_SCLK   GPIO_NUM_13
#define ST7789_PIN_CS     GPIO_NUM_15
#define ST7789_PIN_DC     GPIO_NUM_5
// #define ST7789_PIN_RST    GPIO_NUM_4
#define ST7789_PIN_RST    GPIO_NUM_40
#define ST7789_PIN_BL     GPIO_NUM_48

#define ST7789_SPI_HOST   SPI2_HOST
#define ST7789_SPI_CLOCK_HZ (40 * 1000 * 1000)
#define ST7789_WIDTH      240
#define ST7789_HEIGHT     320

#define COLOR565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

static const char *TAG = "ST7789";
static esp_lcd_panel_handle_t s_panel = NULL;
static TaskHandle_t s_digit_task = NULL;
static SemaphoreHandle_t s_tx_done_sem = NULL;
static bool s_wait_for_tx_done = false;

static uint16_t *s_cmd_anim_buf = NULL;
static const int CMD_ANIM_W = 72;
static const int CMD_ANIM_H = 28;

static bool IRAM_ATTR st7789_on_color_trans_done(esp_lcd_panel_io_handle_t panel_io,
                                                esp_lcd_panel_io_event_data_t *edata,
                                                void *user_ctx)
{
    (void)panel_io;
    (void)edata;
    BaseType_t higher_woken = pdFALSE;
    SemaphoreHandle_t sem = (SemaphoreHandle_t)user_ctx;
    if (sem) {
        xSemaphoreGiveFromISR(sem, &higher_woken);
    }
    return higher_woken == pdTRUE;
}

static void st7789_tx_done_sem_reset(void)
{
    if (!s_tx_done_sem) {
        return;
    }
    while (xSemaphoreTake(s_tx_done_sem, 0) == pdTRUE) {
    }
}

static esp_err_t st7789_draw_bitmap_sync(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    if (!s_panel) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_wait_for_tx_done) {
        st7789_tx_done_sem_reset();
    }

    esp_err_t ret = esp_lcd_panel_draw_bitmap(s_panel, x_start, y_start, x_end, y_end, color_data);
    if (ret != ESP_OK) {
        return ret;
    }

    if (!s_wait_for_tx_done) {
        return ESP_OK;
    }

    if (xSemaphoreTake(s_tx_done_sem, pdMS_TO_TICKS(2000)) != pdTRUE) {
        ESP_LOGW(TAG, "LCD flush timeout (%d,%d)-(%d,%d)", x_start, y_start, x_end, y_end);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static inline void st7789_fill_rect(uint16_t *buf, int buf_w, int x, int y, int w, int h, uint16_t color)
{
    for (int yy = y; yy < y + h; ++yy) {
        uint16_t *row = buf + yy * buf_w;
        for (int xx = x; xx < x + w; ++xx) {
            row[xx] = color;
        }
    }
}

static void st7789_backlight_on(void)
{
    if (ST7789_PIN_BL == GPIO_NUM_NC) {
        return;
    }
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << ST7789_PIN_BL,
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(ST7789_PIN_BL, 1);
}

esp_err_t st7789_display_init(void)
{
    if (s_panel) {
        return ESP_OK;
    }

    if (!s_tx_done_sem) {
        s_tx_done_sem = xSemaphoreCreateBinary();
        if (!s_tx_done_sem) {
            ESP_LOGW(TAG, "tx done semaphore alloc failed, falling back to best-effort drawing");
        }
    }

    spi_bus_config_t buscfg = {
        .sclk_io_num = ST7789_PIN_SCLK,
        .mosi_io_num = ST7789_PIN_MOSI,
        .miso_io_num = GPIO_NUM_NC,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = ST7789_WIDTH * 40 * sizeof(uint16_t),
    };

    ESP_RETURN_ON_ERROR(spi_bus_initialize(ST7789_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "spi bus init failed");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = ST7789_PIN_CS,
        .dc_gpio_num = ST7789_PIN_DC,
        .spi_mode = 0,
        .pclk_hz = ST7789_SPI_CLOCK_HZ,
        .trans_queue_depth = 1,
        .on_color_trans_done = s_tx_done_sem ? st7789_on_color_trans_done : NULL,
        .user_ctx = s_tx_done_sem,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .flags = {
            .dc_low_on_data = 0,
            .cs_high_active = 0,
        },
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(ST7789_SPI_HOST, &io_config, &io_handle), TAG, "panel io init failed");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = ST7789_PIN_RST,
        .color_space = ESP_LCD_COLOR_SPACE_BGR,
        .bits_per_pixel = 16,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(io_handle, &panel_config, &s_panel), TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "panel start failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, false), TAG, "invert failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, false, false), TAG, "mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), TAG, "display on failed");

    st7789_backlight_on();

    s_wait_for_tx_done = (s_tx_done_sem != NULL);
    ESP_LOGI(TAG, "ST7789 init done (%dx%d)", ST7789_WIDTH, ST7789_HEIGHT);
    return ESP_OK;
}

esp_err_t st7789_display_run_self_test(void)
{
    if (!s_panel) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint16_t colors[] = {
        COLOR565(255, 0, 0),
        COLOR565(0, 255, 0),
        COLOR565(0, 0, 255),
        COLOR565(255, 255, 0),
        COLOR565(0, 255, 255),
        COLOR565(255, 0, 255),
        COLOR565(255, 255, 255),
        COLOR565(0, 0, 0),
    };

    const int block_h = 20;
    size_t buf_size = ST7789_WIDTH * block_h * sizeof(uint16_t);
    uint16_t *line_buf = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (!line_buf) {
        return ESP_ERR_NO_MEM;
    }

    for (size_t i = 0; i < sizeof(colors) / sizeof(colors[0]); ++i) {
        for (int y = 0; y < block_h; ++y) {
            for (int x = 0; x < ST7789_WIDTH; ++x) {
                line_buf[y * ST7789_WIDTH + x] = colors[i];
            }
        }
        for (int y = 0; y < ST7789_HEIGHT; y += block_h) {
            int h = (y + block_h > ST7789_HEIGHT) ? (ST7789_HEIGHT - y) : block_h;
            ESP_RETURN_ON_ERROR(st7789_draw_bitmap_sync(0, y, ST7789_WIDTH, y + h, line_buf), TAG, "draw color bar failed");
        }
    }

    // Simple vertical gradient across the whole panel
    for (int y = 0; y < ST7789_HEIGHT; y += block_h) {
        int h = (y + block_h > ST7789_HEIGHT) ? (ST7789_HEIGHT - y) : block_h;
        for (int row = 0; row < h; ++row) {
            uint8_t g = (uint8_t)(((y + row) * 255) / (ST7789_HEIGHT - 1));
            uint16_t color = COLOR565(g / 2, g, g / 2);
            uint16_t *row_ptr = line_buf + row * ST7789_WIDTH;
            for (int x = 0; x < ST7789_WIDTH; ++x) {
                row_ptr[x] = color;
            }
        }
        ESP_RETURN_ON_ERROR(st7789_draw_bitmap_sync(0, y, ST7789_WIDTH, y + h, line_buf), TAG, "draw gradient failed");
    }

    heap_caps_free(line_buf);
    ESP_LOGI(TAG, "ST7789 self-test pattern drawn");
    return ESP_OK;
}

static const struct {
    char c;
    uint8_t cols[5]; // each byte: bit0=top, bit6=bottom
} s_font5x7[] = {
    {' ', {0x00,0x00,0x00,0x00,0x00}},
    {'-', {0x08,0x08,0x08,0x08,0x08}},
    {'0', {0x3E,0x51,0x49,0x45,0x3E}},
    {'1', {0x00,0x42,0x7F,0x40,0x00}},
    {'2', {0x62,0x51,0x49,0x49,0x46}},
    {'3', {0x22,0x49,0x49,0x49,0x36}},
    {'4', {0x18,0x14,0x12,0x7F,0x10}},
    {'5', {0x2F,0x49,0x49,0x49,0x31}},
    {'6', {0x3E,0x49,0x49,0x49,0x32}},
    {'7', {0x01,0x71,0x09,0x05,0x03}},
    {'8', {0x36,0x49,0x49,0x49,0x36}},
    {'9', {0x26,0x49,0x49,0x49,0x3E}},
    {'.', {0x00,0x60,0x60,0x00,0x00}},
    {':', {0x00,0x36,0x36,0x00,0x00}},
    {'%', {0x61,0x12,0x08,0x24,0x43}},
    {'C', {0x3E,0x41,0x41,0x41,0x22}},
    {'D', {0x7F,0x41,0x41,0x41,0x3E}},
    {'E', {0x7F,0x49,0x49,0x49,0x41}},
    {'F', {0x7F,0x09,0x09,0x09,0x01}},
    {'G', {0x3E,0x41,0x49,0x49,0x7A}},
    {'H', {0x7F,0x08,0x08,0x08,0x7F}},
    {'I', {0x41,0x41,0x7F,0x41,0x41}},
    {'L', {0x7F,0x40,0x40,0x40,0x40}},
    {'M', {0x7F,0x02,0x04,0x02,0x7F}},
    {'N', {0x7F,0x02,0x04,0x08,0x7F}},
    {'P', {0x7F,0x09,0x09,0x09,0x06}},
    {'R', {0x7F,0x09,0x19,0x29,0x46}},
    {'S', {0x46,0x49,0x49,0x49,0x31}},
    {'T', {0x01,0x01,0x7F,0x01,0x01}},
    {'U', {0x3F,0x40,0x40,0x40,0x3F}},
    {'W', {0x3F,0x40,0x38,0x40,0x3F}},
    {'A', {0x7E,0x11,0x11,0x11,0x7E}},
    {'O', {0x3E,0x41,0x41,0x41,0x3E}},
    {'V', {0x07,0x38,0x40,0x38,0x07}},
    {'K', {0x7F,0x08,0x14,0x22,0x41}},
};

static const uint8_t *font_lookup(char c)
{
    size_t n = sizeof(s_font5x7) / sizeof(s_font5x7[0]);
    for (size_t i = 0; i < n; ++i) {
        if (s_font5x7[i].c == c) return s_font5x7[i].cols;
    }
    return s_font5x7[0].cols; // space
}

static void st7789_draw_text_to_buf(uint16_t *buf, int buf_w, int buf_h, int x, int y,
                                   const char *text, int scale, uint16_t fg, uint16_t bg)
{
    if (!buf || !text || scale <= 0) {
        return;
    }

    int len = strlen(text);
    if (len <= 0) {
        return;
    }

    int char_w = 5 * scale + 1;
    int char_h = 7 * scale + 1;

    for (int idx = 0; idx < len; ++idx) {
        const uint8_t *glyph = font_lookup(text[idx]);
        for (int col = 0; col < 5; ++col) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 7; ++row) {
                bool on = (bits & (1 << row)) != 0;
                for (int sy = 0; sy < scale; ++sy) {
                    for (int sx = 0; sx < scale; ++sx) {
                        int px = x + idx * char_w + col * scale + sx;
                        int py = y + row * scale + sy;
                        if (px < 0 || py < 0 || px >= buf_w || py >= buf_h) {
                            continue;
                        }
                        buf[py * buf_w + px] = on ? fg : bg;
                    }
                }
            }
        }
    }
}

static esp_err_t st7789_draw_text(int x, int y, const char *text, int scale, uint16_t fg, uint16_t bg)
{
    if (!s_panel) return ESP_ERR_INVALID_STATE;
    if (!text || scale <= 0) return ESP_ERR_INVALID_ARG;

    int len = strlen(text);
    if (len <= 0) return ESP_OK;

    int char_w = 5 * scale + 1;
    int char_h = 7 * scale + 1;
    int width = len * char_w;
    int height = char_h;

    uint16_t *buf = heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) return ESP_ERR_NO_MEM;
    for (int i = 0; i < width * height; ++i) buf[i] = bg;

    for (int idx = 0; idx < len; ++idx) {
        const uint8_t *glyph = font_lookup(text[idx]);
        for (int col = 0; col < 5; ++col) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 7; ++row) {
                if (bits & (1 << row)) {
                    for (int sy = 0; sy < scale; ++sy) {
                        for (int sx = 0; sx < scale; ++sx) {
                            int px = idx * char_w + col * scale + sx;
                            int py = row * scale + sy;
                            buf[py * width + px] = fg;
                        }
                    }
                }
            }
        }
    }

    esp_err_t ret = st7789_draw_bitmap_sync(x, y, x + width, y + height, buf);
    heap_caps_free(buf);
    return ret;
}

esp_err_t st7789_display_draw_cmd_anim(uint8_t frame)
{
    if (!s_panel) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_cmd_anim_buf) {
        s_cmd_anim_buf = heap_caps_malloc(CMD_ANIM_W * CMD_ANIM_H * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (!s_cmd_anim_buf) {
            return ESP_ERR_NO_MEM;
        }
    }

    uint16_t bg = COLOR565(0, 0, 0);
    uint16_t fg = COLOR565(255, 255, 255);
    uint16_t okc = COLOR565(0, 255, 0);

    for (int i = 0; i < CMD_ANIM_W * CMD_ANIM_H; ++i) {
        s_cmd_anim_buf[i] = bg;
    }

    // Border
    st7789_fill_rect(s_cmd_anim_buf, CMD_ANIM_W, 0, 0, CMD_ANIM_W, 1, fg);
    st7789_fill_rect(s_cmd_anim_buf, CMD_ANIM_W, 0, CMD_ANIM_H - 1, CMD_ANIM_W, 1, fg);
    st7789_fill_rect(s_cmd_anim_buf, CMD_ANIM_W, 0, 0, 1, CMD_ANIM_H, fg);
    st7789_fill_rect(s_cmd_anim_buf, CMD_ANIM_W, CMD_ANIM_W - 1, 0, 1, CMD_ANIM_H, fg);

    // Frames 0-3: moving dots (receiving)
    // Frames >=4: show OK blink (applied)
    if (frame < 4) {
        int dot = (int)(frame % 4);
        int cy = 10;
        int x0 = 10 + dot * 12;
        for (int i = 0; i < 3; ++i) {
            int x = x0 + i * 12;
            st7789_fill_rect(s_cmd_anim_buf, CMD_ANIM_W, x, cy, 6, 6, fg);
        }
    } else {
        bool blink_on = ((frame / 2) % 2) == 0;
        if (blink_on) {
            st7789_draw_text_to_buf(s_cmd_anim_buf, CMD_ANIM_W, CMD_ANIM_H, 18, 6, "OK", 3, okc, bg);
        } else {
            st7789_draw_text_to_buf(s_cmd_anim_buf, CMD_ANIM_W, CMD_ANIM_H, 18, 6, "OK", 3, fg, bg);
        }
    }

    const int x = ST7789_WIDTH - CMD_ANIM_W - 6;
    const int y = 6;
    return st7789_draw_bitmap_sync(x, y, x + CMD_ANIM_W, y + CMD_ANIM_H, s_cmd_anim_buf);
}

esp_err_t st7789_display_clear(uint16_t color)
{
    if (!s_panel) return ESP_ERR_INVALID_STATE;
    const int block_h = 20;
    size_t buf_size = ST7789_WIDTH * block_h * sizeof(uint16_t);
    uint16_t *line_buf = heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (!line_buf) return ESP_ERR_NO_MEM;
    for (int i = 0; i < ST7789_WIDTH * block_h; ++i) line_buf[i] = color;
    for (int y = 0; y < ST7789_HEIGHT; y += block_h) {
        int h = (y + block_h > ST7789_HEIGHT) ? (ST7789_HEIGHT - y) : block_h;
        esp_err_t ret = st7789_draw_bitmap_sync(0, y, ST7789_WIDTH, y + h, line_buf);
        if (ret != ESP_OK) {
            heap_caps_free(line_buf);
            return ret;
        }
    }
    heap_caps_free(line_buf);
    return ESP_OK;
}

esp_err_t st7789_display_show_status(float temp_c, float humi_percent, uint8_t gear, float water_ml,
                                     bool timer_mode, uint32_t timer_remaining_sec,
                                     const char *date_str, const char *time_str,
                                     bool schedule_enabled, bool schedule_should_run,
                                     const char *sched_start_time, const char *sched_end_time)
{
    ESP_RETURN_ON_ERROR(st7789_display_clear(COLOR565(0, 0, 0)), TAG, "clear fail");

    char line[64];
    const int scale = 3;
    const int line_step = 40;
    uint16_t fg = COLOR565(255, 255, 255);
    uint16_t bg = COLOR565(0, 0, 0);

    // Date/time (smaller font)
    const int scale_small = 2;
    if (date_str && date_str[0]) {
        snprintf(line, sizeof(line), "DATE:%s", date_str);
        ESP_RETURN_ON_ERROR(st7789_draw_text(10, 0, line, scale_small, fg, bg), TAG, "draw date fail");
    }
    if (time_str && time_str[0]) {
        snprintf(line, sizeof(line), "TIME:%s", time_str);
        ESP_RETURN_ON_ERROR(st7789_draw_text(10, 16, line, scale_small, fg, bg), TAG, "draw time fail");
    }

    if (schedule_enabled && sched_start_time && sched_start_time[0] && sched_end_time && sched_end_time[0]) {
        snprintf(line, sizeof(line), "ST:%s ED:%s", sched_start_time, sched_end_time);
        ESP_RETURN_ON_ERROR(st7789_draw_text(10, 32, line, scale_small, fg, bg), TAG, "draw sched time fail");
    }

    int y = 56;
    snprintf(line, sizeof(line), "TEMP: %.1fC", temp_c);
    ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, line, scale, fg, bg), TAG, "draw temp fail");
    y += line_step;

    snprintf(line, sizeof(line), "HUMI: %.1f%%", humi_percent);
    ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, line, scale, fg, bg), TAG, "draw humi fail");
    y += line_step;

    snprintf(line, sizeof(line), "GEAR: %u", (unsigned)gear);
    ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, line, scale, fg, bg), TAG, "draw gear fail");
    y += line_step;

    snprintf(line, sizeof(line), "WATER: %.0fML", water_ml);
    ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, line, scale, fg, bg), TAG, "draw water fail");

    y += line_step;
    ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, gear > 0 ? "PWR: ON" : "PWR: OFF", scale, fg, bg), TAG, "draw pwr fail");

    y += line_step;
    if (schedule_enabled) {
        ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, "SCH: EN", scale, fg, bg), TAG, "draw sch fail");
    } else {
        ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, "SCH: DIS", scale, fg, bg), TAG, "draw sch fail");
    }

    y += line_step;
    if (schedule_enabled) {
        if (schedule_should_run) {
            if (timer_mode && timer_remaining_sec > 0) {
                unsigned mins = (unsigned)(timer_remaining_sec / 60);
                unsigned secs = (unsigned)(timer_remaining_sec % 60);
                if (mins > 99) {
                    mins = 99;
                    secs = 59;
                }
                snprintf(line, sizeof(line), "RUN: ON %02u:%02u", mins, secs);
                ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, line, scale, fg, bg), TAG, "draw run fail");
            } else {
                ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, "RUN: ON", scale, fg, bg), TAG, "draw run fail");
            }
        } else {
            ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, "RUN: OFF", scale, fg, bg), TAG, "draw run fail");
        }
    } else {
        ESP_RETURN_ON_ERROR(st7789_draw_text(10, y, "MODE: NORM", scale, fg, bg), TAG, "draw mode fail");
    }

    return ESP_OK;
}

static esp_err_t st7789_draw_segmented_digit(uint8_t digit)
{
    if (!s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    if (digit > 9) {
        return ESP_ERR_INVALID_ARG;
    }

    const int digit_w = 120;
    const int digit_h = 200;
    const int thick = 20;
    const int x0 = (ST7789_WIDTH - digit_w) / 2;
    const int y0 = (ST7789_HEIGHT - digit_h) / 2;

    uint16_t bg = COLOR565(0, 0, 0);
    uint16_t fg = COLOR565(0, 255, 0);

    static const uint8_t seg_map[10] = {
        0b1111110, // 0
        0b0110000, // 1
        0b1101101, // 2
        0b1111001, // 3
        0b0110011, // 4
        0b1011011, // 5
        0b1011111, // 6
        0b1110000, // 7
        0b1111111, // 8
        0b1111011  // 9
    };

    uint16_t *buf = heap_caps_malloc(digit_w * digit_h * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < digit_w * digit_h; ++i) {
        buf[i] = bg;
    }

    bool a = seg_map[digit] & (1 << 6);
    bool b = seg_map[digit] & (1 << 5);
    bool c = seg_map[digit] & (1 << 4);
    bool d = seg_map[digit] & (1 << 3);
    bool e = seg_map[digit] & (1 << 2);
    bool f = seg_map[digit] & (1 << 1);
    bool g = seg_map[digit] & (1 << 0);

    int top = 0;
    int mid = (digit_h - thick) / 2;
    int bottom = digit_h - thick;
    int left = 0;
    int right = digit_w - thick;

    if (a) st7789_fill_rect(buf, digit_w, left + thick, top, digit_w - 2 * thick, thick, fg);
    if (b) st7789_fill_rect(buf, digit_w, right, top + thick, thick, mid - thick, fg);
    if (c) st7789_fill_rect(buf, digit_w, right, mid + thick, thick, mid - thick, fg);
    if (d) st7789_fill_rect(buf, digit_w, left + thick, bottom, digit_w - 2 * thick, thick, fg);
    if (e) st7789_fill_rect(buf, digit_w, left, mid + thick, thick, mid - thick, fg);
    if (f) st7789_fill_rect(buf, digit_w, left, top + thick, thick, mid - thick, fg);
    if (g) st7789_fill_rect(buf, digit_w, left + thick, mid, digit_w - 2 * thick, thick, fg);

    esp_err_t ret = st7789_draw_bitmap_sync(x0, y0, x0 + digit_w, y0 + digit_h, buf);
    heap_caps_free(buf);
    return ret;
}

esp_err_t st7789_display_show_digit(uint8_t digit)
{
    return st7789_draw_segmented_digit(digit);
}

static void st7789_digit_task(void *param)
{
    uint8_t digit = (uint32_t)param & 0xFF;
    for (;;) {
        st7789_draw_segmented_digit(digit);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t st7789_display_start_digit_task(uint8_t digit)
{
    if (!s_panel) {
        return ESP_ERR_INVALID_STATE;
    }
    if (digit > 9) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_digit_task) {
        return ESP_OK;
    }
    BaseType_t ok = xTaskCreate(st7789_digit_task, "st7789_digit", 4096, (void *)(uint32_t)digit, 4, &s_digit_task);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
