#include "lcd_char2x8.h"

#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define LCD_I2C_ADDR_0 0x3C
#define LCD_I2C_ADDR_1 0x3E
#define LCD_I2C_ADDR_2 0x27
#define LCD_I2C_ADDR_3 0x3F

static const char *TAG = "lcd_char2x8";
static i2c_master_bus_handle_t s_lcd_bus = NULL;
static i2c_master_dev_handle_t s_lcd_handle = NULL;

static esp_err_t lcd_write_cmd(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};
    return i2c_master_transmit(s_lcd_handle, data, sizeof(data), -1);
}

static esp_err_t lcd_write_data(const char *text, size_t len)
{
    if (len == 0) {
        return ESP_OK;
    }

    uint8_t buffer[17];
    buffer[0] = 0x40;
    memcpy(&buffer[1], text, len);
    return i2c_master_transmit(s_lcd_handle, buffer, len + 1, -1);
}

static bool lcd_try_init(uint8_t addr)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &s_lcd_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(ret));
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ,
    };

    ret = i2c_master_bus_add_device(s_lcd_bus, &dev_cfg, &s_lcd_handle);
    if (ret != ESP_OK) {
        i2c_del_master_bus(s_lcd_bus);
        s_lcd_bus = NULL;
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    if (lcd_write_cmd(0x38) != ESP_OK ||
        lcd_write_cmd(0x39) != ESP_OK ||
        lcd_write_cmd(0x14) != ESP_OK ||
        lcd_write_cmd(0x70) != ESP_OK ||
        lcd_write_cmd(0x56) != ESP_OK ||
        lcd_write_cmd(0x6C) != ESP_OK ||
        lcd_write_cmd(0x38) != ESP_OK ||
        lcd_write_cmd(0x0C) != ESP_OK ||
        lcd_write_cmd(0x01) != ESP_OK) {
        i2c_master_bus_rm_device(s_lcd_handle);
        i2c_del_master_bus(s_lcd_bus);
        s_lcd_handle = NULL;
        s_lcd_bus = NULL;
        return false;
    }

    return true;
}

void lcd_char2x8_init(void)
{
    if (s_lcd_handle != NULL) {
        return;
    }

    static const uint8_t addrs[] = {LCD_I2C_ADDR_0, LCD_I2C_ADDR_1, LCD_I2C_ADDR_2, LCD_I2C_ADDR_3};
    for (size_t i = 0; i < sizeof(addrs) / sizeof(addrs[0]); ++i) {
        if (lcd_try_init(addrs[i])) {
            ESP_LOGI(TAG, "LCD initialized at 0x%02X", addrs[i]);
            return;
        }
    }

    ESP_LOGE(TAG, "LCD initialization failed");
}

void lcd_char2x8_write(int line, const char *text)
{
    if (s_lcd_handle == NULL || text == NULL) {
        return;
    }

    if (line == 0) {
        char line_buf[9];
        memset(line_buf, ' ', sizeof(line_buf) - 1);
        line_buf[8] = '\0';

        size_t len = strlen(text);
        if (len > 8) {
            len = 8;
        }
        memcpy(line_buf, text, len);

        lcd_write_cmd(0x80);
        lcd_write_data(line_buf, 8);
    } else if (line == 1) {
        char line_buf[9];
        memset(line_buf, ' ', sizeof(line_buf) - 1);
        line_buf[8] = '\0';

        size_t len = strlen(text);
        if (len > 8) {
            len = 8;
        }
        memcpy(line_buf, text, len);

        lcd_write_cmd(0xC0);
        lcd_write_data(line_buf, 8);
    } else if (line == -1) {
        char line1_buf[9];
        char line2_buf[9];
        memset(line1_buf, ' ', sizeof(line1_buf) - 1);
        memset(line2_buf, ' ', sizeof(line2_buf) - 1);
        line1_buf[8] = '\0';
        line2_buf[8] = '\0';

        size_t total_len = strlen(text);
        if (total_len > 16) {
            total_len = 16;
        }
        if (total_len > 8) {
            memcpy(line1_buf, text, 8);
            memcpy(line2_buf, text + 8, total_len - 8);
        } else {
            memcpy(line1_buf, text, total_len);
        }

        lcd_write_cmd(0x80);
        lcd_write_data(line1_buf, 8);
        lcd_write_cmd(0xC0);
        lcd_write_data(line2_buf, 8);
    }
}
