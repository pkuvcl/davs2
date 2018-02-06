/*****************************************************************************
 * Copyright (C) 2013-2017 MulticoreWare, Inc
 *
 * Authors: Nabajit Deka <nabajit@multicorewareinc.com>
;*          Min Chen <chenm003@163.com>
 *          Jiaqi Zhang <zhangjiaqi.cs@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/


#ifndef DAVS2_I386_DCT8_H
#define DAVS2_I386_DCT8_H
#ifdef __cplusplus
extern "C" {
#endif

void FPFX(idct_4x4_sse2 )(const coeff_t *src, coeff_t *dst, int i_dst);
void FPFX(idct_8x8_ssse3)(const coeff_t *src, coeff_t *dst, int i_dst);
#if ARCH_X86_64
void FPFX(idct_4x4_avx2  )(const coeff_t *src, coeff_t *dst, int i_dst);
void FPFX(idct_8x8_sse2  )(const coeff_t *src, coeff_t *dst, int i_dst);
void FPFX(idct_8x8_avx2  )(const coeff_t *src, coeff_t *dst, int i_dst);
void FPFX(idct_16x16_avx2)(const coeff_t *src, coeff_t *dst, int i_dst);
void FPFX(idct_32x32_avx2)(const coeff_t *src, coeff_t *dst, int i_dst);
#endif

#ifdef __cplusplus
}
#endif
#endif // ifndef DAVS2_I386_DCT8_H
