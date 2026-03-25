#include <stdio.h>
#include <string.h>

#include "ads131m0x.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

// ── Pin Definitions ──────────────────────────────────────────────────────────
#define PIN_SPI_MOSI  GPIO_NUM_33
#define PIN_SPI_MISO  GPIO_NUM_34
#define PIN_SPI_SCLK  GPIO_NUM_35
#define PIN_CS_ADC    GPIO_NUM_38
#define PIN_ADC_DRDY  GPIO_NUM_37

#define CLOCK_SPEED_HZ 8000000
#define DATA_POINTS    200

static const char* TAG = "main";

static spi_device_handle_t s_spi_dev;
static ADS131M0X s_adc;

// ── ISR -> app_main semaphore ────────────────────────────────────────────────
static SemaphoreHandle_t s_data_sem;

// ── ISR timing accumulators ─────────────────────────────────────────────────
static volatile int64_t s_time_sum  = 0;
static volatile int64_t s_last_time = 0;

// ── HAL Callbacks ────────────────────────────────────────────────────────────

static int halSpiRead(void* const data, const uint8_t length)
{
    spi_transaction_t txn = 
    {
        .length    = length * 8,
        .tx_buffer = NULL,
        .rx_buffer = data,
    };

    esp_err_t ret = spi_device_transmit(s_spi_dev, &txn);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "spi_device_transmit failed: %s", esp_err_to_name(ret));
        return 1;
    }

    return 0;
}

static int halSpiWrite(const void* const data, const uint8_t length)
{
    spi_transaction_t txn = 
    {
        .length    = length * 8,
        .tx_buffer = data,
        .rx_buffer = NULL,
    };

    esp_err_t ret = spi_device_transmit(s_spi_dev, &txn);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "spi_device_transmit failed: %s", esp_err_to_name(ret));
        return 1;
    }

    return 0;
}

static bool halDrdyGet(void)
{
    return gpio_get_level(PIN_ADC_DRDY) == 0;
}

static void halDelayMs(const uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms) + 1);
}

// ── DRDY ISR ─────────────────────────────────────────────────────────────────

static void IRAM_ATTR drdy_isr_handler(void* arg)
{
    int64_t now = esp_timer_get_time();

    if (s_last_time != 0)
        s_time_sum += now - s_last_time;
    s_last_time = now;

    xSemaphoreGiveFromISR(s_data_sem, NULL);
}

// ── SPI Bus Init ─────────────────────────────────────────────────────────────

static void spiInit(void)
{
    const spi_bus_config_t bus_cfg = 
    {
        .mosi_io_num   = PIN_SPI_MOSI,
        .miso_io_num   = PIN_SPI_MISO,
        .sclk_io_num   = PIN_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    const spi_device_interface_config_t dev_cfg = 
    {
        .clock_speed_hz = CLOCK_SPEED_HZ,
        .mode           = 1, // SPI Mode 1: CPOL=0, CPHA=1
        .spics_io_num   = PIN_CS_ADC,
        .queue_size     = 10, // Max no. transactions that can be in the queue at the same time
        .cs_ena_pretrans = 1,
        .cs_ena_posttrans = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi_dev));

    gpio_set_direction(PIN_ADC_DRDY, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_ADC_DRDY, GPIO_PULLUP_ONLY);
}

// ── GPIO Interrupt Setup ─────────────────────────────────────────────────────

static void drdyInterruptInit(void)
{
    gpio_set_intr_type(PIN_ADC_DRDY, GPIO_INTR_NEGEDGE);
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ADC_DRDY, drdy_isr_handler, NULL));
}

// ── Application Entry ────────────────────────────────────────────────────────

void app_main(void)
{
    spiInit();

    ADS131M0XError err;

    const ADS131M0XHAL hal = 
    {
        .spiRead      = halSpiRead,
        .spiWrite     = halSpiWrite,
        .drdyGet      = halDrdyGet,
        .syncResetSet = NULL,
        .delayMs      = halDelayMs,
    };

    /* ── Initialize the driver ──────────────────────────────────────────── */
    err = ads131m0xInit(&s_adc, &hal);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize ADS131M0X: %d", err);
        return;
    }
    ESP_LOGI(TAG, "ADS131M0X initialized and in standby mode");

    /* ── Read chip ID ───────────────────────────────────────────────────── */
    uint16_t chip_id = 0;
    err = ads131m0xReadChipId(&s_adc, &chip_id);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "Failed to read chip ID: %d", err);
        return;
    }
    ESP_LOGI(TAG, "Chip ID: 0x%04X", chip_id);

    /* ── Configure ADC ─────────────────────────────────────────────────── */
    // f_data = f_CLKIN / (OSR * turbo_div) = 8192000 / (4096 * 1) = 2000 Hz
    const ADS131M0XConfig config =
    {
        .power_mode         = ADS131M0X_POWER_HIGH_RES,
        .oversampling_ratio = ADS131M0X_OSR_4096,
        .turbo_mode         = true,
        .word_length        = ADS131M0X_WLENGTH_24_BIT,
        .spi_timeout_enabled = true,
    };
    err = ads131m0xConfigure(&s_adc, &config);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "Failed to configure ADC: %d", err);
        return;
    }
    ESP_LOGI(TAG, "ADC configured: 2 kHz, 24-bit, high-res");

    /* ── Configure CH0 gain ──────────────────────────────────────────── */
    const ADS131M0XChannelConfig ch0_cfg =
    {
        .is_enabled         = true,
        .gain               = ADS131M0X_GAIN_128,
        .mux                = ADS131M0X_MUX_NORMAL,
        .phase_delay_cycles = 0,
        .dc_block_disabled  = false,
        .offset_cal         = 0,
        .gain_cal           = 0x800000,
    };
    err = ads131m0xConfigureChannel(&s_adc, 0, &ch0_cfg);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "Failed to configure CH0: %d", err);
        return;
    }
    ESP_LOGI(TAG, "CH0 configured: gain=128x");

    /* ── Setup DRDY interrupt ─────────────────────────────────────────── */
    s_data_sem = xSemaphoreCreateBinary();
    drdyInterruptInit();
    ESP_LOGI(TAG, "DRDY interrupt installed (GPIO %d, falling edge)", PIN_ADC_DRDY);

    /* ── Data buffer ─────────────────────────────────────────────────────── */
    static int32_t buffer[DATA_POINTS];
    uint32_t sample_index = 0;

    /* ── Reset ISR timing before ADC starts pulsing DRDY ────────────────── */
    s_time_sum  = 0;
    s_last_time = 0;

    /* ── Wake ADC -> starts pulsing DRDY at 2 kHz ───────────────────────── */
    err = ads131m0xWakeup(&s_adc);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "Failed to wake ADC: %d", err);
        return;
    }

    /* ── Acquisition loop ───────────────────────────────────────────────── */
    while (sample_index < DATA_POINTS)
    {
        if (xSemaphoreTake(s_data_sem, portMAX_DELAY) == pdTRUE)
        {
            ADS131M0XData data;
            err = ads131m0xReadData(&s_adc, &data);

            buffer[sample_index] = data.channel_data[0];
            sample_index++;
        }
    }

    ads131m0xStandby(&s_adc);

    xSemaphoreTake(s_data_sem, portMAX_DELAY); // Clear the last semaphore flag

    // Fs = (N-1) / T: N samples yield N-1 intervals measured in the ISR
    double sampling_rate = (double)(DATA_POINTS - 1) / ((double)s_time_sum / 1000000.0);

    ESP_LOGI(TAG, "DATA START");
    for (uint32_t i = 0; i < DATA_POINTS; i++)
    {
        printf("%ld\n", (long)buffer[i]);
    }
    ESP_LOGI(TAG, "DATA END");
    ESP_LOGI(TAG, "Elapsed: %lld us, Sampling rate: %.2f Hz", s_time_sum, sampling_rate);

    while(1)
    {
        if(xSemaphoreTake(s_data_sem, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGE(TAG, "DRDY ISR triggered");
        }
    }

}
