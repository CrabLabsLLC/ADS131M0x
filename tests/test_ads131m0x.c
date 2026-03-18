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

void setUp(void)
{
    mockReset();
}

void tearDown(void) {}

void testPlaceholder(void)
{
    TEST_ASSERT_EQUAL(1, 1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(testPlaceholder);
    return UNITY_END();
}