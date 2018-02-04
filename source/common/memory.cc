/*
 * memory.cc
 *
 * Description of this file:
 *    Memory functions definition of the davs2 library
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
#include "primitives.h"

#if HAVE_MMX
#include "vec/intrinsic.h"
#endif

/* ---------------------------------------------------------------------------
 */
void *memzero_aligned_c(void *dst, size_t n)
{
    return memset(dst, 0, n);
}

/* ---------------------------------------------------------------------------
 */
void davs2_memory_init(uint32_t cpuid, ao_funcs_t* pf)
{
    /* memory copy */
    pf->fast_memcpy      = memcpy;
    pf->fast_memset      = memset;
    pf->memcpy_aligned   = memcpy;
    pf->fast_memzero     = memzero_aligned_c;
    pf->memzero_aligned  = memzero_aligned_c;

    /* init asm function handles */
#if HAVE_MMX
    if (cpuid & DAVS2_CPU_MMX) {
        pf->fast_memcpy     = davs2_fast_memcpy_mmx;
        pf->memcpy_aligned  = davs2_memcpy_aligned_mmx;
        pf->fast_memset     = davs2_fast_memset_mmx;
        pf->fast_memzero    = davs2_fast_memzero_mmx;
        pf->memzero_aligned = davs2_fast_memzero_mmx;
    }

    if (cpuid & DAVS2_CPU_SSE) {
        // pf->memcpy_aligned  = davs2_memcpy_aligned_sse;
        // pf->memzero_aligned = davs2_memzero_aligned_sse;
    }

    if (cpuid & DAVS2_CPU_SSE2) {
        pf->memzero_aligned = davs2_memzero_aligned_c_sse2;
        // gf_davs2.memcpy_aligned  = davs2_memcpy_aligned_c_sse2;
    }

    if (cpuid & DAVS2_CPU_AVX2) {
        pf->memzero_aligned = davs2_memzero_aligned_c_avx;
    }
#endif // HAVE_MMX
}
