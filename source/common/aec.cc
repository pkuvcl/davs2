/*
 *  aec.cc
 *
 * Description of this file:
 *    AEC functions definition of the davs2 library
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
#include "block_info.h"
#include "alf.h"
#include "aec.h"
#include "vlc.h"
#include "sao.h"
#include "scantab.h"

/**
 * ===========================================================================
 * macros
 * ===========================================================================
 */
#define CTRL_OPT_AEC                       1  /* 是否采用基于查表的AEC上下文状态更新 */
#define MAKE_CONTEXT(lg_pmps, mps, cycno)  (((uint16_t)(cycno) << 0) | ((uint16_t)(mps) << 2) | (uint16_t)(lg_pmps << 3))

/**
 * ===========================================================================
 * global & local variables
 * ===========================================================================
 */

#if AVS2_TRACE
int      symbolCount = 0;
#endif

#if CTRL_OPT_AEC
/* [8 * lg_pmps + 4 * mps + cycno] */
static context_t g_tab_ctx_mps[2048 * 4 * 2];
static context_t g_tab_ctx_lps[2048 * 4 * 2];
#endif

/* ---------------------------------------------------------------------------
 * 0: INTRA_PRED_VER
 * 1: INTRA_PRED_HOR
 * 2: INTRA_PRED_DC_DIAG
 */
const int tab_intra_mode_scan_type[NUM_INTRA_MODE] = {
    2, 2, 2, 1, 1, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 0
};

static const int EO_OFFSET_INV__MAP[] = { 1, 0, 2, -1, 3, 4, 5, 6 };
static const int T_Chr[5] = { 0, 1, 2, 4, 3000 };
static const int8_t tab_rank[6] = { 0, 1, 2, 3, 3, 4/*, 4 ...*/ };

static const uint8_t raster2ZZ_4x4[] = {
    0,  1,  5,  6,
    2,  4,  7, 12,
    3,  8, 11, 13,
    9, 10, 14, 15
};

static const uint8_t raster2ZZ_8x8[] = {
     0,  1,  5,  6, 14, 15, 27, 28,
     2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43,
     9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

static const uint8_t raster2ZZ_2x8[] = {
    0, 1, 4, 5,  8,  9, 12, 13,
    2, 3, 6, 7, 10, 11, 14, 15
};


static const uint8_t raster2ZZ_8x2[] = {
    0,  1,
    2,  4,
    3,  5,
    6,  8,
    7,  9,
    10, 12,
    11, 13,
    14, 15
};

static const uint8_t tab_scan_coeff_pos_in_cg[4][4] = {
    { 0,  1,  5,  6 },
    { 2,  4,  7, 12 },
    { 3,  8, 11, 13 },
    { 9, 10, 14, 15 }
};

static const uint8_t tab_cwr[] = {
    3, 3, 4, 5, 5, 5, 5 /* 5, 5, 5, 5 */
};

static const uint16_t tab_lg_pmps_offset[] = {
    0, 0, 0, 197, 95, 46 /* 5, 5, 5, 5 */
};

static const int tab_pdir_bskip[DS_MAX_NUM] = {
    PDIR_SYM, PDIR_BID, PDIR_BWD, PDIR_SYM, PDIR_FWD
};

/**
 * ===========================================================================
 * defines
 * ===========================================================================
 */

enum aec_const_e {
    LG_PMPS_SHIFTNO    = 2,
    B_BITS             = 10,
    QUARTER_SHIFT      = (B_BITS-2),
    HALF               = (1 << (B_BITS-1)),
    QUARTER            = (1 << (B_BITS-2)),
    AEC_VALUE_BOUND    = 254,     /* make sure rs1 will not overflow for 8-bit uint8_t */
};


static const int8_t tab_intra_mode_luma2chroma[NUM_INTRA_MODE] = {
    DC_PRED_C,   -1, BI_PRED_C, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    VERT_PRED_C, -1,        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    HOR_PRED_C,  -1,        -1, -1, -1, -1, -1, -1, -1
};


/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
int aec_get_next_bit(aec_t *p_aec)
{
    uint32_t next_bit;

    if (--p_aec->i_bits_to_go < 0) {
        int diff = p_aec->i_bytes - p_aec->i_byte_pos;
        uint8_t *p_buffer = p_aec->p_buffer + p_aec->i_byte_pos;

#if 1
        if (diff > 7) {
            p_aec->i_byte_buf = ((uint64_t)p_buffer[0] << 56) | ((uint64_t)p_buffer[1] << 48) | ((uint64_t)p_buffer[2] << 40) | ((uint64_t)p_buffer[3] << 32) |
                                ((uint64_t)p_buffer[4] << 24) | ((uint64_t)p_buffer[5] << 16) | ((uint64_t)p_buffer[6] <<  8) |  (uint64_t)p_buffer[7];
            p_aec->i_bits_to_go = 63;
            p_aec->i_byte_pos += 8;
        } else if (diff > 0) {
            /* 一帧剩余码流长度小于8，这在一帧图像解码过程中只出现一次 */
            int i;
            p_aec->i_bits_to_go += (int8_t)(diff << 3);
            p_aec->i_byte_pos += (p_aec->i_bits_to_go + 1) >> 3;

            p_aec->i_byte_buf = 0;
            for (i = 0; i < diff; i++) {
                p_aec->i_byte_buf = (p_aec->i_byte_buf << 8) | p_buffer[i];
            }
        } else {
            p_aec->b_bit_error = 1;
            return 1;
        }
#else
        int i;
        if (diff > 8) {
            diff = 8;
        } else if (diff <= 0) {
            p_aec->b_bit_error = 1;
            return 1;
        }
        p_aec->i_bits_to_go += (diff << 3);
        p_aec->i_byte_pos += (p_aec->i_bits_to_go + 1) >> 3;

        p_aec->i_byte_buf = 0;
        for (i = 0; i < diff; i++) {
            p_aec->i_byte_buf = (p_aec->i_byte_buf << 8) | p_buffer[i];
        }
#endif
    }

    /* get next bit */
    next_bit = ((p_aec->i_byte_buf >> p_aec->i_bits_to_go) & 0x01);

    p_aec->i_value_t = (p_aec->i_value_t << 1) | next_bit;

    return 0;
}



/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
int aec_get_next_n_bit(aec_t *p_aec, int num_bits)
{
    if (p_aec->i_bits_to_go >= num_bits) {
        uint32_t next_bits;
        p_aec->i_bits_to_go -= (int8_t)num_bits;
        next_bits = (p_aec->i_byte_buf >> p_aec->i_bits_to_go) & ((1 << num_bits) - 1);

        p_aec->i_value_t = (p_aec->i_value_t << num_bits) | next_bits;

        return 0;
    } else {
        for (; num_bits != 0; num_bits--) {
            aec_get_next_bit(p_aec);
        }
        return p_aec->b_bit_error;
    }
}
    
/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void update_ctx_mps(context_t *ctx)
{
#if CTRL_OPT_AEC
    ctx->v = g_tab_ctx_mps[ctx->v].v;
#else
    uint32_t lg_pmps = ctx->LG_PMPS;
    uint8_t  cycno   = (uint8_t)ctx->cycno; 
    uint32_t cwr     = tab_cwr[cycno];

    // update probability estimation and other parameters
    if (cycno == 0) {
        ctx->cycno = 1;
    }
    lg_pmps -= (lg_pmps >> cwr) + (lg_pmps >> (cwr + 2));

    ctx->LG_PMPS = (uint16_t)lg_pmps;
#endif
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void update_ctx_lps(context_t *ctx)
{
#if CTRL_OPT_AEC
    ctx->v = g_tab_ctx_lps[ctx->v].v;
#else
    uint32_t cycno   = ctx->cycno;
    uint32_t cwr     = tab_cwr[cycno];
    uint32_t lg_pmps = ctx->LG_PMPS + tab_lg_pmps_offset[cwr];
    uint32_t mps     = ctx->MPS;

    // update probability estimation and other parameters
    if (cycno != 3) {
        ++cycno;
    }

    if (lg_pmps >= (256 << LG_PMPS_SHIFTNO)) {
        lg_pmps  = (512 << LG_PMPS_SHIFTNO) - 1 - lg_pmps;
        mps = !mps;
    }

    ctx->v = MAKE_CONTEXT(lg_pmps, mps, cycno);
#endif
}

#if CTRL_OPT_AEC
/* ---------------------------------------------------------------------------
 */
void init_aec_context_tab(void)
{
    static bool_t b_inited = 0;
    context_t ctx_i;
    context_t ctx_o;
    int cycno;
    int mps;

    if (b_inited != 0) {
        return;
    }
    /* init context table */
    b_inited = 1;
    ctx_i.v = 0;
    ctx_o.v = 0;
    memset(g_tab_ctx_mps, 0, sizeof(g_tab_ctx_mps));
    memset(g_tab_ctx_lps, 0, sizeof(g_tab_ctx_lps));

    /* mps */
    for (cycno = 0; cycno < 4; cycno++) {
        uint32_t cwr = tab_cwr[cycno];
        ctx_i.cycno = cycno;
        ctx_o.cycno = (uint8_t)DAVS2_MAX(cycno, 1);

        for (mps = 0; mps < 2; mps++) {
            ctx_i.MPS = (uint8_t)mps;
            ctx_o.MPS = (uint8_t)mps;
            for (ctx_i.LG_PMPS = 0; ctx_i.LG_PMPS <= 1024; ctx_i.LG_PMPS++) {
                uint32_t lg_pmps = ctx_i.LG_PMPS;
                lg_pmps -= (lg_pmps >> cwr) + (lg_pmps >> (cwr + 2));
                ctx_o.LG_PMPS = (uint16_t)lg_pmps;
                g_tab_ctx_mps[ctx_i.v].v = ctx_o.v;
            }
        }
    }

    /* lps */
    for (cycno = 0; cycno < 4; cycno++) {
        uint32_t cwr = tab_cwr[cycno];
        ctx_i.cycno = cycno;
        ctx_o.cycno = (uint8_t)DAVS2_MIN(cycno + 1, 3);

        for (mps = 0; mps < 2; mps++) {
            ctx_i.MPS = (uint8_t)mps;
            ctx_o.MPS = (uint8_t)mps;
            for (ctx_i.LG_PMPS = 0; ctx_i.LG_PMPS <= 1024; ctx_i.LG_PMPS++) {
                uint32_t lg_pmps = ctx_i.LG_PMPS + tab_lg_pmps_offset[cwr];
                if (lg_pmps >= (256 << LG_PMPS_SHIFTNO)) {
                    lg_pmps = (512 << LG_PMPS_SHIFTNO) - 1 - lg_pmps;
                    ctx_o.MPS = !mps;
                }
                ctx_o.LG_PMPS = (uint16_t)lg_pmps;
                g_tab_ctx_lps[ctx_i.v].v = ctx_o.v;
            }
        }
    }
}
#endif

/* ---------------------------------------------------------------------------
 * initializes the aec_t for the arithmetic decoder
 */
int aec_start_decoding(aec_t *p_aec, uint8_t *p_start, int i_byte_pos, int i_bytes)
{
#if CTRL_OPT_AEC
    init_aec_context_tab();
#endif
    p_aec->p_buffer         = p_start;
    p_aec->i_byte_pos       = i_byte_pos;
    p_aec->i_bytes          = i_bytes;
    p_aec->i_bits_to_go     = 0;
    p_aec->b_bit_error      = 0;
    p_aec->b_val_domain     = 1;
    p_aec->i_s1             = 0;
    p_aec->i_t1             = QUARTER - 1; // 0xff
    p_aec->i_value_s        = 0;
    p_aec->i_value_t        = 0;

    if (p_aec->i_bits_to_go < B_BITS - 1) {
        if (aec_get_next_n_bit(p_aec, B_BITS - 1)) {
            return 0;
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
int aec_bits_read(aec_t *p_aec)
{
    return (p_aec->i_byte_pos << 3) - p_aec->i_bits_to_go;
}

/* ---------------------------------------------------------------------------
 */
static INLINE
int biari_decode_symbol(aec_t *p_aec, context_t *ctx)
{
    uint32_t lg_pmps = ctx->LG_PMPS >> LG_PMPS_SHIFTNO;
    uint32_t t2;
    uint32_t s2;
    uint32_t s_flag;
    uint32_t i_value_s = p_aec->i_value_s;
    int bit = ctx->MPS;
    int is_LPS;

    // p_aec->i_value_t is in R domain  p_aec->i_s1=0 or p_aec->i_s1 == AEC_VALUE_BOUND
    if (p_aec->b_val_domain != 0 || (p_aec->i_s1 == AEC_VALUE_BOUND && p_aec->b_val_bound != 0)) {
        i_value_s   = 0;
        p_aec->i_s1 = 0;

        while (p_aec->i_value_t < QUARTER && i_value_s < AEC_VALUE_BOUND) {
            if (aec_get_next_bit(p_aec)) {
                return 0;
            }
            i_value_s++;
        }

        p_aec->b_val_bound = p_aec->i_value_t < QUARTER;
        p_aec->i_value_t   = p_aec->i_value_t & 0xff;
    }

    if (p_aec->i_value_s > AEC_VALUE_BOUND) {
        /// davs2_log(NULL, DAVS2_LOG_ERROR, "p_aec->i_value_s (>254).");
        p_aec->b_bit_error = 1;
        p_aec->i_value_s   = i_value_s;
        return 0;
    }

    s_flag = p_aec->i_t1 < lg_pmps;
    s2     = p_aec->i_s1 + s_flag;
    t2     = p_aec->i_t1 - lg_pmps + (s_flag << 8);            // 8bits
    is_LPS = (s2 > i_value_s || (s2 == i_value_s && p_aec->i_value_t >= t2)) && p_aec->b_val_bound == 0;

    p_aec->b_val_domain = (bool_t)is_LPS;

    if (is_LPS) {     // LPS
        uint32_t t_rlps = (s_flag == 0) ? (lg_pmps) : (p_aec->i_t1 + lg_pmps);
        int n_bits = 0;
        bit = !bit;

        if (s2 == i_value_s) {
            p_aec->i_value_t -= t2;
        } else {
            if (aec_get_next_bit(p_aec)) {
                return 0;
            }
            p_aec->i_value_t += 256 - t2;
        }

        // restore range
        while (t_rlps < QUARTER) {
            t_rlps <<= 1;
            n_bits++;
        }
        if (n_bits) {
            if (aec_get_next_n_bit(p_aec, n_bits)) {
                return 0;
            }
        }

        p_aec->i_s1 = 0;
        p_aec->i_t1 = t_rlps & 0xff;
        update_ctx_lps(ctx);
    } else {        // MPS
        p_aec->i_s1 = s2;
        p_aec->i_t1 = t2;
        update_ctx_mps(ctx);
    }

    p_aec->i_value_s = i_value_s;

    return bit;
}

/* ---------------------------------------------------------------------------
 * return the decoded symbol
 */
static INLINE
int biari_decode_symbol_eq_prob(aec_t *p_aec)
{
    if (p_aec->b_val_domain != 0 || (p_aec->i_s1 == AEC_VALUE_BOUND && p_aec->b_val_bound != 0)) {
        p_aec->i_s1 = 0;

        if (aec_get_next_bit(p_aec)) {
            return 0;
        }

        if (p_aec->i_value_t >= (256 + p_aec->i_t1)) {  // LPS
            p_aec->i_value_t -= (256 + p_aec->i_t1);
            return 1;
        } else {
            return 0;
        }
    } else {
        uint32_t s2 = p_aec->i_s1 + 1;
        uint32_t t2 = p_aec->i_t1;
        int is_LPS = s2 > p_aec->i_value_s || (s2 == p_aec->i_value_s && p_aec->i_value_t >= t2) && p_aec->b_val_bound == 0;

        p_aec->b_val_domain = (bool_t)is_LPS;

        if (is_LPS) {    //LPS
            if (s2 == p_aec->i_value_s) {
                p_aec->i_value_t -= t2;
            } else {
                if (aec_get_next_bit(p_aec)) {
                    return 0;
                }
                p_aec->i_value_t += 256 - t2;
            }
            return 1;
        } else {
            p_aec->i_s1 = s2;
            p_aec->i_t1 = t2;
            return 0;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static INLINE
int biari_decode_final(aec_t *p_aec)
{
    // static context_t ctx = { (1 << LG_PMPS_SHIFTNO), 0, 0 };
    const uint32_t lg_pmps = 1; // ctx.LG_PMPS >> LG_PMPS_SHIFTNO;
    uint32_t t2;
    uint32_t s2;
    uint32_t s_flag;
    int is_LPS;

    // p_aec->i_value_t is in R domain  p_aec->i_s1=0 or p_aec->i_s1 == AEC_VALUE_BOUND
    if (p_aec->b_val_domain != 0 || (p_aec->i_s1 == AEC_VALUE_BOUND && p_aec->b_val_bound != 0)) {
        p_aec->i_s1 = 0;
        p_aec->i_value_s = 0;

        while (p_aec->i_value_t < QUARTER && p_aec->i_value_s < AEC_VALUE_BOUND) {
            if (aec_get_next_bit(p_aec)) {
                return 0;
            }
            p_aec->i_value_s++;
        }

        p_aec->b_val_bound = p_aec->i_value_t < QUARTER;
        p_aec->i_value_t = p_aec->i_value_t & 0xff;
    }

    s_flag = p_aec->i_t1 < lg_pmps;
    s2 = p_aec->i_s1 + s_flag;
    t2 = p_aec->i_t1 - lg_pmps + (s_flag << 8);            // 8bits

    /* 返回值 */
    is_LPS = (s2 > p_aec->i_value_s || (s2 == p_aec->i_value_s && p_aec->i_value_t >= t2)) && p_aec->b_val_bound == 0;
    p_aec->b_val_domain = (bool_t)is_LPS;

    if (is_LPS) {     // LPS
        uint32_t t_rlps = 1;
        int n_bits = 0;

        if (s2 == p_aec->i_value_s) {
            p_aec->i_value_t -= t2;
        } else {
            if (aec_get_next_bit(p_aec)) {
                return 0;
            }
            p_aec->i_value_t += 256 - t2;
        }

        // restore range
        while (t_rlps < QUARTER) {
            t_rlps <<= 1;
            n_bits++;
        }
        if (n_bits) {
            if (aec_get_next_n_bit(p_aec, n_bits)) {
                return 0;
            }
        }

        p_aec->i_s1 = 0;
        p_aec->i_t1 = 0;
        // return 1;  // !ctx.MPS
    } else {        // MPS
        p_aec->i_s1 = s2;
        p_aec->i_t1 = t2;
        // return 0;  // ctx.MPS
    }

    return is_LPS;
}


/* ---------------------------------------------------------------------------
 * decode symbols until a zero bit is obtained or passed max_num symbols
 * 使用相同上下文解码多个符号，直至解码出0或者达到最大数量(max_num)
 */
static INLINE
int biari_decode_symbol_continue0(aec_t *p_aec, context_t *ctx, int max_num)
{
    uint32_t i_value_s = p_aec->i_value_s;
    int bit = 0;
    int i;

    for (i = 0; i < max_num && !bit; i++) {
        uint32_t lg_pmps = ctx->LG_PMPS >> LG_PMPS_SHIFTNO;
        uint32_t t2;
        uint32_t s2;
        uint32_t s_flag;
        int is_LPS;

        bit = ctx->MPS;

        if (p_aec->b_val_domain != 0 || (p_aec->i_s1 == AEC_VALUE_BOUND && p_aec->b_val_bound != 0)) {
            p_aec->i_s1 = 0;
            i_value_s = 0;

            while (p_aec->i_value_t < QUARTER && i_value_s < AEC_VALUE_BOUND) {
                if (aec_get_next_bit(p_aec)) {
                    return 0;
                }
                i_value_s++;
            }

            p_aec->b_val_bound = p_aec->i_value_t < QUARTER;
            p_aec->i_value_t = p_aec->i_value_t & 0xff;
        }

        s_flag = p_aec->i_t1 < lg_pmps;
        s2 = p_aec->i_s1 + s_flag;
        t2 = p_aec->i_t1 - lg_pmps + (s_flag << 8);            // 8bits

        if (i_value_s > AEC_VALUE_BOUND) {
            /// davs2_log(NULL, DAVS2_LOG_ERROR, "i_value_s (>254).");
            p_aec->b_bit_error = 1;
            return 0;
        }

        is_LPS = (s2 > i_value_s || (s2 == i_value_s && p_aec->i_value_t >= t2)) && p_aec->b_val_bound == 0;
        p_aec->b_val_domain = (bool_t)is_LPS;

        if (is_LPS) {     // LPS
            uint32_t t_rlps = (s_flag == 0) ? (lg_pmps) : (p_aec->i_t1 + lg_pmps);
            int n_bits = 0;
            bit = !bit;

            if (s2 == i_value_s) {
                p_aec->i_value_t -= t2;
            } else {
                if (aec_get_next_bit(p_aec)) {
                    return 0;
                }
                p_aec->i_value_t += 256 - t2;
            }

            // restore range
            while (t_rlps < QUARTER) {
                t_rlps <<= 1;
                n_bits++;
            }
            if (n_bits) {
                if (aec_get_next_n_bit(p_aec, n_bits)) {
                    return 0;
                }
            }

            p_aec->i_s1 = 0;
            p_aec->i_t1 = t_rlps & 0xff;
            update_ctx_lps(ctx);
        } else {        // MPS
            p_aec->i_s1 = s2;
            p_aec->i_t1 = t2;
            update_ctx_mps(ctx);
        }
    }

    p_aec->i_value_s = i_value_s;
    return i - bit;
}

/* ---------------------------------------------------------------------------
 */
static
int biari_decode_symbol_continu0_ext(aec_t *p_aec, context_t *ctx, int max_ctx_inc, int max_num)
{
    int bit = 0;
    int i;

    for (i = 0; i < max_num && !bit; i++) {
        int ctx_add = DAVS2_MIN(i, max_ctx_inc);
        context_t *p_ctx = ctx + ctx_add;
        uint32_t lg_pmps = p_ctx->LG_PMPS >> LG_PMPS_SHIFTNO;
        uint32_t t2;
        uint32_t s2;
        int is_LPS;
        int s_flag;

        bit = p_ctx->MPS;

        if (p_aec->b_val_domain != 0 || (p_aec->i_s1 == AEC_VALUE_BOUND && p_aec->b_val_bound != 0)) {
            p_aec->i_s1 = 0;
            p_aec->i_value_s = 0;

            while (p_aec->i_value_t < QUARTER && p_aec->i_value_s < AEC_VALUE_BOUND) {
                if (aec_get_next_bit(p_aec)) {
                    return 0;
                }
                p_aec->i_value_s++;
            }

            p_aec->b_val_bound = p_aec->i_value_t < QUARTER;
            p_aec->i_value_t = p_aec->i_value_t & 0xff;
        }

        s_flag = p_aec->i_t1 < lg_pmps;
        s2 = p_aec->i_s1 + s_flag;
        t2 = p_aec->i_t1 - lg_pmps + (s_flag << 8);            // 8bits

        if (p_aec->i_value_s > AEC_VALUE_BOUND) {
            /// davs2_log(NULL, DAVS2_LOG_ERROR, "p_aec->i_value_s (>254).");
            /// exit(1);
            p_aec->b_bit_error = 1;
            return 0;
        }

        is_LPS = (s2 > p_aec->i_value_s || (s2 == p_aec->i_value_s && p_aec->i_value_t >= t2)) && p_aec->b_val_bound == 0;
        p_aec->b_val_domain = (bool_t)is_LPS;

        if (is_LPS) {     // LPS
            uint32_t t_rlps = (s_flag == 0) ? (lg_pmps) : (p_aec->i_t1 + lg_pmps);
            bit = !bit;

            if (s2 == p_aec->i_value_s) {
                p_aec->i_value_t -= t2;
            } else {
                if (aec_get_next_bit(p_aec)) {
                    return 0;
                }
                p_aec->i_value_t += 256 - t2;
            }

            // restore range
            while (t_rlps < QUARTER) {
                t_rlps <<= 1;

                if (aec_get_next_bit(p_aec)) {
                    return 0;
                }
            }

            p_aec->i_s1 = 0;
            p_aec->i_t1 = t_rlps & 0xff;
            update_ctx_lps(p_ctx);
        } else {        // MPS
            p_aec->i_s1 = s2;
            p_aec->i_t1 = t2;
            update_ctx_mps(p_ctx);
        }
    }

    return i - bit;
}

/* ---------------------------------------------------------------------------
 * decoding of unary binarization using one or 2 distinct models for the first
 * and all remaining bins; no terminating "0" for max_symbol
 */
static int unary_bin_max_decode(aec_t *p_aec, context_t *ctx, int ctx_offset, int max_symbol)
{
    int symbol = biari_decode_symbol(p_aec, ctx);

    if (symbol == 1) {
        return 0;
    } else {
        if (max_symbol == 1) {
            return symbol;
        } else {
            context_t *p_ctx = ctx + ctx_offset;

            symbol = 1 + biari_decode_symbol_continue0(p_aec, p_ctx, max_symbol - 1);

            return symbol;
        }
    }
}

/* ---------------------------------------------------------------------------
 */
void aec_init_contexts(aec_t *p_aec)
{
    const uint16_t lg_pmps = ((QUARTER << LG_PMPS_SHIFTNO) - 1);
    uint16_t  v = MAKE_CONTEXT(lg_pmps, 0, 0);
    uint16_t *d = (uint16_t *)&p_aec->syn_ctx;
    int ctx_cnt = sizeof(context_set_t) / sizeof(uint16_t);

    while (ctx_cnt-- != 0) {
        *d++ = v;
    }
}

/* ---------------------------------------------------------------------------
 */
void aec_new_slice(davs2_t *h)
{
    h->i_last_dquant = 0;
}

/* ---------------------------------------------------------------------------
 */
int aec_read_dmh_mode(aec_t *p_aec, int i_cu_level)
{
    context_t *p_ctx = p_aec->syn_ctx.pu_type_index + (i_cu_level - 3) * 3 + NUM_INTER_DIR_DHP_CTX;

    assert(NUM_INTER_DIR_DHP_CTX + NUM_DMH_MODE_CTX == NUM_INTER_DIR_CTX);

    if (biari_decode_symbol(p_aec, p_ctx) == 0) {
        return 0;
    } else {
        if (biari_decode_symbol(p_aec, p_ctx + 1) == 0) {
            return 3 + biari_decode_symbol_eq_prob(p_aec);    // 3, 4: 二元符号串：10x
        } else {
            if (biari_decode_symbol(p_aec, p_ctx + 2) == 0) {
                return 7 + biari_decode_symbol_eq_prob(p_aec);    // 7, 8: 二元符号串：110x
            } else {
                /* 1,2，二元符号串：1110x
                 * 5,6，二元符号串：1111x
                 */
                int b3 = biari_decode_symbol_eq_prob(p_aec);
                int b4 = biari_decode_symbol_eq_prob(p_aec);
                return 1 + (b3 << 2) + b4;
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * arithmetically decode the motion vector difference
 */
static INLINE
int aec_read_mvd(aec_t *p_aec, context_t *p_ctx)
{
    int binary_symbol = 0;
    int golomb_order = 0;
    int act_sym;

    if (!biari_decode_symbol(p_aec, p_ctx + 0)) {
        act_sym = 0;
    } else if (!biari_decode_symbol(p_aec, p_ctx + 1)) {
        act_sym = 1;
    } else if (!biari_decode_symbol(p_aec, p_ctx + 2)) {
        act_sym = 2;
    } else {   // 1110
        int add_sym = biari_decode_symbol_eq_prob(p_aec);
        act_sym = 0;

        for (;;) {
            int l = biari_decode_symbol_eq_prob(p_aec);

            AEC_RETURN_ON_ERROR(0);

            if (l == 0) {
                act_sym += (1 << golomb_order);
                golomb_order++;
            } else {
                break;
            }
        }

        while (golomb_order--) {
            // next binary part
            if (biari_decode_symbol_eq_prob(p_aec)) {
                binary_symbol |= (1 << golomb_order);
            }
        }

        act_sym += binary_symbol;
        act_sym = (act_sym << 1) + 3 + add_sym;
    }

    if (act_sym != 0) {
        if (biari_decode_symbol_eq_prob(p_aec)) {
            act_sym = -act_sym;
        }
    }

    return act_sym;
}

/* ---------------------------------------------------------------------------
 * arithmetically decode the motion vector difference
 */
void aec_read_mvds(aec_t *p_aec, mv_t *p_mvd)
{
    p_mvd->x = (int16_t)aec_read_mvd(p_aec, p_aec->syn_ctx.mvd_contexts[0]);
    p_mvd->y = (int16_t)aec_read_mvd(p_aec, p_aec->syn_ctx.mvd_contexts[1]);
}


/* ---------------------------------------------------------------------------
 * arithmetically decode the 8x8 block type
 */
static INLINE
int aec_read_wpm(aec_t *p_aec, int num_of_references)
{
    context_t *p_ctx = p_aec->syn_ctx.weighted_skip_mode;
    return biari_decode_symbol_continu0_ext(p_aec, p_ctx, 2, num_of_references - 1);
}

/* ---------------------------------------------------------------------------
 */
static INLINE
int aec_read_dir_skip_mode(aec_t *p_aec)
{
    context_t *p_ctx = p_aec->syn_ctx.cu_subtype_index;
    int act_sym = biari_decode_symbol_continu0_ext(p_aec, p_ctx, 32768, 3);
    if (act_sym == 3) {
        act_sym += (!biari_decode_symbol(p_aec, p_ctx + 3));
    }
    return act_sym;
}

/* ---------------------------------------------------------------------------
 * TU split type when TU split is enabled for current CU */
static ALWAYS_INLINE int cu_set_tu_split_type(davs2_t *h, cu_t *p_cu, int transform_split_flag)
{
    // split types
    // [mode][(NSQT enable or SDIP enables) and cu_level > B8X8_IN_BIT]
    //  split_type for block non-SDIP/NSQT:[0] and SDIP/NSQT:[1]
    static const int8_t TU_SPLIT_TYPE[MAX_PRED_MODES][2] = {
        { TU_SPLIT_CROSS,   TU_SPLIT_CROSS   }, // 0: 8x8, ---, ---, --- (PRED_SKIP   )
        { TU_SPLIT_CROSS,   TU_SPLIT_CROSS   }, // 1: 8x8, ---, ---, --- (PRED_2Nx2N  )
        { TU_SPLIT_CROSS,   TU_SPLIT_HOR     }, // 2: 8x4, 8x4, ---, --- (PRED_2NxN   )
        { TU_SPLIT_CROSS,   TU_SPLIT_VER     }, // 3: 4x8, 4x8, ---, --- (PRED_Nx2N   )
        { TU_SPLIT_CROSS,   TU_SPLIT_HOR     }, // 4: 8x2, 8x6, ---, --- (PRED_2NxnU  )
        { TU_SPLIT_CROSS,   TU_SPLIT_HOR     }, // 5: 8x6, 8x2, ---, --- (PRED_2NxnD  )
        { TU_SPLIT_CROSS,   TU_SPLIT_VER     }, // 6: 2x8, 6x8, ---, --- (PRED_nLx2N  )
        { TU_SPLIT_CROSS,   TU_SPLIT_VER     }, // 7: 6x8, 2x8, ---, --- (PRED_nRx2N  )
        { TU_SPLIT_NON,     TU_SPLIT_INVALID }, // 8: 8x8, ---, ---, --- (PRED_I_2Nx2N)
        { TU_SPLIT_CROSS,   TU_SPLIT_CROSS   }, // 9: 4x4, 4x4, 4x4, 4x4 (PRED_I_NxN  )
        { TU_SPLIT_INVALID, TU_SPLIT_HOR     }, //10: 8x2, 8x2, 8x2, 8x2 (PRED_I_2Nxn )
        { TU_SPLIT_INVALID, TU_SPLIT_VER     }  //11: 2x8, 2x8, 2x8, 2x8 (PRED_I_nx2N )
    };
    int mode = p_cu->i_cu_type;
    int level = p_cu->i_cu_level;
    int enable_nsqt_sdip = IS_INTRA_MODE(mode) ? h->seq_info.enable_sdip : h->seq_info.enable_nsqt;

    enable_nsqt_sdip = enable_nsqt_sdip && level > B8X8_IN_BIT;
    p_cu->i_trans_size = transform_split_flag ? TU_SPLIT_TYPE[mode][enable_nsqt_sdip] : TU_SPLIT_NON;
    assert(p_cu->i_trans_size != TU_SPLIT_INVALID);

    return p_cu->i_trans_size;
}

/* ---------------------------------------------------------------------------
 */
int aec_read_intra_cu_type(aec_t *p_aec, cu_t *p_cu, int b_sdip, davs2_t *h)
{
    int cu_type = PRED_I_NxN;
    int b_tu_split = 0;
    b_sdip = (p_cu->i_cu_level == B32X32_IN_BIT || p_cu->i_cu_level == B16X16_IN_BIT) && b_sdip;

    /* 1, read intra cu split flag */
    if (p_cu->i_cu_level == B8X8_IN_BIT || b_sdip) {
        context_t * p_ctx = p_aec->syn_ctx.transform_split_flag;

        b_tu_split = biari_decode_symbol(p_aec, p_ctx + 1 + b_sdip);
    }

#if AVS2_TRACE
    avs2_trace("Transform_Size = %3d \n", b_tu_split);
#endif

    /* 2, read intra CU partition type */
    if (!b_tu_split) {
        cu_type = PRED_I_2Nx2N;
    } else if (b_sdip) {
        context_t * p_ctx = p_aec->syn_ctx.intra_pu_type_contexts;
        int symbol1 = biari_decode_symbol(p_aec, p_ctx);
        cu_type = symbol1 ? PRED_I_2Nxn : PRED_I_nx2N;
    }

#if AVS2_TRACE
    avs2_trace_string("cuType", cu_type, 1);
#endif

    p_cu->i_cu_type = (int8_t)cu_type;
    cu_set_tu_split_type(h, p_cu, b_tu_split);

    return cu_type;
}

/* ---------------------------------------------------------------------------
 * arithmetically decode the coding unit type info of a given CU
 */
int aec_read_cu_type(aec_t *p_aec, cu_t *p_cu, int img_type, int b_amp, int b_mhp, int b_wsm, int num_references)
{
    // 0: SKIP, 1: 2Nx2N, 2: 2NxN / 2NxnU / 2NxnD, 3: Nx2N / nLx2N / nRx2N, 9: INTRA
    static const int MAP_CU_TYPE[2][7] = {
        {-1, 0, 1, 2, 3, -1/*PRED_NxN*/, PRED_I_NxN},
        {-1, 0, 1, 2, 3, PRED_I_NxN}
    };

    int real_cu_type;

    if (img_type != AVS2_I_SLICE) {
        context_t *p_ctx = p_aec->syn_ctx.cu_type_contexts;
        int bin_idx = 0;
        int act_ctx = 0;
        int act_sym = 0;
        int max_bit = 6 - (p_cu->i_cu_level == B8X8_IN_BIT);
        int symbol;

        while (act_sym < max_bit) {
            if ((bin_idx == 5) && (p_cu->i_cu_level != MIN_CU_SIZE_IN_BIT)) {
                symbol = biari_decode_final(p_aec);
            } else {
                symbol = biari_decode_symbol(p_aec, p_ctx + act_ctx);
            }

            AEC_RETURN_ON_ERROR(-1);
            bin_idx++;

            if (symbol == 0) {
                act_sym++;
                act_ctx = DAVS2_MIN(5, act_ctx + 1);
            } else {
                break;
            }
        }

        real_cu_type = MAP_CU_TYPE[p_cu->i_cu_level == B8X8_IN_BIT][act_sym];

        // for AMP
        if (p_cu->i_cu_level >= B16X16_IN_BIT && b_amp && (real_cu_type == 2 || real_cu_type == 3)) {
            context_t *p_ctx_amp = p_aec->syn_ctx.shape_of_partition_index;
            if (!biari_decode_symbol(p_aec, p_ctx_amp + 0)) {
                real_cu_type = real_cu_type * 2 + (!biari_decode_symbol(p_aec, p_ctx_amp + 1));
            }
        }
    } else {
        real_cu_type = PRED_I_NxN;     /* intra mode */
    }

#if AVS2_TRACE
    {
        int trace_cu_type = real_cu_type;
        if (trace_cu_type == PRED_I_2Nxn || trace_cu_type == PRED_I_nx2N) {
            trace_cu_type += 2;             /* in order to trace same text as RM */
        }

        trace_cu_type += (img_type == AVS2_B_SLICE);    /* also here */
        avs2_trace_string("cuType", trace_cu_type, 1);
    }
#endif

    if (real_cu_type <= 0) {    /* Skip Mode */
        int weighted_skipmode_fix = 0;
        int md_directskip_mode    = DS_NONE;

        if (img_type == AVS2_F_SLICE && b_wsm && num_references > 1) {
            weighted_skipmode_fix = aec_read_wpm(p_aec, num_references);
#if AVS2_TRACE
            avs2_trace("weighted_skipmode1 = %3d \n", weighted_skipmode_fix);
#endif
        }
        p_cu->i_weighted_skipmode = (int8_t)weighted_skipmode_fix;

        if ((weighted_skipmode_fix == 0) &&
            ((b_mhp && img_type == AVS2_F_SLICE) || img_type == AVS2_B_SLICE)) {
            md_directskip_mode = aec_read_dir_skip_mode(p_aec);
#if AVS2_TRACE
            avs2_trace("p_directskip_mode = %3d \n", md_directskip_mode);
#endif
        } else {
            md_directskip_mode = DS_NONE;
        }

        p_cu->i_md_directskip_mode = (int8_t)md_directskip_mode;
    }

    return real_cu_type;
}

/* ---------------------------------------------------------------------------
 */
int aec_read_cu_type_sframe(aec_t *p_aec)
{
    static const int MapSCUType[7] = {-1, PRED_SKIP, PRED_I_NxN};
    context_t * p_ctx = p_aec->syn_ctx.cu_type_contexts;
    int act_ctx = 0;
    int cu_type = 0;

    for (;;) {
        if (biari_decode_symbol(p_aec, p_ctx + act_ctx) == 0) {
            cu_type++;
            act_ctx++;
        } else {
            break;
        }

        if (cu_type >= 2) {
            break;
        }
    }

    cu_type = MapSCUType[cu_type];    /* cu type */
#if AVS2_TRACE
    avs2_trace_string("cuType", cu_type, 1);
#endif

    return cu_type;       /* return cu type */
}

/* ---------------------------------------------------------------------------
 */
static INLINE
int aec_read_b_pdir(aec_t * p_aec, cu_t * p_cu)
{
    static const int dir2offset[4][4] = {
        {  0,  2,  4,  9 },
        {  3,  1,  5, 10 },
        {  6,  7,  8, 11 },
        { 12, 13, 14, 15 }
    };

    int new_pdir[4] = { 3, 1, 0, 2 };
    context_t *p_ctx = p_aec->syn_ctx.pu_type_index;
    int act_ctx = 0;
    int act_sym = 0;
    int pdir    = PDIR_FWD;
    int pdir0 = 0, pdir1 = 0;
    int symbol;

    if (p_cu->i_cu_type == PRED_2Nx2N) {
        /* act_ctx: 0, 1, 2 */
        act_sym = biari_decode_symbol_continu0_ext(p_aec, p_ctx, 32768, 2);
        if (act_sym == 2) {
            act_sym += (!biari_decode_symbol(p_aec, p_ctx + 2));
        }
        pdir = act_sym;
    } else if ((p_cu->i_cu_type >= PRED_2NxN && p_cu->i_cu_type <= PRED_nRx2N) && p_cu->i_cu_level == B8X8_IN_BIT) {
        p_ctx = p_aec->syn_ctx.b_pu_type_min_index;
        pdir0 = !biari_decode_symbol(p_aec, p_ctx + act_ctx);  // BW

        if (biari_decode_symbol(p_aec, p_ctx + act_ctx + 1)) {
            pdir1 = pdir0;
        } else {
            pdir1 = !pdir0;
        }

        pdir = dir2offset[pdir0][pdir1];
    } else if (p_cu->i_cu_type >= PRED_2NxN || p_cu->i_cu_type <= PRED_nRx2N) {
        /* act_ctx: 3, 4 */
        act_sym = biari_decode_symbol_continu0_ext(p_aec, p_ctx + 3, 32768, 2);

        /* act_ctx: 5 */
        if (act_sym == 2) {
            act_sym += (!biari_decode_symbol(p_aec, p_ctx + 5));
        }
        pdir0 = act_sym;

        if (biari_decode_symbol(p_aec, p_ctx + 6)) {
            pdir1 = pdir0;
        } else {
            switch (pdir0) {
            case 0:
                if (biari_decode_symbol(p_aec, p_ctx + 7)) {
                    pdir1 = 1;
                } else {
                    symbol = biari_decode_symbol(p_aec, p_ctx + 8);
                    pdir1 = symbol ? 2 : 3;
                }

                break;
            case 1:
                if (biari_decode_symbol(p_aec, p_ctx + 9)) {
                    pdir1 = 0;
                } else {
                    symbol = biari_decode_symbol(p_aec, p_ctx + 10);
                    pdir1 = symbol ? 2 : 3;
                }

                break;
            case 2:
                if (biari_decode_symbol(p_aec, p_ctx + 11)) {
                    pdir1 = 0;
                } else {
                    symbol = biari_decode_symbol(p_aec, p_ctx + 12);
                    pdir1 = symbol ? 1 : 3;
                }

                break;
            case 3:
                if (biari_decode_symbol(p_aec, p_ctx + 13)) {
                    pdir1 = 0;
                } else {
                    symbol = biari_decode_symbol(p_aec, p_ctx + 14);
                    pdir1 = symbol ? 1 : 2;
                }

                break;
            }
        }

        pdir0 = new_pdir[pdir0];
        pdir1 = new_pdir[pdir1];
        pdir  = dir2offset[pdir0][pdir1];
    }

#if AVS2_TRACE
    if (p_cu->i_cu_type >= PRED_2NxN && p_cu->i_cu_type <= PRED_nRx2N) {
        avs2_trace_string("B_Pred_Dir0 ", pdir0, 1);
        avs2_trace_string("B_Pred_Dir1 ", pdir1, 1);
    } else if (p_cu->i_cu_type == PRED_2Nx2N) {
        avs2_trace_string("B_Pred_Dir ", pdir0, 1);
    }
#endif

    return pdir;
}

/* ---------------------------------------------------------------------------
 * arithmetically decode the PU type
 */
static INLINE
int aec_read_pdir_dhp(aec_t * p_aec, cu_t * p_cu)
{
    static const int dir2offset[2][2] = {
        { 0, 1 },
        { 2, 3 }
    };

    context_t *p_ctx = p_aec->syn_ctx.pu_type_index;
    int pdir = PDIR_FWD;
    int pdir0, pdir1;
    int symbol;

    if (p_cu->i_cu_type == PRED_2Nx2N) {
        pdir = pdir0 = biari_decode_symbol(p_aec, p_ctx);
    } else if (p_cu->i_cu_type >= PRED_2NxN || p_cu->i_cu_type <= PRED_nRx2N) {
        pdir0 = biari_decode_symbol(p_aec, p_ctx + 1);

        symbol = biari_decode_symbol(p_aec, p_ctx + 2);
        if (symbol) {
            pdir1 = pdir0;
        } else {
            pdir1 = 1 - pdir0;
        }

        pdir = dir2offset[pdir0][pdir1];
    }

#if AVS2_TRACE
    if (p_cu->i_cu_type >= PRED_2NxN && p_cu->i_cu_type <= PRED_nRx2N) {
        avs2_trace_string("P_Pred_Dir0 ", pdir0, 1);
        avs2_trace_string("P_Pred_Dir1 ", pdir1, 1);
    } else if (p_cu->i_cu_type == PRED_2Nx2N) {
        avs2_trace_string("P_Pred_Dir ", pdir0, 1);
    }
#endif

    return pdir;
}

/* ---------------------------------------------------------------------------
 * set CU prediction direction for P/F-Frames
 */
static INLINE void cu_set_pdir_PFframe(cu_t *p_cu, int pdir)
{
    static const int8_t pdir0[4] = { PDIR_FWD, PDIR_FWD, PDIR_DUAL, PDIR_DUAL };
    static const int8_t pdir1[4] = { PDIR_FWD, PDIR_DUAL, PDIR_FWD, PDIR_DUAL };
    int i_cu_type = p_cu->i_cu_type;
    int i;

    if (i_cu_type == PRED_2Nx2N) { // 16x16
        /* PU不划分情况下，PU数量为 1；[2/3]必须赋值以简化DMH模式号解码的条件判断 */
        pdir = (pdir == PDIR_FWD ? PDIR_FWD : PDIR_DUAL);
        for (i = 0; i < 4; i++) {
            p_cu->b8pdir[i] = (int8_t)pdir;
        }
    } else if (IS_HOR_PU_PART(i_cu_type)) { // horizontal: 16x8, 16x4, 16x12
        /* 水平PU划分情况下，PU数量为2；[2/3]必须赋值以简化DMH模式号解码的条件判断 */
        p_cu->b8pdir[0] = p_cu->b8pdir[2] = pdir0[pdir];
        p_cu->b8pdir[1] = p_cu->b8pdir[3] = pdir1[pdir];
    } else if (IS_VER_PU_PART(i_cu_type)) { // vertical:
        /* 垂直PU划分情况下，PU数量为2；[2/3]必须赋值以简化DMH模式号解码的条件判断 */
        p_cu->b8pdir[0] = p_cu->b8pdir[2] = pdir0[pdir];
        p_cu->b8pdir[1] = p_cu->b8pdir[3] = pdir1[pdir];
    } else {  /* intra mode */
        for (i = 0; i < 4; i++) {
            p_cu->b8pdir[i] = PDIR_INVALID;
        }
    }
}

/* ---------------------------------------------------------------------------
 * set CU prediction direction for B-Frames
 */
static INLINE void cu_set_pdir_Bframe(cu_t *p_cu, int pdir)
{
    static const int8_t pdir0[16] = { PDIR_FWD, PDIR_BWD, PDIR_FWD, PDIR_BWD, PDIR_FWD, PDIR_BWD, PDIR_SYM, PDIR_SYM, PDIR_SYM, PDIR_FWD, PDIR_BWD, PDIR_SYM, PDIR_BID, PDIR_BID, PDIR_BID, PDIR_BID };
    static const int8_t pdir1[16] = { PDIR_FWD, PDIR_BWD, PDIR_BWD, PDIR_FWD, PDIR_SYM, PDIR_SYM, PDIR_FWD, PDIR_BWD, PDIR_SYM, PDIR_BID, PDIR_BID, PDIR_BID, PDIR_FWD, PDIR_BWD, PDIR_SYM, PDIR_BID };
    static const int8_t pdir2refidx[4][2] = {
        { B_FWD, INVALID_REF },  // PDIR_FWD
        { INVALID_REF, B_BWD },  // PDIR_BWD
        { B_FWD, B_BWD },
        { B_FWD, B_BWD }
    };
    int i_cu_type = p_cu->i_cu_type;
    int8_t *b8pdir = p_cu->b8pdir;
    int i;

    //--- set b8type, and b8pdir ---
    if (i_cu_type == PRED_SKIP) {   // direct
        /* Skip模式下PU数量为1或4，按照最大PU数量设置 */
        pdir = tab_pdir_bskip[p_cu->i_md_directskip_mode];
        for (i = 0; i < 4; i++) {
            b8pdir[i] = (int8_t)pdir;
        }
    } else if (i_cu_type == PRED_2Nx2N) { // 16x16
        /* PU不划分情况下，PU数量为 1 */
        for (i = 0; i < 4; i++) {
            b8pdir[i] = (int8_t)pdir;
        }
    } else if (IS_HOR_PU_PART(i_cu_type)) { // 16x8, 16x4, 16x12
        /* 水平PU划分情况下，PU数量为2 */
        b8pdir[0] = b8pdir[2] = pdir0[pdir];
        b8pdir[1] = b8pdir[3] = pdir1[pdir];
    } else if (IS_VER_PU_PART(i_cu_type)) {
        /* 垂直PU划分情况下，PU数量为2 */
        b8pdir[0] = b8pdir[2] = pdir0[pdir];
        b8pdir[1] = b8pdir[3] = pdir1[pdir];
    } else {  // intra mode
        for (i = 0; i < 4; i++) {
            b8pdir[i] = PDIR_INVALID;
        }
    }

    for (i = 0; i < 4; i++) {
        const int8_t *p_idx = pdir2refidx[b8pdir[i]];
        p_cu->ref_idx[i].r[0] = p_idx[0];
        p_cu->ref_idx[i].r[1] = p_idx[1];
    }
}

/* ---------------------------------------------------------------------------
 * arithmetically decode the reference parameter of a given MB
 */
static INLINE
int aec_read_ref_frame(aec_t *p_aec, int num_of_references)
{
    context_t *p_ctx = p_aec->syn_ctx.pu_reference_index;
    int act_sym;

    if (biari_decode_symbol(p_aec, p_ctx)) {
        act_sym = 0;
    } else {
        int act_ctx = 1;
        act_sym = 1;

        // TODO: 此处代码可继续优化
        while ((act_sym != num_of_references - 1) && (!biari_decode_symbol(p_aec, p_ctx + act_ctx))) {
            act_sym++;
            act_ctx = DAVS2_MIN(2, act_ctx + 1);
        }
    }

    return act_sym;
}

/* ---------------------------------------------------------------------------
 */
static INLINE
int cu_read_references(davs2_t *h, aec_t *p_aec, cu_t *p_cu)
{
    int idx_pu;
    int num_pu = p_cu->i_cu_type == PRED_2Nx2N ? 1 : 2;

    //  If multiple ref. frames, read reference frame for the MB *********************************
    for (idx_pu = 0; idx_pu < num_pu; idx_pu++) {
        int8_t ref_1st, ref_2nd;
        // non skip (direct)
        assert(p_cu->b8pdir[idx_pu] == PDIR_FWD || p_cu->b8pdir[idx_pu] == PDIR_DUAL);
        if (h->num_of_references > 1) {
            ref_1st = (int8_t)aec_read_ref_frame(p_aec, h->num_of_references);
            AEC_RETURN_ON_ERROR(-1);
#if AVS2_TRACE
            avs2_trace("Fwd Ref frame no  = %3d \n", ref_1st);
#endif
        } else {
            ref_1st = 0;
        }

        if (p_cu->b8pdir[idx_pu] == PDIR_DUAL) {
            ref_2nd = !ref_1st;
        } else {
            ref_2nd = INVALID_REF;
        }

        p_cu->ref_idx[idx_pu].r[0] = ref_1st;
        p_cu->ref_idx[idx_pu].r[1] = ref_2nd;
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
void aec_read_inter_pred_dir(aec_t * p_aec, cu_t *p_cu, davs2_t *h)
{
    int pdir = PDIR_FWD;
    int real_cu_type = p_cu->i_cu_type;

    if ((h->i_frame_type == AVS2_B_SLICE)) {  // B frame
        if (real_cu_type >= PRED_2Nx2N && real_cu_type <= PRED_nRx2N) {
            pdir = aec_read_b_pdir(p_aec, p_cu);
        }
        cu_set_pdir_Bframe(p_cu, pdir);
    } else {  // other Inter frame
        if (IS_SKIP_MODE(real_cu_type)) {
            int i;
            if (p_cu->i_weighted_skipmode || 
                p_cu->i_md_directskip_mode == DS_DUAL_1ST || 
                p_cu->i_md_directskip_mode == DS_DUAL_2ND) {
                pdir = PDIR_DUAL;
            }
            for (i = 0; i < 4; i++) {
                p_cu->b8pdir[i] = (int8_t)pdir;
            }
        } else {
            if (h->i_frame_type == AVS2_F_SLICE && h->num_of_references > 1 && h->seq_info.enable_dhp) {
                if (!(p_cu->i_cu_level == B8X8_IN_BIT && real_cu_type >= PRED_2NxN && real_cu_type <= PRED_nRx2N)) {
                    pdir = aec_read_pdir_dhp(p_aec, p_cu);
                }
            }
            cu_set_pdir_PFframe(p_cu, pdir);
        }

        if (h->i_frame_type != AVS2_S_SLICE && p_cu->i_cu_type != PRED_SKIP) {
            cu_read_references(h, p_aec, p_cu);
        }
    }
}

/* ---------------------------------------------------------------------------
 * arithmetically decode a pair of intra prediction modes of a given MB
 */
int aec_read_intra_pmode(aec_t * p_aec)
{
    context_t * p_ctx = p_aec->syn_ctx.intra_luma_pred_mode;
    int symbol;

    if (biari_decode_symbol(p_aec, p_ctx) == 1) {
        symbol = biari_decode_symbol(p_aec, p_ctx + 6) - 2;
    } else {
        symbol  = biari_decode_symbol(p_aec, p_ctx + 1) << 4;
        symbol += biari_decode_symbol(p_aec, p_ctx + 2) << 3;
        symbol += biari_decode_symbol(p_aec, p_ctx + 3) << 2;
        symbol += biari_decode_symbol(p_aec, p_ctx + 4) << 1;
        symbol += biari_decode_symbol(p_aec, p_ctx + 5);
    }

#if AVS2_TRACE
    avs2_trace("@%d %s\t\t\t%d\n", symbolCount++, p_aec->tracestring, symbol);
#endif

    return symbol;
}

/* ---------------------------------------------------------------------------
 * arithmetically decode the delta qp of a given CU
 */
static INLINE
int aec_read_cu_delta_qp(aec_t * p_aec, int i_last_dequant)
{
    context_t * p_ctx = p_aec->syn_ctx.delta_qp_contexts;
    int act_sym;
    int dquant;

    act_sym = 1 - biari_decode_symbol(p_aec, p_ctx + (!!i_last_dequant));
    if (act_sym != 0) {
        act_sym = unary_bin_max_decode(p_aec, p_ctx + 2, 1, 256) + 1;
    }

    /* cu_qp_delta: (-32  -  4× (BitDepth-8)) ～(32  + 4× (BitDepth -8)) */
    dquant = (act_sym + 1) >> 1;
    if ((act_sym & 0x01) == 0) {    // LSB is signed bit
        dquant = -dquant;
    }

#if AVS2_TRACE
    avs2_trace("@%d %s\t\t\t%d\n", symbolCount++, p_aec->tracestring, dquant);
#endif

    return dquant;
}

/* ---------------------------------------------------------------------------
 * arithmetically decode the ctp_y[i] of a given cu
 */
static int aec_read_ctp_y(davs2_t *h, aec_t *p_aec, int b8, cu_t *p_cu, int scu_x, int scu_y)
{
    context_t *p_ctx;
    int b_hor   = p_cu->i_trans_size == TU_SPLIT_HOR;  // is current CU hor TU partition
    int b_ver   = p_cu->i_trans_size == TU_SPLIT_VER;  // is current CU ver TU partition
    int i_level = p_cu->i_cu_level;
    int cu_size = 1 << i_level;
    int a = 0, b = 0;                   // ctp_y[i] of neighboring blocks
    int x, y;

    /* 当前TB在CU中的位置 */
    if (b_hor) {
        x = 0;
        y = ((cu_size * b8) >> 2);
    } else if (b_ver) {
        x = ((cu_size * b8) >> 2);
        y = 0;
    } else {
        x = ((cu_size * (b8  & 1)) >> 1);
        y = ((cu_size * (b8 >> 1)) >> 1);
    }

    /* TB在图像中的位置 */
    x += (scu_x << MIN_CU_SIZE_IN_BIT);
    y += (scu_y << MIN_CU_SIZE_IN_BIT);
    /* 转换成4x4块位置 */
    x >>= MIN_PU_SIZE_IN_BIT;
    y >>= MIN_PU_SIZE_IN_BIT;

    /* 取左相邻块对应位置的CTP */
    if (b_ver && b8 > 0) {
        a = (p_cu->i_cbp >> (b8 - 1)) & 1;
    } else {
        a = get_neighbor_cbp_y(h, x - 1, y, scu_x, scu_y, p_cu);
    }

    /* 取上相邻块对应位置的CTP */
    if (b_hor && b8 > 0) {
        b = (p_cu->i_cbp >> (b8 - 1)) & 1;
    } else {
        b = get_neighbor_cbp_y(h, x, y - 1, scu_x, scu_y, p_cu);
    }

    p_ctx = p_aec->syn_ctx.cbp_contexts + a + 2 * b;

    return biari_decode_symbol(p_aec, p_ctx);
}

/* ---------------------------------------------------------------------------
 */
static INLINE
int aec_read_cbp(aec_t *p_aec, davs2_t *h, cu_t *p_cu, int scu_x, int scu_y)
{
    int cbp = 0;
    int cbp_bit = 0;

    if (IS_INTER(p_cu)) {
        if (IS_NOSKIP_INTER_MODE(p_cu->i_cu_type)) {
            cbp_bit = biari_decode_symbol(p_aec, p_aec->syn_ctx.cbp_contexts + 8);  // "ctp_zero_flag"
        }
        if (cbp_bit == 0) {
            // transform size
            int b_tu_split = biari_decode_symbol(p_aec, p_aec->syn_ctx.transform_split_flag);
            cu_set_tu_split_type(h, p_cu, b_tu_split);

            // chroma
            if (h->i_chroma_format != CHROMA_400) {
                cbp_bit = biari_decode_symbol(p_aec, p_aec->syn_ctx.cbp_contexts + 4);
                if (cbp_bit) {
                    cbp_bit = biari_decode_symbol(p_aec, p_aec->syn_ctx.cbp_contexts + 5);

                    if (cbp_bit) {
                        cbp += 48;
                    } else {
                        cbp_bit = biari_decode_symbol(p_aec, p_aec->syn_ctx.cbp_contexts + 5);
                        cbp += (cbp_bit == 1) ? 32 : 16;
                    }
                }
            }

            // luma
            if (b_tu_split == 0) {
                if (cbp == 0) {
                    cbp = 1;   // 色度块全零，ctp_zero_flag指示有非零系数
                } else {
                    cbp_bit = aec_read_ctp_y(h, p_aec, 0, p_cu, scu_x, scu_y);
                    cbp    += cbp_bit;
                }
            } else {
                cbp_bit = aec_read_ctp_y(h, p_aec, 0, p_cu, scu_x, scu_y);
                cbp    += cbp_bit;
                p_cu->i_cbp = (int8_t)cbp;

                cbp_bit = aec_read_ctp_y(h, p_aec, 1, p_cu, scu_x, scu_y);
                cbp    += (cbp_bit << 1);
                p_cu->i_cbp = (int8_t)cbp;

                cbp_bit = aec_read_ctp_y(h, p_aec, 2, p_cu, scu_x, scu_y);
                cbp    += (cbp_bit << 2);
                p_cu->i_cbp = (int8_t)cbp;

                cbp_bit = aec_read_ctp_y(h, p_aec, 3, p_cu, scu_x, scu_y);
                cbp    += (cbp_bit << 3);
                p_cu->i_cbp = (int8_t)cbp;
            }
        } else {
            cu_set_tu_split_type(h, p_cu, 1);
            p_cu->i_cbp = 0;
            cbp = 0;
        }
    } else {
        // intra luma
        if (p_cu->i_cu_type == PRED_I_2Nx2N) {
            cbp     = aec_read_ctp_y(h, p_aec, 0, p_cu, scu_x, scu_y);
        } else {
            cbp_bit = aec_read_ctp_y(h, p_aec, 0, p_cu, scu_x, scu_y);
            cbp    += cbp_bit;
            p_cu->i_cbp = (int8_t)cbp;

            cbp_bit = aec_read_ctp_y(h, p_aec, 1, p_cu, scu_x, scu_y);
            cbp    += (cbp_bit << 1);
            p_cu->i_cbp = (int8_t)cbp;

            cbp_bit = aec_read_ctp_y(h, p_aec, 2, p_cu, scu_x, scu_y);
            cbp    += (cbp_bit << 2);
            p_cu->i_cbp = (int8_t)cbp;

            cbp_bit = aec_read_ctp_y(h, p_aec, 3, p_cu, scu_x, scu_y);
            cbp    += (cbp_bit << 3);
            p_cu->i_cbp = (int8_t)cbp;
        }

        // chroma decoding
        if (h->i_chroma_format != CHROMA_400) {
            cbp_bit = biari_decode_symbol(p_aec, p_aec->syn_ctx.cbp_contexts + 6);
            if (cbp_bit) {
                cbp_bit = biari_decode_symbol(p_aec, p_aec->syn_ctx.cbp_contexts + 7);

                if (cbp_bit) {
                    cbp += 48;
                } else {
                    cbp_bit = biari_decode_symbol(p_aec, p_aec->syn_ctx.cbp_contexts + 7);
                    cbp += 16 << cbp_bit;
                }
            }
        }   // 色度CBP
    }

    if (!cbp) {
        h->i_last_dquant = 0;
    }

#if AVS2_TRACE
    avs2_trace("@%d %s\t\t\t\t%d\n", symbolCount++, p_aec->tracestring, cbp);
#endif

    return cbp;
}

/* ---------------------------------------------------------------------------
 */
int cu_read_cbp(davs2_t *h, aec_t *p_aec, cu_t *p_cu, int scu_x, int scu_y)
{
#if AVS2_TRACE
    snprintf(p_aec->tracestring, TRACESTRING_SIZE, "CBP");
#endif
    p_cu->i_cbp = (int8_t)aec_read_cbp(p_aec, h, p_cu, scu_x, scu_y);    // check: first_mb_nr

    // delta quant only if nonzero coeffs
    if (h->b_DQP) {
        int i_delta_qp = 0;
        if (p_cu->i_cbp) {
            const int max_delta_qp = 32 + 4 * (h->sample_bit_depth - 8);
            const int min_delta_qp = -max_delta_qp;
#if AVS2_TRACE
            snprintf(p_aec->tracestring, TRACESTRING_SIZE, "delta quant");
#endif
            i_delta_qp = i_delta_qp = (int8_t)aec_read_cu_delta_qp(p_aec, h->i_last_dquant);
            if (i_delta_qp < min_delta_qp ||
                i_delta_qp > max_delta_qp) {
                i_delta_qp = DAVS2_CLIP3(min_delta_qp, max_delta_qp, i_delta_qp);
                davs2_log(h, DAVS2_LOG_ERROR, "Invalid cu_qp_delta: %d.", i_delta_qp);
            }
        }

        h->i_last_dquant = i_delta_qp;
        p_cu->i_qp = (int8_t)i_delta_qp + h->lcu.i_left_cu_qp;
    } else {
        p_cu->i_qp = (int8_t)h->i_qp;
    }

    AEC_RETURN_ON_ERROR(-1);

    return 0;
}

/* ---------------------------------------------------------------------------
 * arithmetically decode the chroma intra prediction mode of a given CU
 */
int aec_read_intra_pmode_c(aec_t *p_aec, davs2_t *h, int luma_mode)
{
    context_t *p_ctx = p_aec->syn_ctx.intra_chroma_pred_mode;
    int act_ctx      = h->lcu.c_ipred_mode_ctx;
    int lmode        = tab_intra_mode_luma2chroma[luma_mode];
    int is_redundant = lmode >= 0;
    int act_sym;

    act_sym = !biari_decode_symbol(p_aec, p_ctx + act_ctx);
    if (act_sym != 0) {
        act_sym = unary_bin_max_decode(p_aec, p_ctx + 2, 0, 3) + 1;
        if (is_redundant && act_sym >= lmode) {
            if (act_sym == 4) {
                davs2_log(h, DAVS2_LOG_ERROR, "Error in intra_chroma_pred_mode. (%d, %d) (%d, %d)", h->lcu.i_pix_x, h->lcu.i_pix_y, h->lcu.i_scu_x, h->lcu.i_scu_y);
                return 4;
            }

            act_sym++;
        }
    }

#if AVS2_TRACE
    avs2_trace("@%d %s\t\t%d\n", symbolCount++, p_aec->tracestring, act_sym);
#endif

    return act_sym;
}

/* ---------------------------------------------------------------------------
*/
static INLINE
int aec_read_last_cg_pos(aec_t *p_aec, context_t *p_ctx, cu_t *p_cu,
                         int *CGx, int *CGy, int b_luma, int num_cg, int is_dc_diag,
                         int num_cg_x_minus1, int num_cg_y_minus1)
{
    int last_cg_x = 0;
    int last_cg_y = 0;
    int last_cg_idx = 0;

    if (b_luma && is_dc_diag) {
        DAVS2_SWAP(num_cg_x_minus1, num_cg_y_minus1);
    }

    if (num_cg == 4) {  // 8x8
        last_cg_idx = 0;
        last_cg_idx += biari_decode_symbol_continu0_ext(p_aec, p_ctx, 2, 3);

        if (b_luma && p_cu->i_trans_size == TU_SPLIT_HOR) {
            last_cg_x = last_cg_idx;
            last_cg_y = 0;
        } else if (b_luma && p_cu->i_trans_size == TU_SPLIT_VER) {
            last_cg_x = 0;
            last_cg_y = last_cg_idx;
        } else {
            last_cg_x = last_cg_idx &  1;
            last_cg_y = last_cg_idx >> 1;
        }
    } else { // 16x16 and 32x32
        int last_cg_bit;

        p_ctx += 3;
        last_cg_bit = biari_decode_symbol(p_aec, p_ctx);

        if (last_cg_bit == 0) {
            last_cg_x = 0;
            last_cg_y = 0;
            last_cg_idx  = 0;
        } else {
            p_ctx++;
            last_cg_x = biari_decode_symbol_continue0(p_aec, p_ctx, num_cg_x_minus1);

            p_ctx++;
            if (last_cg_x == 0) {
                if (num_cg_y_minus1 != 1) {
                    last_cg_y = biari_decode_symbol_continue0(p_aec, p_ctx, num_cg_y_minus1 - 1);
                }

                last_cg_y++;
            } else {
                last_cg_y = biari_decode_symbol_continue0(p_aec, p_ctx, num_cg_y_minus1);
            }
        }

        if (b_luma && is_dc_diag) {
            DAVS2_SWAP(last_cg_x, last_cg_y);
        }

        if (b_luma && p_cu->i_trans_size == TU_SPLIT_HOR) {
            last_cg_idx = raster2ZZ_2x8[last_cg_y * 8 + last_cg_x];
        } else if (b_luma && p_cu->i_trans_size == TU_SPLIT_VER) {
            last_cg_idx = raster2ZZ_8x2[last_cg_y * 2 + last_cg_x];
        } else if (num_cg == 16) {
            last_cg_idx = raster2ZZ_4x4[last_cg_y * 4 + last_cg_x];
        } else {
            last_cg_idx = raster2ZZ_8x8[last_cg_y * 8 + last_cg_x];
        }
    }

    *CGx = last_cg_x;
    *CGy = last_cg_y;
    return last_cg_idx;
}

/* ---------------------------------------------------------------------------
 */
static INLINE 
int aec_read_last_coeff_pos_in_cg(aec_t *p_aec, context_t *p_ctx,
                                  int rank, int cg_x, int cg_y, int b_luma, 
                                  int b_one_cg, int is_dc_diag)
{
    int xx, yy;
    int offset;

    /* AVS2-P2国标: 8.3.3.2.14   确定last_coeff_pos_x 或last_coeff_pos_y 的ctxIdxInc */
    if (b_luma == 0) {                    // 色度分量共占用12个上下文
        offset = b_one_cg ? 0 : 4 + (rank == 0) * 4;
    } else if (b_one_cg) {                // Log2TransformSize 为 2，占用8个上下文
        offset = 40 + is_dc_diag * 4;
    } else if (cg_x != 0 && cg_y != 0) {  // cg_x 和 cg_y 均不为零，占用8个上下文
        offset = 32 + (rank == 0) * 4;
    } else {                              // 其他亮度位置占用40个上下文
        offset = (4 * (rank == 0) + 2 * (cg_x == 0 && cg_y == 0) + is_dc_diag) * 4;
    }

    p_ctx += offset;
    xx = biari_decode_symbol_continu0_ext(p_aec, p_ctx, 1, 3);

    p_ctx += 2;
    yy = biari_decode_symbol_continu0_ext(p_aec, p_ctx, 1, 3);

    if (cg_x == 0 && cg_y > 0 && is_dc_diag) {
        DAVS2_SWAP(xx, yy);
    }
    if (rank != 0) {
        xx = 3 - xx;
        if (is_dc_diag) {
            yy = 3 - yy;
        }
    }

    return tab_scan_coeff_pos_in_cg[yy][xx];
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
int get_abssum_of_n_last_coeffs(runlevel_pair_t *p_runlevel, int end_pair_pos, int start_pair_pos)
{
    int absSum5 = 0;
    int n = 0;
    int k;

    for (k = end_pair_pos - 1; k >= start_pair_pos; k--) {
        n += p_runlevel[k].run;
        if (n >= 6) {
            break;
        }
        absSum5 += DAVS2_ABS(p_runlevel[k].level);
        n++;
    }

    return absSum5;
}

/* ---------------------------------------------------------------------------
 */
typedef int (*aec_read_run_f)(aec_t *p_aec, context_t *p_ctx, int pos, int b_only_one_cg, int b_1st_cg);

/* ---------------------------------------------------------------------------
 */
static
int aec_read_run_luma1(aec_t *p_aec, context_t *p_ctx, int pos, int b_only_one_cg, int b_1st_cg)
{
    int ctxpos;
    int Run = 0;
    int offset = 0;

    b_only_one_cg = b_only_one_cg ? 0 : 4;

    for (ctxpos = 0; Run != pos; ctxpos++) {
        if (ctxpos < pos) {
            int moddiv; // 0，1，2
            moddiv = (tab_scan_4x4[pos - 1 - ctxpos][1] + 1) >> 1;
            offset = (b_1st_cg ? (pos == ctxpos + 1 ? 0 : (1 + moddiv)) : (4 + moddiv)) + b_only_one_cg;  // 0,...,10
        }

        assert(offset >= 0 && offset < NUM_MAP_CTX);
        if (biari_decode_symbol(p_aec, p_ctx + offset)) {
            break;
        }

        Run++;
    }

    return Run;
}

/* ---------------------------------------------------------------------------
 */
static
int aec_read_run_luma2(aec_t *p_aec, context_t *p_ctx, int pos, int b_only_one_cg, int b_1st_cg)
{
    int ctxpos;
    int Run = 0;
    int offset = 0;

    b_only_one_cg = b_only_one_cg ? 0 : 4;
    
    for (ctxpos = 0; Run != pos; ctxpos++) {
        if (ctxpos < pos) {
            int moddiv; // 0，1，2
            moddiv = ((pos < ctxpos + 4) ? 0 : (pos < ctxpos + 11 ? 1 : 2));
            offset = (b_1st_cg ? (pos == ctxpos + 1 ? 0 : (1 + moddiv)) : (4 + moddiv)) + b_only_one_cg;  // 0,...,10
        }

        assert(offset >= 0 && offset < NUM_MAP_CTX);
        if (biari_decode_symbol(p_aec, p_ctx + offset)) {
            break;
        }

        Run++;
    }

    return Run;
}


/* ---------------------------------------------------------------------------
 */
static
int aec_read_run_chroma(aec_t *p_aec, context_t *p_ctx, int pos, int b_only_one_cg, int b_1st_cg)
{
    int ctxpos;
    int Run = 0;
    int offset = 0;

    b_only_one_cg = b_only_one_cg ? 0 : 3;

    for (ctxpos = 0; Run != pos; ctxpos++) {
        if (ctxpos < pos) {
            int moddiv = (pos >= 6 + ctxpos);
            offset = (b_1st_cg ? (pos == ctxpos + 1 ? 0 : (1 + moddiv)) : (3 + moddiv)) + b_only_one_cg;
        }

        assert(offset >= 0 && offset < NUM_MAP_CTX);
        if (biari_decode_symbol(p_aec, p_ctx + offset)) {
            break;
        }

        Run++;
    }

    return Run;
}


/* ---------------------------------------------------------------------------
 */
static
int aec_read_run_level(aec_t *p_aec, cu_t *p_cu, int num_cg, int b_luma, int is_dc_diag,
                       runlevel_t *runlevel, int scale, int shift)
{
    static const int numOfCoeffInCG = 16;
    const int add = (1 << (shift - 1));

    //--- read coefficients for whole block ---
    const int16_t(*tab_cg_scan)[2]         = runlevel->cg_scan;
    context_t(*ctxa_run)[NUM_MAP_CTX]      = runlevel->p_ctx_run;
    context_t *p_ctx_level                 = runlevel->p_ctx_level;
    context_t *p_ctx_nonzero_cg_flag       = runlevel->p_ctx_sig_cg;
    context_t *p_ctx_last_cg_pos           = runlevel->p_ctx_last_cg;
    context_t *p_ctx_last_pos_in_cg        = runlevel->p_ctx_last_pos_in_cg;
    runlevel_pair_t *p_runlevel            = runlevel->run_level;
    int idx_cg;
    int cg_pos = 0;
    int CGx = 0;
    int CGy = 0;
    int b_only_one_cg = (num_cg == 1);
    int8_t dct_pattern = DCT_QUAD;
    int w_tr_half, w_tr_quad; // CG position limitation
    int h_tr_half, h_tr_quad; // CG position limitation
    int w_tr = runlevel->w_tr;
    int h_tr = runlevel->h_tr;
#if AVS2_TRACE
    int idx_runlevel = 0;
#endif
    int rank = 0;
    aec_read_run_f f_read_run = b_luma ? (!is_dc_diag ? aec_read_run_luma1 : aec_read_run_luma2) : aec_read_run_chroma;

    /* dct_pattern_e */
    if (w_tr == h_tr) {
        w_tr_half = w_tr >> 1;
        h_tr_half = h_tr >> 1;
        w_tr_quad = w_tr >> 2;
        h_tr_quad = h_tr >> 2;
    } else if (w_tr > h_tr) {
        w_tr_half = w_tr >> 1;
        h_tr_half = h_tr >> 0;
        w_tr_quad = w_tr >> 2;
        h_tr_quad = h_tr >> 0;
    } else {
        w_tr_half = w_tr >> 0;
        h_tr_half = h_tr >> 1;
        w_tr_quad = w_tr >> 0;
        h_tr_quad = h_tr >> 2;
    }
    /* 转换成CG位置索引的边界位置 */
    w_tr_half >>= 2;
    h_tr_half >>= 2;
    w_tr_quad >>= 2;
    h_tr_quad >>= 2;

    /* 1, read last CG position */
    if (num_cg > 1) {
        int num_cg_x_minus1 = tab_cg_scan[num_cg - 1][0];
        int num_cg_y_minus1 = tab_cg_scan[num_cg - 1][1];
        cg_pos = aec_read_last_cg_pos(p_aec, p_ctx_last_cg_pos, p_cu, &CGx, &CGy, b_luma, num_cg, is_dc_diag, num_cg_x_minus1, num_cg_y_minus1);
    }

    num_cg = cg_pos + 1;
    runlevel->num_nonzero_cg = num_cg;

    /* 2, read coefficients in each CG */
    for (idx_cg = 0; idx_cg < num_cg; idx_cg++) {
        int b_1st_cg = (cg_pos == 0);
        int nonzero_cg_flag = 1;

        /* 2.1, sig CG flag */
        if (rank > 0) {
            /* update CG position */
            int ctx_sig_cg = (b_luma && cg_pos != 0);
            CGx = tab_cg_scan[cg_pos][0];
            CGy = tab_cg_scan[cg_pos][1];
            nonzero_cg_flag = biari_decode_symbol(p_aec, p_ctx_nonzero_cg_flag + ctx_sig_cg);
        }

        /* 2.2, coefficients in CG */
        if (nonzero_cg_flag) {
            int num_pairs_in_cg = 0;
            int i;

            // last in CG
            int pos = aec_read_last_coeff_pos_in_cg(p_aec, p_ctx_last_pos_in_cg, rank, CGx, CGy, b_luma, b_only_one_cg, is_dc_diag);

            for (i = -numOfCoeffInCG; i != 0; i++) {
                // level
                int Run = 0;
                int Level = 1;
                int absSum5;
                context_t *p_ctx;

                /* coeff_level_minus1_band[j] */
                if (biari_decode_final(p_aec)) {
                    int golomb_order  = 0;
                    int binary_symbol = 0;

                    for (;;) {
                        int l = biari_decode_symbol_eq_prob(p_aec);
                        AEC_RETURN_ON_ERROR(-1);
                        if (l) {
                            break;
                        }
                        Level += (1 << golomb_order);
                        golomb_order++;
                    }

                    while (golomb_order--) {
                        // next binary part
                        int sig = biari_decode_symbol_eq_prob(p_aec);
                        binary_symbol |= (sig << golomb_order);
                    }

                    Level += binary_symbol;
                    Level += 32;
                } else {
                    int pairsInCGIdx = (num_pairs_in_cg + 1) >> 1;
                    pairsInCGIdx = DAVS2_MIN(2, pairsInCGIdx);
                    p_ctx = p_ctx_level;
                    p_ctx += 10 * (b_1st_cg && pos < 3) + DAVS2_MIN(rank, pairsInCGIdx + 2) + ((5 * pairsInCGIdx) >> 1);
                    Level += biari_decode_symbol_continue0(p_aec, p_ctx, 31);
                }

                AEC_RETURN_ON_ERROR(-1);
                absSum5 = get_abssum_of_n_last_coeffs(p_runlevel, num_pairs_in_cg, 0);
                absSum5 = (absSum5 + Level) >> 1;
                p_ctx = ctxa_run[DAVS2_MIN(absSum5, 2)];

                // run
                Run = 0;
                if (pos > 0) {
                    Run = f_read_run(p_aec, p_ctx, pos, b_only_one_cg, b_1st_cg);
                }
                AEC_RETURN_ON_ERROR(-1);

#if AVS2_TRACE
                if (b_luma) {
                    avs2_trace("  Luma8x8 sng");
                    avs2_trace("(%2d) level =%3d run =%2d\n", idx_runlevel, level, run);
                } else {
                    avs2_trace("  AC chroma 8X8 ");
                    avs2_trace("%2d: level =%3d run =%2d\n", idx_runlevel, level, run);
                }
                idx_runlevel++;
#endif
                p_runlevel[num_pairs_in_cg].level = (int16_t)Level;
                p_runlevel[num_pairs_in_cg].run   = (int16_t)Run;

                num_pairs_in_cg++;
                if (Level > T_Chr[rank]) {
                    rank = tab_rank[DAVS2_MIN(5, Level)];
                }
                if (Run == pos) {
                    break;
                }

                pos -= (Run + 1);
            } // for (i = -numOfCoeffInCG; i != 0; i++)

            // sign of level
            for (i = 0; i < num_pairs_in_cg; i++) {
                if (biari_decode_symbol_eq_prob(p_aec)) {
                    p_runlevel[i].level = -p_runlevel[i].level;
                }
            }

            /* convert run-level to coefficients */
            {
                const int b_swap_xy  = runlevel->b_swap_xy;
                const int i_coeff    = runlevel->i_res;
                coeff_t *p_res = runlevel->p_res;
                int num_pairs  = num_pairs_in_cg;
                int coef_ctr   = -1;
                if (b_swap_xy) {
                    DAVS2_SWAP(CGx, CGy);
                }
                p_res += i_coeff * (CGy << 2) + (CGx << 2);
                // 将RunLevel转换成CG内的非零系数
                while (num_pairs > 0) {  /* leave if len=1 */
                    int x_in_cg, y_in_cg;

                    int level = p_runlevel[num_pairs - 1].level;
                    int run   = p_runlevel[num_pairs - 1].run;
                    num_pairs--;
                    if (run < 0 || run >= 16) {
                        // davs2_log(h, DAVS2_LOG_ERROR, "wrong run level.");
                        return -1;
                    }
                    coef_ctr += run + 1;

                    x_in_cg = tab_scan_4x4[coef_ctr][ b_swap_xy];
                    y_in_cg = tab_scan_4x4[coef_ctr][!b_swap_xy];

                    level = (level * scale + add) >> shift;
                    p_res[y_in_cg * i_coeff + x_in_cg] = (coeff_t)DAVS2_CLIP3(-32768, 32767, level);
                }

                if (CGy >= h_tr_half || CGx >= w_tr_half) {
                    dct_pattern = DCT_DEAULT;
                } else if ((CGy >= h_tr_quad || CGx >= w_tr_quad) && dct_pattern != DCT_DEAULT) {
                    dct_pattern = DCT_HALF;
                }
            }
        }  // end of reading one CG
        cg_pos--;
    }  // end of reading all CGs

    return dct_pattern;
}

/* ---------------------------------------------------------------------------
 * get coefficients of one block
 */
int8_t cu_get_block_coeffs(aec_t *p_aec, runlevel_t *runlevel,
                           cu_t *p_cu, coeff_t *p_res, int w_tr, int h_tr,
                           int i_tu_level, int b_luma,
                           int intra_pred_class, int b_swap_xy,
                           int scale, int shift, int wq_size_id)
{
    int num_coeffs = w_tr * h_tr;
    int num_cg = num_coeffs >> 4;

    runlevel->p_res         = p_res;
    runlevel->i_res         = w_tr;
    runlevel->b_swap_xy     = b_swap_xy;
    runlevel->i_tu_level    = i_tu_level;
    runlevel->w_tr          = w_tr;
    runlevel->h_tr          = h_tr;
    UNUSED_PARAMETER(wq_size_id);

    return (int8_t)aec_read_run_level(p_aec, p_cu, num_cg, b_luma, intra_pred_class == INTRA_PRED_DC_DIAG, runlevel, scale, shift);
}

/* ---------------------------------------------------------------------------
 * finding end of a slice in case this is not the end of a frame
 *
 * unsure whether the "correction" below actually solves an off-by-one
 * problem or whether it introduces one in some cases :-(  Anyway,
 * with this change the bit stream format works with AEC again.
 */
int aec_startcode_follows(aec_t *p_aec, int eos_bit)
{
    int bit = 0;

    if (eos_bit) {
        bit = biari_decode_final(p_aec);

#if AVS2_TRACE
        avs2_trace("@%d %s\t\t%d\n", symbolCount++, "Decode Sliceterm", bit);
#endif
    }

    /* the best way to be sure that the current slice is end,
     * is to check if a start code is followed */

    return bit;
}

/* ---------------------------------------------------------------------------
 */
int aec_read_split_flag(aec_t *p_aec, int i_level)
{
    context_t *p_ctx = p_aec->syn_ctx.cu_split_flag + (i_level - MIN_CU_SIZE_IN_BIT - 1);
    int split_flag = biari_decode_symbol(p_aec, p_ctx);

#if AVS2_TRACE
    avs2_trace("SplitFlag = %3d\n", split_flag);
#endif

    return split_flag;
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE int read_sao_mergeflag(aec_t *p_aec, int act_ctx)
{
    int act_sym = 0;

    if (act_ctx == 1) {
        act_sym = biari_decode_symbol(p_aec, &p_aec->syn_ctx.sao_mergeflag_context[0]);
    } else if (act_ctx == 2) {
        act_sym = biari_decode_symbol(p_aec, &p_aec->syn_ctx.sao_mergeflag_context[1]);
        if (act_sym != 1) {
            act_sym += (biari_decode_symbol(p_aec, &p_aec->syn_ctx.sao_mergeflag_context[2]) << 1);
        }
    }

    return act_sym;
}

/* ---------------------------------------------------------------------------
 */
int aec_read_sao_mergeflag(aec_t *p_aec, int mergeleft_avail, int mergeup_avail)
{
    int merge_left  = 0;
    int merge_top   = 0;
    int merge_index = read_sao_mergeflag(p_aec, mergeleft_avail + mergeup_avail);

    assert(merge_index <= 2);

    if (mergeleft_avail) {
        merge_left  = merge_index & 0x01;
        merge_index = merge_index >> 1;
    }
    if (mergeup_avail && !merge_left) {
        merge_top = merge_index & 0x01;
    }

    return (merge_left << 1) + merge_top;
}

/* ---------------------------------------------------------------------------
 */
int aec_read_sao_mode(aec_t *p_aec)
{
    int t2 = !biari_decode_symbol(p_aec, p_aec->syn_ctx.sao_mode_context);
    int act_sym;

    if (t2) {
        int t1 = !biari_decode_symbol_eq_prob(p_aec);
        act_sym = t2 + (t1 << 1);
    } else {
        act_sym = 0;
    }

    return act_sym;
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE int read_sao_offset(aec_t *p_aec, int offset_type)
{
    int maxvalue = saoclip[offset_type][2];
    int cnt = 0;
    int act_sym, sym;

    if (offset_type == SAO_CLASS_BO) {
        sym = !biari_decode_symbol(p_aec, &p_aec->syn_ctx.sao_offset_context[0]);
    } else {
        sym = !biari_decode_symbol_eq_prob(p_aec);
    }

    while (sym) {
        cnt++;
        if (cnt == maxvalue) {
            break;
        }
        sym = !biari_decode_symbol_eq_prob(p_aec);
    }

    if (offset_type == SAO_CLASS_EO_FULL_VALLEY) {
        act_sym = EO_OFFSET_INV__MAP[cnt];
    } else if (offset_type == SAO_CLASS_EO_FULL_PEAK) {
        act_sym = -EO_OFFSET_INV__MAP[cnt];
    } else if (offset_type == SAO_CLASS_EO_HALF_PEAK) {
        act_sym = -cnt;
    } else {
        act_sym = cnt;
    }

    if (offset_type == SAO_CLASS_BO && act_sym) {
        if (biari_decode_symbol_eq_prob(p_aec)) { // sign symbol
            act_sym = -act_sym;
        }
    }

    return act_sym;
}

/* ---------------------------------------------------------------------------
 */
void aec_read_sao_offsets(aec_t *p_aec, sao_param_t *p_sao_param, int *offset)
{
    int i;

    assert(p_sao_param->modeIdc == SAO_MODE_NEW);

    for (i = 0; i < 4; i++) {
        int offset_type;
        if (p_sao_param->typeIdc == SAO_TYPE_BO) {
            offset_type = SAO_CLASS_BO;
        } else {
            offset_type = (i >= 2) ? (i + 1) : i;
        }
        offset[i] = read_sao_offset(p_aec, offset_type);
    }
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE int read_sao_type(aec_t *p_aec, int act_ctx)
{
    int act_sym = 0;
    int golomb_order = 1;
    int length;

    if (act_ctx == 0) {
        length = NUM_SAO_EO_TYPES_LOG2;
    } else if (act_ctx == 1) {
        length = NUM_SAO_BO_CLASSES_LOG2;
    } else {
        assert(act_ctx == 2);
        length = NUM_SAO_BO_CLASSES_LOG2 - 1;
    }

    if (act_ctx == 2) {
        int temp;
        int rest;

        do {
            temp = biari_decode_symbol_eq_prob(p_aec);
            AEC_RETURN_ON_ERROR(-1);

            if (temp == 0) {
                act_sym += (1 << golomb_order);
                golomb_order++;
            }

            if (golomb_order == 4) {
                golomb_order = 0;
                temp = 1;
            }

        } while (temp != 1);

        rest = 0;
        while (golomb_order--) {
            // next binary part
            temp = biari_decode_symbol_eq_prob(p_aec);
            if (temp == 1) {
                rest |= (temp << golomb_order);
            }
        }

        act_sym += rest;
    } else {
        int i;

        for (i = 0; i < length; i++) {
            act_sym = act_sym + (biari_decode_symbol_eq_prob(p_aec) << i);
        }
    }

    return act_sym;
}

/* ---------------------------------------------------------------------------
 */
int aec_read_sao_type(aec_t *p_aec, sao_param_t *p_sao_param)
{
    int stBnd[2];
    
    assert(p_sao_param->modeIdc == SAO_MODE_NEW);
    if (p_sao_param->typeIdc == SAO_TYPE_BO) {
        stBnd[0] = read_sao_type(p_aec, 1);

        // read delta start band for BO
        stBnd[1] = read_sao_type(p_aec, 2) + 2;
        return (stBnd[0] + (stBnd[1] << NUM_SAO_BO_CLASSES_LOG2));
    } else {
        assert(p_sao_param->typeIdc == SAO_TYPE_EO_0);
        return read_sao_type(p_aec, 0);
    }
}

/* ---------------------------------------------------------------------------
 */
int aec_read_alf_lcu_ctrl(aec_t *p_aec)
{
    context_t *ctx = p_aec->syn_ctx.alf_lcu_enable_scmodel;

    return biari_decode_symbol(p_aec, ctx);
}
