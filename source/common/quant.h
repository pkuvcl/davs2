/*
 * quant.h
 *
 * Description of this file:
 *    Quant functions definition of the davs2 library
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

#ifndef DAVS2_QUANT_H
#define DAVS2_QUANT_H
#ifdef __cplusplus
extern "C" {
#endif

#define QP_SCALE_CR FPFX(QP_SCALE_CR)
extern const uint8_t  QP_SCALE_CR[];
#define IQ_SHIFT FPFX(IQ_SHIFT)
extern const int16_t  IQ_SHIFT[];
#define IQ_TAB FPFX(IQ_TAB)
extern const uint16_t IQ_TAB[];
#define wq_param_default FPFX(wq_param_default)
extern const int16_t wq_param_default[2][6];


/**
 * ---------------------------------------------------------------------------
 * Weight Quant
 * - Adaptive Frequency Weighting Quantization, include:
 *      a). Frequency weighting model, quantization
 *      b). Picture level user-defined frequency weighting
 *      c). LCU level adaptive frequency weighting mode decision
 *   According to adopted proposals: m1878, m2148, m2331
 * ---------------------------------------------------------------------------
 */
#define PARAM_NUM  6
#define WQ_MODEL_NUM 3

#define UNDETAILED 0
#define DETAILED   1

#define WQ_MODE_F  0
#define WQ_MODE_U  1
#define WQ_MODE_D  2

#define wq_get_default_matrix FPFX(wq_get_default_matrix)
const int *wq_get_default_matrix(int sizeId);

#define wq_init_frame_quant_param FPFX(wq_init_frame_quant_param)
void wq_init_frame_quant_param(davs2_t *h);
#define wq_update_frame_matrix FPFX(wq_update_frame_matrix)
void wq_update_frame_matrix(davs2_t *h);


/* dequant */
#define dequant_coeffs FPFX(dequant_coeffs)
void dequant_coeffs(davs2_t *h, coeff_t *p_coeff, int bsx, int bsy, int scale, int shift, int WQMSizeId);
#define davs2_quant_init FPFX(quant_init)
void davs2_quant_init(uint32_t cpuid, ao_funcs_t *fh);


/* ---------------------------------------------------------------------------
 * get qp in chroma component
 */
static ALWAYS_INLINE
int cu_get_chroma_qp(davs2_t * h, int luma_qp, int uv)
{
    int qp = luma_qp + (uv == 0 ? h->chroma_quant_param_delta_u : h->chroma_quant_param_delta_v);

#if HIGH_BIT_DEPTH
    const int bit_depth_offset = ((h->sample_bit_depth - 8) << 3);
    qp -= bit_depth_offset;
    qp = qp < 0 ? qp : QP_SCALE_CR[qp];
    qp = DAVS2_CLIP3(0, 63 + bit_depth_offset, qp + bit_depth_offset);

#else
    qp = QP_SCALE_CR[DAVS2_CLIP3(0, 63, qp)];
#endif

    return qp;
}



/* ---------------------------------------------------------------------------
 * get quant parameters
 */
static ALWAYS_INLINE
void cu_get_quant_params(davs2_t * h, int qp, int bit_size, 
                         int *shift, int *scale)
{
    *shift = IQ_SHIFT[qp] + (h->sample_bit_depth + 1) + bit_size - LIMIT_BIT;
    *scale = IQ_TAB[qp];
}


#ifdef __cplusplus
}
#endif
#endif  // DAVS2_QUANT_H
