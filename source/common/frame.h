/*
 * frame.h
 *
 * Description of this file:
 *    Frame handling functions definition of the davs2 library
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

#ifndef DAVS2_FRAME_H
#define DAVS2_FRAME_H
#ifdef __cplusplus
extern "C" {
#endif

/**
 * ===========================================================================
 * function declares
 * ===========================================================================
 */
size_t          davs2_frame_get_size(int width, int height, int chroma_format, int b_extra);
davs2_frame_t *davs2_frame_new(int width, int height, int chroma_format, uint8_t **mem_base, int b_extra);

void davs2_frame_destroy(davs2_frame_t *frame);

void davs2_frame_copy_planes(davs2_frame_t *p_dst, davs2_frame_t *p_src);
void davs2_frame_copy_properties(davs2_frame_t *p_dst, davs2_frame_t *p_src);
void davs2_frame_copy_lcu(davs2_t *h, davs2_frame_t *p_dst, davs2_frame_t *p_src, int i_lcu_x, int i_lcu_y, int pix_offset, int padding_size);
void davs2_frame_copy_lcurow(davs2_t *h, davs2_frame_t *p_dst, davs2_frame_t *p_src, int i_lcu_y, int pix_offset, int padding_size);

void davs2_frame_expand_border(davs2_frame_t *frame);

void pad_line_lcu(davs2_t *h, int lcu_y);

#ifdef __cplusplus
}
#endif
#endif  /* DAVS2_FRAME_H */
