/*
 *  transform.cc
 *
 * Description of this file:
 *    Transform functions definition of the davs2 library
 *
 * --------------------------------------------------------------------------
 *
 *    davs2 - video encoder of AVS2/IEEE1857.4 video coding standard
 *    Copyright (C) 2018~ VCL, NELVT, Peking University
 *
 *    Authors: Falei LUO <falei.luo@gmail.com>
 *             etc.
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 *    This program is also available under a commercial proprietary license.
 *    For more information, contact us at sswang @ pku.edu.cn.
 */

#include "common.h"
#include "quant.h"
#include "transform.h"
#include "block_info.h"

#if HAVE_MMX
#include "vec/intrinsic.h"
#include "x86/dct8.h"
#endif

/**
 * ===========================================================================
 * global & local variables
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * transform */
#define LOT_MAX_WLT_TAP         2     // number of wavelet transform tap, (5-3)


/* ---------------------------------------------------------------------------
 */
static const int16_t g_T4[4][4] = {
    { 32,  32,  32,  32 },
    { 42,  17, -17, -42 },
    { 32, -32, -32,  32 },
    { 17, -42,  42, -17 }
};

/* ---------------------------------------------------------------------------
 */
static const int16_t g_T8[8][8] = {
    { 32,  32,  32,  32,  32,  32,  32,  32 },
    { 44,  38,  25,   9,  -9, -25, -38, -44 },
    { 42,  17, -17, -42, -42, -17,  17,  42 },
    { 38,  -9, -44, -25,  25,  44,   9, -38 },
    { 32, -32, -32,  32,  32, -32, -32,  32 },
    { 25, -44,   9,  38, -38,  -9,  44, -25 },
    { 17, -42,  42, -17, -17,  42, -42,  17 },
    {  9, -25,  38, -44,  44, -38,  25,  -9 }
};

/* ---------------------------------------------------------------------------
 */
static const int16_t g_T16[16][16] = {
    { 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32,  32 },
    { 45,  43,  40,  35,  29,  21,  13,   4,  -4, -13, -21, -29, -35, -40, -43, -45 },
    { 44,  38,  25,   9,  -9, -25, -38, -44, -44, -38, -25,  -9,   9,  25,  38,  44 },
    { 43,  29,   4, -21, -40, -45, -35, -13,  13,  35,  45,  40,  21,  -4, -29, -43 },
    { 42,  17, -17, -42, -42, -17,  17,  42,  42,  17, -17, -42, -42, -17,  17,  42 },
    { 40,   4, -35, -43, -13,  29,  45,  21, -21, -45, -29,  13,  43,  35,  -4, -40 },
    { 38,  -9, -44, -25,  25,  44,   9, -38, -38,   9,  44,  25, -25, -44,  -9,  38 },
    { 35, -21, -43,   4,  45,  13, -40, -29,  29,  40, -13, -45,  -4,  43,  21, -35 },
    { 32, -32, -32,  32,  32, -32, -32,  32,  32, -32, -32,  32,  32, -32, -32,  32 },
    { 29, -40, -13,  45,  -4, -43,  21,  35, -35, -21,  43,   4, -45,  13,  40, -29 },
    { 25, -44,   9,  38, -38,  -9,  44, -25, -25,  44,  -9, -38,  38,   9, -44,  25 },
    { 21, -45,  29,  13, -43,  35,   4, -40,  40,  -4, -35,  43, -13, -29,  45, -21 },
    { 17, -42,  42, -17, -17,  42, -42,  17,  17, -42,  42, -17, -17,  42, -42,  17 },
    { 13, -35,  45, -40,  21,   4, -29,  43, -43,  29,  -4, -21,  40, -45,  35, -13 },
    {  9, -25,  38, -44,  44, -38,  25,  -9,  -9,  25, -38,  44, -44,  38, -25,   9 },
    {  4, -13,  21, -29,  35, -40,  43, -45,  45, -43,  40, -35,  29, -21,  13,  -4 }
};

/* ---------------------------------------------------------------------------
 */
static const int16_t g_T32[32][32] = {
    { 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32 },
    { 45, 45, 44, 43, 41, 39, 36, 34, 30, 27, 23, 19, 15, 11,  7,  2, -2, -7,-11,-15,-19,-23,-27,-30,-34,-36,-39,-41,-43,-44,-45,-45 },
    { 45, 43, 40, 35, 29, 21, 13,  4, -4,-13,-21,-29,-35,-40,-43,-45,-45,-43,-40,-35,-29,-21,-13, -4,  4, 13, 21, 29, 35, 40, 43, 45 },
    { 45, 41, 34, 23, 11, -2,-15,-27,-36,-43,-45,-44,-39,-30,-19, -7,  7, 19, 30, 39, 44, 45, 43, 36, 27, 15,  2,-11,-23,-34,-41,-45 },
    { 44, 38, 25,  9, -9,-25,-38,-44,-44,-38,-25, -9,  9, 25, 38, 44, 44, 38, 25,  9, -9,-25,-38,-44,-44,-38,-25, -9,  9, 25, 38, 44 },
    { 44, 34, 15, -7,-27,-41,-45,-39,-23, -2, 19, 36, 45, 43, 30, 11,-11,-30,-43,-45,-36,-19,  2, 23, 39, 45, 41, 27,  7,-15,-34,-44 },
    { 43, 29,  4,-21,-40,-45,-35,-13, 13, 35, 45, 40, 21, -4,-29,-43,-43,-29, -4, 21, 40, 45, 35, 13,-13,-35,-45,-40,-21,  4, 29, 43 },
    { 43, 23, -7,-34,-45,-36,-11, 19, 41, 44, 27, -2,-30,-45,-39,-15, 15, 39, 45, 30,  2,-27,-44,-41,-19, 11, 36, 45, 34, 7, -23,-43 },
    { 42, 17,-17,-42,-42,-17, 17, 42, 42, 17,-17,-42,-42,-17, 17, 42, 42, 17,-17,-42,-42,-17, 17, 42, 42, 17,-17,-42,-42,-17, 17, 42 },
    { 41, 11,-27,-45,-30,  7, 39, 43, 15,-23,-45,-34,  2, 36, 44, 19,-19,-44,-36, -2, 34, 45, 23,-15,-43,-39, -7, 30, 45, 27,-11,-41 },
    { 40,  4,-35,-43,-13, 29, 45, 21,-21,-45,-29, 13, 43, 35, -4,-40,-40, -4, 35, 43, 13,-29,-45,-21, 21, 45, 29,-13,-43,-35,  4, 40 },
    { 39, -2,-41,-36,  7, 43, 34,-11,-44,-30, 15, 45, 27,-19,-45,-23, 23, 45, 19,-27,-45,-15, 30, 44, 11,-34,-43, -7, 36, 41,  2,-39 },
    { 38, -9,-44,-25, 25, 44,  9,-38,-38,  9, 44, 25,-25,-44, -9, 38, 38, -9,-44,-25, 25, 44,  9,-38,-38,  9, 44, 25,-25,-44, -9, 38 },
    { 36,-15,-45,-11, 39, 34,-19,-45, -7, 41, 30,-23,-44, -2, 43, 27,-27,-43,  2, 44, 23,-30,-41,  7, 45, 19,-34,-39, 11, 45, 15,-36 },
    { 35,-21,-43,  4, 45, 13,-40,-29, 29, 40,-13,-45, -4, 43, 21,-35,-35, 21, 43, -4,-45,-13, 40, 29,-29,-40, 13, 45,  4,-43,-21, 35 },
    { 34,-27,-39, 19, 43,-11,-45,  2, 45,  7,-44,-15, 41, 23,-36,-30, 30, 36,-23,-41, 15, 44, -7,-45, -2, 45, 11,-43,-19, 39, 27,-34 },
    { 32,-32,-32, 32, 32,-32,-32, 32, 32,-32,-32, 32, 32,-32,-32, 32, 32,-32,-32, 32, 32,-32,-32, 32, 32,-32,-32, 32, 32,-32,-32, 32 },
    { 30,-36,-23, 41, 15,-44, -7, 45, -2,-45, 11, 43,-19,-39, 27, 34,-34,-27, 39, 19,-43,-11, 45,  2,-45,  7, 44,-15,-41, 23, 36,-30 },
    { 29,-40,-13, 45, -4,-43, 21, 35,-35,-21, 43,  4,-45, 13, 40,-29,-29, 40, 13,-45,  4, 43,-21,-35, 35, 21,-43, -4, 45,-13,-40, 29 },
    { 27,-43, -2, 44,-23,-30, 41,  7,-45, 19, 34,-39,-11, 45,-15,-36, 36, 15,-45, 11, 39,-34,-19, 45, -7,-41, 30, 23,-44,  2, 43,-27 },
    { 25,-44,  9, 38,-38, -9, 44,-25,-25, 44, -9,-38, 38,  9,-44, 25, 25,-44,  9, 38,-38, -9, 44,-25,-25, 44, -9,-38, 38,  9,-44, 25 },
    { 23,-45, 19, 27,-45, 15, 30,-44, 11, 34,-43,  7, 36,-41,  2, 39,-39, -2, 41,-36, -7, 43,-34,-11, 44,-30,-15, 45,-27,-19, 45,-23 },
    { 21,-45, 29, 13,-43, 35,  4,-40, 40, -4,-35, 43,-13,-29, 45,-21,-21, 45,-29,-13, 43,-35, -4, 40,-40,  4, 35,-43, 13, 29,-45, 21 },
    { 19,-44, 36, -2,-34, 45,-23,-15, 43,-39,  7, 30,-45, 27, 11,-41, 41,-11,-27, 45,-30, -7, 39,-43, 15, 23,-45, 34,  2,-36, 44,-19 },
    { 17,-42, 42,-17,-17, 42,-42, 17, 17,-42, 42,-17,-17, 42,-42, 17, 17,-42, 42,-17,-17, 42,-42, 17, 17,-42, 42,-17,-17, 42,-42, 17 },
    { 15,-39, 45,-30,  2, 27,-44, 41,-19,-11, 36,-45, 34, -7,-23, 43,-43, 23,  7,-34, 45,-36, 11, 19,-41, 44,-27, -2, 30,-45, 39,-15 },
    { 13,-35, 45,-40, 21,  4,-29, 43,-43, 29, -4,-21, 40,-45, 35,-13,-13, 35,-45, 40,-21, -4, 29,-43, 43,-29,  4, 21,-40, 45,-35, 13 },
    { 11,-30, 43,-45, 36,-19, -2, 23,-39, 45,-41, 27, -7,-15, 34,-44, 44,-34, 15,  7,-27, 41,-45, 39,-23,  2, 19,-36, 45,-43, 30,-11 },
    {  9,-25, 38,-44, 44,-38, 25, -9, -9, 25,-38, 44,-44, 38,-25,  9,  9,-25, 38,-44, 44,-38, 25, -9, -9, 25,-38, 44,-44, 38,-25,  9 },
    {  7,-19, 30,-39, 44,-45, 43,-36, 27,-15,  2, 11,-23, 34,-41, 45,-45, 41,-34, 23,-11, -2, 15,-27, 36,-43, 45,-44, 39,-30, 19, -7 },
    {  4,-13, 21,-29, 35,-40, 43,-45, 45,-43, 40,-35, 29,-21, 13, -4, -4, 13,-21, 29,-35, 40,-43, 45,-45, 43,-40, 35,-29, 21,-13,  4 },
    {  2, -7, 11,-15, 19,-23, 27,-30, 34,-36, 39,-41, 43,-44, 45,-45, 45,-45, 44,-43, 41,-39, 36,-34, 30,-27, 23,-19, 15,-11,  7, -2 }
};

/* ---------------------------------------------------------------------------
 */
ALIGN16(static const int16_t g_2T[SEC_TR_SIZE * SEC_TR_SIZE]) = {
    123,  -35,  -8,  -3,
    -32, -120,  30,  10,
     14,   25, 123, -22,
      8,   13,  19, 126
};

/* ---------------------------------------------------------------------------
 */
ALIGN16(static const int16_t g_2T_C[SEC_TR_SIZE * SEC_TR_SIZE]) = {
    34,  58,  72,  81,
    77,  69,  -7, -75,
    79, -33, -75,  58,
    55, -84,  73, -28
};


/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static void partialButterflyInverse4_c(const coeff_t *src, coeff_t *dst, int shift, int line, int clip_depth)
{
    int E[2], O[2];
    const int max_val = (1 << (clip_depth - 1)) - 1;
    const int min_val = -max_val - 1;
    const int add     = 1 << (shift - 1);
    int j;

    for (j = 0; j < line; j++) {
        /* utilizing symmetry properties to the maximum to
         * minimize the number of multiplications */
        O[0] = g_T4[1][0] * src[line] + g_T4[3][0] * src[3 * line];
        O[1] = g_T4[1][1] * src[line] + g_T4[3][1] * src[3 * line];
        E[0] = g_T4[0][0] * src[0   ] + g_T4[2][0] * src[2 * line];
        E[1] = g_T4[0][1] * src[0   ] + g_T4[2][1] * src[2 * line];

        /* combining even and odd terms at each hierarchy levels to
         * calculate the final spatial domain vector */
        dst[0] = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[0] + O[0] + add) >> shift));
        dst[1] = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[1] + O[1] + add) >> shift));
        dst[2] = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[1] - O[1] + add) >> shift));
        dst[3] = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[0] - O[0] + add) >> shift));

        src++;
        dst += 4;
    }
}

/* ---------------------------------------------------------------------------
 */
static void idct_4x4_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
#define BSIZE   4
    ALIGN32(coeff_t coeff[BSIZE * BSIZE]);
    ALIGN32(coeff_t block[BSIZE * BSIZE]);
    int shift1 = 5;
    int shift2 = 20 - g_bit_depth;
    int clip_depth1 = LIMIT_BIT;
    int clip_depth2 = g_bit_depth + 1;
    int i;

    partialButterflyInverse4_c(  src, coeff, shift1, BSIZE, clip_depth1);
    partialButterflyInverse4_c(coeff, block, shift2, BSIZE, clip_depth2);

    for (i = 0; i < BSIZE; i++) {
        memcpy(dst, &block[i * BSIZE], BSIZE * sizeof(coeff_t));
        dst += i_dst;
    }
#undef BSIZE
}

/* ---------------------------------------------------------------------------
 */
static void partialButterflyInverse8_c(const coeff_t *src, coeff_t *dst, int shift, int line, int clip_depth)
{
    int E[4], O[4];
    int EE[2], EO[2];
    const int max_val = (1 << (clip_depth - 1)) - 1;
    const int min_val = -max_val - 1;
    const int add     = 1 << (shift - 1);
    int j, k;

    for (j = 0; j < line; j++) {
        /* utilizing symmetry properties to the maximum to
         * minimize the number of multiplications */
        for (k = 0; k < 4; k++) {
            O[k] = g_T8[1][k] * src[    line] +
                   g_T8[3][k] * src[3 * line] +
                   g_T8[5][k] * src[5 * line] +
                   g_T8[7][k] * src[7 * line];
        }

        EO[0] = g_T8[2][0] * src[2 * line] + g_T8[6][0] * src[6 * line];
        EO[1] = g_T8[2][1] * src[2 * line] + g_T8[6][1] * src[6 * line];
        EE[0] = g_T8[0][0] * src[0       ] + g_T8[4][0] * src[4 * line];
        EE[1] = g_T8[0][1] * src[0       ] + g_T8[4][1] * src[4 * line];

        /* combining even and odd terms at each hierarchy levels to
         * calculate the final spatial domain vector */
        E[0] = EE[0] + EO[0];
        E[3] = EE[0] - EO[0];
        E[1] = EE[1] + EO[1];
        E[2] = EE[1] - EO[1];

        for (k = 0; k < 4; k++) {
            dst[k]     = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[k] + O[k] + add) >> shift));
            dst[k + 4] = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[3 - k] - O[3 - k] + add) >> shift));
        }

        src++;
        dst += 8;
    }
}

/* ---------------------------------------------------------------------------
 */
static void idct_8x8_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
#define BSIZE   8
    ALIGN32(coeff_t coeff[BSIZE * BSIZE]);
    ALIGN32(coeff_t block[BSIZE * BSIZE]);
    int shift1 = 5;
    int shift2 = 20 - g_bit_depth;
    int clip_depth1 = LIMIT_BIT;
    int clip_depth2 = g_bit_depth + 1;
    int i;

    partialButterflyInverse8_c(  src, coeff, shift1, BSIZE, clip_depth1);
    partialButterflyInverse8_c(coeff, block, shift2, BSIZE, clip_depth2);

    for (i = 0; i < BSIZE; i++) {
        memcpy(&dst[0], &block[i * BSIZE], BSIZE * sizeof(coeff_t));
        dst += i_dst;
    }
#undef BSIZE
}

/* ---------------------------------------------------------------------------
 */
static void partialButterflyInverse16_c(const coeff_t *src, coeff_t *dst, int shift, int line, int clip_depth)
{
    int E[8], O[8];
    int EE[4], EO[4];
    int EEE[2], EEO[2];
    const int max_val = (1 << (clip_depth - 1)) - 1;
    const int min_val = -max_val - 1;
    const int add     = 1 << (shift - 1);
    int j, k;

    for (j = 0; j < line; j++) {
        /* utilizing symmetry properties to the maximum to
         * minimize the number of multiplications */
        for (k = 0; k < 8; k++) {
            O[k] = g_T16[ 1][k] * src[     line] +
                   g_T16[ 3][k] * src[ 3 * line] +
                   g_T16[ 5][k] * src[ 5 * line] +
                   g_T16[ 7][k] * src[ 7 * line] +
                   g_T16[ 9][k] * src[ 9 * line] +
                   g_T16[11][k] * src[11 * line] +
                   g_T16[13][k] * src[13 * line] +
                   g_T16[15][k] * src[15 * line];
        }

        for (k = 0; k < 4; k++) {
            EO[k] = g_T16[ 2][k] * src[ 2 * line] +
                    g_T16[ 6][k] * src[ 6 * line] +
                    g_T16[10][k] * src[10 * line] +
                    g_T16[14][k] * src[14 * line];
        }

        EEO[0] = g_T16[4][0] * src[4 * line] + g_T16[12][0] * src[12 * line];
        EEE[0] = g_T16[0][0] * src[0       ] + g_T16[ 8][0] * src[ 8 * line];
        EEO[1] = g_T16[4][1] * src[4 * line] + g_T16[12][1] * src[12 * line];
        EEE[1] = g_T16[0][1] * src[0       ] + g_T16[ 8][1] * src[ 8 * line];

        /* combining even and odd terms at each hierarchy levels to
         * calculate the final spatial domain vector */
        for (k = 0; k < 2; k++) {
            EE[k    ] = EEE[k    ] + EEO[k    ];
            EE[k + 2] = EEE[1 - k] - EEO[1 - k];
        }

        for (k = 0; k < 4; k++) {
            E[k    ] = EE[k    ] + EO[k    ];
            E[k + 4] = EE[3 - k] - EO[3 - k];
        }

        for (k = 0; k < 8; k++) {
            dst[k]     = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[k] + O[k] + add) >> shift));
            dst[k + 8] = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[7 - k] - O[7 - k] + add) >> shift));
        }

        src++;
        dst += 16;
    }
}


/* ---------------------------------------------------------------------------
 */
static void idct_16x16_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
#define BSIZE   16
    ALIGN32(coeff_t coeff[BSIZE * BSIZE]);
    ALIGN32(coeff_t block[BSIZE * BSIZE]);
    int shift1 = 5;
    int shift2 = 20 - g_bit_depth;
    int clip_depth1 = LIMIT_BIT;
    int clip_depth2 = g_bit_depth + 1;
    int i;

    partialButterflyInverse16_c(  src, coeff, shift1, BSIZE, clip_depth1);
    partialButterflyInverse16_c(coeff, block, shift2, BSIZE, clip_depth2);

    for (i = 0; i < BSIZE; i++) {
        memcpy(&dst[0], &block[i * BSIZE], BSIZE * sizeof(coeff_t));
        dst += i_dst;
    }
#undef BSIZE
}

/* ---------------------------------------------------------------------------
 */
static void partialButterflyInverse32_c(const coeff_t *src, coeff_t *dst, int shift, int line, int clip_depth)
{
    int E[16], O[16];
    int EE[8], EO[8];
    int EEE[4], EEO[4];
    int EEEE[2], EEEO[2];
    const int max_val = (1 << (clip_depth - 1)) - 1;
    const int min_val = -max_val - 1;
    const int add     = 1 << (shift - 1);
    int j, k;

    for (j = 0; j < line; j++) {
        /* utilizing symmetry properties to the maximum to
         * minimize the number of multiplications */
        for (k = 0; k < 16; k++) {
            O[k] = g_T32[ 1][k] * src[     line] +
                   g_T32[ 3][k] * src[ 3 * line] +
                   g_T32[ 5][k] * src[ 5 * line] +
                   g_T32[ 7][k] * src[ 7 * line] +
                   g_T32[ 9][k] * src[ 9 * line] +
                   g_T32[11][k] * src[11 * line] +
                   g_T32[13][k] * src[13 * line] +
                   g_T32[15][k] * src[15 * line] +
                   g_T32[17][k] * src[17 * line] +
                   g_T32[19][k] * src[19 * line] +
                   g_T32[21][k] * src[21 * line] +
                   g_T32[23][k] * src[23 * line] +
                   g_T32[25][k] * src[25 * line] +
                   g_T32[27][k] * src[27 * line] +
                   g_T32[29][k] * src[29 * line] +
                   g_T32[31][k] * src[31 * line];
        }

        for (k = 0; k < 8; k++) {
            EO[k] = g_T32[ 2][k] * src[ 2 * line] +
                    g_T32[ 6][k] * src[ 6 * line] +
                    g_T32[10][k] * src[10 * line] +
                    g_T32[14][k] * src[14 * line] +
                    g_T32[18][k] * src[18 * line] +
                    g_T32[22][k] * src[22 * line] +
                    g_T32[26][k] * src[26 * line] +
                    g_T32[30][k] * src[30 * line];
        }

        for (k = 0; k < 4; k++) {
            EEO[k] = g_T32[ 4][k] * src[ 4 * line] +
                     g_T32[12][k] * src[12 * line] +
                     g_T32[20][k] * src[20 * line] +
                     g_T32[28][k] * src[28 * line];
        }

        EEEO[0] = g_T32[8][0] * src[8 * line] + g_T32[24][0] * src[24 * line];
        EEEO[1] = g_T32[8][1] * src[8 * line] + g_T32[24][1] * src[24 * line];
        EEEE[0] = g_T32[0][0] * src[0       ] + g_T32[16][0] * src[16 * line];
        EEEE[1] = g_T32[0][1] * src[0       ] + g_T32[16][1] * src[16 * line];

        /* combining even and odd terms at each hierarchy levels to
         * calculate the final spatial domain vector */
        EEE[0] = EEEE[0] + EEEO[0];
        EEE[3] = EEEE[0] - EEEO[0];
        EEE[1] = EEEE[1] + EEEO[1];
        EEE[2] = EEEE[1] - EEEO[1];
        for (k = 0; k < 4; k++) {
            EE[k    ] = EEE[k    ] + EEO[k    ];
            EE[k + 4] = EEE[3 - k] - EEO[3 - k];
        }

        for (k = 0; k < 8; k++) {
            E[k    ] = EE[k    ] + EO[k    ];
            E[k + 8] = EE[7 - k] - EO[7 - k];
        }

        for (k = 0; k < 16; k++) {
            dst[k]      = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[k] + O[k] + add) >> shift));
            dst[k + 16] = (coeff_t)DAVS2_CLIP3(min_val, max_val, ((E[15 - k] - O[15 - k] + add) >> shift));
        }

        src++;
        dst += 32;
    }
}


/* ---------------------------------------------------------------------------
 * NOTE:
 * i_dst - the stride of dst (the lowest bit is additional wavelet flag)
 */
static void idct_32x32_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
#define BSIZE   32
    ALIGN32(coeff_t coeff[BSIZE * BSIZE]);
    ALIGN32(coeff_t block[BSIZE * BSIZE]);
    int a_flag = i_dst & 0x01;
    int shift1 = 5;
    int shift2 = 20 - g_bit_depth - a_flag;
    int clip_depth1 = LIMIT_BIT;
    int clip_depth2 = g_bit_depth + 1 + a_flag;
    int i;

    i_dst &= 0xFE;    /* remember to remove the flag bit */

    partialButterflyInverse32_c(  src, coeff, shift1, BSIZE, clip_depth1);
    partialButterflyInverse32_c(coeff, block, shift2, BSIZE, clip_depth2);

    for (i = 0; i < BSIZE; i++) {
        memcpy(&dst[0], &block[i * BSIZE], BSIZE * sizeof(coeff_t));
        dst += i_dst;
    }
#undef BSIZE
}

/* ---------------------------------------------------------------------------
 */
static void idct_64x64_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
    ALIGN32(coeff_t row_buf[64 + LOT_MAX_WLT_TAP * 2]);
    coeff_t *pExt = row_buf + LOT_MAX_WLT_TAP;
    const int N0  = 64;
    const int N1  = 64 >> 1;
    int x, y, offset;

    /* step 0: idct 32x32 transform */
    idct_32x32_c(src, dst, i_dst | 1);

    /* step 1: vertical transform */
    for (x = 0; x < N0; x++) {
        /* copy */
        for (y = 0, offset = 0; y < N1; y++, offset += 32) {
            pExt[y << 1] = dst[x + offset];
        }

        /* reflection */
        pExt[N0] = pExt[N0 - 2];

        /* filtering (even pixel) */
        for (y = 0; y <= N0; y += 2) {
            pExt[y] >>= 1;
        }

        /* filtering (odd pixel) */
        for (y = 1; y < N0; y += 2) {
            pExt[y] = (pExt[y - 1] + pExt[y + 1]) >> 1;
        }

        /* copy */
        for (y = 0, offset = 0; y < N0; y++, offset += N0) {
            dst[x + offset] = pExt[y];
        }
    }

    /* step 2: horizontal transform */
    for (y = 0, offset = 0; y < N0; y++, offset += N0) {
        /* copy */
        for (x = 0; x < N1; x++) {
            pExt[x << 1] = dst[offset + x];
        }

        /* reflection */
        pExt[N0] = pExt[N0 - 2];

        /* filtering (odd pixel) */
        for (x = 1; x < N0; x += 2) {
            pExt[x] = (pExt[x - 1] + pExt[x + 1]) >> 1;
        }

        /* copy */
        memcpy(dst + offset, pExt, N0 * sizeof(coeff_t));
    }
}


/* ---------------------------------------------------------------------------
 */
static void idct_16x4_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
#define BSIZE_H   16
#define BSIZE_V   4
    ALIGN32(coeff_t coeff[BSIZE_H * BSIZE_V]);
    ALIGN32(coeff_t block[BSIZE_H * BSIZE_V]);
    int shift1 = 5;
    int shift2 = 20 - g_bit_depth;
    int clip_depth1 = LIMIT_BIT;
    int clip_depth2 = g_bit_depth + 1;
    int i;

    partialButterflyInverse4_c (src,   coeff, shift1, BSIZE_H, clip_depth1);
    partialButterflyInverse16_c(coeff, block, shift2, BSIZE_V, clip_depth2);

    for (i = 0; i < BSIZE_V; i++) {
        memcpy(&dst[i * i_dst], &block[i * BSIZE_H], BSIZE_H * sizeof(coeff_t));
    }
#undef BSIZE_H
#undef BSIZE_V
}

/* ---------------------------------------------------------------------------
 */
static void idct_4x16_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
#define BSIZE_H   4
#define BSIZE_V   16
    ALIGN32(coeff_t coeff[BSIZE_H * BSIZE_V]);
    ALIGN32(coeff_t block[BSIZE_H * BSIZE_V]);
    int shift1 = 5;
    int shift2 = 20 - g_bit_depth;
    int clip_depth1 = LIMIT_BIT;
    int clip_depth2 = g_bit_depth + 1;
    int i;

    partialButterflyInverse16_c(src,   coeff, shift1, BSIZE_H, clip_depth1);
    partialButterflyInverse4_c (coeff, block, shift2, BSIZE_V, clip_depth2);

    for (i = 0; i < BSIZE_V; i++) {
        memcpy(&dst[i * i_dst], &block[i * BSIZE_H], BSIZE_H * sizeof(coeff_t));
    }
#undef BSIZE_H
#undef BSIZE_V
}

/* ---------------------------------------------------------------------------
 * NOTE:
 * i_dst - the stride of dst (the lowest bit is additional wavelet flag)
 */
static void idct_32x8_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
#define BSIZE_H   32
#define BSIZE_V   8
    ALIGN32(coeff_t coeff[BSIZE_H * BSIZE_V]);
    ALIGN32(coeff_t block[BSIZE_H * BSIZE_V]);
    int shift1 = 5;
    int shift2 = 20 - g_bit_depth - (i_dst & 0x01);
    int clip_depth1 = LIMIT_BIT;
    int clip_depth2 = g_bit_depth + 1 + (i_dst & 0x01);
    int i;

    partialButterflyInverse8_c (src,   coeff, shift1, BSIZE_H, clip_depth1);
    partialButterflyInverse32_c(coeff, block, shift2, BSIZE_V, clip_depth2);

    i_dst &= 0xFE;
    for (i = 0; i < BSIZE_V; i++) {
        memcpy(&dst[i * i_dst], &block[i * BSIZE_H], BSIZE_H * sizeof(coeff_t));
    }
#undef BSIZE_H
#undef BSIZE_V
}


/* ---------------------------------------------------------------------------
 * NOTE:
 * i_dst - the stride of dst (the lowest bit is additional wavelet flag)
 */
static void idct_8x32_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
#define BSIZE_H   8
#define BSIZE_V   32
    ALIGN32(coeff_t coeff[BSIZE_H * BSIZE_V]);
    ALIGN32(coeff_t block[BSIZE_H * BSIZE_V]);
    int shift1 = 5;
    int shift2 = 20 - g_bit_depth - (i_dst & 0x01);
    int clip_depth1 = LIMIT_BIT;
    int clip_depth2 = g_bit_depth + 1 + (i_dst & 0x01);
    int i;

    partialButterflyInverse32_c(src,   coeff, shift1, BSIZE_H, clip_depth1);
    partialButterflyInverse8_c (coeff, block, shift2, BSIZE_V, clip_depth2);

    i_dst &= 0xFE;
    for (i = 0; i < BSIZE_V; i++) {
        memcpy(&dst[i * i_dst], &block[i * BSIZE_H], BSIZE_H * sizeof(coeff_t));
    }
#undef BSIZE_H
#undef BSIZE_V
}

/* ---------------------------------------------------------------------------
 */
static void idct_64x16_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
    ALIGN32(coeff_t row_buf[64 + LOT_MAX_WLT_TAP * 2]);
    coeff_t *pExt = row_buf + LOT_MAX_WLT_TAP;
    const int N0  = 64;
    const int N1  = 16;
    int x, y, offset;

    /* step 0: idct 32x32 transform */
    idct_32x8_c(src, dst, i_dst | 1);

    /* step 1: vertical transform */
    for (x = 0; x < (N0 >> 1); x++) {
        /* copy */
        for (y = 0, offset = 0; y < N1 >> 1; y++, offset += (N0 >> 1)) {
            pExt[y << 1] = dst[x + offset];
        }

        /* reflection */
        pExt[N1] = pExt[N1 - 2];

        /* filtering (even pixel) */
        for (y = 0; y <= N1; y += 2) {
            pExt[y] >>= 1;
        }

        /* filtering (odd pixel) */
        for (y = 1; y < N1; y += 2) {
            pExt[y] = (pExt[y - 1] + pExt[y + 1]) >> 1;
        }

        /* copy */
        for (y = 0, offset = 0; y < N1; y++, offset += N0) {
            dst[x + offset] = pExt[y];
        }
    }

    /* step 2: horizontal transform */
    for (y = 0, offset = 0; y < N1; y++, offset += N0) {
        /* copy */
        for (x = 0; x < N0 >> 1; x++) {
            pExt[x << 1] = dst[offset + x];
        }

        /* reflection */
        pExt[N0] = pExt[N0 - 2];

        /* filtering (odd pixel) */
        for (x = 1; x < N0; x += 2) {
            pExt[x] = (pExt[x - 1] + pExt[x + 1]) >> 1;
        }

        /* copy */
        memcpy(dst + offset, pExt, N0 * sizeof(coeff_t));
    }
}

/* ---------------------------------------------------------------------------
 */
static void idct_16x64_c(const coeff_t *src, coeff_t *dst, int i_dst)
{
    ALIGN32(coeff_t row_buf[64 + LOT_MAX_WLT_TAP * 2]);
    coeff_t *pExt = row_buf + LOT_MAX_WLT_TAP;
    const int N0 = 16;
    const int N1 = 64;
    int x, y, offset;

    /* step 0: idct 8x32 transform */
    idct_8x32_c(src, dst, i_dst | 1);

    /* step 1: vertical transform */
    for (x = 0; x < (N0 >> 1); x++) {
        /* copy */
        for (y = 0, offset = 0; y < N1 >> 1; y++, offset += (N0 >> 1)) {
            pExt[y << 1] = dst[x + offset];
        }

        /* reflection */
        pExt[N1] = pExt[N1 - 2];

        /* filtering (even pixel) */
        for (y = 0; y <= N1; y += 2) {
            pExt[y] >>= 1;
        }

        /* filtering (odd pixel) */
        for (y = 1; y < N1; y += 2) {
            pExt[y] = (pExt[y - 1] + pExt[y + 1]) >> 1;
        }

        /* copy */
        for (y = 0, offset = 0; y < N1; y++, offset += N0) {
            dst[x + offset] = pExt[y];
        }
    }

    /* step 2: horizontal transform */
    for (y = 0, offset = 0; y < N1; y++, offset += N0) {
        /* copy */
        for (x = 0; x < N0 >> 1; x++) {
            pExt[x << 1] = dst[offset + x];
        }

        /* reflection */
        pExt[N0] = pExt[N0 - 2];

        /* filtering (odd pixel) */
        for (x = 1; x < N0; x += 2) {
            pExt[x] = (pExt[x - 1] + pExt[x + 1]) >> 1;
        }

        /* copy */
        memcpy(dst + offset, pExt, N0 * sizeof(coeff_t));
    }
}

/* ---------------------------------------------------------------------------
 */
static void xTr2nd_4_1d_Inv_Ver(coeff_t *coeff, int i_coeff, int i_shift, const int16_t *tc)
{
    int tmp_dct[SEC_TR_SIZE * SEC_TR_SIZE];
    const int add = 1 << (i_shift - 1);
    int i, j, k, sum;

    for (i = 0; i < SEC_TR_SIZE; i++) {
        for (j = 0; j < SEC_TR_SIZE; j++) {
            tmp_dct[i * SEC_TR_SIZE + j] = coeff[i * i_coeff + j];
        }
    }

    for (i = 0; i < SEC_TR_SIZE; i++) {
        for (j = 0; j < SEC_TR_SIZE; j++) {
            sum = add;
            for (k = 0; k < SEC_TR_SIZE; k++) {
                sum += tc[k * SEC_TR_SIZE + i] * tmp_dct[k * SEC_TR_SIZE + j];
            }
            coeff[i * i_coeff + j] = (coeff_t)DAVS2_CLIP3(-32768, 32767, sum >> i_shift);
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void xTr2nd_4_1d_Inv_Hor(coeff_t *coeff, int i_coeff, int i_shift, int clip_depth, const int16_t *tc)
{
    int tmp_dct[SEC_TR_SIZE * SEC_TR_SIZE];
    const int max_val = (1 << (clip_depth - 1)) - 1;
    const int min_val = -max_val - 1;
    const int add = 1 << (i_shift - 1);
    int i, j, k, sum;

    for (i = 0; i < SEC_TR_SIZE; i++) {
        for (j = 0; j < SEC_TR_SIZE; j++) {
            tmp_dct[i * SEC_TR_SIZE + j] = coeff[i * i_coeff + j];
        }
    }

    for (i = 0; i < SEC_TR_SIZE; i++) {
        for (j = 0; j < SEC_TR_SIZE; j++) {
            sum = add;
            for (k = 0; k < SEC_TR_SIZE; k++) {
                sum += tc[k * SEC_TR_SIZE + i] * tmp_dct[j * SEC_TR_SIZE + k];
            }
            coeff[j * i_coeff + i] = (coeff_t)DAVS2_CLIP3(min_val, max_val, sum >> i_shift);
        }
    }
}

/* ---------------------------------------------------------------------------
*/
static void inv_transform_4x4_2nd_c(coeff_t *coeff, int i_coeff)
{
    const int shift1 = 5;
    const int shift2 = 20 - g_bit_depth + 2;
    const int clip_depth2 = g_bit_depth + 1;

    xTr2nd_4_1d_Inv_Ver(coeff, i_coeff, shift1, g_2T_C);
    xTr2nd_4_1d_Inv_Hor(coeff, i_coeff, shift2, clip_depth2, g_2T_C);
}


/* ---------------------------------------------------------------------------
 * i_mode - real intra mode (luma)
 * b_top  - block top available?
 * b_left - block left available?
 */
static void inv_transform_2nd_c(coeff_t *coeff, int i_coeff, int i_mode, int b_top, int b_left)
{
    int vt = (i_mode >=  0 && i_mode <= 23);
    int ht = (i_mode >= 13 && i_mode <= 32) || (i_mode >= 0 && i_mode <= 2);

    if (ht && b_left) {
        xTr2nd_4_1d_Inv_Hor(coeff, i_coeff, 7, 16, g_2T);
    }
    if (vt && b_top) {
        xTr2nd_4_1d_Inv_Ver(coeff, i_coeff, 7, g_2T);
    }
}

/* ---------------------------------------------------------------------------
 */
static INLINE
void inv_transform(davs2_row_rec_t *row_rec, coeff_t *p_coeff, cu_t *p_cu, int i_coeff, int bsx, int bsy,
                   int b_secT, int blockidx, int i_luma_intra_mode)
{
    int part_idx = PART_INDEX(bsx, bsy);
    dct_t idct = gf_davs2.idct[part_idx][p_cu->dct_pattern[blockidx]];

    b_secT = b_secT && IS_INTRA(p_cu) && blockidx < 4;

    if (part_idx == PART_4x4) {
        if (b_secT) {
            gf_davs2.inv_transform_4x4_2nd(p_coeff, i_coeff);
        } else {
            idct(p_coeff, p_coeff, i_coeff);
        }
    } else {
        if (b_secT) {
            gf_davs2.inv_transform_2nd(p_coeff, i_coeff, i_luma_intra_mode, row_rec->b_block_avail_top, row_rec->b_block_avail_left);
        }
        idct(p_coeff, p_coeff, i_coeff);
    }
}


/* ---------------------------------------------------------------------------
 * copy region of h->lcu.residual[] corresponding to blockidx to p_dst
 */
static ALWAYS_INLINE
coeff_t *get_quanted_coeffs(davs2_row_rec_t *row_rec, cu_t *p_cu, int blockidx)
{
    int idx_cu_zscan = row_rec->idx_cu_zscan;
    coeff_t *p_res;

    if (blockidx < 4) {
        int block_offset = blockidx << ((p_cu->i_cu_level - 1) << 1);
        p_res = &row_rec->p_rec_info->coeff_buf_y[idx_cu_zscan << 6];
        p_res += block_offset;
    } else {
        p_res = &row_rec->p_rec_info->coeff_buf_uv[blockidx - 4][idx_cu_zscan << 4];
    }

    return p_res;
}


/* ---------------------------------------------------------------------------
 * get reconstruction pixels for blocks (include luma and chroma component)
 */
void davs2_get_recons(davs2_row_rec_t *row_rec, cu_t *p_cu, int blockidx, cb_t *p_tu, int ctu_x, int ctu_y)
{
    int bsx     = p_tu->w;
    int bsy     = p_tu->h;
    int x_start = p_tu->x;
    int y_start = p_tu->y;
    int b_luma  = blockidx < 4;
    int b_wavelet_conducted = (b_luma && p_cu->i_cu_level == B64X64_IN_BIT && p_cu->i_trans_size != TU_SPLIT_CROSS);
    coeff_t *p_coeff;
    pel_t *p_dst;
    int i_coeff;
    int i_dst;
    davs2_t *h = row_rec->h;

    assert(((p_cu->i_cbp >> blockidx) & 1) != 0);

    // inverse transform
    p_tu->v >>= b_wavelet_conducted;
    i_coeff = p_tu->w;
    p_coeff = get_quanted_coeffs(row_rec, p_cu, blockidx);
    inv_transform(row_rec, p_coeff, p_cu, i_coeff, bsx, bsy, h->seq_info.enable_2nd_transform, blockidx, p_cu->intra_pred_modes[blockidx]);
    i_coeff <<= b_wavelet_conducted;

    if (b_luma) {
        x_start += ctu_x;
        y_start += ctu_y;

        i_dst    = row_rec->ctu.i_fdec[0];
        p_dst    = row_rec->ctu.p_fdec[0] + y_start * i_dst + x_start;
    } else {
        x_start  = (ctu_x >> 1);
        y_start  = (ctu_y >> 1);

        i_dst    = row_rec->ctu.i_fdec[blockidx - 3];
        p_dst    = row_rec->ctu.p_fdec[blockidx - 3] + y_start * i_dst + x_start;
    }

    // normalize
    gf_davs2.add_ps[PART_INDEX(bsx, bsy)](p_dst, i_dst, p_dst, p_coeff, i_dst, i_coeff);
}

/* ---------------------------------------------------------------------------
 */
void davs2_dct_init(uint32_t cpuid, ao_funcs_t *fh)
{
    int i;
    UNUSED_PARAMETER(cpuid);

    /* init c function handles */
    fh->inv_transform_4x4_2nd = inv_transform_4x4_2nd_c;
    fh->inv_transform_2nd     = inv_transform_2nd_c;

    for (i = 0; i < DCT_PATTERN_NUM; i++) {
        fh->idct[PART_4x4  ][i] = idct_4x4_c;
        fh->idct[PART_8x8  ][i] = idct_8x8_c;
        fh->idct[PART_16x16][i] = idct_16x16_c;
        fh->idct[PART_32x32][i] = idct_32x32_c;
        fh->idct[PART_64x64][i] = idct_64x64_c;

        fh->idct[PART_4x16 ][i] = idct_4x16_c;
        fh->idct[PART_8x32 ][i] = idct_8x32_c;
        fh->idct[PART_16x4 ][i] = idct_16x4_c;
        fh->idct[PART_32x8 ][i] = idct_32x8_c;
        fh->idct[PART_64x16][i] = idct_64x16_c;
        fh->idct[PART_16x64][i] = idct_16x64_c;
    }

    /* init asm function handles */
#if HAVE_MMX
    /* functions defined in file intrinsic_dct.c */
    if (cpuid & DAVS2_CPU_SSE2) {
        fh->inv_transform_4x4_2nd = inv_transform_4x4_2nd_sse128;
        fh->inv_transform_2nd     = inv_transform_2nd_sse128;

        for (i = 0; i < DCT_PATTERN_NUM; i++) {
            fh->idct[PART_4x4  ][i] = idct_4x4_sse128;
            fh->idct[PART_8x8  ][i] = idct_8x8_sse128;
            fh->idct[PART_16x16][i] = idct_16x16_sse128;
            fh->idct[PART_32x32][i] = idct_32x32_sse128;
            fh->idct[PART_64x64][i] = idct_64x64_sse128;
            fh->idct[PART_64x16][i] = idct_64x16_sse128;
            fh->idct[PART_16x64][i] = idct_16x64_sse128;

            fh->idct[PART_4x16][i] = idct_4x16_sse128;
            fh->idct[PART_8x32][i] = idct_8x32_sse128;
            fh->idct[PART_16x4][i] = idct_16x4_sse128;
            fh->idct[PART_32x8][i] = idct_32x8_sse128;

#if !HIGH_BIT_DEPTH
            fh->idct[PART_4x4 ][i] = FPFX(idct_4x4_sse2);
#if ARCH_X86_64
            fh->idct[PART_8x8 ][i] = FPFX(idct_8x8_sse2);
#endif
#endif
        }
    }

    if (cpuid & DAVS2_CPU_SSSE3) {
        for (i = 0; i < DCT_PATTERN_NUM; i++) {
#if HIGH_BIT_DEPTH
            // 10bit assemble
#else
            fh->idct[PART_8x8 ][i] = davs2_idct_8x8_ssse3;
#endif
        }
    }

    /* TODO: 初始化非默认DCT模板 */
    if (cpuid & DAVS2_CPU_SSE2) {
        /* square */
        fh->idct[PART_8x8  ][DCT_HALF] = idct_8x8_half_sse128;
        fh->idct[PART_8x8  ][DCT_QUAD] = idct_8x8_quad_sse128;
        fh->idct[PART_16x16][DCT_HALF] = idct_16x16_half_sse128;
        fh->idct[PART_16x16][DCT_QUAD] = idct_16x16_quad_sse128;
        fh->idct[PART_32x32][DCT_HALF] = idct_32x32_half_sse128;
        fh->idct[PART_32x32][DCT_QUAD] = idct_32x32_quad_sse128;
        fh->idct[PART_64x64][DCT_HALF] = idct_64x64_half_sse128;
        fh->idct[PART_64x64][DCT_QUAD] = idct_64x64_quad_sse128;

        /* non-square */
        fh->idct[PART_4x16 ][DCT_HALF] = idct_4x16_half_sse128;
        fh->idct[PART_4x16 ][DCT_QUAD] = idct_4x16_quad_sse128;
        fh->idct[PART_16x4 ][DCT_HALF] = idct_16x4_half_sse128;
        fh->idct[PART_16x4 ][DCT_QUAD] = idct_16x4_quad_sse128;
        fh->idct[PART_8x32 ][DCT_QUAD] = idct_8x32_quad_sse128;
        fh->idct[PART_8x32 ][DCT_HALF] = idct_8x32_half_sse128;
        fh->idct[PART_32x8 ][DCT_HALF] = idct_32x8_half_sse128;
        fh->idct[PART_32x8 ][DCT_QUAD] = idct_32x8_quad_sse128;
        fh->idct[PART_16x64][DCT_HALF] = idct_16x64_half_sse128;
        fh->idct[PART_16x64][DCT_QUAD] = idct_16x64_quad_sse128;
        fh->idct[PART_64x16][DCT_HALF] = idct_64x16_half_sse128;
        fh->idct[PART_64x16][DCT_QUAD] = idct_64x16_quad_sse128;
    }

#if ARCH_X86_64
    if (cpuid & DAVS2_CPU_AVX2) {
        fh->idct[PART_8x8  ][DCT_DEAULT]   = idct_8x8_avx2;
        fh->idct[PART_16x16][DCT_DEAULT] = idct_16x16_avx2;
        fh->idct[PART_64x64][DCT_DEAULT] = idct_64x64_avx2;
        fh->idct[PART_64x16][DCT_DEAULT] = idct_64x16_avx2;
        fh->idct[PART_16x64][DCT_DEAULT] = idct_16x64_avx2;
        fh->idct[PART_32x32][DCT_DEAULT] = idct_32x32_avx2;    // @luofl i7-6700k 速度比sse128快一倍

        /* square */
        // fh->idct[PART_8x8  ][DCT_HALF] = idct_8x8_half_avx2;
        // fh->idct[PART_8x8  ][DCT_QUAD] = idct_8x8_quad_avx2;
        // fh->idct[PART_16x16][DCT_HALF] = idct_16x16_half_avx2;
        // fh->idct[PART_16x16][DCT_QUAD] = idct_16x16_quad_avx2;
        // fh->idct[PART_32x32][DCT_HALF] = idct_32x32_half_avx2;
        // fh->idct[PART_32x32][DCT_QUAD] = idct_32x32_quad_avx2;
        // fh->idct[PART_64x64][DCT_HALF] = idct_64x64_half_avx2;
        // fh->idct[PART_64x64][DCT_QUAD] = idct_64x64_quad_avx2;

        /* non-square */
        // fh->idct[PART_4x16 ][DCT_HALF] = idct_4x16_half_avx2;
        // fh->idct[PART_4x16 ][DCT_QUAD] = idct_4x16_quad_avx2;
        // fh->idct[PART_16x4 ][DCT_HALF] = idct_16x4_half_avx2;
        // fh->idct[PART_16x4 ][DCT_QUAD] = idct_16x4_quad_avx2;
        // fh->idct[PART_8x32 ][DCT_QUAD] = idct_8x32_quad_avx2;
        // fh->idct[PART_8x32 ][DCT_HALF] = idct_8x32_half_avx2;
        // fh->idct[PART_32x8 ][DCT_HALF] = idct_32x8_half_avx2;
        // fh->idct[PART_32x8 ][DCT_QUAD] = idct_32x8_quad_avx2;
        // fh->idct[PART_16x64][DCT_HALF] = idct_16x64_half_avx2;
        // fh->idct[PART_16x64][DCT_QUAD] = idct_16x64_quad_avx2;
        // fh->idct[PART_64x16][DCT_HALF] = idct_64x16_half_avx2;
        // fh->idct[PART_64x16][DCT_QUAD] = idct_64x16_quad_avx2;
    }
#endif  // if ARCH_X86_X64
#endif  // if HAVE_MMX
}
