#include <string.h>

#include "unity.h"
#include "ads131m0x.h"

/* ── Mock state ──────────────────────────────────────────────────────────── */
#define MOCK_BUF_SIZE 24

static uint8_t mock_rx_buf[MOCK_BUF_SIZE];
static uint8_t mock_tx_buf[MOCK_BUF_SIZE];
static int mock_spi_error;

static int mockSpiRead(void* const data, const uint8_t length)
{
    memcpy(data, mock_rx_buf, length);
    return mock_spi_error;
}

static int mockSpiWrite(const void* const data, const uint8_t length)
{
    memcpy(mock_tx_buf, data, length);
    return mock_spi_error;
}

static bool mockDrdyGet(void) { return true; }
static void mockSyncResetSet(bool state) { (void) state; }
static void mockDelayMs(uint32_t ms) { (void) ms; }

static const ADS131M0XHAL mock_hal = {
    .spiRead = mockSpiRead,
    .spiWrite = mockSpiWrite,
    .drdyGet = mockDrdyGet,
    .syncResetSet = mockSyncResetSet,
    .delayMs = mockDelayMs,
};

/* ── Mock helpers ────────────────────────────────────────────────────────── */

/** Call in setUp() to reset state between tests */
static void mockReset(void)
{
    memset(mock_rx_buf, 0, sizeof(mock_rx_buf));
    memset(mock_tx_buf, 0, sizeof(mock_tx_buf));
    mock_spi_error = 0;
}

/* Pre-load a 16-bit word into the RX buffer at byte offset 0 (big-endian, 24-bit words) */
static void mockSetRxWord(uint16_t word)
{
    mock_rx_buf[0] = (uint8_t) (word >> 8U);
    mock_rx_buf[1] = (uint8_t) (word & 0xFFU);
}

void testErrorToStringReturnsCorrectStrings(void)
{
    TEST_ASSERT_EQUAL_STRING("OK",              ads131m0xErrorToString(ADS131M0X_ERROR_OK));
    TEST_ASSERT_EQUAL_STRING("NULL_PARAM",      ads131m0xErrorToString(ADS131M0X_ERROR_NULL_PARAM));
    TEST_ASSERT_EQUAL_STRING("NOT_INITIALIZED", ads131m0xErrorToString(ADS131M0X_ERROR_NOT_INITIALIZED));
    TEST_ASSERT_EQUAL_STRING("SPI",             ads131m0xErrorToString(ADS131M0X_ERROR_SPI));
    TEST_ASSERT_EQUAL_STRING("INVALID_CHANNEL", ads131m0xErrorToString(ADS131M0X_ERROR_INVALID_CHANNEL));
    TEST_ASSERT_EQUAL_STRING("INVALID_PARAM",   ads131m0xErrorToString(ADS131M0X_ERROR_INVALID_PARAM));
    TEST_ASSERT_EQUAL_STRING("CRC",             ads131m0xErrorToString(ADS131M0X_ERROR_CRC));
    TEST_ASSERT_EQUAL_STRING("LOCKED",          ads131m0xErrorToString(ADS131M0X_ERROR_LOCKED));
    TEST_ASSERT_EQUAL_STRING("ID_MISMATCH",     ads131m0xErrorToString(ADS131M0X_ERROR_ID_MISMATCH));
    TEST_ASSERT_EQUAL_STRING("ADS131M0X_ERROR_UNKNOWN", ads131m0xErrorToString((ADS131M0XError)999));
}

void testConvertToMicrovoltsGainAndScale(void)
{
    struct { int32_t raw; ADS131M0XGain gain; int64_t expected_uv; const char* desc; } cases[] =
    {
        // raw_code | gain         | expected_uv | description
        {        0, ADS131M0X_GAIN_1,          0LL, "Zero input is always 0 uV"},
        { 8388607,  ADS131M0X_GAIN_1,     1199999LL, "Full-scale positive, gain 1"},
        {-8388608,  ADS131M0X_GAIN_1,    -1200000LL, "Full-scale negative, gain 1"},
        { 8388607,  ADS131M0X_GAIN_2,      599999LL, "Full-scale positive, gain 2"},
        { 8388607,  ADS131M0X_GAIN_128,      9374LL, "Full-scale positive, gain 128"},
        { 4194304,  ADS131M0X_GAIN_1,      600000LL, "Mid-scale positive, gain 1"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int64_t result = ads131m0xConvertToMicrovolts(cases[i].raw, cases[i].gain);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(cases[i].expected_uv, result, cases[i].desc);
    }
}

void setUp(void)
{
    mockReset();
}

void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(testErrorToStringReturnsCorrectStrings);
    RUN_TEST(testConvertToMicrovoltsGainAndScale);
    return UNITY_END();
}