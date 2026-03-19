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
#define MOCK_RX_QUEUE_DEPTH    8
#define MOCK_TX_HISTORY_DEPTH  8

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
    mockPrimeRx(0x0000); /* GAIN default */

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
    mockPrimeRx(0x0000);

    const ADS131M0XChannelConfig ch_cfg = { .is_enabled = false };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigureChannel(&dev, 0, &ch_cfg));

    const uint16_t written_clock = ((uint16_t)mock_tx_history[1][3] << 8U) | mock_tx_history[1][4];
    TEST_ASSERT_BITS_LOW(1U << ADS131M0X_CLOCK_CH0_EN_SHIFT, written_clock);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xReadChipId
* ═══════════════════════════════════════════════════════════════════════════ */

void testReadChipIdRejectsNullParams(void)
{
    ADS131M0X dev;
    uint16_t id;
    initDev(&dev);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xReadChipId(NULL, &id));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM, ads131m0xReadChipId(&dev, NULL));
}

void testReadChipIdRejectsNotInitialized(void)
{
    ADS131M0X dev = {0};
    uint16_t id;
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NOT_INITIALIZED, ads131m0xReadChipId(&dev, &id));
}

void testReadChipIdReturnsIdFromDevice(void)
{
    ADS131M0X dev;
    uint16_t id = 0;
    initDev(&dev);

    mockPrimeRx(ADS131M0X_ID_DEFAULT);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xReadChipId(&dev, &id));
    TEST_ASSERT_EQUAL_HEX16(ADS131M0X_ID_DEFAULT, id);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xReadRegisters — multi-register burst (count > 1)
*
* Per datasheet §8.5.1.10.7.2, when nnn_nnnn > 0 the response starts with
* an RREG ack word at word[0], then register data at words[1..N].
* ═══════════════════════════════════════════════════════════════════════════ */

void testReadRegistersCountTwoReadsFromWordsOneAndTwo(void)
{
    /*
    * For count=2 at MODE address (0x02):
    *   RX word 0 bytes [0..1] = RREG ack (ignored by extraction)
    *   RX word 1 bytes [3..4] = MODE register value
    *   RX word 2 bytes [6..7] = CLOCK register value
    */
    ADS131M0X dev;
    uint16_t vals[2] = {0};
    initDev(&dev);

    /* Build a full RX frame: ack at word 0, data at words 1 and 2 */
    uint8_t frame[18] = {0};
    /* word 0: RREG ack (don't care for extraction) */
    frame[0] = 0xE1; frame[1] = 0x01;
    /* word 1: MODE value = 0x0510 */
    frame[3] = 0x05; frame[4] = 0x10;
    /* word 2: CLOCK value = 0x0F0E */
    frame[6] = 0x0F; frame[7] = 0x0E;

    mockPrimeRxFrame(frame, sizeof(frame));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK,
                    ads131m0xReadRegisters(&dev, ADS131M0X_MODE_ADDRESS, vals, 2));
    TEST_ASSERT_EQUAL_HEX16(0x0510, vals[0]);
    TEST_ASSERT_EQUAL_HEX16(0x0F0E, vals[1]);
}

void testReadRegistersBurstSendsCorrectOpcodeWithCount(void)
{
    /*
    * Burst of 3 at CLOCK address (0x03):
    *   opcode = RREG | (address << 7) | (count-1)
    *          = 0xA000 | 0x0180 | 0x02 = 0xA182
    */
    ADS131M0X dev;
    uint16_t vals[3] = {0};
    initDev(&dev);

    mockPrimeRx(0x0000);
    ads131m0xReadRegisters(&dev, ADS131M0X_CLOCK_ADDRESS, vals, 3);

    const uint16_t expected_opcode = ADS131M0X_CMD_RREG
                                    | ((uint16_t)ADS131M0X_CLOCK_ADDRESS << 7U)
                                    | (3U - 1U);
    const uint16_t actual_opcode = ((uint16_t)mock_tx_history[0][0] << 8U) | mock_tx_history[0][1];
    TEST_ASSERT_EQUAL_HEX16(expected_opcode, actual_opcode);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xUnlock — guard clause coverage
* ═══════════════════════════════════════════════════════════════════════════ */

void testUnlockRejectsNullAndNotInitialized(void)
{
    ADS131M0X dev = {0};
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NULL_PARAM,       ads131m0xUnlock(NULL));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NOT_INITIALIZED,  ads131m0xUnlock(&dev));
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xReset — SPI failure and wrong response
* ═══════════════════════════════════════════════════════════════════════════ */

void testResetRejectsNotInitialized(void)
{
    ADS131M0X dev = {0};
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_NOT_INITIALIZED, ads131m0xReset(&dev));
}

void testResetRejectsWrongResponse(void)
{
    ADS131M0X dev;
    initDev(&dev);
    mockPrimeRx(0xDEAD);
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_SPI, ads131m0xReset(&dev));
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xConfigure — CFG and THRSHLD register packing
*
* tx_history layout (24-bit mode, 3 SPI writes from configure):
*   tx[0] = WREG burst MODE+CLOCK
*   tx[1] = WREG CFG
*   tx[2] = WREG burst THRSHLD_MSB+THRSHLD_LSB
* ═══════════════════════════════════════════════════════════════════════════ */

void testConfigurePacksCfgRegister(void)
{
    /*
    * CFG register fields:
    *   GC_DLY  = 0x05 (64 clocks)         → bits [12:9] = 0x0A00
    *   GC_EN   = true                      → bit 8      = 0x0100
    *   CD_ALLCH= true                      → bit 7      = 0x0080
    *   CD_NUM  = 0x03 (8 consecutive)      → bits [6:4]  = 0x0030
    *   CD_LEN  = 0x02 (512 cycles)         → bits [3:1]  = 0x0004
    *   CD_EN   = true                      → bit 0      = 0x0001
    *   Expected CFG = 0x0BB5
    */
    ADS131M0X dev;
    initDev(&dev);

    const ADS131M0XConfig cfg = {
        .word_length         = ADS131M0X_WLENGTH_24_BIT,
        .global_chop.is_enabled = true,
        .global_chop.delay      = ADS131M0X_GC_DLY_64,
        .current_detect.is_enabled  = true,
        .current_detect.all_channels = true,
        .current_detect.num         = ADS131M0X_CD_NUM_8,
        .current_detect.len         = ADS131M0X_CD_LEN_4,
    };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigure(&dev, &cfg));

    /* tx_history[1] = WREG CFG; value is at bytes [3..4] */
    const uint16_t actual_cfg = ((uint16_t)mock_tx_history[1][3] << 8U) | mock_tx_history[1][4];

    /* Verify individual fields */
    TEST_ASSERT_BITS_HIGH(ADS131M0X_CFG_GC_EN_MASK,    actual_cfg);
    TEST_ASSERT_BITS_HIGH(ADS131M0X_CFG_CD_EN_MASK,    actual_cfg);
    TEST_ASSERT_BITS_HIGH(ADS131M0X_CFG_CD_ALLCH_MASK, actual_cfg);

    const uint16_t gc_dly = (actual_cfg & ADS131M0X_CFG_GC_DLY_MASK) >> ADS131M0X_CFG_GC_DLY_SHIFT;
    TEST_ASSERT_EQUAL_HEX16(ADS131M0X_GC_DLY_64, gc_dly);

    const uint16_t cd_num = (actual_cfg & ADS131M0X_CFG_CD_NUM_MASK) >> ADS131M0X_CFG_CD_NUM_SHIFT;
    TEST_ASSERT_EQUAL_HEX16(ADS131M0X_CD_NUM_8, cd_num);

    const uint16_t cd_len = (actual_cfg & ADS131M0X_CFG_CD_LEN_MASK) >> ADS131M0X_CFG_CD_LEN_SHIFT;
    TEST_ASSERT_EQUAL_HEX16(ADS131M0X_CD_LEN_4, cd_len);
}

void testConfigurePacksThresholdAndDcBlock(void)
{
    /*
    * threshold = 0x123456 (24-bit value)
    *   THRSHLD_MSB = (threshold >> 8) & 0xFFFF = 0x1234
    *   THRSHLD_LSB[15:8] = (threshold & 0xFF) << 8 = 0x5600
    *
    * dc_block = ADS131M0X_DCBLOCK_1_256 = 0x07
    *   THRSHLD_LSB[3:0] = 0x07
    *   Combined THRSHLD_LSB = 0x5607
    *
    * tx_history[2] = WREG burst THRSHLD_MSB+THRSHLD_LSB:
    *   THRSHLD_MSB at bytes [3..4]
    *   THRSHLD_LSB at bytes [6..7]
    */
    ADS131M0X dev;
    initDev(&dev);

    const ADS131M0XConfig cfg = {
        .word_length            = ADS131M0X_WLENGTH_24_BIT,
        .current_detect.threshold = 0x123456,
        .dc_block               = ADS131M0X_DCBLOCK_1_256,
    };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigure(&dev, &cfg));

    const uint16_t actual_msb = ((uint16_t)mock_tx_history[2][3] << 8U) | mock_tx_history[2][4];
    const uint16_t actual_lsb = ((uint16_t)mock_tx_history[2][6] << 8U) | mock_tx_history[2][7];

    TEST_ASSERT_EQUAL_HEX16(0x1234, actual_msb);
    TEST_ASSERT_EQUAL_HEX16(0x5607, actual_lsb);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xConfigureChannel — CHn_CFG, OCAL, GCAL packing
*
* SPI call sequence (24-bit mode):
*   tx[0] = WREG RREG CLOCK (spiWrite for the RREG command)
*   rx[0] = CLOCK value (spiRead for the RREG response)
*   tx[1] = WREG CLOCK (write back modified)
*   tx[2] = WREG RREG GAIN (spiWrite for the RREG command)
*   rx[1] = GAIN value (spiRead for the RREG response)
*   tx[3] = WREG GAIN (write back modified)
*   tx[4] = WREG CHn_CFG
*   tx[5] = WREG burst CHn_OCAL_MSB/LSB + CHn_GCAL_MSB/LSB
* ═══════════════════════════════════════════════════════════════════════════ */

void testConfigureChannelPacksGainRegister(void)
{
    /*
    * Channel 1 -> GAIN1 (addr 0x04), shift 4
    * Set gain to ADS131M0X_GAIN_32 (0x05)
    * Expected GAIN1 value = (0x05 << 4) = 0x0050
    * tx_history[3] = WREG GAIN1
    */
    ADS131M0X dev;
    initDev(&dev);

    mockPrimeRx(ADS131M0X_CLOCK_DEFAULT);
    mockPrimeRx(0x0000); /* GAIN1 default */

    const ADS131M0XChannelConfig ch_cfg = {
        .is_enabled = true,
        .gain       = ADS131M0X_GAIN_32,
        .mux        = ADS131M0X_MUX_NORMAL,
    };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigureChannel(&dev, 1, &ch_cfg));

    const uint16_t expected_opcode = ADS131M0X_CMD_WREG | ((uint16_t)ADS131M0X_GAIN1_ADDRESS << 7U);
    const uint16_t actual_opcode   = ((uint16_t)mock_tx_history[3][0] << 8U) | mock_tx_history[3][1];
    TEST_ASSERT_EQUAL_HEX16(expected_opcode, actual_opcode);

    const uint16_t actual_gain     = ((uint16_t)mock_tx_history[3][3] << 8U) | mock_tx_history[3][4];
    TEST_ASSERT_EQUAL_HEX16(0x0050, actual_gain);
}

void testConfigureChannelPacksPhaseAndMux(void)
{
    /*
    * CHn_CFG fields:
    *   PHASE[9:0]  = 42 → bits [15:6] = (42 << 6) = 0x0A80
    *   DCBLK_DIS   = true → bit 2 = 0x0004
    *   MUX[1:0]    = SHORTED (0x01) → bits [1:0] = 0x0001
    *   Expected CHn_CFG = 0x0A85
    *
    * tx_history[4] = WREG CHn_CFG; value at bytes [3..4]
    */
    ADS131M0X dev;
    initDev(&dev);

    mockPrimeRx(ADS131M0X_CLOCK_DEFAULT);
    mockPrimeRx(0x0000);

    const ADS131M0XChannelConfig ch_cfg = {
        .is_enabled         = true,
        .gain               = ADS131M0X_GAIN_1,
        .mux                = ADS131M0X_MUX_SHORTED,
        .phase_delay_cycles = 42,
        .dc_block_disabled  = true,
        .offset_cal         = 0,
        .gain_cal           = 0x800000,
    };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigureChannel(&dev, 0, &ch_cfg));

    const uint16_t actual_chn_cfg = ((uint16_t)mock_tx_history[4][3] << 8U) | mock_tx_history[4][4];

    const uint16_t phase = (actual_chn_cfg & ADS131M0X_CHn_CFG_PHASE_MASK) >> ADS131M0X_CHn_CFG_PHASE_SHIFT;
    TEST_ASSERT_EQUAL(42, phase);

    TEST_ASSERT_BITS_HIGH(ADS131M0X_CHn_CFG_DCBLK_DIS_MASK, actual_chn_cfg);

    const uint16_t mux = (actual_chn_cfg & ADS131M0X_CHn_CFG_MUX_MASK) >> ADS131M0X_CHn_CFG_MUX_SHIFT;
    TEST_ASSERT_EQUAL(ADS131M0X_MUX_SHORTED, mux);
}

void testConfigureChannelPacksCalibrationRegisters(void)
{
    /*
    * offset_cal = 0x123456 → OCAL_MSB = 0x1234, OCAL_LSB = 0x5600
    * gain_cal   = 0xABCDEF → GCAL_MSB = 0xABCD, GCAL_LSB = 0xEF00
    *
    * tx_history[5] = WREG burst of 4 at CHn_OCAL_MSB:
    *   OCAL_MSB at bytes [3..4]  (word 1, 24-bit mode)
    *   OCAL_LSB at bytes [6..7]  (word 2)
    *   GCAL_MSB at bytes [9..10] (word 3)
    *   GCAL_LSB at bytes [12..13](word 4)
    */
    ADS131M0X dev;
    initDev(&dev);

    mockPrimeRx(ADS131M0X_CLOCK_DEFAULT);
    mockPrimeRx(0x0000);

    const ADS131M0XChannelConfig ch_cfg = {
        .is_enabled  = true,
        .gain        = ADS131M0X_GAIN_1,
        .mux         = ADS131M0X_MUX_NORMAL,
        .offset_cal  = 0x123456,
        .gain_cal    = 0xABCDEF,
    };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigureChannel(&dev, 1, &ch_cfg));

    const uint16_t ocal_msb = ((uint16_t)mock_tx_history[5][3]  << 8U) | mock_tx_history[5][4];
    const uint16_t ocal_lsb = ((uint16_t)mock_tx_history[5][6]  << 8U) | mock_tx_history[5][7];
    const uint16_t gcal_msb = ((uint16_t)mock_tx_history[5][9]  << 8U) | mock_tx_history[5][10];
    const uint16_t gcal_lsb = ((uint16_t)mock_tx_history[5][12] << 8U) | mock_tx_history[5][13];

    TEST_ASSERT_EQUAL_HEX16(0x1234, ocal_msb);
    TEST_ASSERT_EQUAL_HEX16(0x5600, ocal_lsb);
    TEST_ASSERT_EQUAL_HEX16(0xABCD, gcal_msb);
    TEST_ASSERT_EQUAL_HEX16(0xEF00, gcal_lsb);
}

void testConfigureChannelChannel3UsesCorrectBaseAddress(void)
{
    /*
    * Channel 3 base = 0x09 + 3*5 = 0x18
    * tx_history[4] = WREG CHn_CFG; opcode at bytes [0..1]
    *   opcode = WREG | (0x18 << 7) = 0x6000 | 0x0C00 = 0x6C00
    */
    ADS131M0X dev;
    initDev(&dev);

    mockPrimeRx(ADS131M0X_CLOCK_DEFAULT);
    mockPrimeRx(0x0000);

    const ADS131M0XChannelConfig ch_cfg = {
        .is_enabled = true,
        .gain       = ADS131M0X_GAIN_1,
        .mux        = ADS131M0X_MUX_NORMAL,
        .gain_cal   = 0x800000,
    };

    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xConfigureChannel(&dev, 3, &ch_cfg));

    const uint16_t expected_opcode = ADS131M0X_CMD_WREG | ((uint16_t)ADS131M0X_CH3_CFG_ADDRESS << 7U);
    const uint16_t actual_opcode   = ((uint16_t)mock_tx_history[4][0] << 8U) | mock_tx_history[4][1];
    TEST_ASSERT_EQUAL_HEX16(expected_opcode, actual_opcode);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xReadData — 16-bit word length
*
* Frame layout (16-bit mode, 4 channels = 12 bytes total):
*   Bytes  0– 1 : status word
*   Bytes  2– 3 : channel 0  (16-bit signed, big-endian)
*   Bytes  4– 5 : channel 1
*   Bytes  6– 7 : channel 2
*   Bytes  8– 9 : channel 3
*   Bytes 10–11 : CRC word
* ═══════════════════════════════════════════════════════════════════════════ */

void testReadData16BitWordExtractsChannelData(void)
{
    /*
    * In 16-bit mode, signExtend takes 2 bytes:
    *   ch0: {0x00, 0x80} → (0x00<<24 | 0x80<<16) >> 16 = +128
    *   ch1: {0xFF, 0x80} → (0xFF<<24 | 0x80<<16) >> 16 = -128
    */
    ADS131M0X dev;
    ADS131M0XData data;
    initDev(&dev);
    dev.word_length = ADS131M0X_WLENGTH_16_BIT;

    uint8_t frame[12] = {
        0x05, 0x00,   /* status = 0x0500            */
        0x00, 0x80,   /* ch0    = +128              */
        0xFF, 0x80,   /* ch1    = -128              */
        0x00, 0x00,   /* ch2    = 0                 */
        0x00, 0x00,   /* ch3    = 0                 */
        0x00, 0x00,   /* CRC (disabled, ignored)    */
    };

    mockPrimeRxFrame(frame, sizeof(frame));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xReadData(&dev, &data));

    TEST_ASSERT_EQUAL_HEX16(0x0500, data.status);
    TEST_ASSERT_EQUAL_INT32( 128,   data.channel_data[0]);
    TEST_ASSERT_EQUAL_INT32(-128,   data.channel_data[1]);
    TEST_ASSERT_EQUAL_INT32(   0,   data.channel_data[2]);
    TEST_ASSERT_EQUAL_INT32(   0,   data.channel_data[3]);
}

void testReadDataFullScaleSignExtension(void)
{
    /*
    * 24-bit mode full-scale values:
    *   ch0: {0x7F, 0xFF, 0xFF} → +8388607 (positive full-scale)
    *   ch1: {0x80, 0x00, 0x00} → -8388608 (negative full-scale)
    */
    ADS131M0X dev;
    ADS131M0XData data;
    initDev(&dev);

    uint8_t frame[18] = {
        0x00, 0x0F, 0x00,   /* status                     */
        0x7F, 0xFF, 0xFF,   /* ch0 = +full-scale          */
        0x80, 0x00, 0x00,   /* ch1 = -full-scale          */
        0x00, 0x00, 0x01,   /* ch2 = +1 (with LSB set)    */
        0xFF, 0xFF, 0xFF,   /* ch3 = -1                   */
        0x00, 0x00, 0x00,   /* CRC (disabled)             */
    };

    mockPrimeRxFrame(frame, sizeof(frame));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xReadData(&dev, &data));

    TEST_ASSERT_EQUAL_INT32( 8388607, data.channel_data[0]);
    TEST_ASSERT_EQUAL_INT32(-8388608, data.channel_data[1]);
    TEST_ASSERT_EQUAL_INT32(       1, data.channel_data[2]);
    TEST_ASSERT_EQUAL_INT32(      -1, data.channel_data[3]);
}

void testReadData32BitSignExtendedWord(void)
{
    /*
    * In 32-bit sign-extended mode:
    * Frame is 24 bytes total (6 words * 4 bytes).
    * signExtend parses all 4 bytes directly as a 32-bit int.
    * ch0: bytes {0x00, 0x00, 0x00, 0x80} -> +128
    * ch1: bytes {0xFF, 0xFF, 0xFF, 0x80} -> -128
    */
    ADS131M0X dev;
    ADS131M0XData data;
    initDev(&dev);
    dev.word_length = ADS131M0X_WLENGTH_32_BIT_SIGN;

    uint8_t frame[24] = {
        0x05, 0x00, 0x00, 0x00, /* status = 0x0500 */
        0x00, 0x00, 0x00, 0x80, /* ch0 = +128 */
        0xFF, 0xFF, 0xFF, 0x80, /* ch1 = -128 */
        0x00, 0x00, 0x00, 0x00, /* ch2 = 0 */
        0x00, 0x00, 0x00, 0x00, /* ch3 = 0 */
        0x00, 0x00, 0x00, 0x00, /* CRC = 0 */
    };

    mockPrimeRxFrame(frame, sizeof(frame));
    TEST_ASSERT_EQUAL(ADS131M0X_ERROR_OK, ads131m0xReadData(&dev, &data));

    TEST_ASSERT_EQUAL_HEX16(0x0500, data.status);
    TEST_ASSERT_EQUAL_INT32( 128,   data.channel_data[0]);
    TEST_ASSERT_EQUAL_INT32(-128,   data.channel_data[1]);
    TEST_ASSERT_EQUAL_INT32(   0,   data.channel_data[2]);
    TEST_ASSERT_EQUAL_INT32(   0,   data.channel_data[3]);
}


/* ═══════════════════════════════════════════════════════════════════════════
* ads131m0xConvertToMicrovolts — additional edge cases
* ═══════════════════════════════════════════════════════════════════════════ */

void testConvertToMicrovoltsAllGainValues(void)
{
    /*
    * Verify gain scaling for every supported PGA gain.
    * With raw_code = 2^23/2 = 4194304, gain=1 → 600000 uV (half scale).
    * Each doubling of gain halves the result.
    *
    * | raw_code | gain | expected_uv | description           |
    */
    struct { int32_t raw; ADS131M0XGain gain; int64_t uv; const char* desc; } cases[] =
    {
        { 4194304, ADS131M0X_GAIN_1,   600000LL, "Half-scale, gain 1"   },
        { 4194304, ADS131M0X_GAIN_2,   300000LL, "Half-scale, gain 2"   },
        { 4194304, ADS131M0X_GAIN_4,   150000LL, "Half-scale, gain 4"   },
        { 4194304, ADS131M0X_GAIN_8,    75000LL, "Half-scale, gain 8"   },
        { 4194304, ADS131M0X_GAIN_16,   37500LL, "Half-scale, gain 16"  },
        { 4194304, ADS131M0X_GAIN_32,   18750LL, "Half-scale, gain 32"  },
        { 4194304, ADS131M0X_GAIN_64,    9375LL, "Half-scale, gain 64"  },
        { 4194304, ADS131M0X_GAIN_128,   4687LL, "Half-scale, gain 128" },
    };

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        int64_t result = ads131m0xConvertToMicrovolts(cases[i].raw, cases[i].gain);
        TEST_ASSERT_EQUAL_INT64_MESSAGE(cases[i].uv, result, cases[i].desc);
    }
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
    RUN_TEST(testResetRejectsNotInitialized);
    RUN_TEST(testResetRejectsWrongResponse);
    RUN_TEST(testResetSucceedsAndRestoresDefaults);

    RUN_TEST(testConfigureRejectsNullAndGuards);
    RUN_TEST(testConfigureUpdatesDevState);
    RUN_TEST(testConfigurePacksModeAndClockRegisters);
    RUN_TEST(testConfigurePacksCfgRegister);
    RUN_TEST(testConfigurePacksThresholdAndDcBlock);

    RUN_TEST(testConfigureChannelRejectsNullAndGuards);
    RUN_TEST(testConfigureChannelRejectsInvalidChannel);
    RUN_TEST(testConfigureChannelSetsEnableBitInClock);
    RUN_TEST(testConfigureChannelClearsEnableBitWhenDisabled);
    RUN_TEST(testConfigureChannelPacksGainRegister);
    RUN_TEST(testConfigureChannelPacksPhaseAndMux);
    RUN_TEST(testConfigureChannelPacksCalibrationRegisters);
    RUN_TEST(testConfigureChannelChannel3UsesCorrectBaseAddress);

    RUN_TEST(testReadChipIdRejectsNullParams);
    RUN_TEST(testReadChipIdRejectsNotInitialized);
    RUN_TEST(testReadChipIdReturnsIdFromDevice);

    RUN_TEST(testReadRegistersCountTwoReadsFromWordsOneAndTwo);
    RUN_TEST(testReadRegistersBurstSendsCorrectOpcodeWithCount);

    RUN_TEST(testUnlockRejectsNullAndNotInitialized);

    RUN_TEST(testReadData16BitWordExtractsChannelData);
    RUN_TEST(testReadDataFullScaleSignExtension);
    RUN_TEST(testReadData32BitSignExtendedWord);

    RUN_TEST(testConvertToMicrovoltsAllGainValues);

    return UNITY_END();
}