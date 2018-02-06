/*
 *  aec.h
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

#ifndef DAVS2_AEC_H
#define DAVS2_AEC_H

#ifdef __cplusplus
extern "C" {
#endif
    
/* ---------------------------------------------------------------------------
 * global variables */
extern const int        saoclip[NUM_SAO_OFFSET][3];
extern const int        tab_intra_mode_scan_type[NUM_INTRA_MODE];

/* ---------------------------------------------------------------------------
 * aec basic operations */
void aec_init_contexts      (aec_t *p_aec);
void aec_new_slice          (davs2_t *h);

int  aec_start_decoding     (aec_t *p_aec, uint8_t *p_start, int i_byte_pos, int i_bytes);
int  aec_bits_read          (aec_t *p_aec);
int  aec_startcode_follows  (aec_t *p_aec, int eos_bit);

/* ---------------------------------------------------------------------------
 * ctu structure information */
int  aec_read_split_flag    (aec_t *p_aec, int i_level);

/* ---------------------------------------------------------------------------
 * cu type information */
int  aec_read_cu_type       (aec_t *p_aec, cu_t *p_cu, int img_type, int b_amp, int b_mhp, int b_wsm, int num_references);
int  aec_read_cu_type_sframe(aec_t *p_aec);
int  aec_read_intra_cu_type (aec_t *p_aec, cu_t *p_cu, int b_sdip, davs2_t *h);

/* ---------------------------------------------------------------------------
 * inter prediction information */
int  aec_read_dmh_mode      (aec_t *p_aec, int i_cu_level);
void aec_read_mvds          (aec_t *p_aec, mv_t *p_mvd);
void aec_read_inter_pred_dir(aec_t * p_aec, cu_t *p_cu, davs2_t *h);

/* ---------------------------------------------------------------------------
 * intra prediction information */
int  aec_read_intra_pmode   (aec_t *p_aec);
int  aec_read_intra_pmode_c (aec_t *p_aec, davs2_t *h, int luma_mode);

/* ---------------------------------------------------------------------------
 * transform unit (residual) information */
int  cu_read_cbp            (davs2_t *h, aec_t *p_aec, cu_t *p_cu, int scu_x, int scu_y);
int8_t cu_get_block_coeffs  (aec_t *p_aec, runlevel_t *runlevel,
                             cu_t *p_cu, coeff_t *p_res, int w_tr, int h_tr,
                             int i_tu_level, int b_luma,
                             int intra_pred_class, int b_swap_xy,
                             int scale, int shift, int wq_size_id);

/* ---------------------------------------------------------------------------
 * loop filter information */
int  aec_read_sao_mergeflag (aec_t *p_aec, int mergeleft_avail, int mergeup_avail);
int  aec_read_sao_mode      (aec_t *p_aec);
void aec_read_sao_offsets   (aec_t *p_aec, sao_param_t *p_sao_param, int *offset);
int  aec_read_sao_type      (aec_t *p_aec, sao_param_t *p_sao_param);

int  aec_read_alf_lcu_ctrl  (aec_t *p_aec);

#ifndef AEC_RETURN_ON_ERROR
#define AEC_RETURN_ON_ERROR(ret_code) \
        if (p_aec->b_bit_error) {\
            p_aec->b_bit_error = FALSE; /* reset error flag */\
            /* davs2_log(h, AVS2_LOG_ERROR, "aec decoding error."); */\
            return (ret_code);\
        }
#endif

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_AEC_H
