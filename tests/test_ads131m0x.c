#include "unity.h"
#include "ads131m0x.h"
#include <string.h>
#include <stddef.h>

/* ── Mock ─────────────────────────────────────────────────────────────────────
*
* The ADS131M0X SPI protocol is two-frame:
*   TX (frame 1): host sends command  → captured in mock_tx_history[]
*   RX (frame 2): host reads response → served from mock_rx_queue[]
*
* TX history lets tests inspect every command the driver sent.
* RX queue lets tests inject device responses in order, one per spiRead call.
*/

#define MOCK_BUF_SIZE         24   /* (4ch + status + CRC) * 4 bytes = 24 max */
#define MOCK_RX_QUEUE_DEPTH    4
#define MOCK_TX_HISTORY_DEPTH  4

static uint8_t mock_rx_queue  [MOCK_RX_QUEUE_DEPTH]   [MOCK_BUF_SIZE];
static int     mock_rx_count;   /* total primed responses */
static int     mock_rx_index;   /* next response to serve */

static uint8_t mock_tx_history [MOCK_TX_HISTORY_DEPTH] [MOCK_BUF_SIZE];
static int     mock_tx_count;   /* how many spiWrite calls happened */

static int     mock_spi_error;  /* 0 = success, -1 = bus failure */

static int mockSpiRead(void* const data, const uint8_t length)
{
    if (mock_rx_index < mock_rx_count)
        memcpy(data, mock_rx_queue[mock_rx_index++], length);
    else
        memset(data, 0, length);
    return mock_spi_error;
}

static int mockSpiWrite(const void* const data, const uint8_t length)
{
    if (mock_tx_count < MOCK_TX_HISTORY_DEPTH)
    {
        uint8_t copy_len = length < MOCK_BUF_SIZE ? length : MOCK_BUF_SIZE;
        memcpy(mock_tx_history[mock_tx_count], data, copy_len);
        mock_tx_count++;
    }
    return mock_spi_error;
}

static bool mockDrdyGet(void)            { return true;    }
static void mockSyncResetSet(bool state) { (void)state;    }
static void mockDelayMs(uint32_t ms)     { (void)ms;       }

static const ADS131M0XHAL mock_hal = {
    .spiRead      = mockSpiRead,
    .spiWrite     = mockSpiWrite,
    .drdyGet      = mockDrdyGet,
    .syncResetSet = mockSyncResetSet,
    .delayMs      = mockDelayMs,
};

static void mockReset(void)
{
    memset(mock_rx_queue,   0, sizeof(mock_rx_queue));
    memset(mock_tx_history, 0, sizeof(mock_tx_history));
    mock_rx_count  = 0;
    mock_rx_index  = 0;
    mock_tx_count  = 0;
    mock_spi_error = 0;
}

/* Enqueue a response whose first two bytes are [word_high, word_low], rest zero */
static void mockPrimeRx(uint16_t word)
{
    if (mock_rx_count >= MOCK_RX_QUEUE_DEPTH) return;
    memset(mock_rx_queue[mock_rx_count], 0, MOCK_BUF_SIZE);
    mock_rx_queue[mock_rx_count][0] = (uint8_t)(word >> 8U);
    mock_rx_queue[mock_rx_count][1] = (uint8_t)(word & 0xFFU);
    mock_rx_count++;
}

/* Enqueue a full raw RX frame (for readData tests that need specific byte layout) */
static void mockPrimeRxFrame(const uint8_t* bytes, uint8_t len)
{
    if (mock_rx_count >= MOCK_RX_QUEUE_DEPTH) return;
    memset(mock_rx_queue[mock_rx_count], 0, MOCK_BUF_SIZE);
    memcpy(mock_rx_queue[mock_rx_count], bytes,
            len < MOCK_BUF_SIZE ? len : MOCK_BUF_SIZE);
    mock_rx_count++;
}

/* ── Test Helper ──────────────────────────────────────────────────────────────
* Use at the start of any test that needs a properly initialized device.
* Primes the boot frame, runs init, then clears mock state so only the
* function under test sees a clean TX history and RX queue.
*/
static void initDev(ADS131M0X* dev)
{
    mockPrimeRx(ADS131M0X_RESP_RESET_OK);
    ads131m0xInit(dev, &mock_hal);
    mockReset();
}

/* ── Unity Hooks ──────────────────────────────────────────────────────────── */

void setUp(void)    { mockReset(); }
void tearDown(void) {}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xErrorToString
* ═══════════════════════════════════════════════════════════════════════════ */

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
    TEST_ASSERT_EQUAL_STRING("ADS131M0X_ERROR_UNKNOWN",
                            ads131m0xErrorToString((ADS131M0XError)999));
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xConvertToMicrovolts
* ═══════════════════════════════════════════════════════════════════════════ */

void testConvertToMicrovoltsGainAndScale(void)
{
    // | raw_code  | gain                  | expected_uv | description                          |
    struct { int32_t raw; ADS131M0XGain gain; int64_t uv; const char* desc; } cases[] =
    {
        {        0, ADS131M0X_GAIN_1,          0LL, "Zero input is always 0 uV"                },
        {  8388607, ADS131M0X_GAIN_1,    1199999LL, "Full-scale positive, gain 1"              },
        { -8388608, ADS131M0X_GAIN_1,   -1200000LL, "Full-scale negative, gain 1"              },
        {  8388607, ADS131M0X_GAIN_2,     599999LL, "Full-scale positive, gain 2"              },
        {  4194304, ADS131M0X_GAIN_1,     600000LL, "Mid-scale positive, gain 1"               },
        {  8388607, ADS131M0X_GAIN_128,     9374LL, "Full-scale positive, gain 128 (truncates)"},
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int64_t result = ads131m0xConvertToMicrovolts(cases[i].raw, cases[i].gain);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(cases[i].uv, result, cases[i].desc);
    }
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xInit
* ═══════════════════════════════════════════════════════════════════════════ */

void testInitRejectsNullDev(void)
{
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xInit(NULL, &mock_hal));
}

void testInitRejectsNullHal(void)
{
    ADS131M0X dev;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xInit(&dev, NULL));
}

void testInitRejectsNullHalFunctionPointers(void)
{
    ADS131M0X dev;

    // | spiRead     | spiWrite     | drdyGet     | syncResetSet     | delayMs     | desc               |
    struct { ADS131M0XHAL hal; const char* desc; } cases[] =
    {
        {{ NULL,        mockSpiWrite, mockDrdyGet, mockSyncResetSet, mockDelayMs }, "NULL spiRead"      },
        {{ mockSpiRead, NULL,         mockDrdyGet, mockSyncResetSet, mockDelayMs }, "NULL spiWrite"     },
        {{ mockSpiRead, mockSpiWrite, NULL,        mockSyncResetSet, mockDelayMs }, "NULL drdyGet"      },
        {{ mockSpiRead, mockSpiWrite, mockDrdyGet, NULL,             mockDelayMs }, "NULL syncResetSet" },
        {{ mockSpiRead, mockSpiWrite, mockDrdyGet, mockSyncResetSet, NULL        }, "NULL delayMs"      },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        ADS131M0XError err = ads131m0xInit(&dev, &cases[i].hal);
        TEST_ASSERT_EQUAL_MESSAGE(ADS131M0X_ERROR_NULL_PARAM, err, cases[i].desc);
    }
}

void testInitReturnsSpiErrorOnReadFailure(void)
{
    ADS131M0X dev;
    mock_spi_error = -1;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_SPI, ads131m0xInit(&dev, &mock_hal));
}

void testInitRejectsWrongResponseWord(void)
{
    ADS131M0X dev;
    mockPrimeRx(0xDEAD);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_ID_MISMATCH, ads131m0xInit(&dev, &mock_hal));
}

void testInitSucceedsAndSetsInitializedFlag(void)
{
    ADS131M0X dev;
    mockPrimeRx(ADS131M0X_RESP_RESET_OK);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xInit(&dev, &mock_hal));
    TEST_ASSERT_TRUE(dev.is_initialized);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xDeinit
* ═══════════════════════════════════════════════════════════════════════════ */

void testDeinitRejectsNullDev(void)
{
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xDeinit(NULL));
}

void testDeinitClearsInitializedFlagAndNullsHal(void)
{
    ADS131M0X dev;
    initDev(&dev);

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xDeinit(&dev));
    TEST_ASSERT_FALSE(dev.is_initialized);
    TEST_ASSERT_NULL(dev.hal.spiRead);
    TEST_ASSERT_NULL(dev.hal.spiWrite);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xRead / ads131m0xWrite (raw SPI)
*
* These are thin wrappers around the HAL — they intentionally do NOT check
* is_initialized. A device with hal assigned but not initialized can call them.
* ═══════════════════════════════════════════════════════════════════════════ */

void testRawReadRejectsNullParams(void)
{
    ADS131M0X dev = { .hal = mock_hal };
    uint8_t buf[4];
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xRead(NULL, buf,  4));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xRead(&dev, NULL, 4));
}

void testRawWriteRejectsNullParams(void)
{
    ADS131M0X dev = { .hal = mock_hal };
    uint8_t buf[4] = {0};
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xWrite(NULL, buf,  4));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xWrite(&dev, NULL, 4));
}

void testRawReadReturnsSpiErrorOnFailure(void)
{
    ADS131M0X dev = { .hal = mock_hal };
    uint8_t buf[4];
    mock_spi_error = -1;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_SPI, ads131m0xRead(&dev, buf, 4));
}

void testRawWriteReturnsSpiErrorOnFailure(void)
{
    ADS131M0X dev = { .hal = mock_hal };
    uint8_t buf[4] = {0};
    mock_spi_error = -1;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_SPI, ads131m0xWrite(&dev, buf, 4));
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xReadRegister
*
* Protocol (24-bit mode):
*   TX word 0 bytes [0..1]: RREG opcode = 0xA000 | (address << 7)
*   RX word 0 bytes [0..1]: register value echoed back by device
* ═══════════════════════════════════════════════════════════════════════════ */

void testReadRegisterRejectsNullParams(void)
{
    ADS131M0X dev;
    uint16_t val;
    initDev(&dev);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xReadRegister(NULL, 0x02, &val));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xReadRegister(&dev, 0x02, NULL));
}

void testReadRegisterRejectsNotInitialized(void)
{
    ADS131M0X dev = {0};
    uint16_t val;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NOT_INITIALIZED,
                    ads131m0xReadRegister(&dev, ADS131M0X_MODE_ADDRESS, &val));
}

void testReadRegisterSendsCorrectOpcode(void)
{
    ADS131M0X dev;
    uint16_t val;
    initDev(&dev);

    mockPrimeRx(0xABCD);
    ads131m0xReadRegister(&dev, ADS131M0X_MODE_ADDRESS, &val);

    /* RREG opcode = 0xA000 | (0x02 << 7) = 0xA100 */
    const uint16_t expected = ADS131M0X_CMD_RREG | ((uint16_t)ADS131M0X_MODE_ADDRESS << 7U);
    const uint16_t actual   = ((uint16_t)mock_tx_history[0][0] << 8U) | mock_tx_history[0][1];
    TEST_ASSERT_EQUAL_HEX16(expected, actual);
}

void testReadRegisterReturnsValueFromRxFrame(void)
{
    ADS131M0X dev;
    uint16_t val = 0;
    initDev(&dev);

    mockPrimeRx(0x1234);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK,
                    ads131m0xReadRegister(&dev, ADS131M0X_MODE_ADDRESS, &val));
    TEST_ASSERT_EQUAL_HEX16(0x1234, val);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xWriteRegister
*
* Protocol (24-bit mode):
*   TX word 0 bytes [0..1]: WREG opcode = 0x6000 | (address << 7)
*   TX word 1 bytes [3..4]: 16-bit value to write
* ═══════════════════════════════════════════════════════════════════════════ */

void testWriteRegisterRejectsNullParams(void)
{
    ADS131M0X dev;
    initDev(&dev);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xWriteRegister(NULL, 0x02, 0x1234));
}

void testWriteRegisterRejectsNotInitialized(void)
{
    ADS131M0X dev = {0};
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NOT_INITIALIZED,
                    ads131m0xWriteRegister(&dev, ADS131M0X_MODE_ADDRESS, 0x1234));
}

void testWriteRegisterRejectsWhenLocked(void)
{
    ADS131M0X dev;
    initDev(&dev);
    dev.is_locked = true;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_LOCKED,
                    ads131m0xWriteRegister(&dev, ADS131M0X_MODE_ADDRESS, 0x1234));
}

void testWriteRegisterSendsCorrectOpcodeAndValue(void)
{
    ADS131M0X dev;
    initDev(&dev);

    ads131m0xWriteRegister(&dev, ADS131M0X_MODE_ADDRESS, 0x0110);

    const uint16_t expected_opcode = ADS131M0X_CMD_WREG | ((uint16_t)ADS131M0X_MODE_ADDRESS << 7U);
    const uint16_t actual_opcode   = ((uint16_t)mock_tx_history[0][0] << 8U) | mock_tx_history[0][1];
    TEST_ASSERT_EQUAL_HEX16(expected_opcode, actual_opcode);

    /* Value is word 1: bytes [3..4] in 24-bit mode (bpw=3, so offset = 1*3 = 3) */
    const uint16_t actual_value = ((uint16_t)mock_tx_history[0][3] << 8U) | mock_tx_history[0][4];
    TEST_ASSERT_EQUAL_HEX16(0x0110, actual_value);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xReadRegisters / ads131m0xWriteRegisters
* ═══════════════════════════════════════════════════════════════════════════ */

void testReadRegistersRejectsInvalidCount(void)
{
    ADS131M0X dev;
    uint16_t vals[2];
    initDev(&dev);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_INVALID_PARAM,
                    ads131m0xReadRegisters(&dev, ADS131M0X_MODE_ADDRESS, vals, 0));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_INVALID_PARAM,
                    ads131m0xReadRegisters(&dev, ADS131M0X_MODE_ADDRESS, vals,
                                            ADS131M0X_CHANNEL_COUNT + 1));
}

void testReadRegistersCountOneReadsFromWordZero(void)
{
    /*
    * For count=1, the device puts the register value in RX word 0 (bytes [0..1]).
    * For count>1, values start at RX word 1 (bytes [3..4]).
    * This asymmetry is device protocol behavior — documented by this test.
    */
    ADS131M0X dev;
    uint16_t val = 0;
    initDev(&dev);

    mockPrimeRx(0x5678);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK,
                    ads131m0xReadRegisters(&dev, ADS131M0X_MODE_ADDRESS, &val, 1));
    TEST_ASSERT_EQUAL_HEX16(0x5678, val);
}

void testWriteRegistersRejectsInvalidCount(void)
{
    ADS131M0X dev;
    uint16_t vals[2] = {0};
    initDev(&dev);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_INVALID_PARAM,
                    ads131m0xWriteRegisters(&dev, ADS131M0X_MODE_ADDRESS, vals, 0));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_INVALID_PARAM,
                    ads131m0xWriteRegisters(&dev, ADS131M0X_MODE_ADDRESS, vals,
                                            ADS131M0X_CHANNEL_COUNT + 1));
}

void testWriteRegistersRejectsWhenLocked(void)
{
    ADS131M0X dev;
    uint16_t vals[2] = {0};
    initDev(&dev);
    dev.is_locked = true;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_LOCKED,
                    ads131m0xWriteRegisters(&dev, ADS131M0X_MODE_ADDRESS, vals, 2));
}

void testWriteRegistersBurstEncodesOpcodeAndValues(void)
{
    /*
    * Burst of 2 at MODE address (0x02):
    *   opcode = WREG | (address << 7) | (count-1) = 0x6000 | 0x0100 | 0x01 = 0x6101
    *   value[0] at bytes [3..4]  (word 1, 24-bit mode)
    *   value[1] at bytes [6..7]  (word 2, 24-bit mode)
    */
    ADS131M0X dev;
    uint16_t vals[2] = {0x0110, 0x0F0E};
    initDev(&dev);

    ads131m0xWriteRegisters(&dev, ADS131M0X_MODE_ADDRESS, vals, 2);

    const uint16_t expected_opcode = ADS131M0X_CMD_WREG
                                    | ((uint16_t)ADS131M0X_MODE_ADDRESS << 7U)
                                    | (2U - 1U);
    const uint16_t actual_opcode = ((uint16_t)mock_tx_history[0][0] << 8U) | mock_tx_history[0][1];
    TEST_ASSERT_EQUAL_HEX16(expected_opcode, actual_opcode);

    const uint16_t val0 = ((uint16_t)mock_tx_history[0][3] << 8U) | mock_tx_history[0][4];
    const uint16_t val1 = ((uint16_t)mock_tx_history[0][6] << 8U) | mock_tx_history[0][7];
    TEST_ASSERT_EQUAL_HEX16(0x0110, val0);
    TEST_ASSERT_EQUAL_HEX16(0x0F0E, val1);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xReadData
*
* Frame layout (24-bit mode, 4 channels = 18 bytes total):
*   Bytes  0– 2 : status word
*   Bytes  3– 5 : channel 0  (24-bit signed, big-endian)
*   Bytes  6– 8 : channel 1
*   Bytes  9–11 : channel 2
*   Bytes 12–14 : channel 3
*   Bytes 15–17 : CRC word
* ═══════════════════════════════════════════════════════════════════════════ */

void testReadDataRejectsNullParams(void)
{
    ADS131M0X dev;
    ADS131M0XData data;
    initDev(&dev);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xReadData(NULL, &data));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xReadData(&dev, NULL));
}

void testReadDataRejectsNotInitialized(void)
{
    ADS131M0X dev = {0};
    ADS131M0XData data;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NOT_INITIALIZED, ads131m0xReadData(&dev, &data));
}

void testReadDataExtractsStatusAndChannelData(void)
{
    /*
    * Sign extension for 24-bit (signExtend shifts up to int32 then right by 8):
    *   ch0: bytes {0x00, 0x01, 0x00} → (0x00<<24 | 0x01<<16 | 0x00<<8) >> 8 = +256
    *   ch1: bytes {0xFF, 0xFF, 0x00} → (0xFF<<24 | 0xFF<<16 | 0x00<<8) >> 8 = -256
    */
    ADS131M0X dev;
    ADS131M0XData data;
    initDev(&dev);

    uint8_t frame[18] = {
        0x05, 0x00, 0x00,   /* status = 0x0500                   */
        0x00, 0x01, 0x00,   /* ch0    = +256                     */
        0xFF, 0xFF, 0x00,   /* ch1    = -256                     */
        0x00, 0x00, 0x00,   /* ch2    = 0                        */
        0x00, 0x00, 0x00,   /* ch3    = 0                        */
        0x00, 0x00, 0x00,   /* CRC (disabled by default, ignored) */
    };

    mockPrimeRxFrame(frame, sizeof(frame));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xReadData(&dev, &data));

    TEST_ASSERT_EQUAL_HEX16(0x0500, data.status);
    TEST_ASSERT_EQUAL_INT32( 256,   data.channel_data[0]);
    TEST_ASSERT_EQUAL_INT32(-256,   data.channel_data[1]);
    TEST_ASSERT_EQUAL_INT32(   0,   data.channel_data[2]);
    TEST_ASSERT_EQUAL_INT32(   0,   data.channel_data[3]);
}

void testReadDataCrcDisabledSetsCrcValidTrue(void)
{
    /*
    * CRC disabled → checkCRC() short-circuits and returns OK.
    * crc_valid = (err == OK) = true.
    * This documents that crc_valid means "no CRC error detected",
    * not "CRC was actually verified" — an important semantic distinction.
    */
    ADS131M0X dev;
    ADS131M0XData data;
    initDev(&dev);

    mockPrimeRx(0x0000);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xReadData(&dev, &data));
    TEST_ASSERT_TRUE(data.crc_valid);
}

void testReadDataCrcMismatchSetsInvalidFlagButReturnsOk(void)
{
    /*
    * CRC enabled, mismatched → crc_valid=false, but return is still OK.
    * The caller receives the data and decides what to do with invalid CRC.
    * An all-zeros frame will not produce CRC=0x0000 for polynomial 0x1021,
    * so bytes [15..16]=0x00 guarantee a mismatch.
    */
    ADS131M0X dev;
    ADS131M0XData data;
    initDev(&dev);
    dev.crc.is_enabled = true;
    dev.crc.type       = ADS131M0X_CRC_POLYNOMIAL_CCITT;

    mockPrimeRx(0x0000);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xReadData(&dev, &data));
    TEST_ASSERT_FALSE(data.crc_valid);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xStandby / ads131m0xWakeup
*
* Both use sendCommand internally: one TX frame, one RX frame, response check.
* ═══════════════════════════════════════════════════════════════════════════ */

void testStandbyRejectsNullAndNotInitialized(void)
{
    ADS131M0X dev = {0};
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM,       ads131m0xStandby(NULL));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NOT_INITIALIZED,  ads131m0xStandby(&dev));
}

void testStandbyRejectsWrongResponse(void)
{
    ADS131M0X dev;
    initDev(&dev);
    mockPrimeRx(0xDEAD);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_SPI, ads131m0xStandby(&dev));
}

void testStandbySucceeds(void)
{
    ADS131M0X dev;
    initDev(&dev);
    mockPrimeRx(ADS131M0X_RESP_STANDBY);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xStandby(&dev));
}

void testWakeupRejectsNullAndNotInitialized(void)
{
    ADS131M0X dev = {0};
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM,       ads131m0xWakeup(NULL));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NOT_INITIALIZED,  ads131m0xWakeup(&dev));
}

void testWakeupSucceeds(void)
{
    ADS131M0X dev;
    initDev(&dev);
    mockPrimeRx(ADS131M0X_RESP_WAKEUP);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xWakeup(&dev));
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xLock / ads131m0xUnlock
* ═══════════════════════════════════════════════════════════════════════════ */

void testLockRejectsNullAndNotInitialized(void)
{
    ADS131M0X dev = {0};
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM,       ads131m0xLock(NULL));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NOT_INITIALIZED,  ads131m0xLock(&dev));
}

void testLockRejectsAlreadyLocked(void)
{
    ADS131M0X dev;
    initDev(&dev);
    dev.is_locked = true;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_LOCKED, ads131m0xLock(&dev));
}

void testLockSucceedsSetsLockedFlag(void)
{
    ADS131M0X dev;
    initDev(&dev);
    mockPrimeRx(ADS131M0X_RESP_LOCK);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xLock(&dev));
    TEST_ASSERT_TRUE(dev.is_locked);
}

void testUnlockSucceedsClearsLockedFlag(void)
{
    ADS131M0X dev;
    initDev(&dev);
    mockPrimeRx(ADS131M0X_RESP_UNLOCK);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xUnlock(&dev));
    TEST_ASSERT_FALSE(dev.is_locked);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xReset
* ═══════════════════════════════════════════════════════════════════════════ */

void testResetRejectsNullAndGuards(void)
{
    ADS131M0X dev;
    initDev(&dev);
    dev.is_locked = true;

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xReset(NULL));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_LOCKED,     ads131m0xReset(&dev));
}

void testResetSucceedsAndRestoresDefaults(void)
{
    /*
    * Set non-default state first to prove reset actually restores them.
    * After reset: word_length=24-bit, is_locked=false, crc.is_enabled=false.
    */
    ADS131M0X dev;
    initDev(&dev);
    dev.word_length    = ADS131M0X_WLENGTH_32_BIT_ZERO;
    dev.crc.is_enabled = true;

    mockPrimeRx(ADS131M0X_RESP_RESET_OK);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xReset(&dev));
    TEST_ASSERT_EQUAL(ADS131M0X_WLENGTH_24_BIT, dev.word_length);
    TEST_ASSERT_FALSE(dev.crc.is_enabled);
    TEST_ASSERT_FALSE(dev.is_locked);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xConfigure
*
* Makes 3 burst writes: [MODE+CLOCK], [CFG], [THRSHLD_MSB+THRSHLD_LSB].
* tx_history[0] = first WREG burst (MODE address, count=2).
* ═══════════════════════════════════════════════════════════════════════════ */

void testConfigureRejectsNullAndGuards(void)
{
    ADS131M0X dev;
    ADS131M0XConfig cfg = {0};
    initDev(&dev);
    dev.is_locked = true;

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xConfigure(NULL, &cfg));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xConfigure(&dev,  NULL));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_LOCKED,     ads131m0xConfigure(&dev,  &cfg));
}

void testConfigureUpdatesDevState(void)
{
    /*
    * dev->word_length, crc.is_enabled, and crc.type are used by every
    * subsequent driver call to determine frame sizes and CRC behavior.
    * Verify configure writes them correctly from the config struct.
    */
    ADS131M0X dev;
    initDev(&dev);

    const ADS131M0XConfig cfg = {
        .word_length        = ADS131M0X_WLENGTH_32_BIT_ZERO,
        .crc.output_enabled = true,
        .crc.polynomial     = ADS131M0X_CRC_POLYNOMIAL_ANSI,
    };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigure(&dev, &cfg));
    TEST_ASSERT_EQUAL(ADS131M0X_WLENGTH_32_BIT_ZERO,   dev.word_length);
    TEST_ASSERT_TRUE(dev.crc.is_enabled);
    TEST_ASSERT_EQUAL(ADS131M0X_CRC_POLYNOMIAL_ANSI,   dev.crc.type);
}

void testConfigurePacksModeAndClockRegisters(void)
{
    /*
    * Config used here:
    *   word_length = 24-bit (0x01)  → MODE bits [9:8] = 0x0100
    *   spi_timeout = true           → MODE bit  4     = 0x0010
    *   Expected MODE  = 0x0110
    *
    *   4 channels enabled           → CLOCK bits [11:8] = 0x0F00
    *   OSR = 1024  (0x03)           → CLOCK bits [4:2]  = 0x000C
    *   PWR = HIGH_RES (0x02)        → CLOCK bits [1:0]  = 0x0002
    *   Expected CLOCK = 0x0F0E
    *
    * tx_history[0] = WREG burst of 2 at MODE address:
    *   opcode  at bytes [0..1]
    *   MODE    at bytes [3..4]  (word 1, 24-bit mode)
    *   CLOCK   at bytes [6..7]  (word 2, 24-bit mode)
    */
    ADS131M0X dev;
    initDev(&dev);

    const ADS131M0XConfig cfg = {
        .word_length         = ADS131M0X_WLENGTH_24_BIT,
        .spi_timeout_enabled = true,
        .oversampling_ratio  = ADS131M0X_OSR_1024,
        .power_mode          = ADS131M0X_POWER_HIGH_RES,
    };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigure(&dev, &cfg));

    const uint16_t expected_opcode = ADS131M0X_CMD_WREG
                                    | ((uint16_t)ADS131M0X_MODE_ADDRESS << 7U)
                                    | 1U;   /* count-1 = 1 */
    const uint16_t actual_opcode = ((uint16_t)mock_tx_history[0][0] << 8U) | mock_tx_history[0][1];
    const uint16_t actual_mode   = ((uint16_t)mock_tx_history[0][3] << 8U) | mock_tx_history[0][4];
    const uint16_t actual_clock  = ((uint16_t)mock_tx_history[0][6] << 8U) | mock_tx_history[0][7];

    TEST_ASSERT_EQUAL_HEX16(expected_opcode, actual_opcode);
    TEST_ASSERT_EQUAL_HEX16(0x0110,          actual_mode);
    TEST_ASSERT_EQUAL_HEX16(0x0F0E,          actual_clock);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xConfigureChannel
*
* Sequence of SPI calls:
*   tx[0] + rx[0] : RREG CLOCK  (read-modify-write the channel enable bit)
*   tx[1]         : WREG CLOCK  (write back modified value)
*   tx[2]         : WREG CHn_CFG
*   tx[3]         : WREG CHn_OCAL/GCAL burst (4 regs)
* ═══════════════════════════════════════════════════════════════════════════ */

void testConfigureChannelRejectsNullAndGuards(void)
{
    ADS131M0X dev;
    ADS131M0XChannelConfig ch_cfg = {0};
    initDev(&dev);
    dev.is_locked = true;

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xConfigureChannel(NULL, 0, &ch_cfg));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xConfigureChannel(&dev, 0, NULL));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_LOCKED,     ads131m0xConfigureChannel(&dev, 0, &ch_cfg));
}

void testConfigureChannelRejectsInvalidChannel(void)
{
    ADS131M0X dev;
    ADS131M0XChannelConfig ch_cfg = {0};
    initDev(&dev);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_INVALID_CHANNEL,
                    ads131m0xConfigureChannel(&dev, ADS131M0X_CHANNEL_COUNT, &ch_cfg));
}

void testConfigureChannelSetsEnableBitInClock(void)
{
    /*
    * Prime the CLOCK read with the power-on default (0x0F0E = all 4 channels on).
    * Configure channel 0 as enabled.
    * tx_history[1] = WREG CLOCK — verify CH0_EN bit (bit 8) is still set.
    */
    ADS131M0X dev;
    initDev(&dev);

    mockPrimeRx(ADS131M0X_CLOCK_DEFAULT);

    const ADS131M0XChannelConfig ch_cfg = {
        .is_enabled = true,
        .gain       = ADS131M0X_GAIN_1,
        .mux        = ADS131M0X_MUX_NORMAL,
    };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigureChannel(&dev, 0, &ch_cfg));

    /* WREG CLOCK is tx_history[1]; the value is at bytes [3..4] */
    const uint16_t written_clock = ((uint16_t)mock_tx_history[1][3] << 8U) | mock_tx_history[1][4];
    TEST_ASSERT_BITS_HIGH(1U << ADS131M0X_CLOCK_CH0_EN_SHIFT, written_clock);
}

void testConfigureChannelClearsEnableBitWhenDisabled(void)
{
    ADS131M0X dev;
    initDev(&dev);

    mockPrimeRx(ADS131M0X_CLOCK_DEFAULT);  /* all 4 channels currently enabled */

    const ADS131M0XChannelConfig ch_cfg = { .is_enabled = false };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigureChannel(&dev, 0, &ch_cfg));

    const uint16_t written_clock = ((uint16_t)mock_tx_history[1][3] << 8U) | mock_tx_history[1][4];
    TEST_ASSERT_BITS_LOW(1U << ADS131M0X_CLOCK_CH0_EN_SHIFT, written_clock);
}


/* ═══════════════════════════════════════════════════════════════════════════
* main
* ═══════════════════════════════════════════════════════════════════════════ */

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(testErrorToStringReturnsCorrectStrings);

    RUN_TEST(testConvertToMicrovoltsGainAndScale);

    RUN_TEST(testInitRejectsNullDev);
    RUN_TEST(testInitRejectsNullHal);
    RUN_TEST(testInitRejectsNullHalFunctionPointers);
    RUN_TEST(testInitReturnsSpiErrorOnReadFailure);
    RUN_TEST(testInitRejectsWrongResponseWord);
    RUN_TEST(testInitSucceedsAndSetsInitializedFlag);

    RUN_TEST(testDeinitRejectsNullDev);
    RUN_TEST(testDeinitClearsInitializedFlagAndNullsHal);

    RUN_TEST(testRawReadRejectsNullParams);
    RUN_TEST(testRawWriteRejectsNullParams);
    RUN_TEST(testRawReadReturnsSpiErrorOnFailure);
    RUN_TEST(testRawWriteReturnsSpiErrorOnFailure);

    RUN_TEST(testReadRegisterRejectsNullParams);
    RUN_TEST(testReadRegisterRejectsNotInitialized);
    RUN_TEST(testReadRegisterSendsCorrectOpcode);
    RUN_TEST(testReadRegisterReturnsValueFromRxFrame);

    RUN_TEST(testWriteRegisterRejectsNullParams);
    RUN_TEST(testWriteRegisterRejectsNotInitialized);
    RUN_TEST(testWriteRegisterRejectsWhenLocked);
    RUN_TEST(testWriteRegisterSendsCorrectOpcodeAndValue);

    RUN_TEST(testReadRegistersRejectsInvalidCount);
    RUN_TEST(testReadRegistersCountOneReadsFromWordZero);
    RUN_TEST(testWriteRegistersRejectsInvalidCount);
    RUN_TEST(testWriteRegistersRejectsWhenLocked);
    RUN_TEST(testWriteRegistersBurstEncodesOpcodeAndValues);

    RUN_TEST(testReadDataRejectsNullParams);
    RUN_TEST(testReadDataRejectsNotInitialized);
    RUN_TEST(testReadDataExtractsStatusAndChannelData);
    RUN_TEST(testReadDataCrcDisabledSetsCrcValidTrue);
    RUN_TEST(testReadDataCrcMismatchSetsInvalidFlagButReturnsOk);

    RUN_TEST(testStandbyRejectsNullAndNotInitialized);
    RUN_TEST(testStandbyRejectsWrongResponse);
    RUN_TEST(testStandbySucceeds);

    RUN_TEST(testWakeupRejectsNullAndNotInitialized);
    RUN_TEST(testWakeupSucceeds);

    RUN_TEST(testLockRejectsNullAndNotInitialized);
    RUN_TEST(testLockRejectsAlreadyLocked);
    RUN_TEST(testLockSucceedsSetsLockedFlag);
    RUN_TEST(testUnlockSucceedsClearsLockedFlag);

    RUN_TEST(testResetRejectsNullAndGuards);
    RUN_TEST(testResetSucceedsAndRestoresDefaults);

    RUN_TEST(testConfigureRejectsNullAndGuards);
    RUN_TEST(testConfigureUpdatesDevState);
    RUN_TEST(testConfigurePacksModeAndClockRegisters);

    RUN_TEST(testConfigureChannelRejectsNullAndGuards);
    RUN_TEST(testConfigureChannelRejectsInvalidChannel);
    RUN_TEST(testConfigureChannelSetsEnableBitInClock);
    RUN_TEST(testConfigureChannelClearsEnableBitWhenDisabled);

    return UNITY_END();
}