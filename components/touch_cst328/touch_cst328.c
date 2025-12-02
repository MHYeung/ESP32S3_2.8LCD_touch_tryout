#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "touch_cst328.h"

static const char *TAG = "cst328";

#define CST328_I2C_ADDR_7BIT   0x1A      // 0x34/0x35 8-bit
#define CST328_BASE_REG        0xD000    // first finger data

static i2c_port_t s_i2c_port;
static gpio_num_t s_rst_gpio = -1;
static gpio_num_t s_irq_gpio = -1;

esp_err_t cst328_init(i2c_port_t port,
                      gpio_num_t sda,
                      gpio_num_t scl,
                      gpio_num_t rst,
                      gpio_num_t irq,
                      uint32_t i2c_clk_hz)
{
    s_i2c_port = port;
    s_rst_gpio = rst;
    s_irq_gpio = irq;

    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = i2c_clk_hz,
    };
    ESP_ERROR_CHECK(i2c_param_config(port, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(port, cfg.mode, 0, 0, 0));

    // Reset pin (active low)
    if (rst >= 0) {
        gpio_config_t io = {
            .pin_bit_mask = 1ULL << rst,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_ERROR_CHECK(gpio_config(&io));
        gpio_set_level(rst, 0);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(rst, 1);
    }

    // IRQ as input
    if (irq >= 0) {
        gpio_config_t io_irq = {
            .pin_bit_mask = 1ULL << irq,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_irq));
    }

    // Give chip time to boot (datasheet ~200ms) :contentReference[oaicite:3]{index=3}
    vTaskDelay(pdMS_TO_TICKS(220));

    ESP_LOGI(TAG, "CST328 initialized");
    return ESP_OK;
}

static esp_err_t cst328_read_regs(uint16_t reg16, uint8_t *buf, size_t len)
{
    uint8_t reg[2] = { (uint8_t)(reg16 >> 8), (uint8_t)(reg16 & 0xFF) };
    return i2c_master_write_read_device(s_i2c_port, CST328_I2C_ADDR_7BIT,
                                        reg, sizeof(reg),
                                        buf, len,
                                        pdMS_TO_TICKS(20));
}

esp_err_t cst328_read_point(cst328_point_t *out_pt)
{
    if (!out_pt) return ESP_ERR_INVALID_ARG;

    uint8_t buf[7] = {0};
    esp_err_t err = cst328_read_regs(CST328_BASE_REG, buf, sizeof(buf));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C read failed: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t id_status = buf[0];
    uint8_t xh        = buf[1];
    uint8_t yh        = buf[2];
    uint8_t xy_low    = buf[3];
    uint8_t pressure  = buf[4];
    uint8_t flags     = buf[5];
    uint8_t fixed     = buf[6];

    (void)flags;
    (void)fixed;

    bool pressed = ((id_status & 0x0F) == 0x06); // "pressed" status as per datasheet :contentReference[oaicite:4]{index=4}

    uint16_t x = ((uint16_t)xh << 4) | (xy_low >> 4);
    uint16_t y = ((uint16_t)yh << 4) | (xy_low & 0x0F);

    out_pt->x = x;
    out_pt->y = y;
    out_pt->pressure = pressure;
    out_pt->pressed = pressed;

    return ESP_OK;
}
