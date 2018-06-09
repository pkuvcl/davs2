/*
 * frame.cc
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

#include "common.h"
#include "frame.h"
#include "header.h"


/**
 * ===========================================================================
 * border expanding
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE
void pad_line_pixel(pel_t *pix, int width, int num_pad)
{
    pel4_t *p_l4 = (pel4_t *)(pix - num_pad);
    pel4_t *p_r4 = (pel4_t *)(pix + width);
    pel4_t l4 = pix[0];
    pel4_t r4 = pix[width - 1];
#if ARCH_X86_64 && !HIGH_BIT_DEPTH
    uint64_t *p_l64 = (uint64_t *)p_l4;
    uint64_t *p_r64 = (uint64_t *)p_r4;
    uint64_t l64;
    uint64_t r64;
#endif

#if HIGH_BIT_DEPTH
    l4 = (l4 << 48) | (l4 << 32) | (l4 << 16) | l4;
    r4 = (r4 << 48) | (r4 << 32) | (r4 << 16) | r4;
#else
    l4 = (l4 << 24) | (l4 << 16) | (l4 << 8) | l4;
    r4 = (r4 << 24) | (r4 << 16) | (r4 << 8) | r4;
#if ARCH_X86_64
    l64 = ((uint64_t)(l4) << 32) | l4;
    r64 = ((uint64_t)(r4) << 32) | r4;
#endif
#endif

#if ARCH_X86_64 && !HIGH_BIT_DEPTH
    assert((num_pad & 7) == 0);
    num_pad >>= 3;

    for (; num_pad != 0; num_pad--) {
        *p_l64++ = l64;              /* pad left */
        *p_r64++ = r64;              /* pad right */
    }
#else
    assert((num_pad & 3) == 0);
    num_pad >>= 2;

    for (; num_pad != 0; num_pad--) {
        *p_l4++ = l4;              /* pad left */
        *p_r4++ = r4;              /* pad right */
    }
#endif
}

/* ---------------------------------------------------------------------------
 */
void pad_line_lcu(davs2_t *h, int lcu_y)
{
    davs2_frame_t *frame = h->fdec;
    int i, j;

    for (i = 0; i < 3; i++) {
        int chroma_shift = !!i;
        int start = ((lcu_y + 0) << h->i_lcu_level) >> chroma_shift; ///< -4 for ALF
        int end   = ((lcu_y + 1) << h->i_lcu_level) >> chroma_shift;
        int i_stride = frame->i_stride[i];
        int i_width  = frame->i_width[i];
        const int num_pad = AVS2_PAD >> chroma_shift;
        pel_t *pix;

        if (lcu_y > 0) {
            start -= 4;
        }
        if (lcu_y < h->i_height_in_lcu - 1) {
            end -= 4;
        }

        /* padding these rows */
        for (j = start; j < end; j++) {
            pix = frame->planes[i] + j * i_stride;
            pad_line_pixel(pix, i_width, num_pad);
        }

        /* for the first row, padding the rows above the picture edges */
        if (lcu_y == 0) {
            pix = frame->planes[i] - (num_pad);

            for (j = 0; j < (num_pad); j++) {
                gf_davs2.memcpy_aligned(pix - i_stride, pix, i_stride * sizeof(pel_t));
                pix -= i_stride;
            }
        }

        /* for the last row, padding the rows under of the picture edges */
        if (lcu_y == h->i_height_in_lcu - 1) {
            pix = frame->planes[i] + (frame->i_lines[i] - 1) * i_stride - (num_pad);

            for (j = 0; j < (num_pad); j++) {
                gf_davs2.memcpy_aligned(pix + i_stride, pix, i_stride * sizeof(pel_t));
                pix += i_stride;
            }
        }
    }
}

/**
 * ===========================================================================
 * memory handling
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE int
align_stride(int x, int align, int disalign)
{
    x = DAVS2_ALIGN(x, align);
    if (!(x & (disalign - 1))) {
        x += align;
    }

    return x;
}

/* ---------------------------------------------------------------------------
 */
static ALWAYS_INLINE int
align_plane_size(int x, int disalign)
{
    if (!(x & (disalign - 1))) {
        x += 128;
    }

    return x;
}

/* ---------------------------------------------------------------------------
 */
size_t davs2_frame_get_size(int width, int height, int chroma_format, int b_extra)
{
    const int width_c        = width >> 1;
    const int height_c       = height >> (chroma_format == CHROMA_420 ? 1 : 0);
    const int width_in_spu   = width  >> MIN_PU_SIZE_IN_BIT;
    const int height_in_spu  = height >> MIN_PU_SIZE_IN_BIT;
    const int max_lcu_height = (height + (1 << 4) - 1) >> 4; /* frame height in 16x16 LCU */
    const int align    = 32;
    const int disalign = 1 << 16;
    int extra_buf_size = 0;     /* extra buffer size */
    int stride_l, stride_c;
    int size_l, size_c;         /* size of luma and chroma plane */
    size_t mem_size;            /* total memory size */

    /* need extra buffer? */
    if (b_extra) {
        /* reference information buffer size (in SPU) */
        extra_buf_size = width_in_spu * height_in_spu;
    }

    /* compute stride and the plane size
     * +PAD for extra data for MC */
    stride_l = align_stride(width + AVS2_PAD * 2, align, disalign);
    stride_c = align_stride(width_c + AVS2_PAD, align, disalign);
    size_l   = align_plane_size(stride_l * (height + AVS2_PAD * 2) + CACHE_LINE_SIZE, disalign);
    size_c   = align_plane_size(stride_c * (height_c + AVS2_PAD) + CACHE_LINE_SIZE,   disalign);

    /* compute space size and alloc memory */
    mem_size = sizeof(davs2_frame_t)                      + /* M0, size of frame handle */
               sizeof(pel_t)  * (size_l + size_c * 2)       + /* M1, size of planes buffer: Y+U+V */
               sizeof(int8_t) * extra_buf_size              + /* M2, size of SPU reference index buffer */
               sizeof(mv_t)   * extra_buf_size              + /* M3, size of SPU motion vector buffer */
               sizeof(davs2_thread_cond_t) * max_lcu_height + /* M4, condition variables for each LCU line */
               sizeof(int) * max_lcu_height                 + /* M5, LCU decoding status */
               CACHE_LINE_SIZE * 6;

    return mem_size;
}

/* ---------------------------------------------------------------------------
 */
davs2_frame_t *davs2_frame_new(int width, int height, int chroma_format, uint8_t **mem_base, int b_extra)
{
    const int width_c        = width >> 1;
    const int height_c       = height >> (chroma_format == CHROMA_420 ? 1 : 0);
    const int width_in_spu   = width  >> MIN_PU_SIZE_IN_BIT;
    const int height_in_spu  = height >> MIN_PU_SIZE_IN_BIT;
    const int max_lcu_height = (height + (1 << 4) - 1) / (1 << 4); /* frame height in 16x16 LCU */
    const int align    = 32;
    const int disalign = 1 << 16;
    int extra_buf_size = 0;     /* extra buffer size */
    int stride_l, stride_c;
    int size_l, size_c;         /* size of luma and chroma plane */
    int i, mem_size;            /* total memory size */
    davs2_frame_t *frame;
    uint8_t *mem_ptr;

    /* need extra buffer? */
    if (b_extra) {
        /* reference information buffer size (in SPU) */
        extra_buf_size = width_in_spu * height_in_spu;
    }

    /* compute stride and the plane size
     * +PAD for extra data for MC */
    stride_l = align_stride(width + AVS2_PAD * 2, align, disalign);
    stride_c = align_stride(width_c + AVS2_PAD, align, disalign);
    size_l   = align_plane_size(stride_l * (height + AVS2_PAD * 2) + CACHE_LINE_SIZE, disalign);
    size_c   = align_plane_size(stride_c * (height_c + AVS2_PAD) + CACHE_LINE_SIZE,   disalign);

    /* compute space size and alloc memory */
    mem_size = sizeof(davs2_frame_t)                       + /* M0, size of frame handle */
               sizeof(pel_t)  * (size_l + size_c * 2)       + /* M1, size of planes buffer: Y+U+V */
               sizeof(int8_t) * extra_buf_size              + /* M2, size of SPU reference index buffer */
               sizeof(mv_t)   * extra_buf_size              + /* M3, size of SPU motion vector buffer */
               sizeof(davs2_thread_cond_t) * max_lcu_height + /* M4, condition variables for each LCU line */
               sizeof(int) * max_lcu_height                 + /* M5, LCU decoding status */
               CACHE_LINE_SIZE * 8;

    if (mem_base == NULL) {
        CHECKED_MALLOC(mem_ptr, uint8_t *, mem_size);
    } else {
        mem_ptr = *mem_base;
    }

    /* M0, frame handle */
    frame    = (davs2_frame_t *)mem_ptr;
    memset(frame, 0, sizeof(davs2_frame_t));
    mem_ptr += sizeof(davs2_frame_t);
    ALIGN_POINTER(mem_ptr);

    /* set frame properties */
    frame->i_plane     = 3;           /* planes: Y+U+V */
    frame->i_width [0] = width;
    frame->i_lines [0] = height;
    frame->i_stride[0] = stride_l;
    frame->i_width [1] = frame->i_width [2] = width_c;
    frame->i_lines [1] = frame->i_lines [2] = height_c;
    frame->i_stride[1] = frame->i_stride[2] = stride_c;

    frame->i_type      = -1;
    frame->i_pts       = -1;
    frame->i_coi       = INVALID_FRAME;
    frame->i_poc       = INVALID_FRAME;
    frame->b_refered_by_others = 0;

    /* M1, buffer for planes: Y+U+V */
    frame->planes[0] = (pel_t *)mem_ptr;
    frame->planes[1] = frame->planes[0] + size_l;
    frame->planes[2] = frame->planes[1] + size_c;
    mem_ptr         += sizeof(pel_t) * (size_l + size_c * 2);

    /* point to plane data area */
    frame->planes[0] += frame->i_stride[0] * (AVS2_PAD    ) + (AVS2_PAD    );
    frame->planes[1] += frame->i_stride[1] * (AVS2_PAD / 2) + (AVS2_PAD / 2);
    frame->planes[2] += frame->i_stride[2] * (AVS2_PAD / 2) + (AVS2_PAD / 2);
    ALIGN_POINTER(frame->planes[0]);
    ALIGN_POINTER(frame->planes[1]);
    ALIGN_POINTER(frame->planes[2]);

    if (b_extra) {
        /* M2, reference index buffer (in SPU) */
        frame->refbuf = (int8_t *)mem_ptr;
        mem_ptr      += sizeof(int8_t) * extra_buf_size;
        ALIGN_POINTER(mem_ptr);

        /* M3, motion vector buffer (in SPU) */
        frame->mvbuf = (mv_t *)mem_ptr;
        mem_ptr     += sizeof(mv_t) * extra_buf_size;
        ALIGN_POINTER(mem_ptr);
    }

    /* M4 */
    frame->conds_lcu_row = (davs2_thread_cond_t *)mem_ptr;
    mem_ptr     += sizeof(davs2_thread_cond_t) * max_lcu_height;
    ALIGN_POINTER(mem_ptr);

    /* M5 */
    frame->num_decoded_lcu_in_row = (int *)mem_ptr;
    mem_ptr += sizeof(int) * max_lcu_height;
    ALIGN_POINTER(mem_ptr);

    assert(mem_ptr - (uint8_t *)frame <= mem_size);

    /* update mem_base */
    if (mem_base != NULL) {
        *mem_base = mem_ptr;
        frame->is_self_malloc = 0;
    } else {
        frame->is_self_malloc = 1;
    }

    frame->i_conds         = max_lcu_height;
    frame->i_decoded_line  = -1;
    frame->i_ref_count     = 0;
    frame->i_disposable    = 0;

    for (i = 0; i < frame->i_conds; i++) {
        if (davs2_thread_cond_init(&frame->conds_lcu_row[i], NULL)) {
            goto fail;
        }
    }

    davs2_thread_cond_init(&frame->cond_aec, NULL);
    davs2_thread_mutex_init(&frame->mutex_frm, NULL);
    davs2_thread_mutex_init(&frame->mutex_recon, NULL);

    return frame;

fail:
    if (mem_ptr) {
        davs2_free(mem_ptr);
    }

    return NULL;
}

/* ---------------------------------------------------------------------------
 */
void davs2_frame_destroy(davs2_frame_t *frame)
{
    int i;

    if (frame == NULL) {
        return;
    }

    davs2_thread_mutex_destroy(&frame->mutex_frm);
    davs2_thread_mutex_destroy(&frame->mutex_recon);

    for (i = 0; i < frame->i_conds; i++) {
        davs2_thread_cond_destroy(&frame->conds_lcu_row[i]);
    }

    /* free the frame itself */
    if (frame->is_self_malloc) {
        davs2_free(frame);
    }
}

/* ---------------------------------------------------------------------------
 */
void davs2_frame_copy_planes(davs2_frame_t *p_dst, davs2_frame_t *p_src)
{
    /* copy frame properties */
    memcpy(p_dst, p_src, (uint8_t *)&p_src->i_ref_count - (uint8_t *)p_src);

    /* copy all plane data */
#if 1
    /* 使用对齐地址的内存拷贝，进行一次大数据量地拷贝 */
    assert(p_src->i_stride[0] == p_dst->i_stride[0]);
    assert(p_src->i_stride[1] == p_dst->i_stride[1]);
    assert(p_src->i_stride[2] == p_dst->i_stride[2]);
    gf_davs2.memcpy_aligned(p_dst->planes[0], p_src->planes[0], p_src->i_stride[0] * p_src->i_lines[0] * sizeof(pel_t));
    gf_davs2.memcpy_aligned(p_dst->planes[1], p_src->planes[1], p_src->i_stride[1] * p_src->i_lines[1] * sizeof(pel_t));
    gf_davs2.memcpy_aligned(p_dst->planes[2], p_src->planes[2], p_src->i_stride[2] * p_src->i_lines[2] * sizeof(pel_t));
#else
    gf_davs2.plane_copy(p_dst->planes[0], p_dst->i_stride[0], p_src->planes[0], p_src->i_stride[0], p_src->i_width[0], p_src->i_lines[0]);
    gf_davs2.plane_copy(p_dst->planes[1], p_dst->i_stride[1], p_src->planes[1], p_src->i_stride[1], p_src->i_width[1], p_src->i_lines[1]);
    gf_davs2.plane_copy(p_dst->planes[2], p_dst->i_stride[2], p_src->planes[2], p_src->i_stride[2], p_src->i_width[2], p_src->i_lines[2]);
#endif
}

/* ---------------------------------------------------------------------------
 * copy frame properties */
void davs2_frame_copy_properties(davs2_frame_t *p_dst, davs2_frame_t *p_src)
{
    memcpy(p_dst, p_src, (uint8_t *)&p_src->i_ref_count - (uint8_t *)p_src);
}

/* ---------------------------------------------------------------------------
 */
void davs2_frame_copy_lcu(davs2_t *h, davs2_frame_t *p_dst, davs2_frame_t *p_src, int i_lcu_x, int i_lcu_y, int pix_offset, int padding_size)
{
    int pix_y = (i_lcu_y << h->i_lcu_level) + pix_offset;
    int pix_x = (i_lcu_x << h->i_lcu_level) + pix_offset;
    int lcu_width  = DAVS2_MIN(h->i_lcu_size, h->i_width - pix_x);
    int lcu_height = DAVS2_MIN(h->i_lcu_size, h->i_height - pix_y);
    int y, len, stride;
    pel_t *src, *dst;

    /* Y */
    stride = p_src->i_stride[0];
    src    = p_src->planes[0] + pix_y * stride + pix_x;
    dst    = p_dst->planes[0] + pix_y * stride + pix_x;
    len    = lcu_width * sizeof(pel_t);
    for (y = 0; y < lcu_height; y++) {
        gf_davs2.fast_memcpy(dst, src, len);
        if (padding_size) {
            pad_line_pixel(dst, p_dst->i_width[0], padding_size);
        }
        src += stride;
        dst += stride;
    }

    pix_y = (i_lcu_y << (h->i_lcu_level - 1)) + pix_offset;
    pix_x = (i_lcu_x << (h->i_lcu_level - 1)) + pix_offset;
    lcu_height >>= 1;

    /* U */
    stride = p_src->i_stride[1];
    src    = p_src->planes[1] + pix_y * stride + pix_x;
    dst    = p_dst->planes[1] + pix_y * stride + pix_x;
    len    = lcu_width * sizeof(pel_t);
    for (y = 0; y < lcu_height; y++) {
        gf_davs2.fast_memcpy(dst, src, len);
        if (padding_size) {
            pad_line_pixel(dst, p_dst->i_width[1], padding_size);
        }
        src += stride;
        dst += stride;
    }

    /* V */
    stride = p_src->i_stride[2];
    src    = p_src->planes[2] + pix_y * stride + pix_x;
    dst    = p_dst->planes[2] + pix_y * stride + pix_x;
    len    = lcu_width * sizeof(pel_t);
    for (y = 0; y < lcu_height; y++) {
        gf_davs2.fast_memcpy(dst, src, len);
        if (padding_size) {
            pad_line_pixel(dst, p_dst->i_width[1], padding_size);
        }
        src += stride;
        dst += stride;
    }
}

/* ---------------------------------------------------------------------------
 * padding_size - padding size for left and right edges
 */
void davs2_frame_copy_lcurow(davs2_t *h, davs2_frame_t *p_dst, davs2_frame_t *p_src, int i_lcu_y, int pix_offset, int padding_size)
{
    int pix_y = (i_lcu_y << h->i_lcu_level) + pix_offset;
    int lcu_h = DAVS2_MIN(h->i_height, ((i_lcu_y + 1) << h->i_lcu_level)) - pix_y;
    int y, len, stride;
    pel_t *src, *dst;

    /* Y */
    stride = p_src->i_stride[0];
    src    = p_src->planes[0] + pix_y * stride;
    dst    = p_dst->planes[0] + pix_y * stride;
    len    = p_src->i_width[0] * sizeof(pel_t);
    for (y = 0; y < lcu_h; y++) {
        gf_davs2.fast_memcpy(dst, src, len);
        if (padding_size) {
            pad_line_pixel(dst, p_dst->i_width[0], padding_size);
        }
        src += stride;
        dst += stride;
    }

    pix_y = (i_lcu_y << (h->i_lcu_level - 1)) + pix_offset;
    lcu_h = DAVS2_MIN(h->i_height >> 1, ((i_lcu_y + 1) << (h->i_lcu_level - 1))) - pix_y;

    /* U */
    stride = p_src->i_stride[1];
    src    = p_src->planes[1] + pix_y * stride;
    dst    = p_dst->planes[1] + pix_y * stride;
    len    = p_src->i_width[1] * sizeof(pel_t);
    for (y = 0; y < lcu_h; y++) {
        gf_davs2.fast_memcpy(dst, src, len);
        if (padding_size) {
            pad_line_pixel(dst, p_dst->i_width[1], padding_size);
        }
        src += stride;
        dst += stride;
    }

    /* V */
    stride = p_src->i_stride[2];
    src    = p_src->planes[2] + pix_y * stride;
    dst    = p_dst->planes[2] + pix_y * stride;
    len    = p_src->i_width[2] * sizeof(pel_t);
    for (y = 0; y < lcu_h; y++) {
        gf_davs2.fast_memcpy(dst, src, len);
        if (padding_size) {
            pad_line_pixel(dst, p_dst->i_width[2], padding_size);
        }
        src += stride;
        dst += stride;
    }
}
