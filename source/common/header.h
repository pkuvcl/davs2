/*
 * header.h
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

#ifndef DAVS2_HEADER_H
#define DAVS2_HEADER_H
#ifdef __cplusplus
extern "C" {
#endif

#define parse_slice_header FPFX(parse_slice_header)
void parse_slice_header(davs2_t *h, davs2_bs_t *bs);
#define parse_header FPFX(parse_header)
int  parse_header(davs2_t *h, davs2_bs_t *p_bs);

#define release_one_frame FPFX(release_one_frame)
void release_one_frame(davs2_frame_t *frame);
#define task_release_frames FPFX(task_release_frames)
void task_release_frames(davs2_t *h);

#define alloc_picture FPFX(alloc_picture)
davs2_outpic_t *alloc_picture(int w, int h);
#define free_picture FPFX(free_picture)
void free_picture(davs2_outpic_t *pic);

#define destroy_dpb FPFX(destroy_dpb)
void destroy_dpb(davs2_mgr_t *mgr);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_HEADER_H
