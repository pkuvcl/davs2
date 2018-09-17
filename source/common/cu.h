/*
 * cu.h
 *
 * Description of this file:
 *    CU Processing functions definition of the davs2 library
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

#ifndef DAVS2_CU_H
#define DAVS2_CU_H
#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * init LCU decoding
 * \input param
 *     h    : decoder handler
 *  i_lcu_x : LCU position index
 *  i_lcu_y : LCU position index
 */
#define decode_lcu_init FPFX(decode_lcu_init)
void decode_lcu_init (davs2_t *h, int i_lcu_x, int i_lcu_y);

#define rowrec_lcu_init FPFX(rowrec_lcu_init)
void rowrec_lcu_init (davs2_t *h, davs2_row_rec_t *row_rec, int i_lcu_x, int i_lcu_y);

/* ---------------------------------------------------------------------------
 * process LCU entropy decoding (recursively)
 * \input param
 *     h    : decoder handler
 *  i_level : log2(CU size)
 *   pix_x  : pixel position of the decoding CU in the frame in Luma component
 *   pix_y  : pixel position of the decoding CU in the frame in Luma component
 */
#define decode_lcu_parse FPFX(decode_lcu_parse)
int  decode_lcu_parse(davs2_t *h, int i_level, int pix_x, int pix_y);

/* ---------------------------------------------------------------------------
 * process LCU reconstruction (recursively)
 * \input param
 *     h    : decoder handler
 *  i_level : log2(CU size)
 *   pix_x  : pixel position of the decoding CU in the frame in Luma component
 *   pix_y  : pixel position of the decoding CU in the frame in Luma component
 */
#define decode_lcu_recon FPFX(decode_lcu_recon)
int  decode_lcu_recon(davs2_t *h, davs2_row_rec_t *row_rec, int i_level, int pix_x, int pix_y);

#define decoder_wait_lcu_row FPFX(decoder_wait_lcu_row)
void decoder_wait_lcu_row(davs2_t *h, davs2_frame_t *frame, int max_y_in_pic);
#define decoder_wait_row FPFX(decoder_wait_row)
void decoder_wait_row(davs2_t *h, davs2_frame_t *frame, int max_y_in_pic);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_CU_H
