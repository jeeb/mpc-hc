/*
 * QDM2 compatible decoder
 * Copyright (c) 2003 Ewald Snel
 * Copyright (c) 2005 Benjamin Larsson
 * Copyright (c) 2005 Alex Beregszaszi
 * Copyright (c) 2005 Roberto Togni
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

 /**
 * @file qdm2data.h
 * Various QDM2 tables.
 */

#ifndef QDM2DATA_H
#define QDM2DATA_H

/** VLC TABLES **/

/* values in this table range from -1..23; adjust retrieved value by -1 */
static const uint16_t vlc_tab_level_huffcodes[24] = {
    0x037c, 0x0004, 0x003c, 0x004c, 0x003a, 0x002c, 0x001c, 0x001a,
    0x0024, 0x0014, 0x0001, 0x0002, 0x0000, 0x0003, 0x0007, 0x0005,
    0x0006, 0x0008, 0x0009, 0x000a, 0x000c, 0x00fc, 0x007c, 0x017c
};

static const uint8_t vlc_tab_level_huffbits[24] = {
    10, 6, 7, 7, 6, 6, 6, 6, 6, 5, 4, 4, 4, 3, 3, 3, 3, 4, 4, 5, 7, 8, 9, 10
};

/* values in this table range from -1..36; adjust retrieved value by -1 */
static const uint16_t vlc_tab_diff_huffcodes[37] = {
    0x1c57, 0x0004, 0x0000, 0x0001, 0x0003, 0x0002, 0x000f, 0x000e,
    0x0007, 0x0016, 0x0037, 0x0027, 0x0026, 0x0066, 0x0006, 0x0097,
    0x0046, 0x01c6, 0x0017, 0x0786, 0x0086, 0x0257, 0x00d7, 0x0357,
    0x00c6, 0x0386, 0x0186, 0x0000, 0x0157, 0x0c57, 0x0057, 0x0000,
    0x0b86, 0x0000, 0x1457, 0x0000, 0x0457
};

static const uint8_t vlc_tab_diff_huffbits[37] = {
    13, 3, 3, 2, 3, 3, 4, 4, 6, 5, 6, 6, 7, 7, 8, 8,
    8, 9, 8, 11, 9, 10, 8, 10, 9, 12, 10, 0, 10, 13, 11, 0,
    12, 0, 13, 0, 13
};

/* values in this table range from -1..5; adjust retrieved value by -1 */
static const uint8_t vlc_tab_run_huffcodes[6] = {
    0x1f, 0x00, 0x01, 0x03, 0x07, 0x0f
};

static const uint8_t vlc_tab_run_huffbits[6] = {
    5, 1, 2, 3, 4, 5
};

/* values in this table range from -1..19; adjust retrieved value by -1 */
static const uint16_t vlc_tab_tone_level_idx_hi1_huffcodes[20] = {
    0x5714, 0x000c, 0x0002, 0x0001, 0x0000, 0x0004, 0x0034, 0x0054,
    0x0094, 0x0014, 0x0114, 0x0214, 0x0314, 0x0614, 0x0e14, 0x0f14,
    0x2714, 0x0714, 0x1714, 0x3714
};

static const uint8_t vlc_tab_tone_level_idx_hi1_huffbits[20] = {
    15, 4, 2, 1, 3, 5, 6, 7, 8, 10, 10, 11, 11, 12, 12, 12, 14, 14, 15, 14
};

/* values in this table range from -1..23; adjust retrieved value by -1 */
static const uint16_t vlc_tab_tone_level_idx_mid_huffcodes[24] = {
    0x0fea, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x03ea, 0x00ea, 0x002a, 0x001a,
    0x0006, 0x0001, 0x0000, 0x0002, 0x000a, 0x006a, 0x01ea, 0x07ea
};

static const uint8_t vlc_tab_tone_level_idx_mid_huffbits[24] = {
    12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 11, 9, 7, 5, 3, 1, 2, 4, 6, 8, 10, 12
};

/* values in this table range from -1..23; adjust retrieved value by -1 */
static const uint16_t vlc_tab_tone_level_idx_hi2_huffcodes[24] = {
    0x0664, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0064, 0x00e4,
    0x00a4, 0x0068, 0x0004, 0x0008, 0x0014, 0x0018, 0x0000, 0x0001,
    0x0002, 0x0003, 0x000c, 0x0028, 0x0024, 0x0164, 0x0000, 0x0264
};

static const uint8_t vlc_tab_tone_level_idx_hi2_huffbits[24] = {
    11, 0, 0, 0, 0, 0, 10, 8, 8, 7, 6, 6, 5, 5, 4, 2, 2, 2, 4, 7, 8, 9, 0, 11
};

/* values in this table range from -1..8; adjust retrieved value by -1 */
static const uint8_t vlc_tab_type30_huffcodes[9] = {
    0x3c, 0x06, 0x00, 0x01, 0x03, 0x02, 0x04, 0x0c, 0x1c
};

static const uint8_t vlc_tab_type30_huffbits[9] = {
    6, 3, 3, 2, 2, 3, 4, 5, 6
};

/* values in this table range from -1..9; adjust retrieved value by -1 */
static const uint8_t vlc_tab_type34_huffcodes[10] = {
    0x18, 0x00, 0x01, 0x04, 0x05, 0x07, 0x03, 0x02, 0x06, 0x08
};

static const uint8_t vlc_tab_type34_huffbits[10] = {
    5, 4, 3, 3, 3, 3, 3, 3, 3, 5
};

/* values in this table range from -1..22; adjust retrieved value by -1 */
static const uint16_t vlc_tab_fft_tone_offset_0_huffcodes[23] = {
    0x038e, 0x0001, 0x0000, 0x0022, 0x000a, 0x0006, 0x0012, 0x0002,
    0x001e, 0x003e, 0x0056, 0x0016, 0x000e, 0x0032, 0x0072, 0x0042,
    0x008e, 0x004e, 0x00f2, 0x002e, 0x0036, 0x00c2, 0x018e
};

static const uint8_t vlc_tab_fft_tone_offset_0_huffbits[23] = {
    10, 1, 2, 6, 4, 5, 6, 7, 6, 6, 7, 7, 8, 7, 8, 8, 9, 7, 8, 6, 6, 8, 10
};

/* values in this table range from -1..27; adjust retrieved value by -1 */
static const uint16_t vlc_tab_fft_tone_offset_1_huffcodes[28] = {
    0x07a4, 0x0001, 0x0020, 0x0012, 0x001c, 0x0008, 0x0006, 0x0010,
    0x0000, 0x0014, 0x0004, 0x0032, 0x0070, 0x000c, 0x0002, 0x003a,
    0x001a, 0x002c, 0x002a, 0x0022, 0x0024, 0x000a, 0x0064, 0x0030,
    0x0062, 0x00a4, 0x01a4, 0x03a4
};

static const uint8_t vlc_tab_fft_tone_offset_1_huffbits[28] = {
    11, 1, 6, 6, 5, 4, 3, 6, 6, 5, 6, 6, 7, 6, 6, 6,
    6, 6, 6, 7, 8, 6, 7, 7, 7, 9, 10, 11
};

/* values in this table range from -1..31; adjust retrieved value by -1 */
static const uint16_t vlc_tab_fft_tone_offset_2_huffcodes[32] = {
    0x1760, 0x0001, 0x0000, 0x0082, 0x000c, 0x0006, 0x0003, 0x0007,
    0x0008, 0x0004, 0x0010, 0x0012, 0x0022, 0x001a, 0x0000, 0x0020,
    0x000a, 0x0040, 0x004a, 0x006a, 0x002a, 0x0042, 0x0002, 0x0060,
    0x00aa, 0x00e0, 0x00c2, 0x01c2, 0x0160, 0x0360, 0x0760, 0x0f60
};

static const uint8_t vlc_tab_fft_tone_offset_2_huffbits[32] = {
    13, 2, 0, 8, 4, 3, 3, 3, 4, 4, 5, 5, 6, 5, 7, 7,
    7, 7, 7, 7, 8, 8, 8, 9, 8, 8, 9, 9, 10, 11, 13, 12
};

/* values in this table range from -1..34; adjust retrieved value by -1 */
static const uint16_t vlc_tab_fft_tone_offset_3_huffcodes[35] = {
    0x33ea, 0x0005, 0x0000, 0x000c, 0x0000, 0x0006, 0x0003, 0x0008,
    0x0002, 0x0001, 0x0004, 0x0007, 0x001a, 0x000f, 0x001c, 0x002c,
    0x000a, 0x001d, 0x002d, 0x002a, 0x000d, 0x004c, 0x008c, 0x006a,
    0x00cd, 0x004d, 0x00ea, 0x020c, 0x030c, 0x010c, 0x01ea, 0x07ea,
    0x0bea, 0x03ea, 0x13ea
};

static const uint8_t vlc_tab_fft_tone_offset_3_huffbits[35] = {
    14, 4, 0, 10, 4, 3, 3, 4, 4, 3, 4, 4, 5, 4, 5, 6,
    6, 5, 6, 7, 7, 7, 8, 8, 8, 8, 9, 10, 10, 10, 10, 11,
    12, 13, 14
};

/* values in this table range from -1..37; adjust retrieved value by -1 */
static const uint16_t vlc_tab_fft_tone_offset_4_huffcodes[38] = {
    0x5282, 0x0016, 0x0000, 0x0136, 0x0004, 0x0000, 0x0007, 0x000a,
    0x000e, 0x0003, 0x0001, 0x000d, 0x0006, 0x0009, 0x0012, 0x0005,
    0x0025, 0x0022, 0x0015, 0x0002, 0x0076, 0x0035, 0x0042, 0x00c2,
    0x0182, 0x00b6, 0x0036, 0x03c2, 0x0482, 0x01c2, 0x0682, 0x0882,
    0x0a82, 0x0082, 0x0282, 0x1282, 0x3282, 0x2282
};

static const uint8_t vlc_tab_fft_tone_offset_4_huffbits[38] = {
    15, 6, 0, 9, 3, 3, 3, 4, 4, 3, 4, 4, 5, 4, 5, 6,
    6, 6, 6, 8, 7, 6, 8, 9, 9, 8, 9, 10, 11, 10, 11, 12,
    12, 12, 14, 15, 14, 14
};

/** FFT TABLES **/

/* values in this table range from -1..27; adjust retrieved value by -1 */
static const uint16_t fft_level_exp_alt_huffcodes[28] = {
    0x1ec6, 0x0006, 0x00c2, 0x0142, 0x0242, 0x0246, 0x00c6, 0x0046,
    0x0042, 0x0146, 0x00a2, 0x0062, 0x0026, 0x0016, 0x000e, 0x0005,
    0x0004, 0x0003, 0x0000, 0x0001, 0x000a, 0x0012, 0x0002, 0x0022,
    0x01c6, 0x02c6, 0x06c6, 0x0ec6
};

static const uint8_t fft_level_exp_alt_huffbits[28] = {
    13, 7, 8, 9, 10, 10, 10, 10, 10, 9, 8, 7, 6, 5, 4, 3,
    3, 2, 3, 3, 4, 5, 7, 8, 9, 11, 12, 13
};

/* values in this table range from -1..19; adjust retrieved value by -1 */
static const uint16_t fft_level_exp_huffcodes[20] = {
    0x0f24, 0x0001, 0x0002, 0x0000, 0x0006, 0x0005, 0x0007, 0x000c,
    0x000b, 0x0014, 0x0013, 0x0004, 0x0003, 0x0023, 0x0064, 0x00a4,
    0x0024, 0x0124, 0x0324, 0x0724
};

static const uint8_t fft_level_exp_huffbits[20] = {
    12, 3, 3, 3, 3, 3, 3, 4, 4, 5, 5, 6, 6, 6, 7, 8, 9, 10, 11, 12
};

/* values in this table range from -1..6; adjust retrieved value by -1 */
static const uint8_t fft_stereo_exp_huffcodes[7] = {
    0x3e, 0x01, 0x00, 0x02, 0x06, 0x0e, 0x1e
};

static const uint8_t fft_stereo_exp_huffbits[7] = {
    6, 1, 2, 3, 4, 5, 6
};

/* values in this table range from -1..8; adjust retrieved value by -1 */
static const uint8_t fft_stereo_phase_huffcodes[9] = {
    0x35, 0x02, 0x00, 0x01, 0x0d, 0x15, 0x05, 0x09, 0x03
};

static const uint8_t fft_stereo_phase_huffbits[9] = {
    6, 2, 2, 4, 4, 6, 5, 4, 2
};

static const int fft_cutoff_index_table[4][2] = {
    { 1, 2 }, {-1, 0 }, {-1,-2 }, { 0, 0 }
};

static const int16_t fft_level_index_table[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
};

static uint8_t last_coeff[3] = {
    4, 7, 10
};

static uint8_t coeff_per_sb_for_avg[3][30] = {
    { 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
    { 0, 1, 2, 2, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
    { 0, 1, 2, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9 }
};

static uint32_t dequant_table[3][10][30] = {
    { { 256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 256, 256, 205, 154, 102, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 51, 102, 154, 205, 256, 238, 219, 201, 183, 165, 146, 128, 110, 91, 73, 55, 37, 18, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 18, 37, 55, 73, 91, 110, 128, 146, 165, 183, 201, 219, 238, 256, 228, 199, 171, 142, 114, 85, 57, 28 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { 256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 256, 171, 85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 85, 171, 256, 171, 85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 85, 171, 256, 219, 183, 146, 110, 73, 37, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 37, 73, 110, 146, 183, 219, 256, 228, 199, 171, 142, 114, 85, 57, 28, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28, 57, 85, 114, 142, 171, 199, 228, 256, 213, 171, 128, 85, 43 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
    { { 256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 256, 256, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 256, 171, 85, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 85, 171, 256, 192, 128, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 128, 192, 256, 205, 154, 102, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 51, 102, 154, 205, 256, 213, 171, 128, 85, 43, 0, 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 43, 85, 128, 171, 213, 256, 213, 171, 128, 85, 43 } }
};

static uint8_t coeff_per_sb_for_dequant[3][30] = {
    { 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3 },
    { 0, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6 },
    { 0, 1, 2, 3, 4, 4, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9 }
};

/* first index is subband, 2nd index is 0, 1 or 3 (2 is unused) */
static int8_t tone_level_idx_offset_table[30][4] = {
    { -50, -50,  0, -50 },
    { -50, -50,  0, -50 },
    { -50,  -9,  0, -19 },
    { -16,  -6,  0, -12 },
    { -11,  -4,  0,  -8 },
    {  -8,  -3,  0,  -6 },
    {  -7,  -3,  0,  -5 },
    {  -6,  -2,  0,  -4 },
    {  -5,  -2,  0,  -3 },
    {  -4,  -1,  0,  -3 },
    {  -4,  -1,  0,  -2 },
    {  -3,  -1,  0,  -2 },
    {  -3,  -1,  0,  -2 },
    {  -3,  -1,  0,  -2 },
    {  -2,  -1,  0,  -1 },
    {  -2,  -1,  0,  -1 },
    {  -2,  -1,  0,  -1 },
    {  -2,   0,  0,  -1 },
    {  -2,   0,  0,  -1 },
    {  -1,   0,  0,  -1 },
    {  -1,   0,  0,  -1 },
    {  -1,   0,  0,  -1 },
    {  -1,   0,  0,  -1 },
    {  -1,   0,  0,  -1 },
    {  -1,   0,  0,  -1 },
    {  -1,   0,  0,  -1 },
    {  -1,   0,  0,   0 },
    {  -1,   0,  0,   0 },
    {  -1,   0,  0,   0 },
    {  -1,   0,  0,   0 }
};

/* all my samples have 1st index 0 or 1 */
/* second index is subband, only indexes 0-29 seem to be used */
static int8_t coding_method_table[5][30] = {
    { 34, 30, 24, 24, 16, 16, 16, 16, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
    },
    { 34, 30, 24, 24, 16, 16, 16, 16, 10, 10, 10, 10, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
    },
    { 34, 30, 30, 30, 24, 24, 16, 16, 16, 16, 16, 16, 10, 10, 10,
      10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10
    },
    { 34, 34, 30, 30, 24, 24, 24, 24, 16, 16, 16, 16, 16, 16, 16,
      16, 16, 16, 16, 16, 16, 16, 10, 10, 10, 10, 10, 10, 10, 10
    },
    { 34, 34, 30, 30, 30, 30, 30, 30, 24, 24, 24, 24, 24, 24, 24,
      24, 24, 24, 24, 24, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
    },
};

static const int vlc_stage3_values[60] = {
        0,     1,     2,     3,     4,     6,     8,    10,    12,    16,    20,    24,
       28,    36,    44,    52,    60,    76,    92,   108,   124,   156,   188,   220,
      252,   316,   380,   444,   508,   636,   764,   892,  1020,  1276,  1532,  1788,
     2044,  2556,  3068,  3580,  4092,  5116,  6140,  7164,  8188, 10236, 12284, 14332,
    16380, 20476, 24572, 28668, 32764, 40956, 49148, 57340, 65532, 81916, 98300,114684
};

static const float fft_tone_sample_table[4][16][5] = {
    { { .0100000000f,-.0037037037f,-.0020000000f,-.0069444444f,-.0018416207f },
      { .0416666667f, .0000000000f, .0000000000f,-.0208333333f,-.0123456791f },
      { .1250000000f, .0558035709f, .0330687836f,-.0164473690f,-.0097465888f },
      { .1562500000f, .0625000000f, .0370370370f,-.0062500000f,-.0037037037f },
      { .1996007860f, .0781250000f, .0462962948f, .0022727272f, .0013468013f },
      { .2000000000f, .0625000000f, .0370370373f, .0208333333f, .0074074073f },
      { .2127659619f, .0555555556f, .0329218097f, .0208333333f, .0123456791f },
      { .2173913121f, .0473484844f, .0280583613f, .0347222239f, .0205761325f },
      { .2173913121f, .0347222239f, .0205761325f, .0473484844f, .0280583613f },
      { .2127659619f, .0208333333f, .0123456791f, .0555555556f, .0329218097f },
      { .2000000000f, .0208333333f, .0074074073f, .0625000000f, .0370370370f },
      { .1996007860f, .0022727272f, .0013468013f, .0781250000f, .0462962948f },
      { .1562500000f,-.0062500000f,-.0037037037f, .0625000000f, .0370370370f },
      { .1250000000f,-.0164473690f,-.0097465888f, .0558035709f, .0330687836f },
      { .0416666667f,-.0208333333f,-.0123456791f, .0000000000f, .0000000000f },
      { .0100000000f,-.0069444444f,-.0018416207f,-.0037037037f,-.0020000000f } },

    { { .0050000000f,-.0200000000f, .0125000000f,-.3030303030f, .0020000000f },
      { .1041666642f, .0400000000f,-.0250000000f, .0333333333f,-.0200000000f },
      { .1250000000f, .0100000000f, .0142857144f,-.0500000007f,-.0200000000f },
      { .1562500000f,-.0006250000f,-.00049382716f,-.000625000f,-.00049382716f },
      { .1562500000f,-.0006250000f,-.00049382716f,-.000625000f,-.00049382716f },
      { .1250000000f,-.0500000000f,-.0200000000f, .0100000000f, .0142857144f },
      { .1041666667f, .0333333333f,-.0200000000f, .0400000000f,-.0250000000f },
      { .0050000000f,-.3030303030f, .0020000001f,-.0200000000f, .0125000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f } },

    { { .1428571492f, .1250000000f,-.0285714287f,-.0357142873f, .0208333333f },
      { .1818181818f, .0588235296f, .0333333333f, .0212765951f, .0100000000f },
      { .1818181818f, .0212765951f, .0100000000f, .0588235296f, .0333333333f },
      { .1428571492f,-.0357142873f, .0208333333f, .1250000000f,-.0285714287f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f } },

    { { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f },
      { .0000000000f, .0000000000f, .0000000000f, .0000000000f, .0000000000f } }
};

static const float fft_tone_level_table[2][64] = { {
/* pow ~ (i > 46) ? 0 : (((((i & 1) ? 431 : 304) << (i >> 1))) / 1024.0); */
    0.17677669f, 0.42677650f, 0.60355347f, 0.85355347f,
    1.20710683f, 1.68359375f, 2.37500000f, 3.36718750f,
    4.75000000f, 6.73437500f, 9.50000000f, 13.4687500f,
    19.0000000f, 26.9375000f, 38.0000000f, 53.8750000f,
    76.0000000f, 107.750000f, 152.000000f, 215.500000f,
    304.000000f, 431.000000f, 608.000000f, 862.000000f,
    1216.00000f, 1724.00000f, 2432.00000f, 3448.00000f,
    4864.00000f, 6896.00000f, 9728.00000f, 13792.0000f,
    19456.0000f, 27584.0000f, 38912.0000f, 55168.0000f,
    77824.0000f, 110336.000f, 155648.000f, 220672.000f,
    311296.000f, 441344.000f, 622592.000f, 882688.000f,
    1245184.00f, 1765376.00f, 2490368.00f, 0.00000000f,
    0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
    0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
    0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
    0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
  }, {
/* pow = (i > 45) ? 0 : ((((i & 1) ? 431 : 304) << (i >> 1)) / 512.0); */
    0.59375000f, 0.84179688f, 1.18750000f, 1.68359375f,
    2.37500000f, 3.36718750f, 4.75000000f, 6.73437500f,
    9.50000000f, 13.4687500f, 19.0000000f, 26.9375000f,
    38.0000000f, 53.8750000f, 76.0000000f, 107.750000f,
    152.000000f, 215.500000f, 304.000000f, 431.000000f,
    608.000000f, 862.000000f, 1216.00000f, 1724.00000f,
    2432.00000f, 3448.00000f, 4864.00000f, 6896.00000f,
    9728.00000f, 13792.0000f, 19456.0000f, 27584.0000f,
    38912.0000f, 55168.0000f, 77824.0000f, 110336.000f,
    155648.000f, 220672.000f, 311296.000f, 441344.000f,
    622592.000f, 882688.000f, 1245184.00f, 1765376.00f,
    2490368.00f, 3530752.00f, 0.00000000f, 0.00000000f,
    0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
    0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
    0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f,
    0.00000000f, 0.00000000f, 0.00000000f, 0.00000000f
} };

static const float fft_tone_envelope_table[4][31] = {
    { .009607375f, .038060248f, .084265202f, .146446645f, .222214907f, .308658302f,
      .402454883f, .500000060f, .597545207f, .691341758f, .777785182f, .853553414f,
      .915734828f, .961939812f, .990392685f, 1.00000000f, .990392625f, .961939752f,
      .915734768f, .853553295f, .777785063f, .691341639f, .597545087f, .500000000f,
      .402454853f, .308658272f, .222214878f, .146446615f, .084265172f, .038060218f,
      .009607345f },
    { .038060248f, .146446645f, .308658302f, .500000060f, .691341758f, .853553414f,
      .961939812f, 1.00000000f, .961939752f, .853553295f, .691341639f, .500000000f,
      .308658272f, .146446615f, .038060218f, .000000000f, .000000000f, .000000000f,
      .000000000f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f },
    { .146446645f, .500000060f, .853553414f, 1.00000000f, .853553295f, .500000000f,
      .146446615f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f },
    { .500000060f, 1.00000000f, .500000000f, .000000000f, .000000000f, .000000000f,
      .000000000f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f, .000000000f, .000000000f, .000000000f, .000000000f, .000000000f,
      .000000000f }
};

static const float sb_noise_attenuation[32] = {
    0.0f, 0.0f, 0.3f, 0.4f, 0.5f, 0.7f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
};

static const uint8_t fft_subpackets[32] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0
};

/* first index is joined_stereo, second index is 0 or 2 (1 is unused) */
static float dequant_1bit[2][3] = {
    {-0.920000f, 0.000000f, 0.920000f },
    {-0.890000f, 0.000000f, 0.890000f }
};

static const float type30_dequant[8] = {
   -1.0f,-0.625f,-0.291666656732559f,0.0f,
   0.25f,0.5f,0.75f,1.0f,
};

static const float type34_delta[10] = { // FIXME: covers 8 entries..
    -1.0f,-0.60947573184967f,-0.333333343267441f,-0.138071194291115f,0.0f,
    0.138071194291115f,0.333333343267441f,0.60947573184967f,1.0f,0.0f,
};

#endif /* QDM2DATA_H */
