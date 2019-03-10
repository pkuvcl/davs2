/*
 * decoder.cc
 *
 * Description of this file:
 *    Decoder functions definition of the davs2 library
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
#include "decoder.h"
#include "aec.h"
#include "header.h"
#include "bitstream.h"
#include "deblock.h"
#include "cu.h"
#include "sao.h"
#include "alf.h"
#include "quant.h"
#include "frame.h"
#include "intra.h"
#include "mc.h"
#include "transform.h"
#include "cpu.h"
#include "threadpool.h"

#define TRACEFILE "trace_dec_HD.txt"  /* trace file in current directory */

/* disable warning C4127: 条件表达式是常量 */
#pragma warning(disable:4127)


/**
 * ===========================================================================
 * local function defines
 * ===========================================================================
 */


/* ---------------------------------------------------------------------------
 * initializes the parameters for a new frame
 */
static void init_frame(davs2_t *h)
{
    int num_spu = h->i_width_in_spu * h->i_height_in_spu;
    //int i;

    h->lcu.i_scu_xy        = 0;
    h->i_slice_index       = -1;
    h->b_slice_checked     = 0;
    h->fdec->i_parsed_lcu_xy = -1;
    h->decoding_error      = 0;    // 清除解码错误标志

    /* 1, clear intra_mode buffer, set to default value (-1) */
    memset(h->p_ipredmode - h->i_ipredmode - 16, DC_PRED, h->i_ipredmode * (h->i_height_in_spu + 1) * sizeof(int8_t));
    memset(h->p_dirpred, PDIR_INVALID, num_spu * sizeof(int8_t));

    /* 2, clear mv buffer (set all MVs to zero) */
    gf_davs2.fast_memzero(h->p_ref_idx, num_spu * sizeof(ref_idx_t));
    // gf_davs2.fast_memzero(h->p_tmv_1st, num_spu * sizeof(mv_t));
    // gf_davs2.fast_memzero(h->p_tmv_2nd, num_spu * sizeof(mv_t));

    /* 3, clear slice number for all SCU */
    //repeat for init slice for current LCU
    //for (i = 0; i < h->i_size_in_scu; i++) {
    //    h->scu_data[i].i_slice_nr = -1;
    //}

    /* 4, init adaptive frequency weighting quantization */
    if (h->seq_info.enable_weighted_quant) {
        wq_init_frame_quant_param(h);
        wq_update_frame_matrix(h);
    }

    /* 5, copy frame properties for SAO & ALF */
    if (h->b_sao) {
        davs2_frame_copy_properties(h->p_frame_sao, h->fdec);
    }
    if (h->b_alf) {
        int alf_enable = h->pic_alf_on[IMG_Y] != 0 || h->pic_alf_on[IMG_U] != 0 || h->pic_alf_on[IMG_V] != 0;
        if (alf_enable) {
            davs2_frame_copy_properties(h->p_frame_alf, h->fdec);
        }
    }

    /* 6, clear the p_deblock_flag buffer */
    gf_davs2.fast_memzero(h->p_deblock_flag[0], h->i_width_in_scu * h->i_height_in_scu * 2 * sizeof(uint8_t));

    /* 7, clear LCU info buffer */
#if CTRL_AEC_THREAD
    gf_davs2.fast_memzero(h->lcu_infos, sizeof(lcu_info_t) * h->i_width_in_lcu * h->i_height_in_lcu);
#endif
}


/* ---------------------------------------------------------------------------
* cache CTU border
*/
static INLINE
void davs2_cache_lcu_border(pel_t *p_dst, const pel_t *p_top,
const pel_t *p_left, int i_left,
int lcu_width, int lcu_height)
{
    int i;
    /* top, top-right */
    memcpy(p_dst, p_top, (2 * lcu_width + 1) * sizeof(pel_t));
    /* left */
    for (i = 1; i <= lcu_height; i++) {
        p_dst[-i] = p_left[0];
        p_left += i_left;
    }
}

/* ---------------------------------------------------------------------------
* cache CTU border (UV components together)
*/
static INLINE
void davs2_cache_lcu_border_uv(pel_t *p_dst_u, const pel_t *p_top_u, const pel_t *p_left_u,
pel_t *p_dst_v, const pel_t *p_top_v, const pel_t *p_left_v,
int i_left, int lcu_width, int lcu_height)
{
    int i;
    /* top, top-right */
    memcpy(p_dst_u, p_top_u, (2 * lcu_width + 1) * sizeof(pel_t));
    memcpy(p_dst_v, p_top_v, (2 * lcu_width + 1) * sizeof(pel_t));
    /* left */
    for (i = 1; i <= lcu_height; i++) {
        p_dst_u[-i] = p_left_u[0];
        p_dst_v[-i] = p_left_v[0];
        p_left_u += i_left;
        p_left_v += i_left;
    }
}

/* ---------------------------------------------------------------------------
 */
static void save_mv_ref_info(davs2_t *h, int row)
{
    const int w_in_spu     = h->i_width_in_spu;
    const int h_in_spu     = h->i_height_in_spu;
    const int spu_y        = row << (h->i_lcu_level - MIN_PU_SIZE_IN_BIT);
    const int lcu_h_in_spu = 1 << (h->i_lcu_level - MIN_PU_SIZE_IN_BIT);
    mv_t   *p_dst_mv       = &h->fdec->mvbuf[spu_y * w_in_spu];
    int8_t *p_dst_ref      = &h->fdec->refbuf[spu_y * w_in_spu];
    mv_t   *p_src_mv;
    ref_idx_t *p_src_ref;
    int i, j, x, y;

    for (j = spu_y; j < DAVS2_MIN(spu_y + lcu_h_in_spu, h_in_spu); j++) {
        y = ((j >> MV_FACTOR_IN_BIT) << MV_FACTOR_IN_BIT) + 2;
        if (y >= h_in_spu) {
            y = (((j >> MV_FACTOR_IN_BIT) << MV_FACTOR_IN_BIT) + h_in_spu) >> 1;
        }

        p_src_mv  = h->p_tmv_1st + y * w_in_spu;
        p_src_ref = h->p_ref_idx + y * w_in_spu;

        for (i = 0; i < w_in_spu; i++) {
            x = ((i >> MV_FACTOR_IN_BIT) << MV_FACTOR_IN_BIT) + 2;
            if (x >= w_in_spu) {
                x = (((i >> MV_FACTOR_IN_BIT) << MV_FACTOR_IN_BIT) + w_in_spu) >> 1;
            }

            p_dst_mv [i] = p_src_mv [x];
            p_dst_ref[i] = p_src_ref[x].r[0];
        }

        p_dst_mv  += w_in_spu;
        p_dst_ref += w_in_spu;
    }
}

/* ---------------------------------------------------------------------------
 */
static davs2_outpic_t *get_one_free_picture(davs2_mgr_t *mgr, int w, int h)
{
    davs2_outpic_t *pic = NULL;

    for (;;) {
        /* get one from recycle bin */
        pic = (davs2_outpic_t *)xl_remove_head(&mgr->pic_recycle, 0);
        if ((pic == NULL) ||
            (pic->pic->widths[0] == w && pic->pic->lines[0] == h)) {
            break;
        }

        /* obsolete picture */
        free_picture(pic);
        pic = NULL;
    }

    if (pic == NULL) {
        /* no free picture. no wait, just new one. */
        pic = alloc_picture(w, h);
    }

    return pic;
}

/* ---------------------------------------------------------------------------
 * 等待一行LCU熵解码完指定数量的LCU
 */
static ALWAYS_INLINE
void wait_lcu_row_parsed(davs2_t *h, davs2_frame_t *frm, int lcu_xy)
{
    UNUSED_PARAMETER(h);

    if (lcu_xy > frm->i_parsed_lcu_xy) {
        davs2_thread_mutex_lock(&frm->mutex_frm);   /* lock */
        while (lcu_xy > frm->i_parsed_lcu_xy) {
            davs2_thread_cond_wait(&frm->cond_aec, &frm->mutex_frm);
        }
        davs2_thread_mutex_unlock(&frm->mutex_frm); /* unlock */
    }
}

/* ---------------------------------------------------------------------------
 * 等待一行LCU解码重构完指定数量的LCU
 */
static ALWAYS_INLINE
void wait_lcu_row_reconed(davs2_t *h, davs2_frame_t *frm, int wait_lcu_y, int wait_lcu_coded)
{
    UNUSED_PARAMETER(h);
    // wait_lcu_coded = DAVS2_MIN(h->i_width_in_lcu, wait_lcu_coded);

    if (frm->num_decoded_lcu_in_row[wait_lcu_y] < wait_lcu_coded) {
        davs2_thread_mutex_lock(&frm->mutex_recon);   /* lock */
        while (frm->num_decoded_lcu_in_row[wait_lcu_y] < wait_lcu_coded) {
            davs2_thread_cond_wait(&frm->conds_lcu_row[wait_lcu_y], &frm->mutex_recon);
        }
        davs2_thread_mutex_unlock(&frm->mutex_recon); /* unlock */
    }
}

/* ---------------------------------------------------------------------------
 */
static void decoder_signal(davs2_t *h, davs2_frame_t *frame, int line)
{
    if (line > 0) {
        wait_lcu_row_reconed(h, frame, line - 1, h->i_width_in_lcu + 1);
    }

    davs2_thread_mutex_lock(&frame->mutex_recon);
    frame->i_decoded_line++;
    frame->num_decoded_lcu_in_row[line] = h->i_width_in_lcu + 3;
    davs2_thread_mutex_unlock(&frame->mutex_recon);

    davs2_thread_cond_broadcast(&frame->conds_lcu_row[line]);
}

/* ---------------------------------------------------------------------------
 */
static
void task_send_picture_to_output_list(davs2_t *h, davs2_outpic_t *pic)
{
    davs2_mgr_t    *mgr  = h->task_info.taskmgr;
    davs2_outpic_t *curr = NULL;
    davs2_outpic_t *prev = NULL;

    davs2_thread_mutex_lock(&mgr->mutex_mgr);

    curr = mgr->outpics.pics;

    while (curr && curr->frame->i_poc < pic->frame->i_poc) {
        prev = curr;
        curr = curr->next;
    }

    /* duplicate frame? */
    if (curr != NULL && curr->frame->i_poc == pic->frame->i_poc) {
        davs2_log(h, DAVS2_LOG_WARNING, "detected duplicate POC %d", curr->frame->i_poc);
    }

    /* insert this frame before 'curr' */
    pic->next = curr;

    if (prev) {
        prev->next = pic;
    } else {
        mgr->outpics.pics = pic;
    }
    mgr->outpics.num_output_pic++;

    DAVS2_ASSERT(h->task_info.task_status == TASK_BUSY,
        "Invalid task status %d",
        h->task_info.task_status);
    davs2_thread_mutex_unlock(&mgr->mutex_mgr);
}

/* ---------------------------------------------------------------------------
 */
static
void task_output_decoding_frame(davs2_t *h)
{
    davs2_mgr_t       *mgr     = h->task_info.taskmgr;
    davs2_frame_t     *frame   = h->fdec;
    davs2_seq_t       *seqhead = &h->seq_info;
    davs2_outpic_t    *pic     = NULL;

    assert(frame);

    pic = get_one_free_picture(mgr, h->i_image_width, h->i_image_height);
    assert(pic);

    memcpy(pic->head, &seqhead->head, sizeof(davs2_seq_info_t));

    if (frame->i_type == AVS2_GB_SLICE) {
        pic->frame = h->f_background_ref; ///!!! FIXME: actually NOT working (we do not support S frames now).
    } else {
        pic->frame = frame;
    }

    frame->i_chroma_format    = h->i_chroma_format;
    frame->i_output_bit_depth = h->output_bit_depth;
    frame->i_sample_bit_depth = h->sample_bit_depth;
    frame->frm_decode_error   = h->decoding_error;
    h->decoding_error         = 0;  // clear decoding error status

    pic->frame = frame;

    task_send_picture_to_output_list(h, pic);
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
int check_slice_header(davs2_t *h, davs2_bs_t *bs, int lcu_y)
{
    aec_t *p_aec = &h->aec;

    if (h->b_slice_checked && found_slice_header(bs)) {
        /* slice starts at next byte */
        bs->i_bit_pos = (((bs->i_bit_pos + 7) >> 3) << 3);
        h->i_slice_index++;

        parse_slice_header(h, bs);
        aec_init_contexts(p_aec);
        aec_new_slice(h);
        aec_start_decoding(p_aec, bs->p_stream, ((bs->i_bit_pos + 7) / 8), bs->i_stream);
        AEC_RETURN_ON_ERROR(-1);

        /* 当前Slice的上一行的预测模式清空 */
        lcu_y <<= (h->i_lcu_level - MIN_PU_SIZE_IN_BIT);
        memset(h->p_ipredmode + (lcu_y - 1) * h->i_ipredmode - 16, DC_PRED, h->i_ipredmode * sizeof(int8_t));
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void rowrec_store_lcu_recon_samples(davs2_row_rec_t *row_rec)
{
#if 1
    UNUSED_PARAMETER(row_rec);
#else
    gf_davs2.plane_copy(row_rec->ctu.p_frec[0], row_rec->ctu.i_frec[0], 
                         row_rec->ctu.p_fdec[0], row_rec->ctu.i_fdec[0], 
                         row_rec->ctu.i_ctu_w, row_rec->ctu.i_ctu_h);
    gf_davs2.plane_copy(row_rec->ctu.p_frec[1], row_rec->ctu.i_frec[1],
                         row_rec->ctu.p_fdec[1], row_rec->ctu.i_fdec[1],
                         row_rec->ctu.i_ctu_w_c, row_rec->ctu.i_ctu_h_c);
    gf_davs2.plane_copy(row_rec->ctu.p_frec[2], row_rec->ctu.i_frec[2],
                         row_rec->ctu.p_fdec[2], row_rec->ctu.i_fdec[2],
                         row_rec->ctu.i_ctu_w_c, row_rec->ctu.i_ctu_h_c);
#endif
}

/* ---------------------------------------------------------------------------
 * decodes one LCU row
 */
static int decode_one_lcu_row(davs2_t *h, davs2_bs_t *bs, int i_lcu_y)
{
    const int height_in_lcu = h->i_height_in_lcu;
    const int width_in_lcu  = h->i_width_in_lcu;
    int alf_enable          = h->pic_alf_on[0] | h->pic_alf_on[1] | h->pic_alf_on[2];
    int lcu_xy              = i_lcu_y * width_in_lcu;
    int i_lcu_x;
    int i;
    davs2_row_rec_t row_rec;

    /* loop over all LCUs in current LCU row ------------------------
     */
    for (i_lcu_x = 0; i_lcu_x < width_in_lcu && h->decoding_error == 0; i_lcu_x++, lcu_xy++) {
        if (check_slice_header(h, bs, i_lcu_y) < 0) {
            return -1;
        }

#if AVS2_TRACE
        avs2_trace("\n*********** Pic: %i (I/P) MB: %i Slice: %i Type %d **********\n", h->i_poc, h->lcu.i_scu_xy, h->i_slice_index, h->i_frame_type);
#endif
        h->lcu.lcu_aec = row_rec.lcu_info = &h->lcu_infos[lcu_xy];

        rowrec_lcu_init(h, &row_rec, i_lcu_x, i_lcu_y);
        decode_lcu_init(h, i_lcu_x, i_lcu_y);

        /* decode LCU level data before one LCU */
        if (h->b_sao) {
            sao_read_lcu_param(h, lcu_xy, h->slice_sao_on, &h->lcu.lcu_aec->sao_param);
        }

        if (h->b_alf) {
            for (i = 0; i < IMG_COMPONENTS; i++) {
                if (h->pic_alf_on[i]) {
                    h->lcu.lcu_aec->enable_alf[i] = (uint8_t)aec_read_alf_lcu_ctrl(&h->aec);
                } else {
                    h->lcu.lcu_aec->enable_alf[i] = FALSE;
                }
            }
        }

        /* decode one lcu */
        decode_lcu_parse(h, h->i_lcu_level, h->lcu.i_pix_x, h->lcu.i_pix_y);

        /* cache CTU top border for intra prediction */
        if (i_lcu_x == 0) {
            memcpy(row_rec.ctu_border[0].rec_top + 1, h->intra_border[0], row_rec.ctu.i_ctu_w * 2 * sizeof(pel_t));
            memcpy(row_rec.ctu_border[1].rec_top + 1, h->intra_border[1], row_rec.ctu.i_ctu_w * sizeof(pel_t));
            memcpy(row_rec.ctu_border[2].rec_top + 1, h->intra_border[2], row_rec.ctu.i_ctu_w * sizeof(pel_t));
        }

        decode_lcu_recon(h, &row_rec, h->i_lcu_level, h->lcu.i_pix_x, h->lcu.i_pix_y);
        
        rowrec_store_lcu_recon_samples(&row_rec);
        /* cache top and left samples for intra prediction of next CTU */
        davs2_cache_lcu_border(row_rec.ctu_border[0].rec_top, h->intra_border[0] + row_rec.ctu.i_pix_x + row_rec.ctu.i_ctu_w - 1,
                               row_rec.ctu.p_frec[0] + row_rec.ctu.i_ctu_w - 1,
                               row_rec.ctu.i_frec[0], row_rec.ctu.i_ctu_w, row_rec.ctu.i_ctu_h);
        davs2_cache_lcu_border_uv(row_rec.ctu_border[1].rec_top, h->intra_border[1] + row_rec.ctu.i_pix_x_c + row_rec.ctu.i_ctu_w_c - 1, row_rec.ctu.p_frec[1] + row_rec.ctu.i_ctu_w_c - 1,
                                  row_rec.ctu_border[2].rec_top, h->intra_border[2] + row_rec.ctu.i_pix_x_c + row_rec.ctu.i_ctu_w_c - 1, row_rec.ctu.p_frec[2] + row_rec.ctu.i_ctu_w_c - 1,
                                  row_rec.ctu.i_frec[1], row_rec.ctu.i_ctu_w_c, row_rec.ctu.i_ctu_h_c);

        /* backup bottom row pixels */
        if (i_lcu_y < h->i_height_in_lcu - 1) {
            memcpy(h->intra_border[0] + row_rec.ctu.i_pix_x  , row_rec.ctu.p_frec[0] + (row_rec.ctu.i_ctu_h   - 1) * h->fdec->i_stride[0], row_rec.ctu.i_ctu_w   * sizeof(pel_t));
            memcpy(h->intra_border[1] + row_rec.ctu.i_pix_x_c, row_rec.ctu.p_frec[1] + (row_rec.ctu.i_ctu_h_c - 1) * h->fdec->i_stride[1], row_rec.ctu.i_ctu_w_c * sizeof(pel_t));
            memcpy(h->intra_border[2] + row_rec.ctu.i_pix_x_c, row_rec.ctu.p_frec[2] + (row_rec.ctu.i_ctu_h_c - 1) * h->fdec->i_stride[1], row_rec.ctu.i_ctu_w_c * sizeof(pel_t));
        }

        /* decode LCU level data after one LCU
         * update the bit position */
        h->b_slice_checked = (bool_t)aec_startcode_follows(&h->aec, 1);
        bs->i_bit_pos      = aec_bits_read(&h->aec);

        /* deblock one lcu */
        if (h->b_loop_filter) {
            davs2_lcu_deblock(h, h->fdec, i_lcu_x, i_lcu_y);
        }
    }

    if (h->decoding_error != 0) {
        
    } else {
        /* SAO current lcu-row */
        if (h->b_sao) {
            sao_lcurow(h, h->p_frame_sao, h->fdec, i_lcu_y);
        }

        /* ALF current lcu-row */
        if (alf_enable) {
            alf_lcurow(h, h->p_alf->img_param, h->p_frame_alf, h->fdec, i_lcu_y);
        }
    }

    /* save motion vectors for reference frame */
    if (h->rps.refered_by_others && h->i_frame_type != AVS2_I_SLICE) {
        save_mv_ref_info(h, i_lcu_y);
    }

    /* frame padding : line by line */
    if (h->rps.refered_by_others) {
        pad_line_lcu(h, i_lcu_y);

        /* wake up all waiting threads */
        decoder_signal(h, h->fdec, i_lcu_y);
    }

    if (i_lcu_y == height_in_lcu - 1) {

        /* init for AVS-S */
        if ((h->i_frame_type == AVS2_P_SLICE || h->i_frame_type == AVS2_F_SLICE) && h->b_bkgnd_picture && h->b_bkgnd_reference) {
            const int w_in_spu = h->i_width_in_spu;
            const int h_in_spu = h->i_height_in_spu;
            int x, y;

            for (y = 0; y < h_in_spu; y++) {
                for (x = 0; x < w_in_spu; x++) {
                    int refframe = h->p_ref_idx[y * w_in_spu + x].r[0];
                    if (refframe == h->num_of_references - 1) {
                        h->p_ref_idx[y * w_in_spu + x].r[0] = INVALID_REF;
                    }
                }
            }
        }

        task_output_decoding_frame(h);
        task_release_frames(h);
        /* task is free */
        task_unload_packet(h, h->task_info.curr_es_unit);

        // davs2_thread_mutex_lock(&h->task_info.taskmgr->mutex_aec);
        // h->task_info.taskmgr->num_active_decoders--;
        // davs2_thread_mutex_unlock(&h->task_info.taskmgr->mutex_aec);
    }

    return 0;
}

// #if CTRL_AEC_THREAD
/* ---------------------------------------------------------------------------
 * decodes one LCU row
 */
static int decode_one_lcu_row_parse(davs2_t *h, davs2_bs_t *bs, int i_lcu_y)
{
    const int width_in_lcu = h->i_width_in_lcu;
    int lcu_xy             = i_lcu_y * width_in_lcu;
    int i_lcu_x;
    int i;

    /* loop over all LCUs in current LCU row ------------------------
     */
    for (i_lcu_x = 0; i_lcu_x < width_in_lcu; i_lcu_x++, lcu_xy++) {
        if (check_slice_header(h, bs, i_lcu_y) < 0) {
            return -1;
        }

#if AVS2_TRACE
        avs2_trace("\n*********** Pic: %i (I/P) MB: %i Slice: %i Type %d **********\n", h->i_poc, h->lcu.i_scu_xy, h->i_slice_index, h->i_frame_type);
#endif
        h->lcu.lcu_aec = &h->lcu_infos[lcu_xy];
        decode_lcu_init(h, i_lcu_x, i_lcu_y);

        /* decode LCU level data before one LCU */
        if (h->b_sao) {
            sao_read_lcu_param(h, lcu_xy, h->slice_sao_on, &h->lcu.lcu_aec->sao_param);
        }

        if (h->b_alf) {
            for (i = 0; i < IMG_COMPONENTS; i++) {
                if (h->pic_alf_on[i]) {
                    h->lcu.lcu_aec->enable_alf[i] = (uint8_t)aec_read_alf_lcu_ctrl(&h->aec);
                } else {
                    h->lcu.lcu_aec->enable_alf[i] = FALSE;
                }
            }
        }

        /* decode one lcu */
        decode_lcu_parse(h, h->i_lcu_level, h->lcu.i_pix_x, h->lcu.i_pix_y);

        /* decode LCU level data after one LCU
         * update the bit position */
        h->b_slice_checked = (bool_t)aec_startcode_follows(&h->aec, 1);
        bs->i_bit_pos      = aec_bits_read(&h->aec);

        h->fdec->i_parsed_lcu_xy = lcu_xy;
        davs2_thread_cond_broadcast(&h->fdec->cond_aec);
    }

    /* save motion vectors for reference frame */
    if (h->rps.refered_by_others && h->i_frame_type != AVS2_I_SLICE) {
        save_mv_ref_info(h, i_lcu_y);
    }

    return 0;
}

/* ---------------------------------------------------------------------------
 * decodes one LCU row
 */
static int decode_lcu_row_recon(davs2_t *h, int i_lcu_y)
{
    const int width_in_lcu  = h->i_width_in_lcu;
    const int height_in_lcu = h->i_height_in_lcu;
    int alf_enable          = h->pic_alf_on[0] | h->pic_alf_on[1] | h->pic_alf_on[2];
    int i_lcu_level         = h->i_lcu_level;
    int lcu_xy              = i_lcu_y * h->i_width_in_lcu;
    int b_recon_finish      = 0;
    int b_next_row_launched = 0;
    davs2_row_rec_t row_rec;

    while (i_lcu_y < height_in_lcu) {
        /* loop over all LCUs in current LCU row ------------------------
         */
        int i_lcu_x;
        for (i_lcu_x = 0; i_lcu_x < width_in_lcu; i_lcu_x++, lcu_xy++) {
            /* wait until the parsing process of current LCU having finished */
            wait_lcu_row_parsed(h, h->fdec, lcu_xy);

            if (i_lcu_y > 0) {
                wait_lcu_row_reconed(h, h->fdec, i_lcu_y - 1, DAVS2_MIN(i_lcu_x + 2, h->i_width_in_lcu));
            }
            row_rec.lcu_info = &h->lcu_infos[lcu_xy];
#if CTRL_AEC_THREAD
            row_rec.p_rec_info = &row_rec.lcu_info->rec_info;
#endif
            rowrec_lcu_init(h, &row_rec, i_lcu_x, i_lcu_y);

            /* cache CTU top border for intra prediction */
            if (i_lcu_x == 0) {
                memcpy(row_rec.ctu_border[0].rec_top + 1, h->intra_border[0], row_rec.ctu.i_ctu_w * 2 * sizeof(pel_t));
                memcpy(row_rec.ctu_border[1].rec_top + 1, h->intra_border[1], row_rec.ctu.i_ctu_w * sizeof(pel_t));
                memcpy(row_rec.ctu_border[2].rec_top + 1, h->intra_border[2], row_rec.ctu.i_ctu_w * sizeof(pel_t));
            }

            decode_lcu_recon(h, &row_rec, i_lcu_level, i_lcu_x << i_lcu_level, i_lcu_y << i_lcu_level);

            rowrec_store_lcu_recon_samples(&row_rec);
            /* cache top and left samples for intra prediction of next CTU */
            davs2_cache_lcu_border(row_rec.ctu_border[0].rec_top, h->intra_border[0] + row_rec.ctu.i_pix_x + row_rec.ctu.i_ctu_w - 1,
                                   row_rec.ctu.p_frec[0] + row_rec.ctu.i_ctu_w - 1,
                                   row_rec.ctu.i_frec[0], row_rec.ctu.i_ctu_w, row_rec.ctu.i_ctu_h);
            davs2_cache_lcu_border_uv(row_rec.ctu_border[1].rec_top, h->intra_border[1] + row_rec.ctu.i_pix_x_c + row_rec.ctu.i_ctu_w_c - 1, row_rec.ctu.p_frec[1] + row_rec.ctu.i_ctu_w_c - 1,
                                      row_rec.ctu_border[2].rec_top, h->intra_border[2] + row_rec.ctu.i_pix_x_c + row_rec.ctu.i_ctu_w_c - 1, row_rec.ctu.p_frec[2] + row_rec.ctu.i_ctu_w_c - 1,
                                      row_rec.ctu.i_frec[1], row_rec.ctu.i_ctu_w_c, row_rec.ctu.i_ctu_h_c);

            /* backup bottom row pixels */
            if (i_lcu_y < h->i_height_in_lcu - 1) {
                memcpy(h->intra_border[0] + row_rec.ctu.i_pix_x, row_rec.ctu.p_frec[0] + (row_rec.ctu.i_ctu_h - 1) * h->fdec->i_stride[0], row_rec.ctu.i_ctu_w   * sizeof(pel_t));
                memcpy(h->intra_border[1] + row_rec.ctu.i_pix_x_c, row_rec.ctu.p_frec[1] + (row_rec.ctu.i_ctu_h_c - 1) * h->fdec->i_stride[1], row_rec.ctu.i_ctu_w_c * sizeof(pel_t));
                memcpy(h->intra_border[2] + row_rec.ctu.i_pix_x_c, row_rec.ctu.p_frec[2] + (row_rec.ctu.i_ctu_h_c - 1) * h->fdec->i_stride[1], row_rec.ctu.i_ctu_w_c * sizeof(pel_t));
            }

            /* deblock one lcu */
            if (h->b_loop_filter) {
                davs2_lcu_deblock(h, h->fdec, i_lcu_x, i_lcu_y);
            }

            h->fdec->num_decoded_lcu_in_row[i_lcu_y]++;
        }


        /* SAO above lcu-row */
        if (h->b_sao && i_lcu_y) {
            sao_lcurow(h, h->p_frame_sao, h->fdec, i_lcu_y - 1);  // above row

            if (i_lcu_y == height_in_lcu - 1) {
                sao_lcurow(h, h->p_frame_sao, h->fdec, i_lcu_y);  // last row
            }
        }

        /* ALF above lcu-row */
        if (alf_enable && i_lcu_y) {
            alf_lcurow(h, h->p_alf->img_param, h->p_frame_alf, h->fdec, i_lcu_y - 1);  // above row
            if (i_lcu_y == height_in_lcu - 1) {
                alf_lcurow(h, h->p_alf->img_param, h->p_frame_alf, h->fdec, i_lcu_y);  // last row
            }
        }

        if (i_lcu_y > 0) {
            /* frame padding : line by line */
            if (h->rps.refered_by_others) {
                pad_line_lcu(h, i_lcu_y - 1);
            }
            /* wake up all waiting threads */
            decoder_signal(h, h->fdec, i_lcu_y - 1);
        }

        /* The last row in one frame */
        if (i_lcu_y == height_in_lcu - 1) {
            b_recon_finish = 1;
        }

        /* TODO: loop to next LCU row */
        if (b_next_row_launched) {
            break;
        }
        i_lcu_y++;
    }

    /* the bottom LCU row in a frame */
    if (b_recon_finish) {
        if (h->rps.refered_by_others) {
            pad_line_lcu(h, h->i_height_in_lcu - 1);
        }

        decoder_signal(h, h->fdec, h->i_height_in_lcu - 1);
        /* init for AVS-S */
        if ((h->i_frame_type == AVS2_P_SLICE || h->i_frame_type == AVS2_F_SLICE) && h->b_bkgnd_picture && h->b_bkgnd_reference) {
            const int w_in_spu = h->i_width_in_spu;
            const int h_in_spu = h->i_height_in_spu;
            int x, y;

            for (y = 0; y < h_in_spu; y++) {
                for (x = 0; x < w_in_spu; x++) {
                    int refframe = h->p_ref_idx[y * w_in_spu + x].r[0];
                    if (refframe == h->num_of_references - 1) {
                        h->p_ref_idx[y * w_in_spu + x].r[0] = INVALID_REF;
                    }
                }
            }
        }

        // davs2_log(h, DAVS2_LOG_INFO, "POC %3d reconstruction finished.", h->i_poc);
        if (h->i_frame_type == AVS2_G_SLICE) {
            davs2_frame_copy_planes(h->f_background_ref, h->fdec);
        }

        task_output_decoding_frame(h);
        task_release_frames(h);
        /* task is free */
        task_unload_packet(h, h->task_info.curr_es_unit);
    }

    return 0;
}
// #endif  // #if CTRL_AEC_THREAD


/* ---------------------------------------------------------------------------
 */
static void decode_user_data(davs2_t *h, davs2_bs_t *bs)
{
    int      bytes = bs->i_bit_pos >> 3;
    int      left  = bs->i_stream - bytes;
    uint8_t *data  = bs->p_stream + bytes;

    while (left >= 4) {
        if (data[0] == 0 && data[1] == 0 && data[2] == 1) {
            if (data[3] == SC_USER_DATA) {
                /* user data */
            } else if (data[3] <= SC_SLICE_CODE_MAX) {
                /* slice */
                h->b_slice_checked = 1;
                break;
            }

            data += 4;
            left -= 4;
        } else {
            ++data;
            --left;
        }
    }

    if (left >= 4) {
        bs->i_bit_pos = (int)((data - bs->p_stream) << 3);
    }
}

/**
 * ===========================================================================
 * interface function defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
void decoder_free_extra_buffer(davs2_t *h)
{
    if (h->f_background_ref) {
        davs2_frame_destroy(h->f_background_ref);
        h->f_background_ref = NULL;
    }

    if (h->f_background_cur) {
        davs2_frame_destroy(h->f_background_cur);
        h->f_background_cur = NULL;
    }

    if (h->p_frame_alf) {
        davs2_frame_destroy(h->p_frame_alf);
        h->p_frame_alf = NULL;
    }

    if (h->p_frame_sao) {
        davs2_frame_destroy(h->p_frame_sao);
        h->p_frame_sao = NULL;
    }

    if (h->p_integral) {
        davs2_free(h->p_integral);
        h->p_integral = NULL;
    }
}

/* ---------------------------------------------------------------------------
 * alloc extra buffers for the decoder according to the image width & height
 */
int decoder_alloc_extra_buffer(davs2_t *h)
{
    size_t w_in_spu = h->i_width_in_spu;
    size_t h_in_spu = h->i_height_in_spu;
    size_t w_in_scu = h->i_width_in_scu;
    size_t h_in_scu = h->i_height_in_scu;
    size_t size_in_spu = w_in_spu * h_in_spu;
    size_t size_in_lcu = ((h->i_width + h->i_lcu_size_sub1) >> h->i_lcu_level) * ((h->i_height + h->i_lcu_size_sub1) >> h->i_lcu_level);
    size_t size_alf = alf_get_buffer_size(h);
    size_t size_extra_frame = 0;
    size_t mem_size;

    uint8_t *mem_base;

    assert((h->i_width  & 7) == 0);
    assert((h->i_height & 7) == 0);
    size_extra_frame = 2 * davs2_frame_get_size(h->i_width, h->i_height, h->i_chroma_format, 1);
    size_extra_frame += (h->b_alf + h->b_sao) * davs2_frame_get_size(h->i_width, h->i_height, h->i_chroma_format, 0);

    mem_size = sizeof(int8_t)     * (w_in_spu + 16) * (h_in_spu + 1) + /* M1, size of intra prediction mode buffer */
               sizeof(int8_t)     * size_in_spu                      + /* M3, size of prediction direction buffer */
               sizeof(ref_idx_t)  * size_in_spu                      + /* M3, size of reference index (1st+2nd) buffer */
               sizeof(mv_t)       * size_in_spu                      + /* M5, size of motion vector of 4x4 block (1st reference) buffer */
               sizeof(mv_t)       * size_in_spu                      + /* M6, size of motion vector of 4x4 block (2nd reference) buffer */
               sizeof(uint8_t)    * w_in_scu * h_in_scu * 2          + /* M7, size of loop filter flag buffer */
               sizeof(lcu_info_t) * size_in_lcu                      + /* M8, size of SAO block parameter buffer */
               sizeof(cu_t)       * h->i_size_in_scu                 + /* M10, size of cu_t */
               sizeof(pel_t)      * h->i_width * 3                   + /* M13, size of last LCU row bottom border */
               size_alf                                              + /* M11, size of ALF */
               size_extra_frame                                      + /* M12, size of extra frame */
               CACHE_LINE_SIZE * 20;

    /* allocate memory for a decoder */
    CHECKED_MALLOC(mem_base, uint8_t *, mem_size);
    h->p_integral = mem_base;   /* pointer which holds the extra buffer */

    /* M1, intra prediction mode buffer */
    h->p_ipredmode  = (int8_t *)mem_base;
    mem_base       += sizeof(int8_t) * (w_in_spu + 16) * (h_in_spu + 1);
    h->p_ipredmode += (w_in_spu + 16) + 16;
    h->i_ipredmode  = ((int)w_in_spu + 16);
    ALIGN_POINTER(mem_base);

    /* M3, prediction direction buffer */
    h->p_dirpred = (int8_t *)mem_base;
    mem_base += sizeof(int8_t) * size_in_spu;
    ALIGN_POINTER(mem_base);

    /* M3, reference index (1st) buffer */
    h->p_ref_idx = (ref_idx_t *)mem_base;
    mem_base    += sizeof(ref_idx_t) * size_in_spu;
    ALIGN_POINTER(mem_base);

    /* M5, motion vector of 4x4 block (1st reference) buffer */
    h->p_tmv_1st = (mv_t *)mem_base;
    mem_base    += sizeof(mv_t) * size_in_spu;
    ALIGN_POINTER(mem_base);

    /* M6, motion vector of 4x4 block (1st reference) buffer */
    h->p_tmv_2nd = (mv_t *)mem_base;
    mem_base    += sizeof(mv_t) * size_in_spu;
    ALIGN_POINTER(mem_base);

    /* M7, loop filter flag buffer */
    h->p_deblock_flag[0] = (uint8_t *)mem_base;
    mem_base            += sizeof(uint8_t) * w_in_scu * h_in_scu;
    h->p_deblock_flag[1] = (uint8_t *)mem_base;
    mem_base            += sizeof(uint8_t) * w_in_scu * h_in_scu;
    ALIGN_POINTER(mem_base);

    /* M8, LCU level parameter buffer */
    h->lcu_infos    = (lcu_info_t *)mem_base;
    mem_base       += sizeof(lcu_info_t) * size_in_lcu;
    ALIGN_POINTER(mem_base);

    /* allocate memory for scu_data */
    h->scu_data     = (cu_t *)mem_base;
    mem_base       += h->i_size_in_scu * sizeof(cu_t);
    ALIGN_POINTER(mem_base);

    /* LCU bottom border */
    h->intra_border[0] = (pel_t *)mem_base;
    mem_base += h->i_width * sizeof(pel_t);
    ALIGN_POINTER(mem_base);
    h->intra_border[1] = (pel_t *)mem_base;
    mem_base += h->i_width * sizeof(pel_t);
    ALIGN_POINTER(mem_base);
    h->intra_border[2] = (pel_t *)mem_base;
    mem_base += h->i_width * sizeof(pel_t);
    ALIGN_POINTER(mem_base);

    /* ALF */
    h->p_alf        = (alf_var_t *)mem_base;
    mem_base       += size_alf;
    ALIGN_POINTER(mem_base);
    alf_init_buffer(h);

    /* -------------------------------------------------------------
     * allocate frame buffers */

    // AVS-S
    h->f_background_ref = davs2_frame_new(h->i_width, h->i_height, h->i_chroma_format, &mem_base, 1);
    ALIGN_POINTER(mem_base);
    h->f_background_cur = davs2_frame_new(h->i_width, h->i_height, h->i_chroma_format, &mem_base, 1);
    ALIGN_POINTER(mem_base);

    // ALF
    if (h->b_alf) {
        h->p_frame_alf = davs2_frame_new(h->i_width, h->i_height, h->i_chroma_format, &mem_base, 0);
        ALIGN_POINTER(mem_base);
    }

    // SAO
    if (h->b_sao) {
        h->p_frame_sao = davs2_frame_new(h->i_width, h->i_height, h->i_chroma_format, &mem_base, 0);
        ALIGN_POINTER(mem_base);
    }

    if ((int)mem_size < (mem_base - h->p_integral)) {
        davs2_log(h, DAVS2_LOG_ERROR, "No enough memory allocated. mem_size %llu <= %llu\n",
                   mem_size, mem_base - h->p_integral);
        goto fail;
    }
    return 0;

fail:

    decoder_free_extra_buffer(h);

    return -1;
}

/* ---------------------------------------------------------------------------
 * write a frame to output picture
 */
void davs2_write_a_frame(davs2_picture_t *pic, davs2_frame_t *frame)
{
    int img_width    = pic->widths[0];
    int img_height   = pic->lines[0];
    int img_width_c  = (img_width / 2);
    int img_height_c = (img_height / (frame->i_chroma_format == CHROMA_420 ? 2 : 1));
    int num_bytes_per_sample = (frame->i_output_bit_depth == 8 ? 1 : 2);
    int shift1       = frame->i_sample_bit_depth - frame->i_output_bit_depth; // assuming "sample_bit_depth" is greater or equal to "output_bit_depth"
    pel_t *p_src;
    uint8_t *p_dst;
    int k, j, i_src, i_dst;

    pic->num_planes       = (frame->i_chroma_format != CHROMA_400) ? 3 : 1;
    pic->bytes_per_sample = num_bytes_per_sample;
    pic->bit_depth        = frame->i_output_bit_depth;
    pic->b_decode_error   = frame->frm_decode_error;
    pic->dec_frame        = NULL;
    pic->strides[0] = pic->widths[0] * num_bytes_per_sample;
    pic->strides[1] = pic->widths[1] * num_bytes_per_sample;
    pic->strides[2] = pic->widths[2] * num_bytes_per_sample;

    if (!shift1 && sizeof(pel_t) == num_bytes_per_sample) {
        pic->dec_frame = frame;
        // TODO: 如下赋值前的指针需要在适当的时候（进入后续分支时）恢复
        pic->planes[0]  = frame->planes[0];
        pic->planes[1]  = frame->planes[1];
        pic->planes[2]  = frame->planes[2];
        pic->strides[0] = frame->i_stride[0] * num_bytes_per_sample;
        pic->strides[1] = frame->i_stride[1] * num_bytes_per_sample;
        pic->strides[2] = frame->i_stride[2] * num_bytes_per_sample;
    } else if (!shift1 && frame->i_output_bit_depth == 8) { // 8bit encode -> 8bit output
        p_dst = pic->planes[0];
        i_dst = pic->strides[0];
        p_src = frame->planes[0];
        i_src = frame->i_stride[0];

        for (j = 0; j < img_height; j++) {
            for (k = 0; k < img_width; k++) {
                p_dst[k] = (uint8_t)p_src[k];
            }

            p_src += i_src;
            p_dst += i_dst;
        }

        if (pic->num_planes == 3) {
            p_dst = pic->planes[1];
            i_dst = pic->strides[1];
            p_src = frame->planes[1];
            i_src = frame->i_stride[1];

            for (j = 0; j < img_height_c; j++) {
                for (k = 0; k < img_width_c; k++) {
                    p_dst[k] = (uint8_t)p_src[k];
                }

                p_src += i_src;
                p_dst += i_dst;
            }

            p_dst = pic->planes[2];
            i_dst = pic->strides[2];
            p_src = frame->planes[2];
            i_src = frame->i_stride[2];

            for (j = 0; j < img_height_c; j++) {
                for (k = 0; k < img_width_c; k++) {
                    p_dst[k] = (uint8_t)p_src[k];
                }

                p_src += i_src;
                p_dst += i_dst;
            }
        }
    } else if (shift1 && frame->i_output_bit_depth == 8) { // 10bit encode -> 8bit output
        p_dst = pic->planes[0];
        i_dst = pic->strides[0];
        p_src = frame->planes[0];
        i_src = frame->i_stride[0];

        for (j = 0; j < img_height; j++) {
            for (k = 0; k < img_width; k++) {
                p_dst[k] = (uint8_t)DAVS2_CLIP1((p_src[k] + (1 << (shift1 - 1))) >> shift1);
            }

            p_src += i_src;
            p_dst += i_dst;
        }

        if (pic->num_planes == 3) {
            p_dst = pic->planes[1];
            i_dst = pic->strides[1];
            p_src = frame->planes[1];
            i_src = frame->i_stride[1];

            for (j = 0; j < img_height_c; j++) {
                for (k = 0; k < img_width_c; k++) {
                    p_dst[k] = (uint8_t)DAVS2_CLIP1((p_src[k] + (1 << (shift1 - 1))) >> shift1);
                }

                p_src += i_src;
                p_dst += i_dst;
            }

            p_dst = pic->planes[2];
            i_dst = pic->strides[2];
            p_src = frame->planes[2];
            i_src = frame->i_stride[2];

            for (j = 0; j < img_height_c; j++) {
                for (k = 0; k < img_width_c; k++) {
                    p_dst[k] = (uint8_t)DAVS2_CLIP1((p_src[k] + (1 << (shift1 - 1))) >> shift1);
                }

                p_src += i_src;
                p_dst += i_dst;
            }
        }
    }

    pic->type            = frame->i_type;
    pic->qp              = frame->i_qp;
    pic->pts             = frame->i_pts;
    pic->dts             = frame->i_dts;
    pic->pic_order_count = frame->i_poc;
}

/* ---------------------------------------------------------------------------
 */
davs2_t *decoder_open(davs2_mgr_t *mgr, davs2_t *h, int idx_decoder)
{
    /* allocate memory for a decoder */
    memset(h, 0, sizeof(davs2_t));

    /* init log module */
    h->module_log.i_log_level = mgr->param.info_level;
    sprintf(h->module_log.module_name, "Dec[%2d] %06llx", idx_decoder,(long long unsigned int) h);

    /* only initialize some variables, not ready to work */
    h->task_info.taskmgr = mgr;
    h->i_width           = -1;
    h->i_height          = -1;
    h->i_frame_type      = AVS2_I_SLICE;
    h->num_of_references = 0;
    h->b_video_edit_code = 0;

#if AVS2_TRACE
    if (avs2_trace_init(h, TRACEFILE) == -1) {  // append new statistic at the end
        davs2_log(h, DAVS2_LOG_ERROR, "Error open trace file!");
    }
#endif

    return h;
}

/**
 * ---------------------------------------------------------------------------
 * Function   : decode one frame
 * Parameters :
 *       [in] : h       - pointer to struct davs2_t (decoder handler)
 *            : es_unit - pointer to bit-stream buffer (including the following parameters)
 *            :    data - pointer to bitstream buffer
 *            :    len  - data length in bitstream buffer
 *            :    pts  - user pts
 *            :    dts  - user dts
 * Return     : none
 * ---------------------------------------------------------------------------
 */
void *decoder_decode_picture_data(void *arg1, int arg2)
{
    davs2_t *h     = (davs2_t *)arg1;
    davs2_bs_t *bs = h->p_bs;

    UNUSED_PARAMETER(arg2);
    /* decode one frame */
    init_frame(h);
    /* user data and slice header */
    decode_user_data(h, bs);

    /* decode picture data */
    if (h->b_slice_checked != 0) {
        davs2_frame_t *frame = h->fref[0];
        davs2_mgr_t *mgr = h->task_info.taskmgr;
        const int height_in_lcu = h->i_height_in_lcu;
        int lcu_y;

        /* reset LCU decoding status */
        memset(h->fdec->num_decoded_lcu_in_row, 0, sizeof(int) * h->i_height_in_lcu);

        // davs2_thread_mutex_lock(&mgr->mutex_aec);
        // mgr->num_active_decoders++;
        // davs2_thread_mutex_unlock(&mgr->mutex_aec);

        if (mgr->num_rec_thread && davs2_threadpool_is_free((davs2_threadpool_t *)mgr->thread_pool)) {
            /* make sure all its dependency frames have started reconstruction */
            int i;
            for (i = 0; i < h->num_of_references; i++) {
                davs2_frame_t *frm = h->fref[i];
                decoder_wait_lcu_row(h, frm, 0);
            }


            /* run reconstruction thread */
            davs2_threadpool_run((davs2_threadpool_t *)mgr->thread_pool,
                                 (davs2_threadpool_func_t)decode_lcu_row_recon, h, 0,
                                 0);
            /* -------------------------------------------------------------
             * parse all LCU rows
             */
            for (lcu_y = 0; lcu_y < height_in_lcu; lcu_y++) {
                /* TODO: remove the dependency in this thread */
                if (frame != NULL) {
                    decoder_wait_lcu_row(h, frame, lcu_y);
                }

                /* parsing the LCU data */
                decode_one_lcu_row_parse(h, bs, lcu_y);
            }
        } else {
            /* -------------------------------------------------------------
             * decode all LCU rows
             */
            for (lcu_y = 0; lcu_y < height_in_lcu; lcu_y++) {
                if (frame != NULL) {
                    decoder_wait_lcu_row(h, frame, lcu_y);
                }

                /* decode one lcu row */
                decode_one_lcu_row(h, bs, lcu_y);
            }
        }
    } else {
        ///!!! make sure that all row signals of frames with 'b_refered_by_others == 1' have been set before return.
        /// use 'goto fail' instead of 'return' in the half way.
        if (h->rps.refered_by_others) {
            // set all row signals before returning.
            int lcu_y;
            for (lcu_y = 0; lcu_y < h->i_height_in_lcu; ++lcu_y) {
                decoder_signal(h, h->fdec, lcu_y);
            }
        }

        if (h->i_frame_type == AVS2_G_SLICE) {
            davs2_frame_copy_planes(h->f_background_ref, h->fdec);
        }

        /* task is free */
        task_unload_packet(h, h->task_info.curr_es_unit);
    }

    return NULL;
}


/**
 * ---------------------------------------------------------------------------
 * Function   : close the AVS2 decoder
 * Parameters :
 *       [in] : h - pointer to struct davs2_t, the decoder handle
 * Return     : none
 * ---------------------------------------------------------------------------
 */
void decoder_close(davs2_t *h)
{
    /* free extra buffer */
    decoder_free_extra_buffer(h);

#if AVS2_TRACE
    /* destroy the trace */
    avs2_trace_destroy();
#endif
}
