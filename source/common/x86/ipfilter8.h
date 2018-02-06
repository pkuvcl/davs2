/*****************************************************************************
 * Copyright (C) 2013-2017 MulticoreWare, Inc
 *
 * Authors: Steve Borho <steve@borho.org>
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

#ifndef DAVS2_IPFILTER8_H
#define DAVS2_IPFILTER8_H

#include "../vec/intrinsic.h"

#if defined(__cplusplus)
extern "C" {
#endif  /* __cplusplus */

#define SETUP_FUNC_DEF(cpu) \
    FUNCDEF_PU(void, interp_8tap_horiz_pp, cpu, const pel_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int coeffIdx); \
    FUNCDEF_PU(void, interp_8tap_horiz_ps, cpu, const pel_t* src, intptr_t srcStride, int16_t* dst, intptr_t dstStride, int coeffIdx, int isRowExt); \
    FUNCDEF_PU(void, interp_8tap_vert_pp, cpu, const pel_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int coeffIdx); \
    FUNCDEF_PU(void, interp_8tap_vert_ps, cpu, const pel_t* src, intptr_t srcStride, int16_t* dst, intptr_t dstStride, int coeffIdx); \
    FUNCDEF_PU(void, interp_8tap_vert_sp, cpu, const int16_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int coeffIdx); \
    FUNCDEF_PU(void, interp_8tap_vert_ss, cpu, const int16_t* src, intptr_t srcStride, int16_t* dst, intptr_t dstStride, int coeffIdx); \
    FUNCDEF_PU(void, interp_8tap_hv_pp, cpu, const pel_t* src, intptr_t srcStride, pel_t* dst, intptr_t dstStride, int idxX, int idxY)

SETUP_FUNC_DEF(sse2);
SETUP_FUNC_DEF(ssse3);
SETUP_FUNC_DEF(sse3);
SETUP_FUNC_DEF(sse4);
SETUP_FUNC_DEF(avx2);

#if defined(__cplusplus)
}
#endif  /* __cplusplus */
#endif // ifndef DAVS2_IPFILTER8_H
