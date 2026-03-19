#include <stdio.h>
#include <string.h>

#include "ads131m0x.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

// ── Pin Definitions ──────────────────────────────────────────────────────────
#define PIN_SPI_MOSI  GPIO_NUM_33
#define PIN_SPI_MISO  GPIO_NUM_34
#define PIN_SPI_SCLK  GPIO_NUM_35
#define PIN_CS_ADC    GPIO_NUM_38
#define PIN_ADC_DRDY  GPIO_NUM_37

#define CLOCK_SPEED_HZ 8000000

static const char* TAG = "main";

static spi_device_handle_t s_spi_dev;
static ADS131M0X s_adc;

// ── HAL Callbacks ────────────────────────────────────────────────────────────

static int halSpiRead(void* const data, const uint8_t length)
{
    /* Allocate DMA-capable buffers */
    uint8_t* tx_buf = (uint8_t*)heap_caps_malloc(length, MALLOC_CAP_DMA);
    uint8_t* rx_buf = (uint8_t*)heap_caps_malloc(length, MALLOC_CAP_DMA);
    if (!tx_buf || !rx_buf) {
        if (tx_buf) heap_caps_free(tx_buf);
        if (rx_buf) heap_caps_free(rx_buf);
        return 1;
    }

    memset(tx_buf, 0, length);
    memset(rx_buf, 0, length);

    spi_transaction_t txn = {
        .length    = length * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };

    esp_err_t ret = spi_device_polling_transmit(s_spi_dev, &txn);

    if (ret == ESP_OK) {
        memcpy(data, rx_buf, length);
        ESP_LOGI("SPI_RX", "Read %d bytes:", length);
        ESP_LOG_BUFFER_HEX("SPI_RX", rx_buf, length);
    }

    heap_caps_free(tx_buf);
    heap_caps_free(rx_buf);

    return (ret == ESP_OK) ? 0 : 1;
}

static int halSpiWrite(const void* const data, const uint8_t length)
{
    /* Allocate DMA-capable buffers */
    uint8_t* tx_buf = (uint8_t*)heap_caps_malloc(length, MALLOC_CAP_DMA);
    uint8_t* rx_buf = (uint8_t*)heap_caps_malloc(length, MALLOC_CAP_DMA);
    if (!tx_buf || !rx_buf) {
        if (tx_buf) heap_caps_free(tx_buf);
        if (rx_buf) heap_caps_free(rx_buf);
        return 1;
    }

    memcpy(tx_buf, data, length);
    memset(rx_buf, 0, length);

    spi_transaction_t txn = {
        .length    = length * 8,
        .tx_buffer = tx_buf,
        .rx_buffer = rx_buf,
    };

    ESP_LOGI("SPI_TX", "Writing %d bytes:", length);
    ESP_LOG_BUFFER_HEX("SPI_TX", tx_buf, length);

    esp_err_t ret = spi_device_polling_transmit(s_spi_dev, &txn);

    heap_caps_free(tx_buf);
    heap_caps_free(rx_buf);

    return (ret == ESP_OK) ? 0 : 1;
}

static bool halDrdyGet(void)
{
    return gpio_get_level(PIN_ADC_DRDY) == 0;
}

static void halDelayMs(const uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms) + 1);
}

// ── SPI Bus Init ─────────────────────────────────────────────────────────────

static void spiInit(void)
{
    const spi_bus_config_t bus_cfg = {
        .mosi_io_num   = PIN_SPI_MOSI,
        .miso_io_num   = PIN_SPI_MISO,
        .sclk_io_num   = PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    const spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = CLOCK_SPEED_HZ,
        .mode           = 1,
        .spics_io_num   = PIN_CS_ADC,
        .queue_size     = 10,
        .cs_ena_pretrans = 1,
        .cs_ena_posttrans = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi_dev));

    gpio_set_direction(PIN_ADC_DRDY, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_ADC_DRDY, GPIO_PULLUP_ONLY);
}

// ── Application Entry ────────────────────────────────────────────────────────

void app_main(void)
{
    spiInit();

    const ADS131M0XHAL hal = {
        .spiRead      = halSpiRead,
        .spiWrite     = halSpiWrite,
        .drdyGet      = halDrdyGet,
        .syncResetSet = NULL,
        .delayMs      = halDelayMs,
    };

    /* ── Initialize the driver ──────────────────────────────────────────── */
    ADS131M0XError err = ads131m0xInit(&s_adc, &hal);
    if (err != ADS131M0X_ERROR_OK) {
        ESP_LOGE(TAG, "ads131m0xInit failed: %s", ads131m0xErrorToString(err));
        return;
    }
    ESP_LOGI(TAG, "ADS131M0X initialized");

    /* ── Read chip ID ───────────────────────────────────────────────────── */
    uint16_t chip_id = 0;
    err = ads131m0xReadChipId(&s_adc, &chip_id);
    if (err != ADS131M0X_ERROR_OK) {
        ESP_LOGE(TAG, "ads131m0xReadChipId failed: %s", ads131m0xErrorToString(err));
        return;
    }

    const uint8_t num_channels = (chip_id & ADS131M0X_ID_CHANCNT_MASK) >> ADS131M0X_ID_CHANCNT_SHIFT;
    const uint8_t revision     = (chip_id & ADS131M0X_ID_REVID_MASK)   >> ADS131M0X_ID_REVID_SHIFT;

    ESP_LOGI(TAG, "Chip ID: 0x%04X", chip_id);
    ESP_LOGI(TAG, "  Channel count : %u", num_channels);
    ESP_LOGI(TAG, "  Revision ID   : 0x%02X", revision);
    ESP_LOGI(TAG, "  Expected      : 0x%04X (Mask: 0x%04X)", ADS131M0X_ID_MODEL_PATTERN, ADS131M0X_ID_MODEL_MASK);
    ESP_LOGI(TAG, "  Match         : %s",
             (chip_id & ADS131M0X_ID_MODEL_MASK) == ADS131M0X_ID_MODEL_PATTERN ? "YES" : "NO");
}
