/*
 * header.cc
 *
 * Description of this file:
 *    Header functions definition of the davs2 library
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
#include "davs2.h"
#include "transform.h"
#include "vlc.h"
#include "header.h"
#include "aec.h"
#include "alf.h"
#include "quant.h"
#include "bitstream.h"
#include "decoder.h"
#include "frame.h"
#include "predict.h"
#include "quant.h"
#include "cpu.h"

/**
 * ===========================================================================
 * const variable defines
 * ===========================================================================
 */
extern const int8_t *tab_DL_Avails[MAX_CU_SIZE_IN_BIT + 1];
extern const int8_t *tab_TR_Avails[MAX_CU_SIZE_IN_BIT + 1];

static const uint8_t ALPHA_TABLE[64] = {
     0,  0,  0,  0,  0,  0,  1,  1,
     1,  1,  1,  2,  2,  2,  3,  3,
     4,  4,  5,  5,  6,  7,  8,  9,
    10, 11, 12, 13, 15, 16, 18, 20,
    22, 24, 26, 28, 30, 33, 33, 35,
    35, 36, 37, 37, 39, 39, 42, 44,
    46, 48, 50, 52, 53, 54, 55, 56,
    57, 58, 59, 60, 61, 62, 63, 64
};

/* ---------------------------------------------------------------------------
 */
static const uint8_t BETA_TABLE[64] = {
     0,  0,  0,  0,  0,  0,  1,  1,
     1,  1,  1,  1,  1,  2,  2,  2,
     2,  2,  3,  3,  3,  3,  4,  4,
     4,  4,  5,  5,  5,  5,  6,  6,
     6,  7,  7,  7,  8,  8,  8,  9,
     9, 10, 10, 11, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 23, 24, 24, 25, 25, 26, 27
};

/* ---------------------------------------------------------------------------
 * extension id */
enum extension_id_e {
    SEQUENCE_DISPLAY_EXTENSION_ID   = 2,
    TEMPORAL_SCALABLE_EXTENSION_ID  = 3,
    COPYRIGHT_EXTENSION_ID          = 4,
    PICTURE_DISPLAY_EXTENSION_ID    = 7,
    CAMERAPARAMETERS_EXTENSION_ID   = 11,
    LOCATION_DATA_EXTENSION_ID      = 15
};

#define ROI_DATA_FILE   "roi.dat"     // ROI location data output

static bool_t open_dbp_buffer_warning = 1;

/**
 * ===========================================================================
 * local function defines
 * ===========================================================================
 */

static INLINE int is_valid_qp(davs2_t *h, int i_qp)
{
    return i_qp >= 0 && i_qp <= (63 + 8 * (h->sample_bit_depth - 8));
}


/* ---------------------------------------------------------------------------
 */
static
void davs2_reconfigure_decoder(davs2_mgr_t *h)
{
    UNUSED_PARAMETER(h);
}

/* ---------------------------------------------------------------------------
 * sequence header
 */
static
int parse_sequence_header(davs2_mgr_t *mgr, davs2_seq_t *seq, davs2_bs_t *bs)
{
    static const float FRAME_RATE[8] = {
        24000.0f / 1001.0f, 24.0f, 25.0f, 30000.0f / 1001.0f, 30.0f, 50.0f, 60000.0f / 1001.0f, 60.0f
    };
    rps_t *p_rps      = NULL;

    int i, j;
    int num_of_rps;

    bs->i_bit_pos += 32; /* skip start code */

    memset(seq, 0, sizeof(davs2_seq_t));  // reset all value

    seq->head.profile_id       = u_v(bs, 8, "profile_id");
    seq->head.level_id         = u_v(bs, 8, "level_id");
    seq->head.progressive      = u_v(bs, 1, "progressive_sequence");
    seq->b_field_coding        = u_flag(bs, "field_coded_sequence");

    seq->head.width     = u_v(bs, 14, "horizontal_size");
    seq->head.height    = u_v(bs, 14, "vertical_size");

    if (seq->head.width < 16 || seq->head.height < 16) {
        return -1;
    }

    seq->head.chroma_format = u_v(bs, 2, "chroma_format");

    if (seq->head.chroma_format != CHROMA_420 && seq->head.chroma_format != CHROMA_400) {
        return -1;
    }
    if (seq->head.chroma_format == CHROMA_400) {
        davs2_log(mgr, DAVS2_LOG_WARNING, "Un-supported Chroma Format YUV400 as 0 for GB/T.\n");
    }

    /* sample bit depth */
    if (seq->head.profile_id == MAIN10_PROFILE) {
        seq->sample_precision      = u_v(bs, 3, "sample_precision");
        seq->encoding_precision    = u_v(bs, 3, "encoding_precision");
    } else {
        seq->sample_precision      = u_v(bs, 3, "sample_precision");
        seq->encoding_precision    = 1;
    }
    if (seq->sample_precision < 1 || seq->sample_precision > 3 ||
        seq->encoding_precision < 1 || seq->encoding_precision > 3) {
        return -1;
    }

    seq->head.internal_bit_depth   = 6 + (seq->encoding_precision << 1);
    seq->head.output_bit_depth     = 6 + (seq->encoding_precision << 1);
    seq->head.bytes_per_sample     = seq->head.output_bit_depth > 8 ? 2 : 1;

    /*  */
    seq->head.aspect_ratio         = u_v(bs, 4, "aspect_ratio_information");
    seq->head.frame_rate_id        = u_v(bs, 4, "frame_rate_id");
    seq->bit_rate_lower            = u_v(bs, 18, "bit_rate_lower");
    u_v(bs, 1,  "marker bit");
    seq->bit_rate_upper            = u_v(bs, 12, "bit_rate_upper");
    seq->head.low_delay            = u_v(bs, 1, "low_delay");
    u_v(bs, 1,  "marker bit");
    seq->b_temporal_id_exist       = u_flag(bs, "temporal_id exist flag"); // get Extension Flag
    u_v(bs, 18, "bbv buffer size");

    seq->log2_lcu_size             = u_v(bs, 3, "Largest Coding Block Size");

    if (seq->log2_lcu_size < 4 || seq->log2_lcu_size > 6) {
        davs2_log(mgr, DAVS2_LOG_ERROR, "Invalid LCU size: %d\n", seq->log2_lcu_size);
        return -1;
    }

    seq->enable_weighted_quant     = u_flag(bs, "enable_weighted_quant");

    if (seq->enable_weighted_quant) {
        int load_seq_wquant_data_flag;
        int x, y, sizeId, uiWqMSize;
        const int *Seq_WQM;

        load_seq_wquant_data_flag = u_flag(bs,  "load_seq_weight_quant_data_flag");

        for (sizeId = 0; sizeId < 2; sizeId++) {
            uiWqMSize = DAVS2_MIN(1 << (sizeId + 2), 8);
            if (load_seq_wquant_data_flag == 1) {
                for (y = 0; y < uiWqMSize; y++) {
                    for (x = 0; x < uiWqMSize; x++) {
                        seq->seq_wq_matrix[sizeId][y * uiWqMSize + x] = (int16_t)ue_v(bs, "weight_quant_coeff");
                    }
                }
            } else if (load_seq_wquant_data_flag == 0) {
                Seq_WQM = wq_get_default_matrix(sizeId);
                for (i = 0; i < (uiWqMSize * uiWqMSize); i++) {
                    seq->seq_wq_matrix[sizeId][i] = (int16_t)Seq_WQM[i];
                }
            }
        }
    }

    seq->enable_background_picture = u_flag(bs, "background_picture_disable") ^ 0x01;
    seq->enable_mhp_skip           = u_flag(bs, "mhpskip enabled");
    seq->enable_dhp                = u_flag(bs, "dhp enabled");
    seq->enable_wsm                = u_flag(bs, "wsm enabled");
    seq->enable_amp                = u_flag(bs, "Asymmetric Motion Partitions");
    seq->enable_nsqt               = u_flag(bs, "use NSQT");
    seq->enable_sdip               = u_flag(bs, "use NSIP");
    seq->enable_2nd_transform      = u_flag(bs, "secT enabled");
    seq->enable_sao                = u_flag(bs, "SAO Enable Flag");
    seq->enable_alf                = u_flag(bs, "ALF Enable Flag");
    seq->enable_pmvr               = u_flag(bs, "pmvr enabled");

    if (1 != u_v(bs, 1, "marker bit"))  {
        davs2_log(mgr, DAVS2_LOG_ERROR, "expected marker_bit 1 while received 0, FILE %s, Row %d\n", __FILE__, __LINE__);
    }

    num_of_rps                      = u_v(bs, 6, "num_of_RPS");
    if (num_of_rps > AVS2_GOP_NUM) {
        return -1;
    }

    seq->num_of_rps = num_of_rps;

    for (i = 0; i < num_of_rps; i++) {
        p_rps = &seq->seq_rps[i];

        p_rps->refered_by_others        = u_v(bs, 1,  "refered by others");
        p_rps->num_of_ref               = u_v(bs, 3,  "num of reference picture");

        for (j = 0; j < p_rps->num_of_ref; j++) {
            p_rps->ref_pic[j]           = u_v(bs, 6,  "delta COI of ref pic");
        }

        p_rps->num_to_remove            = u_v(bs, 3,  "num of removed picture");

        for (j = 0; j < p_rps->num_to_remove; j++) {
            p_rps->remove_pic[j]        = u_v(bs, 6,  "delta COI of removed pic");
        }

        if (1 != u_v(bs, 1, "marker bit"))  {
            davs2_log(mgr, DAVS2_LOG_ERROR, "expected marker_bit 1 while received 0, FILE %s, Row %d\n", __FILE__, __LINE__);
        }
    }

    if (seq->head.low_delay == 0) {
        seq->picture_reorder_delay = u_v(bs, 5, "picture_reorder_delay");
    }

    seq->cross_loop_filter_flag    = u_flag(bs, "Cross Loop Filter Flag");
    u_v(bs, 2,  "reserved bits");

    bs_align(bs); /* align position */

    seq->head.bitrate    = ((seq->bit_rate_upper << 18) + seq->bit_rate_lower) * 400;
    seq->head.frame_rate = FRAME_RATE[seq->head.frame_rate_id - 1];

    seq->i_enc_width     = ((seq->head.width + MIN_CU_SIZE - 1) >> MIN_CU_SIZE_IN_BIT) << MIN_CU_SIZE_IN_BIT;
    seq->i_enc_height    = ((seq->head.height   + MIN_CU_SIZE - 1) >> MIN_CU_SIZE_IN_BIT) << MIN_CU_SIZE_IN_BIT;
    seq->valid_flag = 1;

    return 0;
}


/* ---------------------------------------------------------------------------
 * init deblock parame of one frame
 */
static INLINE
void deblock_init_frame_parames(davs2_t *h)
{
    int shift = h->sample_bit_depth - 8;
    int QP   = h->i_picture_qp - (shift << 3);
    int QP_c = cu_get_chroma_qp(h, h->i_picture_qp, 0) - (shift << 3);

    h->alpha   = ALPHA_TABLE[DAVS2_CLIP3(0, 63, QP + h->i_alpha_offset)] << shift;
    h->beta    = BETA_TABLE[DAVS2_CLIP3(0, 63, QP + h->i_beta_offset)] << shift;

    h->alpha_c = ALPHA_TABLE[DAVS2_CLIP3(0, 63, QP_c + h->i_alpha_offset)] << shift;
    h->beta_c  = BETA_TABLE[DAVS2_CLIP3(0, 63, QP_c + h->i_beta_offset)] << shift;


    if (gf_davs2.set_deblock_const != NULL) {
        gf_davs2.set_deblock_const();
    }
}

/* ---------------------------------------------------------------------------
 * Intra picture header
 */
static int parse_picture_header_intra(davs2_t *h, davs2_bs_t *bs)
{
    int time_code_flag;
    int progressive_frame;
    int predict;
    int i;

    h->i_frame_type = AVS2_I_SLICE;

    /* skip start code */
    bs->i_bit_pos += 32;

    u_v(bs, 32, "bbv_delay");
    time_code_flag                      = u_v(bs, 1, "time_code_flag");

    if (time_code_flag) {
        /* time_code                 = */ u_v(bs, 24, "time_code");
    }

    if (h->b_bkgnd_picture) {
        int background_picture_flag     = u_v(bs, 1, "background_picture_flag");

        if (background_picture_flag) {
            int b_output                = u_v(bs, 1, "background_picture_output_flag");
            if (b_output) {
                h->i_frame_type = AVS2_G_SLICE;
            } else {
                h->i_frame_type = AVS2_GB_SLICE;
            }
        }
    }

    h->i_coi                            = u_v(bs, 8, "coding_order");

    if (h->seq_info.b_temporal_id_exist == 1) {
        h->i_cur_layer                  = u_v(bs, TEMPORAL_MAXLEVEL_BIT, "temporal_id");
    }

    if (h->seq_info.head.low_delay == 0) {
        h->i_display_delay              = ue_v(bs, "picture_output_delay");
        if (h->i_display_delay >= 64) {
            davs2_log(h, DAVS2_LOG_ERROR, "invalid picture output delay intra.");
            return -1;
        }
    }

    predict                             = u_v(bs, 1, "use RCS in SPS");
    if (predict) {
        int index                       = u_v(bs, 5, "predict for RCS");
        if (index >= h->seq_info.num_of_rps) {
            davs2_log(h, DAVS2_LOG_ERROR, "invalid rps index.");
            return -1;
        }

        h->rps                          = h->seq_info.seq_rps[index];
    } else {
        h->rps.refered_by_others        = u_v(bs, 1, "refered by others");
        h->rps.num_of_ref               = u_v(bs, 3, "num of reference picture");
        if (h->rps.num_of_ref > AVS2_MAX_REFS) {
            davs2_log(h, DAVS2_LOG_ERROR, "invalid number of references.");
            return -1;
        }

        for (i = 0; i < h->rps.num_of_ref; i++) {
            h->rps.ref_pic[i]           = u_v(bs, 6, "delta COI of ref pic");
        }

        h->rps.num_to_remove            = u_v(bs, 3, "num of removed picture");
        assert(h->rps.num_to_remove <= sizeof(h->rps.remove_pic) / sizeof(h->rps.remove_pic[0]));

        for (i = 0; i < h->rps.num_to_remove; i++) {
            h->rps.remove_pic[i]        = u_v(bs, 6, "delta COI of removed pic");
        }
        u_v(bs, 1, "marker bit");
    }

    if (h->seq_info.head.low_delay) {
        /* bbv_check_times           = */ ue_v(bs, "bbv check times");
    }

    progressive_frame                   = u_v(bs, 1, "progressive_frame");

    if (!progressive_frame) {
        h->i_pic_coding_type            = (int8_t)u_v(bs, 1, "picture_structure");
    } else {
        h->i_pic_coding_type            = FRAME;
    }

    h->b_top_field_first                = u_flag(bs, "top_field_first");
    h->b_repeat_first_field             = u_flag(bs, "repeat_first_field");

    if (h->seq_info.b_field_coding) {
        h->b_top_field                  = u_flag(bs, "is_top_field");
        /* reserved                  = */ u_v(bs, 1, "reserved bit for interlace coding");
    }

    h->b_fixed_picture_qp               = u_flag(bs, "fixed_picture_qp");
    h->i_picture_qp                     = u_v(bs, 7, "picture_qp");

    h->b_loop_filter                    = u_v(bs, 1, "loop_filter_disable") ^ 0x01;

    if (h->b_loop_filter) {
        int loop_filter_parameter_flag  = u_v(bs, 1, "loop_filter_parameter_flag");

        if (loop_filter_parameter_flag) {
            h->i_alpha_offset           = se_v(bs, "alpha_offset");
            h->i_beta_offset            = se_v(bs, "beta_offset");
        } else {
            h->i_alpha_offset           = 0;
            h->i_beta_offset            = 0;
        }

        deblock_init_frame_parames(h);
    }

    h->enable_chroma_quant_param        = !u_flag(bs, "chroma_quant_param_disable");
    if (h->enable_chroma_quant_param) {
        h->chroma_quant_param_delta_u = se_v(bs, "chroma_quant_param_delta_cb");
        h->chroma_quant_param_delta_v = se_v(bs, "chroma_quant_param_delta_cr");
    } else {
        h->chroma_quant_param_delta_u = 0;
        h->chroma_quant_param_delta_v = 0;
    }

    // adaptive frequency weighting quantization
    h->seq_info.enable_weighted_quant = 0;

    if (h->seq_info.enable_weighted_quant) {
        int pic_weight_quant_enable     = u_v(bs, 1, "pic_weight_quant_enable");
        if (pic_weight_quant_enable) {
            weighted_quant_t *p = &h->wq;
            p->pic_wq_data_index        = u_v(bs, 2, "pic_wq_data_index");

            if (p->pic_wq_data_index == 1) {
                /* int mb_adapt_wq_disable = */       u_v(bs, 1, "reserved_bits");

                p->wq_param             = u_v(bs, 2, "weighting_quant_param_index");
                p->wq_model             = u_v(bs, 2, "wq_model");

                if (p->wq_param == 1) {
                    for (i = 0; i < 6; i++) {
                        p->quant_param_undetail[i] = (int16_t)se_v(bs, "quant_param_delta_u") + wq_param_default[UNDETAILED][i];
                    }
                }

                if (p->wq_param == 2) {
                    for (i = 0; i < 6; i++) {
                        p->quant_param_detail[i] = (int16_t)se_v(bs, "quant_param_delta_d") + wq_param_default[DETAILED][i];
                    }
                }
            } else if (p->pic_wq_data_index == 2) {
                int x, y, sizeId, uiWqMSize;

                for (sizeId = 0; sizeId < 2; sizeId++) {
                    i = 0;
                    uiWqMSize = DAVS2_MIN(1 << (sizeId + 2), 8);

                    for (y = 0; y < uiWqMSize; y++) {
                        for (x = 0; x < uiWqMSize; x++) {
                            p->pic_user_wq_matrix[sizeId][i++] = (int16_t)ue_v(bs, "weight_quant_coeff");
                        }
                    }
                }
            }

            h->seq_info.enable_weighted_quant = 1;
        }
    }

    alf_read_param(h, bs);

    h->i_qp = h->i_picture_qp;
    if (!is_valid_qp(h, h->i_qp)) {
        davs2_log(h, DAVS2_LOG_ERROR, "Invalid I Picture QP: %d\n", h->i_qp);
    }

    /* align position in bitstream buffer */
    bs_align(bs);

    return 0;
}

/* ---------------------------------------------------------------------------
 * Inter picture header
 */
static int parse_picture_header_inter(davs2_t *h, davs2_bs_t *bs)
{
    int background_pred_flag;
    int progressive_frame;
    int predict;
    int i;

    /* skip start code */
    bs->i_bit_pos += 32;

    u_v(bs, 32, "bbv delay");

    h->i_pic_struct                     = (int8_t)u_v(bs, 2, "picture_coding_type");
    if (h->b_bkgnd_picture && (h->i_pic_struct == 1 || h->i_pic_struct == 3)) {
        if (h->i_pic_struct == 1) {
            background_pred_flag        = u_v(bs, 1, "background_pred_flag");
        } else {
            background_pred_flag        = 0;
        }

        if (background_pred_flag == 0) {
            h->b_bkgnd_reference        = u_flag(bs, "background_reference_enable");
        } else {
            h->b_bkgnd_reference        = 0;
        }
    } else {
        background_pred_flag            = 0;
        h->b_bkgnd_reference            = 0;
    }

    if (h->i_pic_struct == 1 && background_pred_flag) {
        h->i_frame_type = AVS2_S_SLICE;
    } else if (h->i_pic_struct == 1) {
        h->i_frame_type = AVS2_P_SLICE;
    } else if (h->i_pic_struct == 3) {
        h->i_frame_type = AVS2_F_SLICE;
    } else {
        h->i_frame_type = AVS2_B_SLICE;
    }

    h->i_coi                            = u_v(bs, 8, "coding_order");
    if (h->seq_info.b_temporal_id_exist == 1) {
        h->i_cur_layer                  = u_v(bs, TEMPORAL_MAXLEVEL_BIT, "temporal_id");
    }

    if (h->seq_info.head.low_delay == 0) {
        h->i_display_delay              = ue_v(bs, "displaydelay");
        if (h->i_display_delay >= 64) {
            davs2_log(h, DAVS2_LOG_ERROR, "invalid picture output delay inter.");
            return -1;
        }
    }

    /* */
    predict                             = u_v(bs, 1, "use RPS in SPS");
    if (predict) {
        int index                       = u_v(bs, 5, "predict for RPS");
        if (index >= h->seq_info.num_of_rps) {
            davs2_log(h, DAVS2_LOG_ERROR, "invalid rps index.");
            return -1;
        }

        h->rps                          = h->seq_info.seq_rps[index];
    } else {
        // GOP size
        h->rps.refered_by_others        = u_v(bs, 1, "refered by others");
        h->rps.num_of_ref               = u_v(bs, 3, "num of reference picture");

        for (i = 0; i < h->rps.num_of_ref; i++) {
            h->rps.ref_pic[i]           = u_v(bs, 6, "delta COI of ref pic");
        }

        h->rps.num_to_remove            = u_v(bs, 3, "num of removed picture");
        assert(h->rps.num_to_remove <= sizeof(h->rps.remove_pic) / sizeof(h->rps.remove_pic[0]));

        for (i = 0; i < h->rps.num_to_remove; i++) {
            h->rps.remove_pic[i]        = u_v(bs, 6, "delta COI of removed pic");
        }
        u_v(bs, 1, "marker bit");
    }

    if (h->seq_info.head.low_delay) {
        ue_v(bs, "bbv check times");
    }

    progressive_frame                   = u_v(bs, 1, "progressive_frame");

    if (!progressive_frame) {
        h->i_pic_coding_type            = (int8_t)u_v(bs, 1, "picture_structure");
    } else {
        h->i_pic_coding_type            = FRAME;
    }

    h->b_top_field_first                = u_flag(bs, "top_field_first");
    h->b_repeat_first_field             = u_flag(bs, "repeat_first_field");

    if (h->seq_info.b_field_coding) {
        h->b_top_field                  =u_flag(bs, "is_top_field");
        u_v(bs, 1, "reserved bit for interlace coding");
    }

    h->b_fixed_picture_qp               = u_flag(bs, "fixed_picture_qp");
    h->i_picture_qp                     = u_v(bs, 7, "picture_qp");

    if (!(h->i_pic_struct == 2 && h->i_pic_coding_type == FRAME)) {
        u_v(bs, 1, "reserved_bit");
    }

    h->b_ra_decodable                   = u_flag(bs, "random_access_decodable_flag");

    h->b_loop_filter                    = u_v(bs, 1, "loop_filter_disable") ^ 0x01;

    if (h->b_loop_filter) {
        int loop_filter_parameter_flag  = u_v(bs, 1, "loop_filter_parameter_flag");

        if (loop_filter_parameter_flag) {
            h->i_alpha_offset           = se_v(bs, "alpha_offset");
            h->i_beta_offset            = se_v(bs, "beta_offset");
        } else {
            h->i_alpha_offset           = 0;
            h->i_beta_offset            = 0;
        }

        deblock_init_frame_parames(h);
    }

    h->enable_chroma_quant_param = !u_flag(bs, "chroma_quant_param_disable");

    if (h->enable_chroma_quant_param) {
        h->chroma_quant_param_delta_u = se_v(bs, "chroma_quant_param_delta_cb");
        h->chroma_quant_param_delta_v = se_v(bs, "chroma_quant_param_delta_cr");
    } else {
        h->chroma_quant_param_delta_u = 0;
        h->chroma_quant_param_delta_v = 0;
    }

    // adaptive frequency weighting quantization
    h->seq_info.enable_weighted_quant = 0;

    if (h->seq_info.enable_weighted_quant) {
        int pic_weight_quant_enable     = u_v(bs, 1, "pic_weight_quant_enable");

        if (pic_weight_quant_enable) {
            weighted_quant_t *p = &h->wq;
            p->pic_wq_data_index        = u_v(bs, 2, "pic_wq_data_index");

            if (p->pic_wq_data_index == 1) {
                /* int mb_adapt_wq_disable = */     u_v(bs, 1, "reserved_bits");

                p->wq_param             = u_v(bs, 2, "weighting_quant_param_index");
                p->wq_model             = u_v(bs, 2, "wq_model");

                if (p->wq_param == 1) {
                    for (i = 0; i < 6; i++) {
                        p->quant_param_undetail[i] = (int16_t)se_v(bs, "quant_param_delta_u") + wq_param_default[UNDETAILED][i];
                    }
                }

                if (p->wq_param == 2) {
                    for (i = 0; i < 6; i++) {
                        p->quant_param_detail[i] = (int16_t)se_v(bs, "quant_param_delta_d") + wq_param_default[DETAILED][i];
                    }
                }
            } else if (p->pic_wq_data_index == 2) {
                int x, y, sizeId, uiWqMSize;

                for (sizeId = 0; sizeId < 2; sizeId++) {
                    i = 0;
                    uiWqMSize = DAVS2_MIN(1 << (sizeId + 2), 8);

                    for (y = 0; y < uiWqMSize; y++) {
                        for (x = 0; x < uiWqMSize; x++) {
                            p->pic_user_wq_matrix[sizeId][i++] = (int16_t)ue_v(bs, "weight_quant_coeff");
                        }
                    }
                }
            }

            h->seq_info.enable_weighted_quant = 1;
        }
    }

    alf_read_param(h, bs);

    h->i_qp = h->i_picture_qp;
    if (!is_valid_qp(h, h->i_qp)) {
        davs2_log(h, DAVS2_LOG_ERROR, "Invalid PB Picture QP: %d\n", h->i_qp);
    }

    /* align position in bitstream buffer */
    bs_align(bs);

    return 0;
}

/* ---------------------------------------------------------------------------
 */
static
int parse_picture_header(davs2_t *h, davs2_bs_t *bs, uint32_t start_code)
{
    davs2_mgr_t *mgr = h->task_info.taskmgr;

    assert(start_code == SC_INTRA_PICTURE || start_code == SC_INTER_PICTURE);

    if (start_code == SC_INTRA_PICTURE) {
        if (parse_picture_header_intra(h, bs) < 0) {
            return -1;
        }
    } else {
        if (mgr->outpics.output == -1) {
            /* An I frame is expected for the first frame or after the decoder is flushed. */
            davs2_log(h, DAVS2_LOG_ERROR, "sequence should start with an I frame.");
            return -1;
        }

        if (parse_picture_header_inter(h, bs) < 0) {
            return -1;
        }
    }

    /* field picture ? */
    if (h->i_pic_coding_type != FRAME) {
        davs2_log(h, DAVS2_LOG_ERROR, "field is not supported.");
        return -1;
    }

    /* COI should be a periodically-repeated value from 0 to 255 */
    if (mgr->outpics.output != -1 &&
        h->i_coi != (mgr->i_prev_coi + 1) % AVS2_COI_CYCLE) {
        davs2_log(h, DAVS2_LOG_DEBUG, "discontinuous COI (prev: %d --> curr: %d).", mgr->i_prev_coi, h->i_coi);
    }

    /* update COI */
    if (h->i_coi < mgr->i_prev_coi) { /// !!! '='
        mgr->i_tr_wrap_cnt++;
    }

    mgr->i_prev_coi = h->i_coi;

    h->i_coi += mgr->i_tr_wrap_cnt * AVS2_COI_CYCLE;

    if (h->seq_info.head.low_delay == 0) {
        h->i_poc = h->i_coi + h->i_display_delay - h->seq_info.picture_reorder_delay;
    } else {
        h->i_poc = h->i_coi;
    }

    assert(h->i_coi >= 0 && h->i_poc >= 0); /// 'int' (2147483647) should be large enough for 'i_coi' & 'i_poc'.

    if (mgr->outpics.output == -1 && start_code == SC_INTRA_PICTURE) {
        if (h->i_coi != 0) {
            davs2_log(h, DAVS2_LOG_INFO, "COI of the first frame is %d.", h->i_coi);
        }

        mgr->outpics.output = h->i_poc;
    }

    return 0;
}


/**
 * ===========================================================================
 * interface function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
void parse_slice_header(davs2_t *h, davs2_bs_t *bs)
{
    int slice_vertical_position;
    int slice_vertical_position_extension = 0;
    int slice_horizontal_positon;
    int slice_horizontal_positon_extension;
    int mb_row;

    /* skip start code: 00 00 01 */
    bs->i_bit_pos += 24;

    slice_vertical_position = u_v(bs, 8, "slice vertical position");

    if (h->i_image_height > (144 * h->i_lcu_size)) {
        slice_vertical_position_extension = u_v(bs, 3, "slice vertical position extension");
    }

    if (h->i_image_height > (144 * h->i_lcu_size)) {
        mb_row = (slice_vertical_position_extension << 7) + slice_vertical_position;
    } else {
        mb_row = slice_vertical_position;
    }

    slice_horizontal_positon = u_v(bs, 8, "slice horizontal position");
    if (h->i_width > (255 * h->i_lcu_size)) {
        slice_horizontal_positon_extension = u_v(bs, 2, "slice horizontal position extension");
    }

    if (!h->b_fixed_picture_qp) {
        h->b_fixed_slice_qp = u_flag(bs, "fixed_slice_qp");
        h->i_slice_qp       = u_v(bs, 7, "slice_qp");

        h->b_DQP            = !h->b_fixed_slice_qp;
    } else {
        h->i_slice_qp       = h->i_picture_qp;
        h->b_DQP            = 0;
    }
    h->i_qp = h->i_slice_qp;

    if (!is_valid_qp(h, h->i_qp)) {
        davs2_log(h, DAVS2_LOG_ERROR, "Invalid Slice QP: %d\n", h->i_qp);
    }

    if (h->b_sao) {
        h->slice_sao_on[0] = u_flag(bs, "sao_slice_flag_Y");
        h->slice_sao_on[1] = u_flag(bs, "sao_slice_flag_Cb");
        h->slice_sao_on[2] = u_flag(bs, "sao_slice_flag_Cr");
    }
}

/* ---------------------------------------------------------------------------
 */
davs2_outpic_t *alloc_picture(int w, int h)
{
    davs2_outpic_t *pic = NULL;
    uint8_t *buf;

    buf = (uint8_t *)davs2_malloc(sizeof(davs2_outpic_t)     +
                                   sizeof(davs2_seq_info_t) +
                                   sizeof(davs2_picture_t)  + sizeof(pel_t) * w * h * 3 / 2);
    if (buf == NULL) {
        return NULL;
    }

    pic = (davs2_outpic_t *)buf;

    buf += sizeof(davs2_outpic_t); /* davs2_outpic_t */

    pic->frame = NULL;
    pic->next  = NULL;

    pic->head = (davs2_seq_info_t *)buf;
    buf      += sizeof(davs2_seq_info_t);

    pic->pic = (davs2_picture_t *)buf;
    buf     += sizeof(davs2_picture_t);

    pic->pic->num_planes = 3;
    pic->pic->planes[0] = buf;
    pic->pic->planes[1] = pic->pic->planes[0] + w * h * sizeof(pel_t);
    pic->pic->planes[2] = pic->pic->planes[1] + w * h / 4 * sizeof(pel_t);
    pic->pic->widths[0] = w;
    pic->pic->widths[1] = w / 2;
    pic->pic->widths[2] = w / 2;
    pic->pic->lines [0] = h;
    pic->pic->lines [1] = h / 2;
    pic->pic->lines [2] = h / 2;
    pic->pic->dec_frame = NULL;

    return pic;
}

/* ---------------------------------------------------------------------------
 */
void free_picture(davs2_outpic_t *pic)
{
    if (pic) {
        davs2_free(pic);
    }
}

/* ---------------------------------------------------------------------------
 * destroy decoding picture buffer(DPB)
 */
void destroy_dpb(davs2_mgr_t *mgr)
{
    davs2_frame_t *frame = NULL;
    int i;

    for (i = 0; i < mgr->dpbsize; i++) {
        frame = mgr->dpb[i];
        assert(frame);

        mgr->dpb[i] = NULL;

        davs2_thread_mutex_lock(&frame->mutex_frm);

        if (frame->i_ref_count == 0) {
            davs2_thread_mutex_unlock(&frame->mutex_frm);
            davs2_frame_destroy(frame);
        } else {
            frame->i_disposable = 2; /* free when not referenced */
            davs2_thread_mutex_unlock(&frame->mutex_frm);
        }
    }

    davs2_free(mgr->dpb);
    mgr->dpb = NULL;
}

/* ---------------------------------------------------------------------------
 * create decoding picture buffer(DPB)
 */
static INLINE
int create_dpb(davs2_mgr_t *mgr)
{
    davs2_seq_t *seq = &mgr->seq_info;
    uint8_t      *mem_ptr = NULL;
    size_t        mem_size = 0;
    int i;

    mgr->dpbsize = mgr->num_decoders + seq->picture_reorder_delay + 16;  /// !!! FIXME: decide dpb buffer size ?
    mgr->dpbsize += 8;  // FIXME: ÐèÒª¼õÉÙ

    mem_size = mgr->dpbsize * sizeof(davs2_frame_t *)
        + davs2_frame_get_size(seq->i_enc_width, seq->i_enc_height, seq->head.chroma_format, 1) * mgr->dpbsize
        + davs2_frame_get_size(seq->i_enc_width, seq->i_enc_height, seq->head.chroma_format, 0)
        + CACHE_LINE_SIZE * (mgr->dpbsize + 2);

    mem_ptr = (uint8_t *)davs2_malloc(mem_size);
    if (mem_ptr == NULL) {
        return -1;
    }

    mgr->dpb = (davs2_frame_t **)mem_ptr;
    mem_ptr += mgr->dpbsize * sizeof(davs2_frame_t *);
    ALIGN_POINTER(mem_ptr);

    for (i = 0; i < mgr->dpbsize; i++) {
        mgr->dpb[i] = davs2_frame_new(seq->i_enc_width, seq->i_enc_height, seq->head.chroma_format, &mem_ptr, 1);
        ALIGN_POINTER(mem_ptr);

        if (mgr->dpb[i] == NULL) {
            return -1;
        }
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
static void init_fdec(davs2_t *h, int64_t pts, int64_t dts)
{
    int num_in_spu = h->i_width_in_spu * h->i_height_in_spu;
    int i;

    h->fdec->i_type              = h->i_frame_type;
    h->fdec->i_qp                = h->i_qp;
    h->fdec->i_poc               = h->i_poc;
    h->fdec->i_coi               = h->i_coi;
    h->fdec->b_refered_by_others = h->rps.refered_by_others;
    h->fdec->i_decoded_line      = -1;
    h->fdec->i_pts               = pts;
    h->fdec->i_dts               = dts;

    for (i = 0; i < AVS2_MAX_REFS; i++) {
        h->fdec->dist_refs[i] = -1;
        h->fdec->dist_scale_refs[i] = -1;
    }

    if (h->i_frame_type != AVS2_B_SLICE) {
        for (i = 0; i < h->num_of_references; i++) {
            h->fdec->dist_refs[i] = AVS2_DISTANCE_INDEX(2 * (h->fdec->i_poc - h->fref[i]->i_poc));
            if (h->fdec->dist_refs[i] <= 0) {
                davs2_log(h, DAVS2_LOG_ERROR, "invalid reference frame distance.");
                h->fdec->dist_refs[i] = 1;
            }
            h->fdec->dist_scale_refs[i] = (MULTI / h->fdec->dist_refs[i]);
        }
    } else {
        h->fdec->dist_refs[B_FWD] = AVS2_DISTANCE_INDEX(2 * (h->fdec->i_poc - h->fref[B_FWD]->i_poc));
        h->fdec->dist_refs[B_BWD] = AVS2_DISTANCE_INDEX(2 * (h->fref[B_BWD]->i_poc - h->fdec->i_poc));
        if (h->fdec->dist_refs[B_FWD] <= 0) {
            davs2_log(h, DAVS2_LOG_ERROR, "invalid reference frame distance. B_FWD");
            h->fdec->dist_refs[B_FWD] = 1;
        }
        if (h->fdec->dist_refs[B_BWD] <= 0) {
            davs2_log(h, DAVS2_LOG_ERROR, "invalid reference frame distance. B_BWD");
            h->fdec->dist_refs[B_BWD] = 1;
        }
        h->fdec->dist_scale_refs[B_FWD] = (MULTI / h->fdec->dist_refs[B_FWD]);
        h->fdec->dist_scale_refs[B_BWD] = (MULTI / h->fdec->dist_refs[B_BWD]);
    }

    /* clear mvbuf and refbuf */
    memset(h->fdec->mvbuf, 0, num_in_spu * sizeof(mv_t));
    memset(h->fdec->refbuf, INVALID_REF, num_in_spu * sizeof(int8_t));
}

/* ---------------------------------------------------------------------------
 */
int task_decoder_update(davs2_t *h)
{
    davs2_mgr_t *mgr  = h->task_info.taskmgr;
    davs2_seq_t *seq  = &mgr->seq_info;

    if (seq->valid_flag == 0) {
        davs2_log(h, DAVS2_LOG_ERROR, "failed to update decoder (invalid sequence header).");
        return -1;
    }

    if (h->b_sao != seq->enable_sao || h->b_alf != seq->enable_alf ||
        h->i_chroma_format != (int)seq->head.chroma_format || h->i_lcu_level != seq->log2_lcu_size ||
        h->i_image_width != (int)seq->head.width || h->i_image_height != (int)seq->head.height ||
        h->p_integral == NULL) {
        /* resolution changed */
        decoder_free_extra_buffer(h);

        /* key properties of the video sequence: size and color format */
        h->i_lcu_level      = seq->log2_lcu_size;
        h->i_lcu_size       = 1 << h->i_lcu_level;
        h->i_lcu_size_sub1  = (1 << h->i_lcu_level) - 1;
        h->i_chroma_format  = seq->head.chroma_format;
        h->i_image_width    = seq->head.width;
        h->i_image_height   = seq->head.height;
        h->i_width          = seq->i_enc_width;
        h->i_height         = seq->i_enc_height;

        h->i_width_in_scu   = h->i_width  >> MIN_CU_SIZE_IN_BIT;
        h->i_height_in_scu  = h->i_height >> MIN_CU_SIZE_IN_BIT;
        h->i_size_in_scu    = h->i_width_in_scu * h->i_height_in_scu;
        h->i_width_in_spu   = h->i_width  >> MIN_PU_SIZE_IN_BIT;
        h->i_height_in_spu  = h->i_height >> MIN_PU_SIZE_IN_BIT;
        h->i_width_in_lcu   = (h->i_width + h->i_lcu_size_sub1) >> h->i_lcu_level;
        h->i_height_in_lcu  = (h->i_height + h->i_lcu_size_sub1) >> h->i_lcu_level;

        /* encoding tools configuration */
        h->b_sao            = seq->enable_sao;
        h->b_alf            = seq->enable_alf;

        if (decoder_alloc_extra_buffer(h) < 0) {
            h->i_lcu_level     = 0;
            h->i_chroma_format = 0;
            h->i_image_width   = 0;
            h->i_image_height  = 0;

            davs2_log(h, DAVS2_LOG_ERROR, "failed to update the decoder(failed to alloc space).");

            return -1;
        }
    }

    /* update sequence header */
    h->i_chroma_format  = seq->head.chroma_format;
    h->i_lcu_level      = seq->log2_lcu_size;
    h->b_bkgnd_picture  = seq->enable_background_picture;

    // h->b_dmh            = 1;
    h->output_bit_depth = 8;
    h->sample_bit_depth = 8;

    h->p_tab_DL_avail   = tab_DL_Avails[h->i_lcu_level];
    h->p_tab_TR_avail   = tab_TR_Avails[h->i_lcu_level];

    if (seq->head.profile_id == MAIN10_PROFILE) {
        h->output_bit_depth = 6 + (seq->sample_precision << 1);
        h->sample_bit_depth = 6 + (seq->encoding_precision << 1);
    }

#if HIGH_BIT_DEPTH
    g_bit_depth   = h->sample_bit_depth;
    max_pel_value = (1 << g_bit_depth) - 1;
    g_dc_value    = 1 << (g_bit_depth - 1);
#else
    if (g_bit_depth != h->sample_bit_depth) {
        davs2_log(h, DAVS2_LOG_ERROR, "Un-supported bit-depth %d in this version.\n", h->sample_bit_depth);
        return -1;
    }
#endif

    memcpy(h->wq.seq_wq_matrix, seq->seq_wq_matrix, 2 * 64 * sizeof(int16_t)); /* weighting quantization matrix */
    memcpy(&h->seq_info, seq, sizeof(davs2_seq_t));

    return 0;
}

/* ---------------------------------------------------------------------------
 */
static
int task_set_sequence_head(davs2_mgr_t *mgr, davs2_seq_t *seq)
{
    int ret = 0;

    davs2_thread_mutex_lock(&mgr->mutex_mgr);

    davs2_reconfigure_decoder(mgr);

    if (seq->valid_flag) {
        int newres = (mgr->seq_info.head.height != seq->head.height || mgr->seq_info.head.width != seq->head.width);

        memcpy(&mgr->seq_info, seq, sizeof(davs2_seq_t));

        if (newres) {
            /* resolution changed : new sequence */
            davs2_log(mgr, DAVS2_LOG_INFO, "Sequence Resolution: %dx%d.", seq->head.width, seq->head.height);
            if ((seq->head.width & 0) != 0 || (seq->head.height & 1) != 0) {
                davs2_log(mgr, DAVS2_LOG_ERROR, "Sequence Resolution %dx%d is not even\n",
                    seq->head.width, seq->head.height);
            }

            /* COI for the new sequence should be reset */
            mgr->i_tr_wrap_cnt = 0;
            mgr->i_prev_coi    = -1;

            destroy_dpb(mgr);

            if (create_dpb(mgr) < 0) {
                /* error */
                ret = -1;
                memset(&mgr->seq_info, 0, sizeof(davs2_seq_t));
                davs2_log(mgr, DAVS2_LOG_ERROR, "failed to create dpb buffers. %dx%d.", seq->head.width, seq->head.height);
            }
            mgr->new_sps = TRUE;
        }
    } else {
        /* invalid header */
        memset(&mgr->seq_info, 0, sizeof(davs2_seq_t));
        davs2_log(mgr, DAVS2_LOG_ERROR, "decoded an invalid sequence header: %dx%d.", seq->head.width, seq->head.height);
    }

    davs2_thread_mutex_unlock(&mgr->mutex_mgr);
    return ret;
}

/* ---------------------------------------------------------------------------
 */
void clean_one_frame(davs2_frame_t *frame)
{
    frame->i_poc                = INVALID_FRAME;
    frame->i_coi                = INVALID_FRAME;
    frame->i_disposable         = 0;
    frame->b_refered_by_others  = 0;
}

/* ---------------------------------------------------------------------------
 */
void release_one_frame(davs2_frame_t *frame)
{
    int obsolete = 0;

    if (frame == NULL) {
        return;
    }

    davs2_thread_mutex_lock(&frame->mutex_frm);

    assert(frame->i_ref_count > 0);

    frame->i_ref_count--;

    if (frame->i_ref_count == 0) {
        if (frame->i_disposable == 1) {
            clean_one_frame(frame);
        }

        obsolete = frame->i_disposable == 2;
    }

    davs2_thread_mutex_unlock(&frame->mutex_frm);

    if (obsolete != 0) {
        davs2_frame_destroy(frame);
    }
}

/* ---------------------------------------------------------------------------
 */
void task_release_frames(davs2_t *h)
{
    int i;

    /* release reference to all reference frames */
    for (i = 0; i < h->num_of_references; i++) {
        release_one_frame(h->fref[i]);
        h->fref[i] = NULL;
    }

    h->num_of_references = 0;

    /* release reference to the reconstructed frame */
    release_one_frame(h->fdec);
    h->fdec = NULL;
}

/* ---------------------------------------------------------------------------
 */
int has_blocking(davs2_mgr_t *mgr)
{
    davs2_output_t *pics  = &mgr->outpics;
    davs2_outpic_t *pic   = NULL;
    davs2_frame_t  *frame = NULL;

    int decodingframes = 0, outputframes = 0;
    int i;

    /* is the expected frame already in the output list ? */
    for (pic = pics->pics; pic; pic = pic->next) {
        frame = pic->frame;

        if (frame->i_poc == pics->output) {
            /* the expected frame */
            return 0;
        } else if (frame->i_poc < pics->output) {
            /* a late frame: the output thread will dump it.*/
            return 0;
        }

        outputframes++;
    }

    /* is the expected frame still under decoding ? */
    for (i = 0; i < mgr->num_decoders; i++) {
        davs2_t *h = &mgr->decoders[i];

        if (h->task_info.task_status != TASK_FREE) {
            frame = h->fdec;

            if (frame != NULL) {
                if (frame->i_poc == pics->output) {
                    /* the expected frame will be put into the output list soon */
                    return 0;
                }

                if (frame->i_poc >= 0) {
                    decodingframes++;
                }
            }
        }
    }

    assert(outputframes + decodingframes <= mgr->dpbsize);

    /* the expected frame is neither in the output list nor under decoding */
    if (mgr->outpics.busy != 0) {
        /* one frame being delivered, soon it maybe free ? */
        return 0;
    }

    return 1;
}

/* ---------------------------------------------------------------------------
 */
int task_get_references(davs2_t *h, int64_t pts, int64_t dts)
{
    davs2_mgr_t    *mgr   = h->task_info.taskmgr;
    davs2_frame_t **dpb   = mgr->dpb;
    davs2_frame_t  *frame = NULL;
    int i, j;

#define IS_VALID_FRAME(frame) ((frame)->i_coi != INVALID_FRAME && (frame)->i_poc != INVALID_FRAME)

    davs2_thread_mutex_lock(&mgr->mutex_mgr);

    h->fdec = NULL;
    h->num_of_references = 0;
    for (i = 0; i < AVS2_MAX_REFS; i++) {
        h->fref[i] = NULL;
    }

    for (i = 0; i < mgr->num_frames_to_remove; i++) {
        int coi_frame_to_remove = mgr->coi_remove_frame[i];

        for (j = 0; j < mgr->dpbsize; j++) {
            frame = dpb[j];

            if (!IS_VALID_FRAME(frame)) {
                continue;
            }

            if (frame->i_coi == coi_frame_to_remove) {
                break;
            }
        }

        if (j < mgr->dpbsize) {
            davs2_thread_mutex_lock(&frame->mutex_frm);
            // assert(frame->i_disposable == 0);

            if (frame->i_ref_count == 0) {
                clean_one_frame(frame);
            } else {
                frame->i_disposable = 1;
            }

            davs2_thread_mutex_unlock(&frame->mutex_frm);
        }
    }


    if (h->i_frame_type == AVS2_GB_SLICE) {
        h->fdec = h->f_background_cur;
    } else {
        for (i = 0; i < h->rps.num_of_ref; i++) {
            int ref_frame_coi = h->i_coi - h->rps.ref_pic[i];
            for (j = 0; j < mgr->dpbsize; j++) {
                frame = dpb[j];

                if (!IS_VALID_FRAME(frame)) {
                    continue;
                }

                davs2_thread_mutex_lock(&frame->mutex_frm);

                if (frame->i_coi >= 0 && ref_frame_coi == frame->i_coi) {
                    assert(frame->i_disposable == 0);
                    assert(frame->b_refered_by_others != 0);

                    if (frame->i_disposable == 0 && frame->b_refered_by_others != 0) {
                        frame->i_ref_count++;
                        davs2_thread_mutex_unlock(&frame->mutex_frm);

                        h->fref[i] = frame;
                        h->num_of_references++;

                        break;
                    }
                }

                davs2_thread_mutex_unlock(&frame->mutex_frm);
            }

            if (j == mgr->dpbsize) {
                davs2_log(h, DAVS2_LOG_ERROR, "reference frame of [coi: %d, poc: %d]: <COI: %d> not found.",
                    h->i_coi, h->i_poc, ref_frame_coi);
                goto fail;
            }
        }

        if (h->i_frame_type == AVS2_B_SLICE &&
            (h->num_of_references != 2 || h->fref[0]->i_poc <= h->i_poc || h->fref[1]->i_poc >= h->i_poc)) {
            davs2_log(h, DAVS2_LOG_ERROR, "reference frames for B frame [coi: %d, poc: %d] are wrong: %d frames found",
                h->i_coi, h->i_poc, h->num_of_references);
            goto fail;
        }

        /* delete the frame that will never be used */
        mgr->num_frames_to_remove = h->rps.num_to_remove;

        for (i = 0; i < h->rps.num_to_remove; i++) {
            mgr->coi_remove_frame[i] = h->i_coi - h->rps.remove_pic[i];
        }

        /* clean old frames */
        for (i = 0; i < mgr->dpbsize; i++) {
            frame = dpb[i];

            if (!IS_VALID_FRAME(frame)) {
                continue;
            }

            davs2_thread_mutex_lock(&frame->mutex_frm);

            if (DAVS2_ABS(frame->i_poc - h->i_poc) >= MAX_POC_DISTANCE) {
                if (frame->i_ref_count == 0) {
                    davs2_log(h, DAVS2_LOG_WARNING, "force to remove obsolete frame <poc: %d>.", frame->i_poc);
                    /* no one is holding reference to this frame: clean it ! */
                    clean_one_frame(frame);
                } else {
                    /* weird ? */
                    /* some task has forgot to release it ? */
                    if (frame->i_disposable == 0) {
                        frame->i_disposable = 1;
                        davs2_log(h, DAVS2_LOG_WARNING, "force to mark obsolete frame <poc: %d> as to be removed.", frame->i_poc);
                    }
                }
            }

            davs2_thread_mutex_unlock(&frame->mutex_frm);
        }

        /* find fdec */
        for (;;) {
            for (i = 0; i < mgr->dpbsize; i++) {
                frame = dpb[i];

                davs2_thread_mutex_lock(&frame->mutex_frm);

                if (frame->i_ref_count == 0 && frame->b_refered_by_others == 0) {
                    assert(frame->i_disposable == 0);

                    frame->i_ref_count++;   /* for the decoding thread */
                    frame->i_ref_count++;   /* for the output thread */

                    frame->i_disposable = h->rps.refered_by_others == 0 ? 1 : 0;

                    h->fdec = frame;

                    davs2_thread_mutex_unlock(&frame->mutex_frm);

                    break;
                }

                davs2_thread_mutex_unlock(&frame->mutex_frm);
            }

            if (h->fdec != NULL) {
                /* got it */
                break;
            }

            /* DPB full ? */
            if (open_dbp_buffer_warning) {
                davs2_log(h, DAVS2_LOG_WARNING, "running out of DPB buffers, performance may suffer.");
                open_dbp_buffer_warning = 0;      /* avoid too many warnings */
            }

            /* detect possible blocks */
            if (has_blocking(mgr) != 0) {
                if (mgr->outpics.pics == NULL) {
                    /*!!! try to use an earliest frame ??? */
                    /* find the frame with the least POC value */
                    for (i = 0; i < mgr->dpbsize; i++) {
                        frame = dpb[i];
                        davs2_thread_mutex_lock(&frame->mutex_frm);

                        if (frame->i_ref_count == 0 && (h->fdec == NULL || h->fdec->i_poc > frame->i_poc)) {
                            if (h->fdec) {
                                davs2_thread_mutex_lock(&h->fdec->mutex_frm);
                                h->fdec->i_ref_count--;
                                h->fdec->i_ref_count--;
                                davs2_thread_mutex_unlock(&h->fdec->mutex_frm);
                            }

                            frame->i_ref_count++;   /* for the decoding thread */
                            frame->i_ref_count++;   /* for the output thread */

                            h->fdec = frame;
                        }

                        davs2_thread_mutex_unlock(&frame->mutex_frm);
                    }

                    if (NULL == h->fdec) {
                        davs2_log(h, DAVS2_LOG_ERROR, "no frame for new task, DPB size (%d) too small(reorder delay: %d) ?", mgr->dpbsize, mgr->seq_info.picture_reorder_delay);
                        goto fail;
                    }

                    h->fdec->i_disposable = h->rps.refered_by_others == 0 ? 1 : 0;

                    davs2_log(h, DAVS2_LOG_WARNING, "force one frame as the reconstruction frame.");

                    break;
                } else {
                    /* next frame will not be available, skip it */
                    assert(mgr->outpics.output < mgr->outpics.pics->frame->i_poc);
                    /* emit an error */
                    davs2_log(h, DAVS2_LOG_ERROR, "the expected frame %d unavailable, proceed to frame %d.", mgr->outpics.output, mgr->outpics.pics->frame->i_poc);
                    /* output the next available frame */
                    mgr->outpics.output = mgr->outpics.pics->frame->i_poc;
                }
            }

            davs2_thread_mutex_unlock(&mgr->mutex_mgr);

            /* wait for the output thread to release some frames */
            davs2_sleep_ms(1);

            /* check it again */
            davs2_thread_mutex_lock(&mgr->mutex_mgr);
        }

        init_fdec(h, pts, dts);

        if (h->i_frame_type == AVS2_S_SLICE) {
            int num_in_spu = h->i_width_in_spu * h->i_height_in_spu;

            for (i = 0; i < mgr->dpbsize; i++) {
                memset(dpb[i]->mvbuf, 0, num_in_spu * sizeof(mv_t));
                memset(dpb[i]->refbuf, 0, num_in_spu * sizeof(int8_t));
            }
        }
    }

    davs2_thread_mutex_unlock(&mgr->mutex_mgr);

    return 0;

fail:

    davs2_log(NULL, DAVS2_LOG_ERROR, "Failed to decode frame <COI: %d, POC: %d>\n", h->i_coi, h->i_poc);
    davs2_thread_mutex_unlock(&mgr->mutex_mgr);

    task_release_frames(h);

    return -1;
}

/* ---------------------------------------------------------------------------
 */
int parse_header(davs2_t *h, davs2_bs_t *p_bs)
{
    const uint8_t *data   = p_bs->p_stream;
    int           *bitpos = &p_bs->i_bit_pos;
    int            len    = p_bs->i_stream;
    const uint8_t *p_start_code = 0;

    if (len <= 4) {
        return -1;  // at least 4 bytes are needed for decoding
    }

    while ((p_start_code = find_start_code(data + (*bitpos >> 3), len - (*bitpos >> 3))) != 0) {
        uint32_t start_code;
        *bitpos = (int)((p_start_code - data) << 3);

        if ((*bitpos >> 3) + 4 > len) {
            break;
        }

        start_code = data[(*bitpos >> 3) + 3];
        switch (start_code) {
        case SC_INTRA_PICTURE:
        case SC_INTER_PICTURE:
            /* update the decoder */
            if (task_decoder_update(h) < 0) {
                return -1;
            }

            /* decode the picture header */
            if (parse_picture_header(h, p_bs, start_code) < 0) {
                return -1;
            }

            return 0; /// !!! we only decode one frame for a single call.

        case SC_SEQUENCE_HEADER:
            davs2_seq_t new_seq;
            /* decode the sequence head */
            if (parse_sequence_header(h->task_info.taskmgr, &new_seq, p_bs) < 0) {
                davs2_log(h, NULL, "Invalid sequence header.");
                return -1;
            }
            /* update the task manager */
            if (task_set_sequence_head(h->task_info.taskmgr, &new_seq) < 0) {
                return -1;
            }

            break;

        case SC_EXTENSION:
        case SC_USER_DATA:
        case SC_SEQUENCE_END:
        case SC_VIDEO_EDIT_CODE:
        default:
            /* skip this unit */

            /* NOTE: if you want to decode these units, you should avoid */
            /* using a davs2_t structure which will not be updated until a picture header is decoded. */

            *bitpos += 32;
            break;
        }
    }

    return 1;
}
