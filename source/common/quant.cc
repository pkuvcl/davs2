/*
 * quant.c
 *
 * Description of this file:
 *    Quant functions definition of the davs2 library
 *
 * --------------------------------------------------------------------------
 *
 *    davs2 - video decoder of AVS2/IEEE1857.4 video coding standard
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
#include "vec/intrinsic.h"

/* ---------------------------------------------------------------------------
 */
const int16_t wq_param_default[2][6] = {
    { 67, 71, 71, 80, 80, 106},
    { 64, 49, 53, 58, 58, 64 }
};

/* ---------------------------------------------------------------------------
 */
static const int g_WqMDefault4x4[16] = {
    64, 64, 64, 68,
    64, 64, 68, 72,
    64, 68, 76, 80,
    72, 76, 84, 96
};

/* ---------------------------------------------------------------------------
 */
static const int g_WqMDefault8x8[64] = {
    64,  64,  64,  64,  68,  68,  72,  76,
    64,  64,  64,  68,  72,  76,  84,  92,
    64,  64,  68,  72,  76,  80,  88,  100,
    64,  68,  72,  80,  84,  92,  100, 28,
    68,  72,  80,  84,  92,  104, 112, 128,
    76,  80,  84,  92,  104, 116, 132, 152,
    96,  100, 104, 116, 124, 140, 164, 188,
    104, 108, 116, 128, 152, 172, 192, 216
};

/* ---------------------------------------------------------------------------
 */
static const uint8_t WeightQuantModel[4][64] = {
    //   l a b c d h
    //   0 1 2 3 4 5
    {
        // Mode 0
        0, 0, 0, 4, 4, 4, 5, 5,
        0, 0, 3, 3, 3, 3, 5, 5,
        0, 3, 2, 2, 1, 1, 5, 5,
        4, 3, 2, 2, 1, 5, 5, 5,
        4, 3, 1, 1, 5, 5, 5, 5,
        4, 3, 1, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5
    }, {
        // Mode 1
        0, 0, 0, 4, 4, 4, 5, 5,
        0, 0, 4, 4, 4, 4, 5, 5,
        0, 3, 2, 2, 2, 1, 5, 5,
        3, 3, 2, 2, 1, 5, 5, 5,
        3, 3, 2, 1, 5, 5, 5, 5,
        3, 3, 1, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5
    }, {
        // Mode 2
        0, 0, 0, 4, 4, 3, 5, 5,
        0, 0, 4, 4, 3, 2, 5, 5,
        0, 4, 4, 3, 2, 1, 5, 5,
        4, 4, 3, 2, 1, 5, 5, 5,
        4, 3, 2, 1, 5, 5, 5, 5,
        3, 2, 1, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5
    }, {
        // Mode 3
        0, 0, 0, 3, 2, 1, 5, 5,
        0, 0, 4, 3, 2, 1, 5, 5,
        0, 4, 4, 3, 2, 1, 5, 5,
        3, 3, 3, 3, 2, 5, 5, 5,
        2, 2, 2, 2, 5, 5, 5, 5,
        1, 1, 1, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5
    }
};

/* ---------------------------------------------------------------------------
 */
static const uint8_t WeightQuantModel4x4[4][16] = {
    //   l a b c d h
    //   0 1 2 3 4 5
    {
        // Mode 0
        0, 4, 3, 5,
        4, 2, 1, 5,
        3, 1, 1, 5,
        5, 5, 5, 5
    }, {
        // Mode 1
        0, 4, 4, 5,
        3, 2, 2, 5,
        3, 2, 1, 5,
        5, 5, 5, 5
    }, {
        // Mode 2
        0, 4, 3, 5,
        4, 3, 2, 5,
        3, 2, 1, 5,
        5, 5, 5, 5
    }, {
        // Mode 3
        0, 3, 1, 5,
        3, 4, 2, 5,
        1, 2, 2, 5,
        5, 5, 5, 5
    }
};

/**
 * ===========================================================================
 * function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
const int *wq_get_default_matrix(int sizeId)
{
    return (sizeId == 0) ? g_WqMDefault4x4 : g_WqMDefault8x8;
}

/* ---------------------------------------------------------------------------
 */
void wq_init_frame_quant_param(davs2_t *h)
{
    weighted_quant_t *p = &h->wq;
    int uiWQMSizeId;
    int i, j, k;

    assert(h->seq_info.enable_weighted_quant);

    for (uiWQMSizeId = 0; uiWQMSizeId < 4; uiWQMSizeId++) {
        for (i = 0; i < 64; i++) {
            p->cur_wq_matrix[uiWQMSizeId][i] = 1 << 7;
        }
    }

    for (i = 0; i < 2; i++) {
        for (j = 0; j < 6; j++) {
            p->wquant_param[i][j] = 128;
        }
    }

    if (p->wq_param == 0) {
        for (i = 0; i < 6; i++) {
            p->wquant_param[DETAILED][i] = wq_param_default[DETAILED][i];
        }
    } else if (p->wq_param == 1) {
        for (i = 0; i < 6; i++) {
            p->wquant_param[UNDETAILED][i] = p->quant_param_undetail[i];
        }
    }

    if (p->wq_param == 2) {
        for (i = 0; i < 6; i++) {
            p->wquant_param[DETAILED][i] = p->quant_param_detail[i];
        }
    }

    // reconstruct the weighting matrix
    for (k = 0; k < 2; k++) {
        for (j = 0; j < 8; j++) {
            for (i = 0; i < 8; i++) {
                p->wq_matrix[1][k][j * 8 + i] = p->wquant_param[k][WeightQuantModel[p->wq_model][j * 8 + i]];
            }
        }
    }

    for (k = 0; k < 2; k++) {
        for (j = 0; j < 4; j++) {
            for (i = 0; i < 4; i++) {
                p->wq_matrix[0][k][j * 4 + i] = p->wquant_param[k][WeightQuantModel4x4[p->wq_model][j * 4 + i]];
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 */
void wq_update_frame_matrix(davs2_t *h)
{
    weighted_quant_t *p = &h->wq;
    int uiWQMSizeId, uiWMQId;
    int uiBlockSize;
    int i;

    assert(h->seq_info.enable_weighted_quant);

    for (uiWQMSizeId = 0; uiWQMSizeId < 4; uiWQMSizeId++) {
        uiBlockSize = DAVS2_MIN(1 << (uiWQMSizeId + 2), 8);
        uiWMQId = (uiWQMSizeId < 2) ? uiWQMSizeId : 1;

        if (p->pic_wq_data_index == 0) {
            for (i = 0; i < (uiBlockSize * uiBlockSize); i++) {
                p->cur_wq_matrix[uiWQMSizeId][i] = p->seq_wq_matrix[uiWMQId][i];
            }
        } else if (p->pic_wq_data_index == 1) {
            if (p->wq_param == 0) {
                for (i = 0; i < (uiBlockSize * uiBlockSize); i++) {
                    p->cur_wq_matrix[uiWQMSizeId][i] = p->wq_matrix[uiWMQId][DETAILED][i];// detailed weighted matrix
                }
            } else if (p->wq_param == 1) {
                for (i = 0; i < (uiBlockSize * uiBlockSize); i++) {
                    p->cur_wq_matrix[uiWQMSizeId][i] = p->wq_matrix[uiWMQId][0][i];       // undetailed weighted matrix
                }
            }

            if (p->wq_param == 2) {
                for (i = 0; i < (uiBlockSize * uiBlockSize); i++) {
                    p->cur_wq_matrix[uiWQMSizeId][i] = p->wq_matrix[uiWMQId][1][i];       // detailed weighted matrix
                }
            }
        } else if (p->pic_wq_data_index == 2) {
            for (i = 0; i < (uiBlockSize * uiBlockSize); i++) {
                p->cur_wq_matrix[uiWQMSizeId][i] = p->pic_user_wq_matrix[uiWMQId][i];
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void dequant_c(coeff_t *p_coeff, const int i_coef, const int scale, const int shift)
{
    const int add = (1 << (shift - 1));
    int i;

    for (i = 0; i < i_coef; i++) {
        if (p_coeff[i]) {
            p_coeff[i] = (coeff_t)DAVS2_CLIP3(-32768, 32767, (p_coeff[i] * scale + add) >> shift);
        }
    }
}

/* ---------------------------------------------------------------------------
 */
static void dequant_weighted_c(coeff_t *p_coeff, int i_coeff, int bsx, int bsy, int scale, int shift, int16_t *wq_matrix, int wqm_shift, int wqm_size_id)
{
    const int add          = 1 << (shift - 1);
    const int wqm_size     = 1 << (wqm_size_id + 2);
    const int stride_shift = DAVS2_CLIP3(0, 2, wqm_size_id - 1);
    const int stride       = wqm_size >> stride_shift;
    int i, j;

    for (j = 0; j < bsy; j++) {
        for (i = 0; i < bsx; i++) {
            int wqm_coef = wq_matrix[((j >> stride_shift) & (stride - 1)) * stride + ((i >> stride_shift) & (stride - 1))];
            if (p_coeff[i]) {
                int cur_coeff = (((((p_coeff[i] * wqm_coef) >> wqm_shift) * scale) >> 4) + add) >> shift;
                p_coeff[i] = (coeff_t)DAVS2_CLIP3(-32768, 32767, cur_coeff);
            }
        }
        p_coeff += i_coeff;
    }
}

/* ---------------------------------------------------------------------------
 * dequant the coefficients
 */
void dequant_coeffs(davs2_t *h, coeff_t *p_coeff, int bsx, int bsy, int scale, int shift, int WQMSizeId)
{
    if (h->seq_info.enable_weighted_quant) {
        int wqm_shift = (h->wq.pic_wq_data_index == 1) ? 3 : 0;
        dequant_weighted_c(p_coeff, bsx, bsx, bsy, scale, shift, h->wq.cur_wq_matrix[WQMSizeId], wqm_shift, WQMSizeId);
    } else {
        gf_davs2.dequant(p_coeff, bsx * bsy, scale, shift);
    }
}

/* ---------------------------------------------------------------------------
 */
void davs2_quant_init(uint32_t cpuid, ao_funcs_t *fh)
{
    /* init c function handles */
    fh->dequant   = dequant_c;

    /* init asm function handles */
#if HAVE_MMX
    if (cpuid & DAVS2_CPU_SSE4) {
        fh->dequant = davs2_dequant_sse4;
    }
#endif  // if HAVE_MMX
}
