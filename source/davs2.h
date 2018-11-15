/*
 * davs2.h
 *
 * Description of this file:
 *    API functions definition of the davs2 library
 *
 * --------------------------------------------------------------------------
 *
 *    davs2 - video decoder of AVS2/IEEE1857.4 video coding standard
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

#ifndef DAVS2_DAVS2_H
#define DAVS2_DAVS2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {    // only need to export C interface if used by C++ source code
#endif

/* dAVS2 build version, means different API interface
 * (10 * VER_MAJOR + VER_MINOR) */
#define DAVS2_BUILD                16

/**
 * ===========================================================================
 * define DAVS2_API
 * ===========================================================================
 */
#ifdef DAVS2_EXPORTS
#  ifdef __GNUC__                     /* for Linux  */
#    if __GNUC__ >= 4
#      define DAVS2_API __attribute__((visibility("default")))
#    else
#      define DAVS2_API __attribute__((dllexport))
#    endif
#  else                               /* for windows */
#    define DAVS2_API __declspec(dllexport)
#  endif
#else
#  ifdef __GNUC__                     /* for Linux   */
#    define DAVS2_API
#  else                               /* for windows */
#    define DAVS2_API __declspec(dllimport)
#  endif
#endif


/**
 * ===========================================================================
 * const defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * picture type */
enum davs2_picture_type_e {
    DAVS2_PIC_I       = 0,         /* picture-I */
    DAVS2_PIC_P       = 1,         /* picture-P */
    DAVS2_PIC_B       = 2,         /* picture-B */
    DAVS2_PIC_G       = 3,         /* picture-G */
    DAVS2_PIC_F       = 4,         /* picture-F */
    DAVS2_PIC_S       = 5          /* picture-S */
};

/* ---------------------------------------------------------------------------
 * profile id */
enum davs2_profile_id_e {
    DAVS2_PROFILE_MAIN_PIC = 0x12,      /* AVS2 main picture profile */
    DAVS2_PROFILE_MAIN     = 0x20,      /* AVS2 main         profile */
    DAVS2_PROFILE_MAIN10   = 0x22       /* AVS2 main 10bit   profile */
};

/* ---------------------------------------------------------------------------
 * log level
 */
enum davs2_log_level_e {
    DAVS2_LOG_DEBUG   = 0,
    DAVS2_LOG_INFO    = 1,
    DAVS2_LOG_WARNING = 2,
    DAVS2_LOG_ERROR   = 3,
    DAVS2_LOG_MAX     = 4
};

/* ---------------------------------------------------------------------------
 * information of return value for decode/flush()
 */
enum davs2_ret_e {
    DAVS2_ERROR       = -1,   /* Decoding error occurs */
    DAVS2_DEFAULT     = 0,    /* Decoding but no output */
    DAVS2_GOT_FRAME   = 1,    /* Decoding get frame */
    DAVS2_GOT_HEADER  = 2,    /* Decoding get sequence header, always obtained before DAVS2_GOT_FRAME */
    DAVS2_END         = 3,    /* Decoding ended: no more bit-stream to decode and no more frames to output */
};

/**
 * ===========================================================================
 * interface struct type defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * information of sequence header
 */
typedef struct davs2_seq_info_t {
    uint32_t        profile_id;         /* profile ID, davs2_profile_id_e */
    uint32_t        level_id;           /* level   ID */
    uint32_t        progressive;        /* progressive sequence (0: interlace, 1: progressive) */
    uint32_t        width;              /* image width */
    uint32_t        height;             /* image height */
    uint32_t        chroma_format;      /* chroma format(1: 4:2:0, 2: 4:2:2) */
    uint32_t        aspect_ratio;       /* 2: 4:3,  3: 16:9 */
    uint32_t        low_delay;          /* low delay */
    uint32_t        bitrate;            /* bitrate (bps) */
    uint32_t        internal_bit_depth; /* internal sample bit depth */
    uint32_t        output_bit_depth;   /* output sample bit depth */
    uint32_t        bytes_per_sample;   /* bytes per sample */
    float           frame_rate;         /* frame rate */
    uint32_t        frame_rate_id;      /* frame rate code, mpeg12 [1...8] */
} davs2_seq_info_t;  

/* ---------------------------------------------------------------------------
 * packet of bitstream
 */
typedef struct davs2_packet_t {
    const uint8_t  *data;             /* bitstream */
    int             len;              /* bytes of the bitstream */
    int64_t         pts;              /* presentation time stamp */
    int64_t         dts;              /* decoding time stamp */
} davs2_packet_t;

/* ---------------------------------------------------------------------------
 * decoded picture
 */
typedef struct davs2_picture_t {
    void           *magic;            /* must be the 1st member variable (do not change it) */
    /* picture information */
    uint8_t        *planes[3];        /* picture planes */
    int             widths[3];        /* picture width in pixels */
    int             lines[3];         /* picture height in pixels */
    int             strides[3];       /* number of bytes in one line are stored continuously in memory */
    int             pic_order_count;  /* picture number */
    int             type;             /* picture type of the corresponding frame */
    int             qp;               /* QP of the corresponding picture */
    int64_t         pts;              /* presentation time stamp */
    int64_t         dts;              /* decoding time stamp */
    int             num_planes;       /* number of planes */
    int             bytes_per_sample; /* number of bytes for each sample */
    int             bit_depth;        /* number of bytes for each sample */
    int             b_decode_error;   /* is there any decoding error of this frame? */
    void           *dec_frame;        /* pointer to decoding frame in DPB (do not change it) */
} davs2_picture_t;

/* ---------------------------------------------------------------------------
 * parameters for create an AVS2 decoder
 */
typedef struct davs2_param_t {
    int               threads;        /* decoding threads: 0 for auto */
    int               info_level;     /* only output information which is no less then this level (davs2_log_level_e).
                                         0: All; 1: no debug info; 2: only warning and errors; 3: only errors */
    void             *opaque;         /* user data */
    /* additional parameters for version >= 16 */
    int               disable_avx;    /* 1: disable; 0: default (autodetect) */
} davs2_param_t;

/**
 * ===========================================================================
 * interface function declares (DAVS2 library APIs for AVS2 video decoder)
 * ===========================================================================
 */

/**
 * ---------------------------------------------------------------------------
 * Function   : open an AVS2 decoder
 * Parameters :
 *   [in/out] : param - pointer to struct davs2_param_t
 * Return     : handle of the decoder, zero for failure
 * ---------------------------------------------------------------------------
 */
DAVS2_API void *
davs2_decoder_open(davs2_param_t *param);

/**
 * ---------------------------------------------------------------------------
 * Function   : decode one frame
 * Parameters :
 *       [in] : decoder   - pointer to the AVS2 decoder handler
 *       [in] : packet    - pointer to struct davs2_packet_t
 * Return     : see definition of davs2_ret_e
 * ---------------------------------------------------------------------------
 */
DAVS2_API int
davs2_decoder_send_packet(void *decoder, davs2_packet_t *packet);

/**
 * ---------------------------------------------------------------------------
 * Function   : decode one frame
 * Parameters :
 *       [in] : decoder   - pointer to the AVS2 decoder handler
 *      [out] : headerset - pointer to output common frame information (would always appear before frame output)
 *      [out] : out_frame - pointer to output frame information
 * Return     : see definition of davs2_ret_e
 * ---------------------------------------------------------------------------
 */
DAVS2_API int
davs2_decoder_recv_frame(void *decoder, davs2_seq_info_t *headerset, davs2_picture_t *out_frame);

/**
 * ---------------------------------------------------------------------------
 * Function   : flush the decoder
 * Parameters :
 *       [in] : decoder   - decoder handle
 *      [out] : headerset - pointer to output common frame information (would always appear before frame output)
 *      [out] : out_frame - pointer to output frame information
 * Return     : see definition of davs2_ret_e
 * ---------------------------------------------------------------------------
 */
DAVS2_API int
davs2_decoder_flush(void *decoder, davs2_seq_info_t *headerset, davs2_picture_t *out_frame);

/**
 * ---------------------------------------------------------------------------
 * Function   : release one output frame
 * Parameters :
 *       [in] : decoder   - decoder handle
 *            : out_frame - frame to recycle
 * Return     : none
 * ---------------------------------------------------------------------------
 */
DAVS2_API void
davs2_decoder_frame_unref(void *decoder, davs2_picture_t *out_frame);

/**
 * ---------------------------------------------------------------------------
 * Function   : close the AVS2 decoder
 * Parameters :
 *       [in] : decoder - decoder handle
 * Return     : none
 * ---------------------------------------------------------------------------
 */
DAVS2_API void
davs2_decoder_close(void *decoder);

#ifdef __cplusplus
}
#endif

#endif // DAVS2_DAVS2_H
