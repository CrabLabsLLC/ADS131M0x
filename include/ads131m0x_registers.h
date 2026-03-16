#pragma once
#ifndef ADS131M0X_REGISTERS_H
#define ADS131M0X_REGISTERS_H

#ifdef __cplusplus
extern "C" {
#endif

// ── Constants ────────────────────────────────────────────────────────────────
#define ADS131M0X_NUM_REGISTERS         64U

// ── Per-Channel Address Arithmetic ───────────────────────────────────────────
// Channel N registers start at: CH_BASE + N * CH_STRIDE
// Offsets within each channel block:
//   +0 = CHn_CFG, +1 = CHn_OCAL_MSB, +2 = CHn_OCAL_LSB,
//   +3 = CHn_GCAL_MSB, +4 = CHn_GCAL_LSB
#define ADS131M0X_CH_BASE_ADDRESS       ((uint8_t) 0x09)
#define ADS131M0X_CH_STRIDE             5U
#define ADS131M0X_CH_CFG_OFFSET         0U
#define ADS131M0X_CH_OCAL_MSB_OFFSET    1U
#define ADS131M0X_CH_OCAL_LSB_OFFSET    2U
#define ADS131M0X_CH_GCAL_MSB_OFFSET    3U
#define ADS131M0X_CH_GCAL_LSB_OFFSET    4U

// ── Register 0x00: ID ────────────────────────────────────────────────────────
// Read-only. CHANCNT[11:8] always reads the channel count. REVID[7:0] reserved.
#define ADS131M0X_ID_ADDRESS            ((uint8_t) 0x00)
#define ADS131M0X_ID_DEFAULT            ((uint16_t)(0x2000 | ((ADS131M0X_CHANNEL_COUNT) << 8)))

#define ADS131M0X_ID_CHANCNT_SHIFT      8U
#define ADS131M0X_ID_CHANCNT_MASK       (0x0FU << ADS131M0X_ID_CHANCNT_SHIFT)

#define ADS131M0X_ID_REVID_SHIFT        0U
#define ADS131M0X_ID_REVID_MASK         (0xFFU << ADS131M0X_ID_REVID_SHIFT)

// ── Register 0x01: STATUS ────────────────────────────────────────────────────
// Read-only. Hardware-controlled flags and per-channel DRDY indicators.
// Note: DRDY7–DRDY4 are reserved (always 0) on the ADS131M04.
#define ADS131M0X_STATUS_ADDRESS        ((uint8_t) 0x01)
#define ADS131M0X_STATUS_DEFAULT        ((uint16_t) 0x0500)

#define ADS131M0X_STATUS_LOCK_SHIFT     15U
#define ADS131M0X_STATUS_LOCK_MASK      (0x01U << ADS131M0X_STATUS_LOCK_SHIFT)

#define ADS131M0X_STATUS_F_RESYNC_SHIFT 14U
#define ADS131M0X_STATUS_F_RESYNC_MASK  (0x01U << ADS131M0X_STATUS_F_RESYNC_SHIFT)

#define ADS131M0X_STATUS_REG_MAP_SHIFT  13U
#define ADS131M0X_STATUS_REG_MAP_MASK   (0x01U << ADS131M0X_STATUS_REG_MAP_SHIFT)

#define ADS131M0X_STATUS_CRC_ERR_SHIFT  12U
#define ADS131M0X_STATUS_CRC_ERR_MASK   (0x01U << ADS131M0X_STATUS_CRC_ERR_SHIFT)

#define ADS131M0X_STATUS_CRC_TYPE_SHIFT 11U
#define ADS131M0X_STATUS_CRC_TYPE_MASK  (0x01U << ADS131M0X_STATUS_CRC_TYPE_SHIFT)

#define ADS131M0X_STATUS_RESET_SHIFT    10U
#define ADS131M0X_STATUS_RESET_MASK     (0x01U << ADS131M0X_STATUS_RESET_SHIFT)

#define ADS131M0X_STATUS_WLENGTH_SHIFT  8U
#define ADS131M0X_STATUS_WLENGTH_MASK   (0x03U << ADS131M0X_STATUS_WLENGTH_SHIFT)

#define ADS131M0X_STATUS_DRDY7_SHIFT    7U
#define ADS131M0X_STATUS_DRDY7_MASK     (0x01U << ADS131M0X_STATUS_DRDY7_SHIFT)

#define ADS131M0X_STATUS_DRDY6_SHIFT    6U
#define ADS131M0X_STATUS_DRDY6_MASK     (0x01U << ADS131M0X_STATUS_DRDY6_SHIFT)

#define ADS131M0X_STATUS_DRDY5_SHIFT    5U
#define ADS131M0X_STATUS_DRDY5_MASK     (0x01U << ADS131M0X_STATUS_DRDY5_SHIFT)

#define ADS131M0X_STATUS_DRDY4_SHIFT    4U
#define ADS131M0X_STATUS_DRDY4_MASK     (0x01U << ADS131M0X_STATUS_DRDY4_SHIFT)

#define ADS131M0X_STATUS_DRDY3_SHIFT    3U
#define ADS131M0X_STATUS_DRDY3_MASK     (0x01U << ADS131M0X_STATUS_DRDY3_SHIFT)

#define ADS131M0X_STATUS_DRDY2_SHIFT    2U
#define ADS131M0X_STATUS_DRDY2_MASK     (0x01U << ADS131M0X_STATUS_DRDY2_SHIFT)

#define ADS131M0X_STATUS_DRDY1_SHIFT    1U
#define ADS131M0X_STATUS_DRDY1_MASK     (0x01U << ADS131M0X_STATUS_DRDY1_SHIFT)

#define ADS131M0X_STATUS_DRDY0_SHIFT    0U
#define ADS131M0X_STATUS_DRDY0_MASK     (0x01U << ADS131M0X_STATUS_DRDY0_SHIFT)

// ── Register 0x02: MODE ──────────────────────────────────────────────────────
#define ADS131M0X_MODE_ADDRESS          ((uint8_t) 0x02)
#define ADS131M0X_MODE_DEFAULT          ((uint16_t) 0x0510)

#define ADS131M0X_MODE_REG_CRC_EN_SHIFT 13U
#define ADS131M0X_MODE_REG_CRC_EN_MASK  (0x01U << ADS131M0X_MODE_REG_CRC_EN_SHIFT)

#define ADS131M0X_MODE_RX_CRC_EN_SHIFT  12U
#define ADS131M0X_MODE_RX_CRC_EN_MASK   (0x01U << ADS131M0X_MODE_RX_CRC_EN_SHIFT)

#define ADS131M0X_MODE_CRC_TYPE_SHIFT   11U
#define ADS131M0X_MODE_CRC_TYPE_MASK    (0x01U << ADS131M0X_MODE_CRC_TYPE_SHIFT)

#define ADS131M0X_MODE_RESET_SHIFT      10U
#define ADS131M0X_MODE_RESET_MASK       (0x01U << ADS131M0X_MODE_RESET_SHIFT)

#define ADS131M0X_MODE_WLENGTH_SHIFT    8U
#define ADS131M0X_MODE_WLENGTH_MASK     (0x03U << ADS131M0X_MODE_WLENGTH_SHIFT)

#define ADS131M0X_MODE_TIMEOUT_SHIFT    4U
#define ADS131M0X_MODE_TIMEOUT_MASK     (0x01U << ADS131M0X_MODE_TIMEOUT_SHIFT)

#define ADS131M0X_MODE_DRDY_SEL_SHIFT   2U
#define ADS131M0X_MODE_DRDY_SEL_MASK    (0x03U << ADS131M0X_MODE_DRDY_SEL_SHIFT)

#define ADS131M0X_MODE_DRDY_HIZ_SHIFT   1U
#define ADS131M0X_MODE_DRDY_HIZ_MASK    (0x01U << ADS131M0X_MODE_DRDY_HIZ_SHIFT)

#define ADS131M0X_MODE_DRDY_FMT_SHIFT   0U
#define ADS131M0X_MODE_DRDY_FMT_MASK    (0x01U << ADS131M0X_MODE_DRDY_FMT_SHIFT)

// ── Register 0x03: CLOCK ─────────────────────────────────────────────────────
// CH7_EN–CH4_EN are reserved (always 0) on the ADS131M04.
// Default value depends on channel count: all active channels enabled, OSR=1024, PWR=HR.
#if   (ADS131M0X_CHANNEL_COUNT == 8)
#define ADS131M0X_CLOCK_DEFAULT         ((uint16_t) 0xFF0E)
#elif (ADS131M0X_CHANNEL_COUNT == 7)
#define ADS131M0X_CLOCK_DEFAULT         ((uint16_t) 0x7F0E)
#elif (ADS131M0X_CHANNEL_COUNT == 6)
#define ADS131M0X_CLOCK_DEFAULT         ((uint16_t) 0x3F0E)
#elif (ADS131M0X_CHANNEL_COUNT == 5)
#define ADS131M0X_CLOCK_DEFAULT         ((uint16_t) 0x1F0E)
#elif (ADS131M0X_CHANNEL_COUNT == 4)
#define ADS131M0X_CLOCK_DEFAULT         ((uint16_t) 0x0F0E)
#elif (ADS131M0X_CHANNEL_COUNT == 3)
#define ADS131M0X_CLOCK_DEFAULT         ((uint16_t) 0x070E)
#elif (ADS131M0X_CHANNEL_COUNT == 2)
#define ADS131M0X_CLOCK_DEFAULT         ((uint16_t) 0x030E)
#else
#define ADS131M0X_CLOCK_DEFAULT         ((uint16_t) 0x010E)
#endif

#define ADS131M0X_CLOCK_ADDRESS         ((uint8_t) 0x03)

#define ADS131M0X_CLOCK_CH7_EN_SHIFT    15U
#define ADS131M0X_CLOCK_CH7_EN_MASK     (0x01U << ADS131M0X_CLOCK_CH7_EN_SHIFT)

#define ADS131M0X_CLOCK_CH6_EN_SHIFT    14U
#define ADS131M0X_CLOCK_CH6_EN_MASK     (0x01U << ADS131M0X_CLOCK_CH6_EN_SHIFT)

#define ADS131M0X_CLOCK_CH5_EN_SHIFT    13U
#define ADS131M0X_CLOCK_CH5_EN_MASK     (0x01U << ADS131M0X_CLOCK_CH5_EN_SHIFT)

#define ADS131M0X_CLOCK_CH4_EN_SHIFT    12U
#define ADS131M0X_CLOCK_CH4_EN_MASK     (0x01U << ADS131M0X_CLOCK_CH4_EN_SHIFT)

#define ADS131M0X_CLOCK_CH3_EN_SHIFT    11U
#define ADS131M0X_CLOCK_CH3_EN_MASK     (0x01U << ADS131M0X_CLOCK_CH3_EN_SHIFT)

#define ADS131M0X_CLOCK_CH2_EN_SHIFT    10U
#define ADS131M0X_CLOCK_CH2_EN_MASK     (0x01U << ADS131M0X_CLOCK_CH2_EN_SHIFT)

#define ADS131M0X_CLOCK_CH1_EN_SHIFT    9U
#define ADS131M0X_CLOCK_CH1_EN_MASK     (0x01U << ADS131M0X_CLOCK_CH1_EN_SHIFT)

#define ADS131M0X_CLOCK_CH0_EN_SHIFT    8U
#define ADS131M0X_CLOCK_CH0_EN_MASK     (0x01U << ADS131M0X_CLOCK_CH0_EN_SHIFT)

#define ADS131M0X_CLOCK_TBM_SHIFT       5U
#define ADS131M0X_CLOCK_TBM_MASK        (0x01U << ADS131M0X_CLOCK_TBM_SHIFT)

#define ADS131M0X_CLOCK_OSR_SHIFT       2U
#define ADS131M0X_CLOCK_OSR_MASK        (0x07U << ADS131M0X_CLOCK_OSR_SHIFT)

#define ADS131M0X_CLOCK_PWR_SHIFT       0U
#define ADS131M0X_CLOCK_PWR_MASK        (0x03U << ADS131M0X_CLOCK_PWR_SHIFT)

// ── Register 0x04: GAIN1 ─────────────────────────────────────────────────────
// PGA gain for channels 0–3. Each field is 3 bits; bit between fields is reserved.
#define ADS131M0X_GAIN1_ADDRESS         ((uint8_t) 0x04)
#define ADS131M0X_GAIN1_DEFAULT         ((uint16_t) 0x0000)

#define ADS131M0X_GAIN1_PGAGAIN3_SHIFT  12U
#define ADS131M0X_GAIN1_PGAGAIN3_MASK   (0x07U << ADS131M0X_GAIN1_PGAGAIN3_SHIFT)

#define ADS131M0X_GAIN1_PGAGAIN2_SHIFT  8U
#define ADS131M0X_GAIN1_PGAGAIN2_MASK   (0x07U << ADS131M0X_GAIN1_PGAGAIN2_SHIFT)

#define ADS131M0X_GAIN1_PGAGAIN1_SHIFT  4U
#define ADS131M0X_GAIN1_PGAGAIN1_MASK   (0x07U << ADS131M0X_GAIN1_PGAGAIN1_SHIFT)

#define ADS131M0X_GAIN1_PGAGAIN0_SHIFT  0U
#define ADS131M0X_GAIN1_PGAGAIN0_MASK   (0x07U << ADS131M0X_GAIN1_PGAGAIN0_SHIFT)

// ── Register 0x05: GAIN2 ─────────────────────────────────────────────────────
// PGA gain for channels 4–7. Reserved (always write 0x0000) on the ADS131M04.
#define ADS131M0X_GAIN2_ADDRESS         ((uint8_t) 0x05)
#define ADS131M0X_GAIN2_DEFAULT         ((uint16_t) 0x0000)

#define ADS131M0X_GAIN2_PGAGAIN7_SHIFT  12U
#define ADS131M0X_GAIN2_PGAGAIN7_MASK   (0x07U << ADS131M0X_GAIN2_PGAGAIN7_SHIFT)

#define ADS131M0X_GAIN2_PGAGAIN6_SHIFT  8U
#define ADS131M0X_GAIN2_PGAGAIN6_MASK   (0x07U << ADS131M0X_GAIN2_PGAGAIN6_SHIFT)

#define ADS131M0X_GAIN2_PGAGAIN5_SHIFT  4U
#define ADS131M0X_GAIN2_PGAGAIN5_MASK   (0x07U << ADS131M0X_GAIN2_PGAGAIN5_SHIFT)

#define ADS131M0X_GAIN2_PGAGAIN4_SHIFT  0U
#define ADS131M0X_GAIN2_PGAGAIN4_MASK   (0x07U << ADS131M0X_GAIN2_PGAGAIN4_SHIFT)

// ── Register 0x06: CFG ───────────────────────────────────────────────────────
#define ADS131M0X_CFG_ADDRESS           ((uint8_t) 0x06)
#define ADS131M0X_CFG_DEFAULT           ((uint16_t) 0x0600)

#define ADS131M0X_CFG_GC_DLY_SHIFT      9U
#define ADS131M0X_CFG_GC_DLY_MASK       (0x0FU << ADS131M0X_CFG_GC_DLY_SHIFT)

#define ADS131M0X_CFG_GC_EN_SHIFT       8U
#define ADS131M0X_CFG_GC_EN_MASK        (0x01U << ADS131M0X_CFG_GC_EN_SHIFT)

#define ADS131M0X_CFG_CD_ALLCH_SHIFT    7U
#define ADS131M0X_CFG_CD_ALLCH_MASK     (0x01U << ADS131M0X_CFG_CD_ALLCH_SHIFT)

#define ADS131M0X_CFG_CD_NUM_SHIFT      4U
#define ADS131M0X_CFG_CD_NUM_MASK       (0x07U << ADS131M0X_CFG_CD_NUM_SHIFT)

#define ADS131M0X_CFG_CD_LEN_SHIFT      1U
#define ADS131M0X_CFG_CD_LEN_MASK       (0x07U << ADS131M0X_CFG_CD_LEN_SHIFT)

#define ADS131M0X_CFG_CD_EN_SHIFT       0U
#define ADS131M0X_CFG_CD_EN_MASK        (0x01U << ADS131M0X_CFG_CD_EN_SHIFT)

// ── Register 0x07: THRSHLD_MSB ───────────────────────────────────────────────
#define ADS131M0X_THRSHLD_MSB_ADDRESS   ((uint8_t) 0x07)
#define ADS131M0X_THRSHLD_MSB_DEFAULT   ((uint16_t) 0x0000)

#define ADS131M0X_THRSHLD_MSB_CD_TH_MSB_SHIFT  0U
#define ADS131M0X_THRSHLD_MSB_CD_TH_MSB_MASK   (0xFFFFU << ADS131M0X_THRSHLD_MSB_CD_TH_MSB_SHIFT)

// ── Register 0x08: THRSHLD_LSB ───────────────────────────────────────────────
// DCBLOCK[3:0] is not present in the TI 2019 reference code (marked RESERVED).
// It is defined here per the May 2021 datasheet (Table 8-22).
#define ADS131M0X_THRSHLD_LSB_ADDRESS   ((uint8_t) 0x08)
#define ADS131M0X_THRSHLD_LSB_DEFAULT   ((uint16_t) 0x0000)

#define ADS131M0X_THRSHLD_LSB_CD_TH_LSB_SHIFT  8U
#define ADS131M0X_THRSHLD_LSB_CD_TH_LSB_MASK   (0xFFU << ADS131M0X_THRSHLD_LSB_CD_TH_LSB_SHIFT)

#define ADS131M0X_THRSHLD_LSB_DCBLOCK_SHIFT     0U
#define ADS131M0X_THRSHLD_LSB_DCBLOCK_MASK      (0x0FU << ADS131M0X_THRSHLD_LSB_DCBLOCK_SHIFT)

// ── Registers 0x09–0x30: Per-Channel (CH0–CH7) ───────────────────────────────
// Field definitions are identical across all channels. Use the _CHn_ macros
// with the channel's register address (computed via ADS131M0X_CH_BASE_ADDRESS
// + channel * ADS131M0X_CH_STRIDE + offset).
//
// Explicit per-channel addresses for CH0–CH3 (always present):
#define ADS131M0X_CH0_CFG_ADDRESS       ((uint8_t) 0x09)
#define ADS131M0X_CH0_OCAL_MSB_ADDRESS  ((uint8_t) 0x0A)
#define ADS131M0X_CH0_OCAL_LSB_ADDRESS  ((uint8_t) 0x0B)
#define ADS131M0X_CH0_GCAL_MSB_ADDRESS  ((uint8_t) 0x0C)
#define ADS131M0X_CH0_GCAL_LSB_ADDRESS  ((uint8_t) 0x0D)

#define ADS131M0X_CH1_CFG_ADDRESS       ((uint8_t) 0x0E)
#define ADS131M0X_CH1_OCAL_MSB_ADDRESS  ((uint8_t) 0x0F)
#define ADS131M0X_CH1_OCAL_LSB_ADDRESS  ((uint8_t) 0x10)
#define ADS131M0X_CH1_GCAL_MSB_ADDRESS  ((uint8_t) 0x11)
#define ADS131M0X_CH1_GCAL_LSB_ADDRESS  ((uint8_t) 0x12)

#define ADS131M0X_CH2_CFG_ADDRESS       ((uint8_t) 0x13)
#define ADS131M0X_CH2_OCAL_MSB_ADDRESS  ((uint8_t) 0x14)
#define ADS131M0X_CH2_OCAL_LSB_ADDRESS  ((uint8_t) 0x15)
#define ADS131M0X_CH2_GCAL_MSB_ADDRESS  ((uint8_t) 0x16)
#define ADS131M0X_CH2_GCAL_LSB_ADDRESS  ((uint8_t) 0x17)

#define ADS131M0X_CH3_CFG_ADDRESS       ((uint8_t) 0x18)
#define ADS131M0X_CH3_OCAL_MSB_ADDRESS  ((uint8_t) 0x19)
#define ADS131M0X_CH3_OCAL_LSB_ADDRESS  ((uint8_t) 0x1A)
#define ADS131M0X_CH3_GCAL_MSB_ADDRESS  ((uint8_t) 0x1B)
#define ADS131M0X_CH3_GCAL_LSB_ADDRESS  ((uint8_t) 0x1C)

// CH4–CH7 addresses (ADS131M06 and ADS131M08 only):
#define ADS131M0X_CH4_CFG_ADDRESS       ((uint8_t) 0x1D)
#define ADS131M0X_CH4_OCAL_MSB_ADDRESS  ((uint8_t) 0x1E)
#define ADS131M0X_CH4_OCAL_LSB_ADDRESS  ((uint8_t) 0x1F)
#define ADS131M0X_CH4_GCAL_MSB_ADDRESS  ((uint8_t) 0x20)
#define ADS131M0X_CH4_GCAL_LSB_ADDRESS  ((uint8_t) 0x21)

#define ADS131M0X_CH5_CFG_ADDRESS       ((uint8_t) 0x22)
#define ADS131M0X_CH5_OCAL_MSB_ADDRESS  ((uint8_t) 0x23)
#define ADS131M0X_CH5_OCAL_LSB_ADDRESS  ((uint8_t) 0x24)
#define ADS131M0X_CH5_GCAL_MSB_ADDRESS  ((uint8_t) 0x25)
#define ADS131M0X_CH5_GCAL_LSB_ADDRESS  ((uint8_t) 0x26)

#define ADS131M0X_CH6_CFG_ADDRESS       ((uint8_t) 0x27)
#define ADS131M0X_CH6_OCAL_MSB_ADDRESS  ((uint8_t) 0x28)
#define ADS131M0X_CH6_OCAL_LSB_ADDRESS  ((uint8_t) 0x29)
#define ADS131M0X_CH6_GCAL_MSB_ADDRESS  ((uint8_t) 0x2A)
#define ADS131M0X_CH6_GCAL_LSB_ADDRESS  ((uint8_t) 0x2B)

#define ADS131M0X_CH7_CFG_ADDRESS       ((uint8_t) 0x2C)
#define ADS131M0X_CH7_OCAL_MSB_ADDRESS  ((uint8_t) 0x2D)
#define ADS131M0X_CH7_OCAL_LSB_ADDRESS  ((uint8_t) 0x2E)
#define ADS131M0X_CH7_GCAL_MSB_ADDRESS  ((uint8_t) 0x2F)
#define ADS131M0X_CH7_GCAL_LSB_ADDRESS  ((uint8_t) 0x30)

// ── CHn_CFG Fields ────────────────────────────────────────────────────────────
// PHASEn[9:0]: two's-complement phase delay in modulator clock cycles.
// DCBLKn_DIS0: 1 = DC-block filter disabled for this channel (overrides global DCBLOCK).
//              Not present in TI 2019 reference code; added per May 2021 datasheet.
// MUXn[1:0]: input selection (00=normal, 01=shorted, 10=+DC test, 11=-DC test).
#define ADS131M0X_CHn_CFG_DEFAULT           ((uint16_t) 0x0000)

#define ADS131M0X_CHn_CFG_PHASE_SHIFT       6U
#define ADS131M0X_CHn_CFG_PHASE_MASK        (0x03FFU << ADS131M0X_CHn_CFG_PHASE_SHIFT)

#define ADS131M0X_CHn_CFG_DCBLK_DIS_SHIFT   2U
#define ADS131M0X_CHn_CFG_DCBLK_DIS_MASK    (0x01U << ADS131M0X_CHn_CFG_DCBLK_DIS_SHIFT)

#define ADS131M0X_CHn_CFG_MUX_SHIFT         0U
#define ADS131M0X_CHn_CFG_MUX_MASK          (0x03U << ADS131M0X_CHn_CFG_MUX_SHIFT)

// ── CHn_OCAL_MSB Fields ───────────────────────────────────────────────────────
// OCALn[23:8]: upper 16 bits of 24-bit offset calibration word.
#define ADS131M0X_CHn_OCAL_MSB_DEFAULT      ((uint16_t) 0x0000)

#define ADS131M0X_CHn_OCAL_MSB_SHIFT        0U
#define ADS131M0X_CHn_OCAL_MSB_MASK         (0xFFFFU << ADS131M0X_CHn_OCAL_MSB_SHIFT)

// ── CHn_OCAL_LSB Fields ───────────────────────────────────────────────────────
// OCALn[7:0]: lower 8 bits of 24-bit offset calibration word. Bits [7:0] reserved.
#define ADS131M0X_CHn_OCAL_LSB_DEFAULT      ((uint16_t) 0x0000)

#define ADS131M0X_CHn_OCAL_LSB_SHIFT        8U
#define ADS131M0X_CHn_OCAL_LSB_MASK         (0xFFU << ADS131M0X_CHn_OCAL_LSB_SHIFT)

// ── CHn_GCAL_MSB Fields ───────────────────────────────────────────────────────
// GCALn[23:8]: upper 16 bits of 24-bit gain calibration word. Default 0x8000 = 1.0.
#define ADS131M0X_CHn_GCAL_MSB_DEFAULT      ((uint16_t) 0x8000)

#define ADS131M0X_CHn_GCAL_MSB_SHIFT        0U
#define ADS131M0X_CHn_GCAL_MSB_MASK         (0xFFFFU << ADS131M0X_CHn_GCAL_MSB_SHIFT)

// ── CHn_GCAL_LSB Fields ───────────────────────────────────────────────────────
// GCALn[7:0]: lower 8 bits of 24-bit gain calibration word. Bits [7:0] reserved.
#define ADS131M0X_CHn_GCAL_LSB_DEFAULT      ((uint16_t) 0x0000)

#define ADS131M0X_CHn_GCAL_LSB_SHIFT        8U
#define ADS131M0X_CHn_GCAL_LSB_MASK         (0xFFU << ADS131M0X_CHn_GCAL_LSB_SHIFT)

// ── Register 0x3E: REGMAP_CRC ────────────────────────────────────────────────
// Read-only. CRC of the entire register map contents.
#define ADS131M0X_REGMAP_CRC_ADDRESS        ((uint8_t) 0x3E)
#define ADS131M0X_REGMAP_CRC_DEFAULT        ((uint16_t) 0x0000)

#define ADS131M0X_REGMAP_CRC_SHIFT          0U
#define ADS131M0X_REGMAP_CRC_MASK           (0xFFFFU << ADS131M0X_REGMAP_CRC_SHIFT)

// ── Register 0x3F: RESERVED ──────────────────────────────────────────────────
// Always write 0x0000.
#define ADS131M0X_RESERVED_ADDRESS          ((uint8_t) 0x3F)
#define ADS131M0X_RESERVED_DEFAULT          ((uint16_t) 0x0000)

#ifdef __cplusplus
}
#endif

#endif // ADS131M0X_REGISTERS_H