/*
 * decoder.h
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

#ifndef DAVS2_DECODER_H
#define DAVS2_DECODER_H
#ifdef __cplusplus
extern "C" {
#endif

#include "common.h"

#define decoder_open FPFX(decoder_decoder_open)
davs2_t *decoder_open(davs2_mgr_t *mgr, davs2_t *h, int idx_decoder);
#define decoder_decode_picture_data FPFX(decoder_decode_picture_data)
void *decoder_decode_picture_data(void *arg1, int arg2);
#define decoder_close FPFX(decoder_decoder_close)
void decoder_close(davs2_t *h);
#define create_freepictures FPFX(create_freepictures)
int  create_freepictures(davs2_mgr_t *mgr, int w, int h, int size);
#define destroy_freepictures FPFX(destroy_freepictures)
void destroy_freepictures(davs2_mgr_t *mgr);
#define decoder_alloc_extra_buffer FPFX(decoder_alloc_extra_buffer)
int  decoder_alloc_extra_buffer(davs2_t *h);
#define decoder_free_extra_buffer FPFX(decoder_free_extra_buffer)
void decoder_free_extra_buffer(davs2_t *h);
#define davs2_write_a_frame FPFX(write_a_frame)
void davs2_write_a_frame(davs2_picture_t *pic, davs2_frame_t *frame);

#define task_get_references FPFX(task_get_references)
int  task_get_references(davs2_t *h, int64_t pts, int64_t dts);

#define task_unload_packet FPFX(task_unload_packet)
void task_unload_packet(davs2_t *h, es_unit_t *es_unit);
#define decoder_get_output FPFX(decoder_get_output)
int decoder_get_output(davs2_mgr_t *mgr, davs2_seq_info_t *headerset, davs2_picture_t *out_frame, int is_flush);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_DECODER_H
