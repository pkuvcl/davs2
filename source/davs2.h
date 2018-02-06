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

#ifdef __cplusplus
extern "C" {    // only need to export C interface if used by C++ source code
#endif
    
#if !defined(_STDINT_H) && !defined(_STDINT_H_) && !defined(_STDINT_H_INCLUDED) && !defined(_STDINT) &&\
    !defined(_SYS_STDINT_H_) && !defined(_INTTYPES_H) && !defined(_INTTYPES_H_) && !defined(_INTTYPES)
# ifdef _MSC_VER
#  pragma message("You must include stdint.h or inttypes.h before davs2.h")
# else
#  warning You must include stdint.h or inttypes.h before davs2.h
# endif
#endif

/* DAVS2 build version, means different API interface
 * (10 * VER_MAJOR + VER_MINOR) */
#define DAVS2_BUILD                10
/* DAVS2 API version, version 2 the non-caller version(truncated), version 3 save all the input bytes */
#define DAVS2_API_VERSION          1

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
#define AVS2DEC_PIC_I       0         /* picture-I */
#define AVS2DEC_PIC_P       1         /* picture-P */
#define AVS2DEC_PIC_B       2         /* picture-B */
#define AVS2DEC_PIC_G       3         /* picture-G */
#define AVS2DEC_PIC_F       4         /* picture-F */
#define AVS2DEC_PIC_S       5         /* picture-S */

/* ---------------------------------------------------------------------------
 * profile id */
#define PROFILE_MAIN_PIC    0x12      /* main picture profile */
#define PROFILE_MAIN        0x20      /* main         profile */
#define PROFILE_MAIN10      0x22      /* main 10bit   profile */

/* ---------------------------------------------------------------------------
 * log level
 */
enum davs2_log_level_e {
    AVS2_LOG_DEBUG   = 0,
    AVS2_LOG_INFO    = 1,
    AVS2_LOG_WARNING = 2,
    AVS2_LOG_ERROR   = 3,
    AVS2_LOG_FATAL   = 4,
    AVS2_LOG_MAX     = 5
};

/* ---------------------------------------------------------------------------
 * information of return value for decode/flush()
 */
enum davs2_ret_e {
    DAVS2_END         = -1,   /* Decoding ended: no more bit-stream to decode and no more frames to output */
    DAVS2_DEFAULT     = 0,    /* Decoding but no output */
    DAVS2_GOT_FRAME   = 1,    /* Decoding get frame */
    DAVS2_GOT_HEADER  = 2,    /* Decoding get sequence header */
    DAVS2_GOT_BOTH    = 3,    /* Decoding get sequence header and frame together */
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
    uint32_t        profile_id;       /* profile ID */
    uint32_t        level_id;         /* level   ID */
    uint32_t        progressive;      /* progressive sequence (0: interlace, 1: progressive) */
    uint32_t        horizontal_size;  /* image width */
    uint32_t        vertical_size;    /* image height */
    uint32_t        chroma_format;    /* chroma format(1: 4:2:0, 2: 4:2:2) */
    uint32_t        aspect_ratio;     /* 2: 4:3,  3: 16:9 */
    uint32_t        low_delay;        /* low delay */
    uint32_t        bitrate;          /* bitrate (bps) */
    uint32_t        internal_bitdepth;/* internal sample bit depth */
    uint32_t        output_bitdepth;  /* output sample bit depth */
    uint32_t        bytes_per_sample; /* bytes per sample */
    float           frame_rate;       /* frame rate */
    uint32_t        frame_rate_code;  /* frame rate code, mpeg12 [1...8] */
} davs2_seq_info_t;  

/* ---------------------------------------------------------------------------
 * packet of bitstream
 */
typedef struct davs2_packet_t {
    uint8_t        *data;             /* bitstream */
    int             len;              /* bytes of the bitstream */
    int             marker;           /* force a frame boundary after this packet */
    int64_t         pts;              /* presentation time stamp */
    int64_t         dts;              /* decoding time stamp */
} davs2_packet_t;

/* ---------------------------------------------------------------------------
 * decoded picture
 */
typedef struct davs2_picture_t {
    void           *magic;            /* must be the 1st member variable. do not change it */
    /* picture information */
    uint8_t        *planes[3];        /* picture planes */
    int             width[3];         /* picture width in pixels */
    int             lines[3];         /* picture height in pixels */
    /* stride等于一行图像占用的字节数, 一个颜色通道的所有像素点连续存放 */
    int             pic_order_count;  /* picture number */
    int             type;             /* picture type of the corresponding frame */
    int             QP;               /* QP of the corresponding picture */
    int64_t         pts;              /* presentation time stamp */
    int64_t         dts;              /* decoding time stamp */
    int             i_pic_planes;     /* number of planes */
    int             bytes_per_sample; /* number of bytes for each sample */
    int             pic_bit_depth;    /* number of bytes for each sample */
#if DAVS2_API_VERSION >= 2
    int             ret_type;         /* return type, davs2_ret_e */
#endif
    int             pic_decode_error; /* is there any decoding error of this frame? */
} davs2_picture_t;

#if DAVS2_API_VERSION < 2
/* ---------------------------------------------------------------------------
 * Function   : output one picture or a sequence header
 * Parameters :
 *       [in] : pic     - picture
 *       [in] : headset - sequence header
 *       [in] : errCode - possible errors while decoding @pic
 *       [in] : opaque  - user data
 */
typedef void(*davs2_output_func)(davs2_picture_t *pic, davs2_seq_info_t *headset, int errCode, void *opaque);

/* ---------------------------------------------------------------------------
 * parameters for create an AVS2 decoder
 */
typedef struct davs2_param_t {
    int               threads;        /* decoding threads: 0 for auto */
    davs2_output_func    output_f;       /* decoder picture output (MUST exist) */
    int               i_info_level;   /* 0: All; 1: Only warning and errors; 2: only errors */
    void             *opaque;         /* user data */
} davs2_param_t;
#else
/* ---------------------------------------------------------------------------
 * parameters for create an AVS2 decoder
 */
typedef struct davs2_param_t {
    int               threads;        /* decoding threads: 0 for auto */
    int               i_info_level;   /* 0: All; 1: Only warning and errors; 2: only errors */
    void             *opaque;         /* used for future extension */
} davs2_param_t;
#endif


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
 *       [in] : decoder - pointer to the AVS2 decoder handler
 *   [in/out] : packet  - pointer to struct davs2_packet_t
 * Return     : 0 for successful, otherwise -1
 * ---------------------------------------------------------------------------
 */
#if DAVS2_API_VERSION < 2
DAVS2_API int
davs2_decoder_decode(void *decoder, davs2_packet_t *packet);
#else
DAVS2_API int
davs2_decoder_decode(void *decoder, davs2_packet_t *packet, davs2_seq_info_t *headerset, davs2_picture_t *out_frame);
#endif

/**
 * ---------------------------------------------------------------------------
 * Function   : flush the decoder
 * Parameters :
 *       [in] : decoder - decoder handle
 * Return     : none
 * ---------------------------------------------------------------------------
 */
#if DAVS2_API_VERSION < 2
DAVS2_API void
davs2_decoder_flush(void *decoder);
#else
DAVS2_API int
davs2_decoder_flush(void *decoder, davs2_seq_info_t *headerset, davs2_picture_t *out_frame);
#endif

#if DAVS2_API_VERSION >= 2
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
#endif

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
