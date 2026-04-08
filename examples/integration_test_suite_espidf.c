#include <stdio.h>
#include <string.h>
#include <math.h>

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

// ── Test Constants ───────────────────────────────────────────────────────────
#define TEST_NOISE_THRESHOLD_UV   50
#define TEST_SIGNAL_EXPECTED_UV   160000   // (2/15) * 1.2V
#define TEST_SIGNAL_TOLERANCE_PCT 5
#define TEST_NUM_SAMPLES          32
#define TEST_SETTLE_SAMPLES       4
#define TEST_DRDY_TIMEOUT_MS      500

// ── Sweep Constants ──────────────────────────────────────────────────────────
#define SWEEP_NUM_SAMPLES         200

static const char* TAG = "main";

static spi_device_handle_t s_spi_dev;
static ADS131M0X s_adc;

// ── ISR -> app_main semaphore ────────────────────────────────────────────────
static SemaphoreHandle_t s_data_sem;

// ── Test Runner Macro ────────────────────────────────────────────────────────
#define RUN_TEST(fn, dev, pass, fail) do {  \
    ESP_LOGI(TAG, "── %s ──", #fn);        \
    s_spi_log_enabled = true;               \
    bool r = fn(dev);                       \
    s_spi_log_enabled = false;              \
    ESP_LOGI(TAG, "[%s] %s", r ? "PASS" : "FAIL", #fn); \
    r ? (pass)++ : (fail)++;                \
} while (0)

// ── SPI Frame Logging ────────────────────────────────────────────────────────
static volatile bool s_spi_log_enabled = false;

static void logHex(const char* dir, const void* data, uint8_t length)
{
    if (!s_spi_log_enabled) return;
    const uint8_t* p = (const uint8_t*)data;
    char hex[3 * 24 + 1]; // max 24 bytes per frame (8 words × 3 bytes)
    int pos = 0;
    for (uint8_t i = 0; i < length && pos < (int)sizeof(hex) - 3; i++)
        pos += sprintf(&hex[pos], "%02X ", p[i]);
    ESP_LOGI("SPI", "%s [%2d]: %s", dir, length, hex);
}

// ── HAL Callbacks ────────────────────────────────────────────────────────────

static int halSpiRead(void* const data, const uint8_t length)
{
    spi_transaction_t txn =
    {
        .length    = length * 8,
        .tx_buffer = NULL,
        .rx_buffer = data,
    };

    esp_err_t ret = spi_device_polling_transmit(s_spi_dev, &txn);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI read failed: %s", esp_err_to_name(ret));
        return 1;
    }

    logHex("RX", data, length);
    return 0;
}

static int halSpiWrite(const void* const data, const uint8_t length)
{
    logHex("TX", data, length);

    spi_transaction_t txn =
    {
        .length    = length * 8,
        .tx_buffer = data,
        .rx_buffer = NULL,
    };

    esp_err_t ret = spi_device_polling_transmit(s_spi_dev, &txn);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "SPI write failed: %s", esp_err_to_name(ret));
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
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(s_data_sem, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_DISABLED));

    const spi_device_interface_config_t dev_cfg =
    {
        .clock_speed_hz  = CLOCK_SPEED_HZ,
        .mode            = 1, // SPI Mode 1: CPOL=0, CPHA=1
        .spics_io_num    = PIN_CS_ADC,
        .queue_size      = 1,
        .cs_ena_pretrans = 1,
        .cs_ena_posttrans = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi_dev));
}

// ── GPIO Interrupt Setup ─────────────────────────────────────────────────────

static void drdyInterruptInit(void)
{
    const gpio_config_t drdy_cfg =
    {
        .pin_bit_mask = 1ULL << PIN_ADC_DRDY,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&drdy_cfg));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_ADC_DRDY, drdy_isr_handler, NULL));
}

// ── Helpers ──────────────────────────────────────────────────────────────────

static void flushSemaphore(void)
{
    while (xSemaphoreTake(s_data_sem, 0) == pdTRUE) {}
}

static bool acquireSamples(int32_t* buf, uint32_t count, uint32_t discard)
{
    ads131m0xWakeup(&s_adc);
    s_spi_log_enabled = false;
    ESP_ERROR_CHECK(spi_device_acquire_bus(s_spi_dev, portMAX_DELAY));

    for (uint32_t i = 0; i < discard; i++)
    {
        if (xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(TEST_DRDY_TIMEOUT_MS)) != pdTRUE)
        {
            spi_device_release_bus(s_spi_dev);
            ads131m0xStandby(&s_adc);
            return false;
        }
        ADS131M0XData data;
        ads131m0xReadData(&s_adc, &data);
    }

    for (uint32_t i = 0; i < count; i++)
    {
        if (xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(TEST_DRDY_TIMEOUT_MS)) != pdTRUE)
        {
            spi_device_release_bus(s_spi_dev);
            ads131m0xStandby(&s_adc);
            return false;
        }
        ADS131M0XData data;
        if (ads131m0xReadData(&s_adc, &data) != ADS131M0X_ERROR_OK)
        {
            spi_device_release_bus(s_spi_dev);
            ads131m0xStandby(&s_adc);
            return false;
        }
        buf[i] = data.channel_data[0];
    }

    spi_device_release_bus(s_spi_dev);
    ads131m0xStandby(&s_adc);
    return true;
}

static bool acquireSamplesAllChannels(int32_t buf[][ADS131M0X_CHANNEL_COUNT], uint32_t count, uint32_t discard)
{
    ads131m0xWakeup(&s_adc);
    s_spi_log_enabled = false;
    ESP_ERROR_CHECK(spi_device_acquire_bus(s_spi_dev, portMAX_DELAY));

    for (uint32_t i = 0; i < discard; i++)
    {
        if (xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(TEST_DRDY_TIMEOUT_MS)) != pdTRUE)
        {
            spi_device_release_bus(s_spi_dev);
            ads131m0xStandby(&s_adc);
            return false;
        }
        ADS131M0XData data;
        ads131m0xReadData(&s_adc, &data);
    }

    for (uint32_t i = 0; i < count; i++)
    {
        if (xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(TEST_DRDY_TIMEOUT_MS)) != pdTRUE)
        {
            spi_device_release_bus(s_spi_dev);
            ads131m0xStandby(&s_adc);
            return false;
        }
        ADS131M0XData data;
        if (ads131m0xReadData(&s_adc, &data) != ADS131M0X_ERROR_OK)
        {
            spi_device_release_bus(s_spi_dev);
            ads131m0xStandby(&s_adc);
            return false;
        }
        for (uint8_t ch = 0; ch < ADS131M0X_CHANNEL_COUNT; ch++)
            buf[i][ch] = data.channel_data[ch];
    }

    spi_device_release_bus(s_spi_dev);
    ads131m0xStandby(&s_adc);
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// GROUP G — Error Handling (stateless tests first)
// ═════════════════════════════════════════════════════════════════════════════

static bool test_error_null_param(ADS131M0X* dev)
{
    (void)dev;
    uint32_t ok_count = 0;
    uint32_t total = 0;

    uint16_t dummy16 = 0;
    uint16_t dummy16_arr[4] = {0};
    ADS131M0XData dummy_data = {0};
    ADS131M0XConfig dummy_cfg = {0};
    ADS131M0XChannelConfig dummy_ch_cfg = {0};
    ADS131M0XHAL dummy_hal = {0};

    // Init
    total++; if (ads131m0xInit(NULL, &dummy_hal) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;
    total++; if (ads131m0xInit(dev, NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // Deinit
    total++; if (ads131m0xDeinit(NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // Reset
    total++; if (ads131m0xReset(NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // Standby
    total++; if (ads131m0xStandby(NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // Wakeup
    total++; if (ads131m0xWakeup(NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // Lock
    total++; if (ads131m0xLock(NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // Unlock
    total++; if (ads131m0xUnlock(NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // Configure
    total++; if (ads131m0xConfigure(NULL, &dummy_cfg) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;
    total++; if (ads131m0xConfigure(dev, NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // ConfigureChannel
    total++; if (ads131m0xConfigureChannel(NULL, 0, &dummy_ch_cfg) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;
    total++; if (ads131m0xConfigureChannel(dev, 0, NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // ReadChipId
    total++; if (ads131m0xReadChipId(NULL, &dummy16) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;
    total++; if (ads131m0xReadChipId(dev, NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // ReadData
    total++; if (ads131m0xReadData(NULL, &dummy_data) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;
    total++; if (ads131m0xReadData(dev, NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // ReadRegister
    total++; if (ads131m0xReadRegister(NULL, 0, &dummy16) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;
    total++; if (ads131m0xReadRegister(dev, 0, NULL) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // ReadRegisters
    total++; if (ads131m0xReadRegisters(NULL, 0, dummy16_arr, 1) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;
    total++; if (ads131m0xReadRegisters(dev, 0, NULL, 1) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // WriteRegister
    total++; if (ads131m0xWriteRegister(NULL, 0, 0) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    // WriteRegisters
    total++; if (ads131m0xWriteRegisters(NULL, 0, dummy16_arr, 1) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;
    total++; if (ads131m0xWriteRegisters(dev, 0, NULL, 1) == ADS131M0X_ERROR_NULL_PARAM) ok_count++;

    ESP_LOGI(TAG, "  null_param: %lu/%lu passed", (unsigned long)ok_count, (unsigned long)total);
    return ok_count == total;
}

static bool test_error_not_initialized(ADS131M0X* dev)
{
    (void)dev;
    ADS131M0X uninit_dev;
    memset(&uninit_dev, 0, sizeof(uninit_dev));

    uint32_t ok_count = 0;
    uint32_t total = 0;

    uint16_t dummy16 = 0;
    ADS131M0XData dummy_data = {0};
    ADS131M0XConfig dummy_cfg = {0};
    ADS131M0XChannelConfig dummy_ch_cfg = {0};

    total++; if (ads131m0xReadChipId(&uninit_dev, &dummy16) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;
    total++; if (ads131m0xConfigure(&uninit_dev, &dummy_cfg) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;
    total++; if (ads131m0xConfigureChannel(&uninit_dev, 0, &dummy_ch_cfg) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;
    total++; if (ads131m0xReadData(&uninit_dev, &dummy_data) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;
    total++; if (ads131m0xStandby(&uninit_dev) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;
    total++; if (ads131m0xWakeup(&uninit_dev) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;
    total++; if (ads131m0xLock(&uninit_dev) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;
    total++; if (ads131m0xUnlock(&uninit_dev) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;
    total++; if (ads131m0xReadRegister(&uninit_dev, 0, &dummy16) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;
    total++; if (ads131m0xWriteRegister(&uninit_dev, 0, 0) == ADS131M0X_ERROR_NOT_INITIALIZED) ok_count++;

    ESP_LOGI(TAG, "  not_initialized: %lu/%lu passed", (unsigned long)ok_count, (unsigned long)total);
    return ok_count == total;
}

static bool test_error_invalid_channel(ADS131M0X* dev)
{
    ADS131M0XChannelConfig cfg = { .is_enabled = true, .gain = ADS131M0X_GAIN_1 };

    bool ok = true;
    if (ads131m0xConfigureChannel(dev, 4, &cfg) != ADS131M0X_ERROR_INVALID_CHANNEL) ok = false;
    if (ads131m0xConfigureChannel(dev, 255, &cfg) != ADS131M0X_ERROR_INVALID_CHANNEL) ok = false;

    return ok;
}

// ═════════════════════════════════════════════════════════════════════════════
// GROUP A — Identity & Init
// ═════════════════════════════════════════════════════════════════════════════

static bool test_chip_id_readback(ADS131M0X* dev)
{
    uint16_t id = 0;
    ADS131M0XError err = ads131m0xReadChipId(dev, &id);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  ReadChipId failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    bool ok = (id & 0xFF00) == 0x2400;
    ESP_LOGI(TAG, "  Chip ID: 0x%04X (expected 0x24xx)", id);
    return ok;
}

static bool test_reset_default_registers(ADS131M0X* dev)
{
    ADS131M0XError err = ads131m0xReset(dev);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Reset failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    struct { uint8_t addr; uint16_t expected; const char* name; } checks[] =
    {
        { ADS131M0X_MODE_ADDRESS,         ADS131M0X_MODE_DEFAULT,         "MODE"     },
        { ADS131M0X_CLOCK_ADDRESS,        ADS131M0X_CLOCK_DEFAULT,        "CLOCK"    },
        { ADS131M0X_GAIN1_ADDRESS,        ADS131M0X_GAIN1_DEFAULT,        "GAIN1"    },
        { ADS131M0X_CFG_ADDRESS,          ADS131M0X_CFG_DEFAULT,          "CFG"      },
        { ADS131M0X_THRSHLD_MSB_ADDRESS,  ADS131M0X_THRSHLD_MSB_DEFAULT,  "THRSH_M"  },
        { ADS131M0X_THRSHLD_LSB_ADDRESS,  ADS131M0X_THRSHLD_LSB_DEFAULT,  "THRSH_L"  },
        { ADS131M0X_CH0_CFG_ADDRESS,      ADS131M0X_CHn_CFG_DEFAULT,      "CH0_CFG"  },
        { ADS131M0X_CH0_GCAL_MSB_ADDRESS, ADS131M0X_CHn_GCAL_MSB_DEFAULT, "CH0_GCAL" },
        { ADS131M0X_CH1_CFG_ADDRESS,      ADS131M0X_CHn_CFG_DEFAULT,      "CH1_CFG"  },
        { ADS131M0X_CH2_CFG_ADDRESS,      ADS131M0X_CHn_CFG_DEFAULT,      "CH2_CFG"  },
        { ADS131M0X_CH3_CFG_ADDRESS,      ADS131M0X_CHn_CFG_DEFAULT,      "CH3_CFG"  },
    };

    bool all_ok = true;
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++)
    {
        uint16_t val = 0;
        err = ads131m0xReadRegister(dev, checks[i].addr, &val);
        if (err != ADS131M0X_ERROR_OK)
        {
            ESP_LOGE(TAG, "  Read %s failed: %s", checks[i].name, ads131m0xErrorToString(err));
            all_ok = false;
            continue;
        }
        if (val != checks[i].expected)
        {
            ESP_LOGE(TAG, "  %s: got 0x%04X, expected 0x%04X", checks[i].name, val, checks[i].expected);
            all_ok = false;
        }
    }

    return all_ok;
}

// ═════════════════════════════════════════════════════════════════════════════
// GROUP B — Global Config Readback
// ═════════════════════════════════════════════════════════════════════════════

static bool test_global_config_readback(ADS131M0X* dev)
{
    const ADS131M0XConfig config =
    {
        .power_mode            = ADS131M0X_POWER_LOW,
        .oversampling_ratio    = ADS131M0X_OSR_512,
        .is_turbo_mode_enabled = true,
        .word_length           = ADS131M0X_WLENGTH_24_BIT,
        .is_spi_timeout_enabled = false,

        .data_ready =
        {
            .selection      = ADS131M0X_DRDY_SEL_ALL_OR,
            .is_hiz_enabled = true,
            .format         = ADS131M0X_DRDY_FMT_PULSE,
        },

        .global_chop =
        {
            .is_enabled = true,
            .delay      = ADS131M0X_GC_DLY_16,
        },

        .dc_block = ADS131M0X_DCBLOCK_1_256,

        .current_detect =
        {
            .is_enabled              = true,
            .is_all_channels_enabled = true,
            .num                     = ADS131M0X_CD_NUM_4,
            .len                     = ADS131M0X_CD_LEN_512,
            .threshold               = 0x123456,
        },
    };

    ADS131M0XError err = ads131m0xConfigure(dev, &config);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Configure failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    struct { uint8_t addr; uint16_t expected; const char* name; } checks[] =
    {
        { ADS131M0X_MODE_ADDRESS,        0x0107, "MODE"      },
        { ADS131M0X_CLOCK_ADDRESS,        0x0F29, "CLOCK"     },
        { ADS131M0X_CFG_ADDRESS,          0x07A5, "CFG"       },  // GC_DLY=3(16)<<9 | GC_EN=1<<8 | CD_ALLCH=0 | CD_NUM=2(4)<<4 | CD_LEN=2(512)<<1 | CD_EN=1
        { ADS131M0X_THRSHLD_MSB_ADDRESS,  0x1234, "THRSH_MSB" },
        { ADS131M0X_THRSHLD_LSB_ADDRESS,  0x5607, "THRSH_LSB" },
    };

    bool all_ok = true;
    for (size_t i = 0; i < sizeof(checks) / sizeof(checks[0]); i++)
    {
        uint16_t val = 0;
        err = ads131m0xReadRegister(dev, checks[i].addr, &val);
        if (err != ADS131M0X_ERROR_OK)
        {
            ESP_LOGE(TAG, "  Read %s failed: %s", checks[i].name, ads131m0xErrorToString(err));
            all_ok = false;
            continue;
        }
        if (val != checks[i].expected)
        {
            ESP_LOGE(TAG, "  %s: got 0x%04X, expected 0x%04X", checks[i].name, val, checks[i].expected);
            all_ok = false;
        }
    }

    // Reset to restore defaults
    ads131m0xReset(dev);
    return all_ok;
}

// ═════════════════════════════════════════════════════════════════════════════
// GROUP C — Per-Channel Config Readback
// ═════════════════════════════════════════════════════════════════════════════

static bool test_per_channel_config_readback(ADS131M0X* dev)
{
    /* CH0: gain=4x, mux=shorted, phase=5, ocal=0x0A0B0C, gcal=0x810000 */
    const ADS131M0XChannelConfig ch0 =
    {
        .is_enabled = true, .gain = ADS131M0X_GAIN_4, .mux = ADS131M0X_MUX_SHORTED,
        .phase_delay_cycles = 5, .is_dc_block_disabled = true,
        .offset_cal = 0x0A0B0C, .gain_cal = 0x810000,
    };

    /* CH1: gain=8x, mux=+DC, phase=10, ocal=0x112233, gcal=0x820000 */
    const ADS131M0XChannelConfig ch1 =
    {
        .is_enabled = true, .gain = ADS131M0X_GAIN_8, .mux = ADS131M0X_MUX_POSITIVE_DCM,
        .phase_delay_cycles = 10, .is_dc_block_disabled = false,
        .offset_cal = 0x112233, .gain_cal = 0x820000,
    };

    /* CH2: gain=16x, mux=-DC, phase=-3, ocal=0xFEDCBA, gcal=0x900000 */
    const ADS131M0XChannelConfig ch2 =
    {
        .is_enabled = true, .gain = ADS131M0X_GAIN_16, .mux = ADS131M0X_MUX_NEGATIVE_DCM,
        .phase_delay_cycles = -3, .is_dc_block_disabled = true,
        .offset_cal = (int32_t)0xFEDCBA, .gain_cal = 0x900000,
    };

    /* CH3: gain=32x, mux=normal, phase=0, ocal=0, gcal=0x800000 */
    const ADS131M0XChannelConfig ch3 =
    {
        .is_enabled = true, .gain = ADS131M0X_GAIN_32, .mux = ADS131M0X_MUX_NORMAL,
        .phase_delay_cycles = 0, .is_dc_block_disabled = false,
        .offset_cal = 0, .gain_cal = 0x800000,
    };

    ADS131M0XError err;
    err = ads131m0xConfigureChannel(dev, 0, &ch0); if (err != ADS131M0X_ERROR_OK) { ESP_LOGE(TAG, "  CH0 cfg failed"); return false; }
    err = ads131m0xConfigureChannel(dev, 1, &ch1); if (err != ADS131M0X_ERROR_OK) { ESP_LOGE(TAG, "  CH1 cfg failed"); return false; }
    err = ads131m0xConfigureChannel(dev, 2, &ch2); if (err != ADS131M0X_ERROR_OK) { ESP_LOGE(TAG, "  CH2 cfg failed"); return false; }
    err = ads131m0xConfigureChannel(dev, 3, &ch3); if (err != ADS131M0X_ERROR_OK) { ESP_LOGE(TAG, "  CH3 cfg failed"); return false; }

    bool all_ok = true;

    // Check GAIN1 register
    uint16_t gain1 = 0;
    ads131m0xReadRegister(dev, ADS131M0X_GAIN1_ADDRESS, &gain1);
    if (gain1 != 0x5432)
    {
        ESP_LOGE(TAG, "  GAIN1: got 0x%04X, expected 0x5432", gain1);
        all_ok = false;
    }

    // CH0 registers (base=0x09)
    struct { uint8_t addr; uint16_t expected; const char* name; } ch0_checks[] =
    {
        { ADS131M0X_CH0_CFG_ADDRESS,      0x0145, "CH0_CFG"      },
        { ADS131M0X_CH0_OCAL_MSB_ADDRESS, 0x0A0B, "CH0_OCAL_MSB" },
        { ADS131M0X_CH0_OCAL_LSB_ADDRESS, 0x0C00, "CH0_OCAL_LSB" },
        { ADS131M0X_CH0_GCAL_MSB_ADDRESS, 0x8100, "CH0_GCAL_MSB" },
        { ADS131M0X_CH0_GCAL_LSB_ADDRESS, 0x0000, "CH0_GCAL_LSB" },
    };
    for (size_t i = 0; i < sizeof(ch0_checks) / sizeof(ch0_checks[0]); i++)
    {
        uint16_t val = 0;
        ads131m0xReadRegister(dev, ch0_checks[i].addr, &val);
        if (val != ch0_checks[i].expected)
        {
            ESP_LOGE(TAG, "  %s: got 0x%04X, expected 0x%04X", ch0_checks[i].name, val, ch0_checks[i].expected);
            all_ok = false;
        }
    }

    // CH1 registers (base=0x0E)
    struct { uint8_t addr; uint16_t expected; const char* name; } ch1_checks[] =
    {
        { ADS131M0X_CH1_CFG_ADDRESS,      0x0282, "CH1_CFG"      },
        { ADS131M0X_CH1_OCAL_MSB_ADDRESS, 0x1122, "CH1_OCAL_MSB" },
        { ADS131M0X_CH1_OCAL_LSB_ADDRESS, 0x3300, "CH1_OCAL_LSB" },
        { ADS131M0X_CH1_GCAL_MSB_ADDRESS, 0x8200, "CH1_GCAL_MSB" },
        { ADS131M0X_CH1_GCAL_LSB_ADDRESS, 0x0000, "CH1_GCAL_LSB" },
    };
    for (size_t i = 0; i < sizeof(ch1_checks) / sizeof(ch1_checks[0]); i++)
    {
        uint16_t val = 0;
        ads131m0xReadRegister(dev, ch1_checks[i].addr, &val);
        if (val != ch1_checks[i].expected)
        {
            ESP_LOGE(TAG, "  %s: got 0x%04X, expected 0x%04X", ch1_checks[i].name, val, ch1_checks[i].expected);
            all_ok = false;
        }
    }

    // CH2 registers (base=0x13)
    struct { uint8_t addr; uint16_t expected; const char* name; } ch2_checks[] =
    {
        { ADS131M0X_CH2_CFG_ADDRESS,      0xFF47, "CH2_CFG"      },
        { ADS131M0X_CH2_OCAL_MSB_ADDRESS, 0xFEDC, "CH2_OCAL_MSB" },
        { ADS131M0X_CH2_OCAL_LSB_ADDRESS, 0xBA00, "CH2_OCAL_LSB" },
        { ADS131M0X_CH2_GCAL_MSB_ADDRESS, 0x9000, "CH2_GCAL_MSB" },
        { ADS131M0X_CH2_GCAL_LSB_ADDRESS, 0x0000, "CH2_GCAL_LSB" },
    };
    for (size_t i = 0; i < sizeof(ch2_checks) / sizeof(ch2_checks[0]); i++)
    {
        uint16_t val = 0;
        ads131m0xReadRegister(dev, ch2_checks[i].addr, &val);
        if (val != ch2_checks[i].expected)
        {
            ESP_LOGE(TAG, "  %s: got 0x%04X, expected 0x%04X", ch2_checks[i].name, val, ch2_checks[i].expected);
            all_ok = false;
        }
    }

    // CH3 registers (base=0x18)
    struct { uint8_t addr; uint16_t expected; const char* name; } ch3_checks[] =
    {
        { ADS131M0X_CH3_CFG_ADDRESS,      0x0000, "CH3_CFG"      },
        { ADS131M0X_CH3_OCAL_MSB_ADDRESS, 0x0000, "CH3_OCAL_MSB" },
        { ADS131M0X_CH3_OCAL_LSB_ADDRESS, 0x0000, "CH3_OCAL_LSB" },
        { ADS131M0X_CH3_GCAL_MSB_ADDRESS, 0x8000, "CH3_GCAL_MSB" },
        { ADS131M0X_CH3_GCAL_LSB_ADDRESS, 0x0000, "CH3_GCAL_LSB" },
    };
    for (size_t i = 0; i < sizeof(ch3_checks) / sizeof(ch3_checks[0]); i++)
    {
        uint16_t val = 0;
        ads131m0xReadRegister(dev, ch3_checks[i].addr, &val);
        if (val != ch3_checks[i].expected)
        {
            ESP_LOGE(TAG, "  %s: got 0x%04X, expected 0x%04X", ch3_checks[i].name, val, ch3_checks[i].expected);
            all_ok = false;
        }
    }

    // Reset to restore defaults
    ads131m0xReset(dev);
    return all_ok;
}

// ═════════════════════════════════════════════════════════════════════════════
// GROUP D — Device Control
// ═════════════════════════════════════════════════════════════════════════════

static bool test_standby_command(ADS131M0X* dev)
{
    ADS131M0XError err;

    err = ads131m0xWakeup(dev);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Wakeup failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    err = ads131m0xStandby(dev);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Standby failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    return true;
}

static bool test_wakeup_command(ADS131M0X* dev)
{
    // Device should be in standby from previous test (or from init)
    ADS131M0XError err = ads131m0xWakeup(dev);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Wakeup failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    // Put back to standby for subsequent tests
    ads131m0xStandby(dev);
    return true;
}

static bool test_lock_rejects_writes(ADS131M0X* dev)
{
    ADS131M0XError err;

    err = ads131m0xLock(dev);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Lock failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    // Verify STATUS.LOCK bit
    uint16_t status = 0;
    err = ads131m0xReadRegister(dev, ADS131M0X_STATUS_ADDRESS, &status);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Read STATUS failed: %s", ads131m0xErrorToString(err));
        ads131m0xUnlock(dev);
        return false;
    }
    if (!(status & ADS131M0X_STATUS_LOCK_MASK))
    {
        ESP_LOGE(TAG, "  STATUS.LOCK not set: 0x%04X", status);
        ads131m0xUnlock(dev);
        return false;
    }

    bool all_locked = true;

    // WriteRegister should fail
    if (ads131m0xWriteRegister(dev, ADS131M0X_CFG_ADDRESS, 0x0000) != ADS131M0X_ERROR_LOCKED) all_locked = false;

    // Configure should fail
    ADS131M0XConfig cfg = {0};
    if (ads131m0xConfigure(dev, &cfg) != ADS131M0X_ERROR_LOCKED) all_locked = false;

    // ConfigureChannel should fail
    ADS131M0XChannelConfig ch_cfg = { .is_enabled = true };
    if (ads131m0xConfigureChannel(dev, 0, &ch_cfg) != ADS131M0X_ERROR_LOCKED) all_locked = false;

    // Reset should fail
    if (ads131m0xReset(dev) != ADS131M0X_ERROR_LOCKED) all_locked = false;

    // Leave locked for next test
    return all_locked;
}

static bool test_unlock_restores_writes(ADS131M0X* dev)
{
    // Device should be locked from previous test
    ADS131M0XError err = ads131m0xUnlock(dev);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Unlock failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    // Verify STATUS.LOCK cleared
    uint16_t status = 0;
    err = ads131m0xReadRegister(dev, ADS131M0X_STATUS_ADDRESS, &status);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Read STATUS failed: %s", ads131m0xErrorToString(err));
        return false;
    }
    if (status & ADS131M0X_STATUS_LOCK_MASK)
    {
        ESP_LOGE(TAG, "  STATUS.LOCK still set: 0x%04X", status);
        return false;
    }

    // WriteRegister should succeed (write default CFG value back)
    err = ads131m0xWriteRegister(dev, ADS131M0X_CFG_ADDRESS, ADS131M0X_CFG_DEFAULT);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  WriteRegister failed after unlock: %s", ads131m0xErrorToString(err));
        return false;
    }

    // Reset to restore defaults
    ads131m0xReset(dev);
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// GROUP G (continued) — Error Handling: test_error_locked_write
// ═════════════════════════════════════════════════════════════════════════════

static bool test_error_locked_write(ADS131M0X* dev)
{
    ADS131M0XError err;

    err = ads131m0xLock(dev);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Lock failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    bool all_ok = true;

    // 5 write operations should return ERROR_LOCKED
    if (ads131m0xWriteRegister(dev, ADS131M0X_CFG_ADDRESS, 0) != ADS131M0X_ERROR_LOCKED) all_ok = false;

    ADS131M0XConfig cfg = {0};
    if (ads131m0xConfigure(dev, &cfg) != ADS131M0X_ERROR_LOCKED) all_ok = false;

    ADS131M0XChannelConfig ch_cfg = { .is_enabled = true };
    if (ads131m0xConfigureChannel(dev, 0, &ch_cfg) != ADS131M0X_ERROR_LOCKED) all_ok = false;

    if (ads131m0xReset(dev) != ADS131M0X_ERROR_LOCKED) all_ok = false;

    uint16_t vals[2] = {0, 0};
    if (ads131m0xWriteRegisters(dev, ADS131M0X_CFG_ADDRESS, vals, 2) != ADS131M0X_ERROR_LOCKED) all_ok = false;

    // Read should still work
    uint16_t readback = 0;
    if (ads131m0xReadRegister(dev, ADS131M0X_CFG_ADDRESS, &readback) != ADS131M0X_ERROR_OK) all_ok = false;

    // Cleanup: unlock + reset
    ads131m0xUnlock(dev);
    ads131m0xReset(dev);

    return all_ok;
}

// ═════════════════════════════════════════════════════════════════════════════
// GROUP E — Data Acquisition
// ═════════════════════════════════════════════════════════════════════════════

static bool test_shorted_inputs_near_zero(ADS131M0X* dev)
{
    // Configure all channels: MUX_SHORTED, gain=1
    for (uint8_t ch = 0; ch < ADS131M0X_CHANNEL_COUNT; ch++)
    {
        ADS131M0XChannelConfig cfg =
        {
            .is_enabled           = true,
            .gain                 = ADS131M0X_GAIN_1,
            .mux                  = ADS131M0X_MUX_SHORTED,
            .phase_delay_cycles   = 0,
            .is_dc_block_disabled = false,
            .offset_cal           = 0,
            .gain_cal             = 0x800000,
        };
        ADS131M0XError err = ads131m0xConfigureChannel(dev, ch, &cfg);
        if (err != ADS131M0X_ERROR_OK)
        {
            ESP_LOGE(TAG, "  CH%d cfg failed: %s", ch, ads131m0xErrorToString(err));
            return false;
        }
    }

    flushSemaphore();

    static int32_t samples[TEST_NUM_SAMPLES][ADS131M0X_CHANNEL_COUNT];
    if (!acquireSamplesAllChannels(samples, TEST_NUM_SAMPLES, TEST_SETTLE_SAMPLES))
    {
        ESP_LOGE(TAG, "  Acquisition failed (DRDY timeout)");
        ads131m0xReset(dev);
        return false;
    }

    // Only validate CH0 — CH1-CH3 may have PCB-level DC offsets without global-chop
    int64_t sum = 0;
    for (uint32_t i = 0; i < TEST_NUM_SAMPLES; i++)
        sum += samples[i][0];
    int64_t mean_uv = ads131m0xConvertToMicrovolts((int32_t)(sum / TEST_NUM_SAMPLES), ADS131M0X_GAIN_1);
    int64_t abs_mean = mean_uv < 0 ? -mean_uv : mean_uv;

    ESP_LOGI(TAG, "  CH0 mean: %lld uV (threshold: +/-%d uV)", (long long)mean_uv, TEST_NOISE_THRESHOLD_UV);

    for (uint8_t ch = 1; ch < ADS131M0X_CHANNEL_COUNT; ch++)
    {
        int64_t ch_sum = 0;
        for (uint32_t i = 0; i < TEST_NUM_SAMPLES; i++)
            ch_sum += samples[i][ch];
        int64_t ch_mean = ads131m0xConvertToMicrovolts((int32_t)(ch_sum / TEST_NUM_SAMPLES), ADS131M0X_GAIN_1);
        ESP_LOGI(TAG, "  CH%d mean: %lld uV (info only, not validated)", ch, (long long)ch_mean);
    }

    bool ok = (abs_mean <= TEST_NOISE_THRESHOLD_UV);
    if (!ok)
        ESP_LOGE(TAG, "  CH0 FAILED: |%lld| > %d uV", (long long)mean_uv, TEST_NOISE_THRESHOLD_UV);

    ads131m0xReset(dev);
    return ok;
}

static bool test_internal_test_signal(ADS131M0X* dev)
{
    // Test at GAIN_1
    ADS131M0XChannelConfig cfg =
    {
        .is_enabled           = true,
        .gain                 = ADS131M0X_GAIN_1,
        .mux                  = ADS131M0X_MUX_POSITIVE_DCM,
        .phase_delay_cycles   = 0,
        .is_dc_block_disabled = false,
        .offset_cal           = 0,
        .gain_cal             = 0x800000,
    };
    ADS131M0XError err = ads131m0xConfigureChannel(dev, 0, &cfg);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  CH0 cfg (gain1) failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    flushSemaphore();

    static int32_t buf[TEST_NUM_SAMPLES];
    if (!acquireSamples(buf, TEST_NUM_SAMPLES, TEST_SETTLE_SAMPLES))
    {
        ESP_LOGE(TAG, "  Acquisition failed at GAIN_1");
        ads131m0xReset(dev);
        return false;
    }

    int64_t sum1 = 0;
    for (uint32_t i = 0; i < TEST_NUM_SAMPLES; i++)
        sum1 += buf[i];
    int64_t mean_uv_g1 = ads131m0xConvertToMicrovolts((int32_t)(sum1 / TEST_NUM_SAMPLES), ADS131M0X_GAIN_1);

    int64_t diff1 = mean_uv_g1 - TEST_SIGNAL_EXPECTED_UV;
    if (diff1 < 0) diff1 = -diff1;
    int64_t tolerance = (int64_t)TEST_SIGNAL_EXPECTED_UV * TEST_SIGNAL_TOLERANCE_PCT / 100;

    ESP_LOGI(TAG, "  GAIN_1: mean=%lld uV, expected=%d uV, tol=%lld uV",
             (long long)mean_uv_g1, TEST_SIGNAL_EXPECTED_UV, (long long)tolerance);

    bool ok = (diff1 <= tolerance);

    // Test at GAIN_2: expected = TEST_SIGNAL_EXPECTED_UV / 2 because
    // the internal test signal scales to 2/15 × V_Diff_Max, and
    // V_Diff_Max = 1.2V/Gain. ConvertToMicrovolts returns input-referred voltage.
    int64_t expected_g2 = TEST_SIGNAL_EXPECTED_UV / 2;  // 80000 uV
    int64_t tolerance_g2 = expected_g2 * TEST_SIGNAL_TOLERANCE_PCT / 100;

    cfg.gain = ADS131M0X_GAIN_2;
    err = ads131m0xConfigureChannel(dev, 0, &cfg);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  CH0 cfg (gain2) failed: %s", ads131m0xErrorToString(err));
        ads131m0xReset(dev);
        return false;
    }

    flushSemaphore();

    if (!acquireSamples(buf, TEST_NUM_SAMPLES, TEST_SETTLE_SAMPLES))
    {
        ESP_LOGE(TAG, "  Acquisition failed at GAIN_2");
        ads131m0xReset(dev);
        return false;
    }

    int64_t sum2 = 0;
    for (uint32_t i = 0; i < TEST_NUM_SAMPLES; i++)
        sum2 += buf[i];
    int64_t mean_uv_g2 = ads131m0xConvertToMicrovolts((int32_t)(sum2 / TEST_NUM_SAMPLES), ADS131M0X_GAIN_2);

    int64_t diff2 = mean_uv_g2 - expected_g2;
    if (diff2 < 0) diff2 = -diff2;

    ESP_LOGI(TAG, "  GAIN_2: mean=%lld uV, expected=%lld uV, tol=%lld uV",
             (long long)mean_uv_g2, (long long)expected_g2, (long long)tolerance_g2);

    if (diff2 > tolerance_g2)
        ok = false;

    ads131m0xReset(dev);
    return ok;
}

static bool test_crc_validation(ADS131M0X* dev)
{
    // Enable output CRC (CCITT)
    const ADS131M0XConfig config =
    {
        .power_mode            = ADS131M0X_POWER_HIGH_RES,
        .oversampling_ratio    = ADS131M0X_OSR_4096,
        .is_turbo_mode_enabled = true,
        .word_length           = ADS131M0X_WLENGTH_24_BIT,
        .is_spi_timeout_enabled = true,

        .crc =
        {
            .is_output_enabled = true,
            .is_input_enabled  = false,
            .polynomial        = ADS131M0X_CRC_POLYNOMIAL_ANSI,
        },
    };

    ADS131M0XError err = ads131m0xConfigure(dev, &config);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Configure CRC failed: %s", ads131m0xErrorToString(err));
        return false;
    }

    flushSemaphore();

    err = ads131m0xWakeup(dev);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "  Wakeup failed: %s", ads131m0xErrorToString(err));
        ads131m0xReset(dev);
        return false;
    }

    s_spi_log_enabled = false;
    ESP_ERROR_CHECK(spi_device_acquire_bus(s_spi_dev, portMAX_DELAY));

    bool all_valid = true;
    for (int i = 0; i < 5; i++)
    {
        if (xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(TEST_DRDY_TIMEOUT_MS)) != pdTRUE)
        {
            ESP_LOGE(TAG, "  DRDY timeout on frame %d", i);
            all_valid = false;
            break;
        }

        ADS131M0XData data;
        err = ads131m0xReadData(&s_adc, &data);
        if (err != ADS131M0X_ERROR_OK)
        {
            ESP_LOGE(TAG, "  ReadData frame %d failed: %s", i, ads131m0xErrorToString(err));
            all_valid = false;
            break;
        }

        if (!data.is_crc_valid)
        {
            ESP_LOGE(TAG, "  Frame %d CRC invalid (crc=0x%04X)", i, data.crc);
            all_valid = false;
        }
    }

    spi_device_release_bus(s_spi_dev);
    ads131m0xStandby(dev);
    ads131m0xReset(dev);
    return all_valid;
}

// ═════════════════════════════════════════════════════════════════════════════
// GROUP F — Conversion Math (pure, no SPI)
// ═════════════════════════════════════════════════════════════════════════════

static bool test_convert_zero(ADS131M0X* dev)
{
    (void)dev;
    int64_t result = ads131m0xConvertToMicrovolts(0, ADS131M0X_GAIN_1);
    if (result != 0)
    {
        ESP_LOGE(TAG, "  ConvertToMicrovolts(0, GAIN_1) = %lld, expected 0", (long long)result);
        return false;
    }
    return true;
}

static bool test_convert_positive_known(ADS131M0X* dev)
{
    (void)dev;
    int64_t result = ads131m0xConvertToMicrovolts(8388607, ADS131M0X_GAIN_1);
    // 8388607 * 1200000 / 8388608 = 1199999.857 -> round-half-up to 1200000
    if (result != 1200000)
    {
        ESP_LOGE(TAG, "  ConvertToMicrovolts(8388607, GAIN_1) = %lld, expected 1200000", (long long)result);
        return false;
    }
    return true;
}

static bool test_convert_negative_known(ADS131M0X* dev)
{
    (void)dev;
    int64_t result = ads131m0xConvertToMicrovolts(-8388608, ADS131M0X_GAIN_1);
    if (result != -1200000)
    {
        ESP_LOGE(TAG, "  ConvertToMicrovolts(-8388608, GAIN_1) = %lld, expected -1200000", (long long)result);
        return false;
    }
    return true;
}

static bool test_convert_with_gain(ADS131M0X* dev)
{
    (void)dev;
    struct { ADS131M0XGain gain; int64_t expected; } cases[] =
    {
        { ADS131M0X_GAIN_1,   600000 },
        { ADS131M0X_GAIN_2,   300000 },
        { ADS131M0X_GAIN_4,   150000 },
        { ADS131M0X_GAIN_128, 4688   },
    };

    bool all_ok = true;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int64_t result = ads131m0xConvertToMicrovolts(4194304, cases[i].gain);
        if (result != cases[i].expected)
        {
            ESP_LOGE(TAG, "  Half-scale GAIN_%lld: got %lld, expected %lld",
                     (long long)(1LL << cases[i].gain), (long long)result, (long long)cases[i].expected);
            all_ok = false;
        }
    }

    return all_ok;
}

static bool test_convert_small_code(ADS131M0X* dev)
{
    (void)dev;
    bool ok = true;

    // raw=1 -> 0 uV (truncated due to integer division)
    int64_t r1 = ads131m0xConvertToMicrovolts(1, ADS131M0X_GAIN_1);
    if (r1 != 0)
    {
        ESP_LOGE(TAG, "  raw=1: got %lld, expected 0", (long long)r1);
        ok = false;
    }

    // raw=7 -> 1 uV (rounds up)
    int64_t r7 = ads131m0xConvertToMicrovolts(7, ADS131M0X_GAIN_1);
    if (r7 != 1)
    {
        ESP_LOGE(TAG, "  raw=7: got %lld, expected 1", (long long)r7);
        ok = false;
    }

    return ok;
}

// ═════════════════════════════════════════════════════════════════════════════
// PARAMETER SWEEP — Automated signal characterization
// ═════════════════════════════════════════════════════════════════════════════

typedef struct
{
    ADS131M0XOSR osr;
    uint16_t     osr_value;     // numerical value for display
    uint32_t     expected_fs;   // expected Fs (Hz) = f_MOD / OSR = 4.096 MHz / osr_value
} SweepOSR;

typedef struct
{
    ADS131M0XGain gain;
    uint8_t       gain_value;   // numerical value for display
} SweepGain;

// ═════════════════════════════════════════════════════════════════════════════
// GAIN COMPARISON — Single visual dump for GAIN_1 vs GAIN_2
// ═════════════════════════════════════════════════════════════════════════════

static void runGainComparison(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "GAIN COMPARISON — 500Hz sine on AIN0");
    ESP_LOGI(TAG, "========================================");

    static const ADS131M0XGain gains[] = { ADS131M0X_GAIN_1, ADS131M0X_GAIN_2 };
    static const uint8_t gain_vals[]   = { 1, 2 };
    static int32_t buf[SWEEP_NUM_SAMPLES];

    for (size_t gi = 0; gi < 2; gi++)
    {
        ads131m0xReset(&s_adc);

        const ADS131M0XConfig config =
        {
            .power_mode            = ADS131M0X_POWER_HIGH_RES,
            .oversampling_ratio    = ADS131M0X_OSR_4096,
            .is_turbo_mode_enabled = true,
            .word_length           = ADS131M0X_WLENGTH_24_BIT,
            .is_spi_timeout_enabled = true,
        };
        ads131m0xConfigure(&s_adc, &config);

        const ADS131M0XChannelConfig ch_cfg =
        {
            .is_enabled           = true,
            .gain                 = gains[gi],
            .mux                  = ADS131M0X_MUX_NORMAL,
            .phase_delay_cycles   = 0,
            .is_dc_block_disabled = false,
            .offset_cal           = 0,
            .gain_cal             = 0x800000,
        };
        ads131m0xConfigureChannel(&s_adc, 0, &ch_cfg);

        flushSemaphore();
        ads131m0xWakeup(&s_adc);
        ESP_ERROR_CHECK(spi_device_acquire_bus(s_spi_dev, portMAX_DELAY));

        // Discard settling samples
        for (int d = 0; d < TEST_SETTLE_SAMPLES; d++)
        {
            xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(TEST_DRDY_TIMEOUT_MS));
            ADS131M0XData discard;
            ads131m0xReadData(&s_adc, &discard);
        }

        // Acquire
        for (uint32_t i = 0; i < SWEEP_NUM_SAMPLES; i++)
        {
            xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(TEST_DRDY_TIMEOUT_MS));
            ADS131M0XData data;
            ads131m0xReadData(&s_adc, &data);
            buf[i] = data.channel_data[0];
        }

        spi_device_release_bus(s_spi_dev);
        ads131m0xStandby(&s_adc);

        printf("=============================================\n");
        printf("GAIN COMPARE: GAIN=%u, OSR=4096 (Fs=2kHz)\n", gain_vals[gi]);
        printf("DATA START\n");
        for (uint32_t i = 0; i < SWEEP_NUM_SAMPLES; i++)
        {
            printf("%ld", (long)buf[i]);
            printf((i + 1) % 10 == 0 || i == SWEEP_NUM_SAMPLES - 1 ? "\n" : ",");
        }
        printf("DATA END\n\n");
    }
}

static void runParameterSweep(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "PARAMETER SWEEP — Signal Characterization");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "Signal: 500 Hz, 500 mVpp sine on AIN0");
    ESP_LOGI(TAG, "");

    // turbo_mode=false -> OSR bits are used directly
    // f_CLKIN = 8.192 MHz (internal), f_MOD = f_CLKIN / 2 = 4.096 MHz
    // Fs = f_MOD / OSR
    static const SweepOSR osr_table[] =
    {
        { ADS131M0X_OSR_128,  128,  32000 },
        { ADS131M0X_OSR_1024, 1024, 4000  },
        { ADS131M0X_OSR_4096, 4096, 1000  },
    };

    static const SweepGain gain_table[] =
    {
        { ADS131M0X_GAIN_1, 1 },
        { ADS131M0X_GAIN_2, 2 },
    };

    static int32_t sweep_buf[SWEEP_NUM_SAMPLES];

    for (size_t oi = 0; oi < sizeof(osr_table) / sizeof(osr_table[0]); oi++)
    {
        for (size_t gi = 0; gi < sizeof(gain_table) / sizeof(gain_table[0]); gi++)
        {
            // ── Reconfigure ADC ──────────────────────────────────────────
            ads131m0xReset(&s_adc);

            const ADS131M0XConfig config =
            {
                .power_mode            = ADS131M0X_POWER_HIGH_RES,
                .oversampling_ratio    = osr_table[oi].osr,
                .is_turbo_mode_enabled = false,
                .word_length           = ADS131M0X_WLENGTH_24_BIT,
                .is_spi_timeout_enabled = true,
            };
            ADS131M0XError err = ads131m0xConfigure(&s_adc, &config);
            if (err != ADS131M0X_ERROR_OK)
            {
                ESP_LOGE(TAG, "  Sweep configure failed: %s", ads131m0xErrorToString(err));
                continue;
            }

            const ADS131M0XChannelConfig ch_cfg =
            {
                .is_enabled           = true,
                .gain                 = gain_table[gi].gain,
                .mux                  = ADS131M0X_MUX_NORMAL,
                .phase_delay_cycles   = 0,
                .is_dc_block_disabled = false,
                .offset_cal           = 0,
                .gain_cal             = 0x800000,
            };
            err = ads131m0xConfigureChannel(&s_adc, 0, &ch_cfg);
            if (err != ADS131M0X_ERROR_OK)
            {
                ESP_LOGE(TAG, "  Sweep CH0 config failed: %s", ads131m0xErrorToString(err));
                continue;
            }

            // ── Flush stale DRDY events ──────────────────────────────────
            flushSemaphore();

            // ── Wake and acquire ─────────────────────────────────────────
            err = ads131m0xWakeup(&s_adc);
            if (err != ADS131M0X_ERROR_OK)
            {
                ESP_LOGE(TAG, "  Sweep wakeup failed: %s", ads131m0xErrorToString(err));
                continue;
            }

            ESP_ERROR_CHECK(spi_device_acquire_bus(s_spi_dev, portMAX_DELAY));

            // Discard first few samples for settling
            for (int d = 0; d < TEST_SETTLE_SAMPLES; d++)
            {
                xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(TEST_DRDY_TIMEOUT_MS));
                ADS131M0XData discard;
                ads131m0xReadData(&s_adc, &discard);
            }

            int64_t t_first = 0;
            int64_t t_last  = 0;
            bool acq_ok = true;

            for (uint32_t i = 0; i < SWEEP_NUM_SAMPLES; i++)
            {
                if (xSemaphoreTake(s_data_sem, pdMS_TO_TICKS(TEST_DRDY_TIMEOUT_MS)) != pdTRUE)
                {
                    ESP_LOGE(TAG, "  DRDY timeout at sample %lu", (unsigned long)i);
                    acq_ok = false;
                    break;
                }

                ADS131M0XData data;
                err = ads131m0xReadData(&s_adc, &data);
                if (err != ADS131M0X_ERROR_OK)
                {
                    ESP_LOGE(TAG, "  ReadData failed at sample %lu: %s", (unsigned long)i, ads131m0xErrorToString(err));
                    acq_ok = false;
                    break;
                }

                int64_t now = esp_timer_get_time();
                if (i == 0) t_first = now;
                t_last = now;

                sweep_buf[i] = data.channel_data[0];
            }

            spi_device_release_bus(s_spi_dev);
            ads131m0xStandby(&s_adc);

            if (!acq_ok)
            {
                ESP_LOGE(TAG, "  Sweep iteration failed, skipping output");
                continue;
            }

            // ── Output ───────────────────────────────────────────────────
            int64_t elapsed = t_last - t_first;
            double sampling_rate = (elapsed > 0)
                ? (double)(SWEEP_NUM_SAMPLES - 1) / ((double)elapsed / 1000000.0)
                : 0.0;

            printf("=============================================\n");
            printf("SWEEP CONFIG: OSR=%u, GAIN=%u (Fs=%luHz)\n",
                   osr_table[oi].osr_value,
                   gain_table[gi].gain_value,
                   (unsigned long)osr_table[oi].expected_fs);
            printf("DATA START\n");
            for (uint32_t i = 0; i < SWEEP_NUM_SAMPLES; i++)
            {
                printf("%ld", (long)sweep_buf[i]);
                printf((i + 1) % 10 == 0 || i == SWEEP_NUM_SAMPLES - 1 ? "\n" : ",");
            }
            printf("DATA END\n");
            printf("Elapsed: %lld us, Sampling rate: %.2f Hz\n\n",
                   (long long)elapsed, sampling_rate);
        }
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// APPLICATION ENTRY
// ═════════════════════════════════════════════════════════════════════════════

void app_main(void)
{
    spiInit();

    const ADS131M0XHAL hal =
    {
        .spiRead      = halSpiRead,
        .spiWrite     = halSpiWrite,
        .drdyGet      = halDrdyGet,
        .syncResetSet = NULL,
        .delayMs      = halDelayMs,
    };

    ADS131M0XError err = ads131m0xInit(&s_adc, &hal);
    if (err != ADS131M0X_ERROR_OK)
    {
        ESP_LOGE(TAG, "ADS131M0X init failed: %s", ads131m0xErrorToString(err));
        return;
    }
    ESP_LOGI(TAG, "ADS131M0X initialized (standby mode)");

    s_data_sem = xSemaphoreCreateBinary();
    drdyInterruptInit();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "ADS131M04 Integration Test Suite");
    ESP_LOGI(TAG, "========================================");

    uint32_t pass = 0;
    uint32_t fail = 0;

    // ── Group G: Error Handling (stateless) ──────────────────────────────
    RUN_TEST(test_error_null_param,      &s_adc, pass, fail);
    RUN_TEST(test_error_not_initialized, &s_adc, pass, fail);
    RUN_TEST(test_error_invalid_channel, &s_adc, pass, fail);

    // ── Group A: Identity & Init ─────────────────────────────────────────
    RUN_TEST(test_chip_id_readback,        &s_adc, pass, fail);
    RUN_TEST(test_reset_default_registers, &s_adc, pass, fail);

    // ── Group B: Global Config Readback ──────────────────────────────────
    RUN_TEST(test_global_config_readback, &s_adc, pass, fail);

    // ── Group C: Per-Channel Config Readback ─────────────────────────────
    RUN_TEST(test_per_channel_config_readback, &s_adc, pass, fail);

    // ── Group D: Device Control ──────────────────────────────────────────
    RUN_TEST(test_standby_command,       &s_adc, pass, fail);
    RUN_TEST(test_wakeup_command,        &s_adc, pass, fail);
    RUN_TEST(test_lock_rejects_writes,   &s_adc, pass, fail);
    RUN_TEST(test_unlock_restores_writes,&s_adc, pass, fail);

    // ── Group G (continued): Locked-write error ──────────────────────────
    RUN_TEST(test_error_locked_write, &s_adc, pass, fail);

    // ── Group E: Data Acquisition (needs DRDY interrupt) ─────────────────
    RUN_TEST(test_shorted_inputs_near_zero, &s_adc, pass, fail);
    RUN_TEST(test_internal_test_signal,     &s_adc, pass, fail);
    RUN_TEST(test_crc_validation,           &s_adc, pass, fail);

    // ── Group F: Conversion Math (pure, no SPI) ──────────────────────────
    RUN_TEST(test_convert_zero,          &s_adc, pass, fail);
    RUN_TEST(test_convert_positive_known,&s_adc, pass, fail);
    RUN_TEST(test_convert_negative_known,&s_adc, pass, fail);
    RUN_TEST(test_convert_with_gain,     &s_adc, pass, fail);
    RUN_TEST(test_convert_small_code,    &s_adc, pass, fail);

    // ── Summary ──────────────────────────────────────────────────────────
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "RESULTS: %lu passed, %lu failed, %lu total",
             (unsigned long)pass, (unsigned long)fail, (unsigned long)(pass + fail));
    ESP_LOGI(TAG, "========================================");

    // ── Gain comparison + Parameter Sweep (only if all tests passed) ────
    if (fail == 0)
    {
        runGainComparison();
        runParameterSweep();
    }
    else
    {
        ESP_LOGW(TAG, "Skipping sweep due to test failures");
    }

    // ── Cleanup ──────────────────────────────────────────────────────────
    ads131m0xDeinit(&s_adc);

    ESP_LOGI(TAG, "All done. Halting.");
    while (1)
    {
        vTaskDelay(portMAX_DELAY);
    }
}
