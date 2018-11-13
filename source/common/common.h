/*
 * common.h
 *
 * Description of this file:
 *    misc common functionsdefinition of the davs2 library
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

#ifndef DAVS2_COMMON_H
#define DAVS2_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif
/**
 * ===========================================================================
 * common include files
 * ===========================================================================
 */

#include "defines.h"
#include "osdep.h"
#include "davs2.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#if (ARCH_X86 || ARCH_X86_64)
#include <xmmintrin.h>
#endif

/**
 * ===========================================================================
 * basic type defines
 * ===========================================================================
 */

#if HIGH_BIT_DEPTH
typedef uint16_t                pel_t;      /* type for pixel value */
typedef uint64_t                pel4_t;     /* type for 4-pixels value */
typedef int32_t                 itr_t;      /* intra prediction temp */
#else
typedef uint8_t                 pel_t;      /* type for pixel value */
typedef uint32_t                pel4_t;     /* type for 4-pixels value */
typedef int16_t                 itr_t;      /* intra prediction temp */
#endif

typedef int16_t                 coeff_t;    /* type for transform coefficient */
typedef int16_t                 mct_t;       /* motion compensation temp*/
typedef uint8_t                 bool_t;     /* type for flag */

typedef struct cu_t             cu_t;
typedef struct davs2_log_t      davs2_log_t;
typedef struct davs2_t          davs2_t;
typedef struct davs2_mgr_t      davs2_mgr_t;
typedef struct davs2_outpic_t   davs2_outpic_t;


/**
 * ===========================================================================
 * macros
 * ===========================================================================
 */
#define IS_HOR_PU_PART(mode)          (((1 << (mode)) & MASK_HOR_PU_MODES) != 0)
#define IS_VER_PU_PART(mode)          (((1 << (mode)) & MASK_VER_PU_MODES) != 0)
#define IS_INTRA_MODE(mode)           (((1 << (mode)) & MASK_INTRA_MODES ) != 0)
#define IS_INTER_MODE(mode)           (((1 << (mode)) & MASK_INTER_MODES ) != 0)
#define IS_NOSKIP_INTER_MODE(mode)    (((1 << (mode)) & MASK_INTER_NOSKIP) != 0)
#define IS_SKIP_MODE(mode)            ((mode) == PRED_SKIP)

#define IS_INTRA(cu)             IS_INTRA_MODE((cu)->i_cu_type)
#define IS_INTER(cu)             IS_INTER_MODE((cu)->i_cu_type)
#define IS_NOSKIP_INTER(cu)      IS_NOSKIP_INTER_MODE((cu)->i_cu_type)
#define IS_SKIP(cu)              IS_SKIP_MODE((cu)->i_cu_type)

static ALWAYS_INLINE int DAVS2_MAX(int a, int b)
{
    return ((a) > (b) ? (a) : (b));
}
static ALWAYS_INLINE int DAVS2_MIN(int a, int b)
{
    return ((a) < (b) ? (a) : (b));
}
#define DAVS2_ABS(a)             ((a) < 0 ? (-(a)) : (a))
#define DAVS2_CLIP1(a)           (pel_t)((a) > max_pel_value ? max_pel_value : ((a) < 0 ? 0 : (a)))

static ALWAYS_INLINE int DAVS2_CLIP3(int L, int H, int v)
{
    return (((v) < (L)) ? (L) : (((v) > (H)) ? (H) : (v)));
}

#define DAVS2_SWAP(x,y)          { (y)=(y)^(x); (x)=(y)^(x); (y)=(x)^(y); }
#define DAVS2_ALIGN(x, a)        (((x) + ((a) - 1)) & (~((a) - 1)))

#define LCU_STRIDE               (MAX_CU_SIZE)
#define LCU_BUF_SIZE             (LCU_STRIDE * MAX_CU_SIZE)          /* size of LCU buffer size */

/* ---------------------------------------------------------------------------
 * multi line macros
 */
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#define MULTI_LINE_MACRO_BEGIN  do {
#define MULTI_LINE_MACRO_END \
    __pragma(warning(push))\
    __pragma(warning(disable:4127))\
    } while (0)\
    __pragma(warning(pop))
#else
#define MULTI_LINE_MACRO_BEGIN   {
#define MULTI_LINE_MACRO_END     }
#endif


/* ---------------------------------------------------------------------------
 * memory malloc
 */
#define CHECKED_MALLOC(var, type, size) \
    MULTI_LINE_MACRO_BEGIN\
    (var) = (type)davs2_malloc(size);\
    if ((var) == NULL) {\
        goto fail;\
        }\
    MULTI_LINE_MACRO_END

#define CHECKED_MALLOCZERO(var, type, size) \
    MULTI_LINE_MACRO_BEGIN\
    CHECKED_MALLOC(var, type, size);\
    memset(var, 0, size);\
    MULTI_LINE_MACRO_END


/**
 * ===========================================================================
 * enum defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * task status */
enum task_status_t {
    TASK_FREE    = 0,           /* task is free, could be used */
    TASK_BUSY    = 1            /* task busy */
};

/* ---------------------------------------------------------------------------
 * coding types */
enum coding_type_e {
    FRAME_CODING = 0,           /* frame coding */
    FIELD_CODING = 1            /* field coding */
};


/* ---------------------------------------------------------------------------
 * picture struct */
enum pic_struct_e {
    FIELD = 0,                  /* field picture struct */
    FRAME = 1                   /* frame picture struct */
};


/* ---------------------------------------------------------------------------
 * slice type */
enum {
    AVS2_I_SLICE = 0,           /* slice type: I frame */
    AVS2_P_SLICE = 1,           /* slice type: P frame */
    AVS2_B_SLICE = 2,           /* slice type: B frame */
    AVS2_G_SLICE = 3,           /* AVSS2 type: G frame, should be output (as I frame) */
    AVS2_F_SLICE = 4,           /* slice type: F frame */
    AVS2_S_SLICE = 5,           /* AVSS2 type: S frame */
    AVS2_GB_SLICE = 6,          /* AVSS2 type: GB frame, should not be output */
};


/* ---------------------------------------------------------------------------
 * start codes */
enum start_code_e {
    SC_SEQUENCE_HEADER = 0xB0,  /* sequence header start code */
    SC_SEQUENCE_END    = 0xB1,  /* sequence end    start code */
    SC_USER_DATA       = 0xB2,  /* user data       start code */
    SC_INTRA_PICTURE   = 0xB3,  /* intra picture   start code */
    SC_EXTENSION       = 0xB5,  /* extension       start code */
    SC_INTER_PICTURE   = 0xB6,  /* inter picture   start code */
    SC_VIDEO_EDIT_CODE = 0xB7,  /* video edit      start code */
    SC_SLICE_CODE_MIN  = 0x00,  /* min slice       start code */
    SC_SLICE_CODE_MAX  = 0x8F   /* max slice       start code */
};


/* ---------------------------------------------------------------------------
 * all prediction modes (n = N/2) */
enum cu_pred_mode_e {
    /* all inter modes: 8                                           */
    PRED_SKIP  = 0,      /*  skip/direct           block: 1  */
    PRED_2Nx2N = 1,      /*  2N x 2N               block: 1  */
    PRED_2NxN  = 2,      /*  2N x  N               block: 2  */
    PRED_Nx2N  = 3,      /*   N x 2N               block: 2  */
    PRED_2NxnU = 4,      /*  2N x  n  +  2N x 3n   block: 2  */
    PRED_2NxnD = 5,      /*  2N x 3n  +  2N x  n   block: 2  */
    PRED_nLx2N = 6,      /*   n x 2N  +  3n x 2N   block: 2  */
    PRED_nRx2N = 7,      /*  3n x 2N  +   n x 2N   block: 2  */
    /* all intra modes: 4                                           */
    PRED_I_2Nx2N = 8,      /*  2N x 2N               block: 1  */
    PRED_I_NxN   = 9,      /*   N x  N               block: 4  */
    PRED_I_2Nxn  = 10,     /*  2N x  n  (32x8, 16x4) block: 4  */
    PRED_I_nx2N  = 11,     /*   n x 2N  (8x32, 4x16) block: 4  */
    /* mode numbers                                                 */
    MAX_PRED_MODES  = 12,     /* total 12 pred modes, include:    */
    MAX_INTER_MODES = 8,      /*       8 inter modes              */
    MAX_INTRA_MODES = 4,      /*       4 intra modes              */
    /* masks                                                        */
    MASK_HOR_TU_MODES = 0x0430, /* mask for horizontal TU partition */
    MASK_VER_TU_MODES = 0x08C0, /* mask for vertical   TU partition */
    MASK_HOR_PU_MODES = 0x0434, /* mask for horizontal PU partition */
    MASK_VER_PU_MODES = 0x08C8, /* mask for vertical   PU partition */
    MASK_INTER_MODES  = 0x00FF, /* mask for inter modes             */
    MASK_INTER_NOSKIP = 0x00FE, /* mask for inter modes except skip */
    MASK_INTRA_MODES  = 0x0F00  /* mask for intra modes             */
};


/* ---------------------------------------------------------------------------
 * splitting type of transform unit */
enum tu_split_type_e {
    TU_SPLIT_INVALID  = -1,     /*      invalid split type          */
    TU_SPLIT_NON      = 0,      /*          not split               */
    TU_SPLIT_HOR      = 1,      /* horizontally split into 4 blocks */
    TU_SPLIT_VER      = 2,      /*   vertically split into 4 blocks */
    TU_SPLIT_CROSS    = 3,      /*    cross     split into 4 blocks */
    NUM_TU_SPLIT_TYPE = 4       /* number of transform split types  */
};


/* ---------------------------------------------------------------------------
 * pu partition */
enum PU_PART {
    /* square */
    PART_4x4, PART_8x8, PART_16x16, PART_32x32, PART_64x64,
    /* rectangular */
    PART_8x4, PART_4x8,
    PART_16x8, PART_8x16,
    PART_32x16, PART_16x32,
    PART_64x32, PART_32x64,
    /* asymmetrical (0.75, 0.25) */
    PART_16x12, PART_12x16, PART_16x4, PART_4x16,
    PART_32x24, PART_24x32, PART_32x8, PART_8x32,
    PART_64x48, PART_48x64, PART_64x16, PART_16x64,
    /* max number of partitions */
    MAX_PART_NUM
};


/* ---------------------------------------------------------------------------
 * DCT pattern */
enum dct_pattern_e {
    DCT_DEAULT,      /* default */
    DCT_HALF,        /* 方形块仅左上角1/2宽高（面积1/4）， 非方形块为左上角1/2（面积1/2） */
    DCT_QUAD,        /* 方形块仅左上角1/4宽高（面积1/16），非方形块为左上角1/4（面积1/4） */
    /* max number of DCT pattern */
    DCT_PATTERN_NUM
};


/* ---------------------------------------------------------------------------
 * context mode */
enum context_mode_e {
    INTRA_PRED_VER     = 0,     /* intra vertical predication */
    INTRA_PRED_HOR     = 1,     /* intra horizontal predication */
    INTRA_PRED_DC_DIAG = 2      /* intra DC predication */
};


/* ---------------------------------------------------------------------------
 * image component index */
enum img_component_index_e {
    IMG_Y = 0,         /* image component: Y */
    IMG_U = 1,         /* image component: Cb */
    IMG_V = 2,         /* image component: Cr */
    IMG_COMPONENTS = 3          /* number of image components */
};


/* ---------------------------------------------------------------------------
 * predicate direction for inter frame */
enum inter_pred_direction_e {
    INVALID_REF = -1,           /* invalid */
    B_BWD = 0,                  /* backward */
    B_FWD = 1                   /* forward */
};


/* ---------------------------------------------------------------------------
 * neighboring position used in inter coding (MVP) or intra prediction */
enum neighbor_block_pos_e {
    BLK_TOPLEFT     = 0,        /* D: top-left   block: (x     - 1, y     - 1) */
    BLK_TOP         = 1,        /* B: top        block: (x        , y     - 1) */
    BLK_LEFT        = 2,        /* A: left       block: (x     - 1, y        ) */
    BLK_TOPRIGHT    = 3,        /* C: top-right  block: (x + W    , y     - 1) */
    BLK_TOP2        = 4,        /* G: top        block: (x + W - 1, y     - 1) */
    BLK_LEFT2       = 5,        /* F: left       block: (x     - 1, y + H - 1) */
    BLK_COLLOCATED  = 6,         /* Col: mode of temporal neighbor */
    NUM_INTER_NEIGHBOR = BLK_COLLOCATED + 1
};


/* ---------------------------------------------------------------------------
 * neighboring position used in inter coding (MVP) or intra prediction */
enum direct_skip_mode_e {
    DS_NONE  = 0,       /* no spatial direct/skip mode */

    /* spatial direct/skip mode for B frame */
    DS_B_BID = 1,        /* skip/direct mode: bi-direction */
    DS_B_BWD = 2,        /*                 : backward direction */
    DS_B_SYM = 3,        /*                 : symmetrical direction */
    DS_B_FWD = 4,        /*                 : forward direction */

    /* spatial direct/skip mode for F frame */
    DS_DUAL_1ST   = 1,        /* skip/direct mode: dual 1st */
    DS_DUAL_2ND   = 2,        /*                 : dual 2nd */
    DS_SINGLE_1ST = 3,        /*                 : single 1st */
    DS_SINGLE_2ND = 4,        /*                 : single 2st */

    /* max number */
    DS_MAX_NUM    = 5         /* max spatial direct/skip mode number of B or F frames */
};


/* ---------------------------------------------------------------------------
 */
enum intra_avail_e {
    MD_I_LEFT      = 0,
    MD_I_TOP       = 1,
    MD_I_LEFT_DOWN = 2,
    MD_I_TOP_RIGHT = 3,
    MD_I_TOP_LEFT  = 4,
    MD_I_NUM       = 5,
#define IS_NEIGHBOR_AVAIL(i_avai, md)    ((i_avai) & (1 << (md)))
};


/* ---------------------------------------------------------------------------
 * sao modes */
enum sao_mode_e {
    SAO_MODE_OFF   = 0,         /* sao mode: off */
    SAO_MODE_MERGE = 1,         /* sao mode: merge */
    SAO_MODE_NEW   = 2          /* sao mode: new */
};


/* ---------------------------------------------------------------------------
* sao mode merge types */
enum sao_mode_merge_type_e {
    SAO_MERGE_LEFT      = 0,    /* sao merge type: left */
    SAO_MERGE_ABOVE     = 1,    /* sao merge type: above */
    NUM_SAO_MERGE_TYPES = 2     /* number of sao merge types */
};


/* ---------------------------------------------------------------------------
* sao mode types */
enum sao_mode_type_e {
    SAO_TYPE_EO_0   = 0,        /* sao mode type: EO - 0   */
    SAO_TYPE_EO_90  = 1,        /* sao mode type: EO - 90  */
    SAO_TYPE_EO_135 = 2,        /* sao mode type: EO - 135 */
    SAO_TYPE_EO_45  = 3,        /* sao mode type: EO - 45  */
    SAO_TYPE_BO     = 4         /* sao mode type: BO       */
};


/* ---------------------------------------------------------------------------
 * sao EO classes
 * the assignments depended on how you implement the edgeType calculation */
enum sao_EO_classes_e {
    SAO_CLASS_EO_FULL_VALLEY = 0,
    SAO_CLASS_EO_HALF_VALLEY = 1,
    SAO_CLASS_EO_PLAIN       = 2,
    SAO_CLASS_EO_HALF_PEAK   = 3,
    SAO_CLASS_EO_FULL_PEAK   = 4,
    SAO_CLASS_BO             = 5,
    NUM_SAO_OFFSET           = 6
};


/* ---------------------------------------------------------------------------
 * contexts for syntax elements */
#define NUM_CUTYPE_CTX          6
#define NUM_SPLIT_CTX           3     // CU depth
#define NUM_INTRA_PU_TYPE_CTX   1
/* 预测 */
#define NUM_MVD_CTX             3
#define NUM_REF_NO_CTX          3
#define NUM_DELTA_QP_CTX        4
#define NUM_INTER_DIR_CTX       15
#define NUM_INTER_DIR_DHP_CTX   3
#define NUM_DMH_MODE_CTX        12
#define NUM_AMP_CTX             2
#define NUM_C_INTRA_MODE_CTX    3
#define NUM_CTP_CTX             9
#define NUM_INTRA_MODE_CTX      7
#define NUM_TU_SPLIT_CTX        3
#define WPM_NUM                 3
#define NUM_DIR_SKIP_CTX        4     /* B Skip mode, F Skip mode */
/* 变换系数 */
#define NUM_BLOCK_TYPES         3
#define NUM_MAP_CTX             11
#define NUM_LAST_CG_CTX_LUMA    6
#define NUM_LAST_CG_CTX_CHROMA  6
#define NUM_SIGCG_CTX_LUMA      2
#define NUM_SIGCG_CTX_CHROMA    1
#define NUM_LAST_POS_CTX_LUMA   48
#define NUM_LAST_POS_CTX_CHROMA 12
#define NUM_COEFF_LEVEL_CTX     40
#define NUM_LAST_CG_CTX         (NUM_LAST_CG_CTX_LUMA+NUM_LAST_CG_CTX_CHROMA)
#define NUM_SIGCG_CTX           (NUM_SIGCG_CTX_LUMA+NUM_SIGCG_CTX_CHROMA)
#define NUM_LAST_POS_CTX        (NUM_LAST_POS_CTX_LUMA+NUM_LAST_POS_CTX_CHROMA)
/* 后处理 */
#define NUM_SAO_MERGE_FLAG_CTX  3
#define NUM_SAO_MODE_CTX        1
#define NUM_SAO_OFFSET_CTX      2
#define NUM_INTER_DIR_MIN_CTX   2
#define NUM_ALF_LCU_CTX         4     /* adaptive loop filter */

/**
 * ===========================================================================
 * struct type defines
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * node
 */
typedef struct node_t   node_t;
struct node_t {
    node_t      *next;                /* pointer to next node */
};


/* ---------------------------------------------------------------------------
 * xlist_t
 */
typedef struct xlist_t {
    node_t              *p_list_head;     /* pointer to head of node list */
    node_t              *p_list_tail;     /* pointer to tail of node list */
    davs2_thread_cond_t  list_cond;       /* list condition variable */
    davs2_thread_mutex_t list_mutex;      /* list mutex lock */
    int                  i_node_num;      /* node number in the list */
} xlist_t;


#if defined(_MSC_VER) || defined(__ICL)
#pragma warning(disable: 4201)        // non-standard extension used (nameless struct/union)
#endif

/* ---------------------------------------------------------------------------
 * syntax context type */
typedef union context_t {
    struct {
        unsigned    cycno   : 2;      // 2  bits
        unsigned    MPS     : 1;      // 1  bit
        unsigned    LG_PMPS : 11;     // 11 bits
    };
    uint16_t        v;
} context_t;


/* ---------------------------------------------------------------------------
 * syntax context management */
typedef struct context_set_t {
    /* CU级 */
    context_t cu_type_contexts         [NUM_CUTYPE_CTX];
    context_t intra_pu_type_contexts   [NUM_INTRA_PU_TYPE_CTX];
    context_t cu_split_flag            [NUM_SPLIT_CTX];
    context_t transform_split_flag     [NUM_TU_SPLIT_CTX];
    context_t shape_of_partition_index [NUM_AMP_CTX];
    context_t pu_reference_index       [NUM_REF_NO_CTX];
    context_t cbp_contexts             [NUM_CTP_CTX];
    context_t mvd_contexts          [2][NUM_MVD_CTX];
    /* 帧间预测 */
    context_t pu_type_index            [NUM_INTER_DIR_CTX];    // b_pu_type_index[15] = f_pu_type_index[3] + dir_multi_hypothesis_mode[12]
    context_t b_pu_type_min_index      [NUM_INTER_DIR_MIN_CTX];
    context_t cu_subtype_index         [NUM_DIR_SKIP_CTX];  // B_Skip/B_Direct, F_Skip/F_Direct 公用
    context_t weighted_skip_mode       [WPM_NUM];
    context_t delta_qp_contexts        [NUM_DELTA_QP_CTX];
    /* 帧内预测 */
    context_t intra_luma_pred_mode     [NUM_INTRA_MODE_CTX];
    context_t intra_chroma_pred_mode   [NUM_C_INTRA_MODE_CTX];
    /* 变换系数 */
    context_t coeff_run             [2][NUM_BLOCK_TYPES][NUM_MAP_CTX];
    context_t coeff_level              [NUM_COEFF_LEVEL_CTX];
    context_t last_cg_contexts         [NUM_LAST_CG_CTX];
    context_t sig_cg_contexts          [NUM_SIGCG_CTX];
    context_t last_coeff_pos           [NUM_LAST_POS_CTX];
    /* 后处理 */
    context_t sao_mergeflag_context    [NUM_SAO_MERGE_FLAG_CTX];
    context_t sao_mode_context         [NUM_SAO_MODE_CTX];
    context_t sao_offset_context       [NUM_SAO_OFFSET_CTX];
    context_t alf_lcu_enable_scmodel   [NUM_ALF_LCU_CTX * 3];
} context_set_t;


/* ---------------------------------------------------------------------------
 * bitstream */
typedef struct davs2_bs_t {
    uint8_t    *p_stream;             /* pointer to the code-buffer */
    int         i_stream;             /* over code-buffer length, byte-oriented */
    int         i_bit_pos;            /* actual position in the code-buffer, bit-oriented */
#if !ARCH_X86_64
    int         reserved;             /* reserved */
#endif
} davs2_bs_t;


/* ---------------------------------------------------------------------------
 * SAO parameters for component block */
typedef struct sao_param_t {
    int         modeIdc;              // NEW, MERGE, OFF
    int         typeIdc;              // NEW: EO_0, EO_90, EO_135, EO_45, BO. MERGE: left, above
    int         startBand;            //BO: starting band index
    int         startBand2;
    int         offset[MAX_NUM_SAO_CLASSES];
} sao_param_t;


/* ---------------------------------------------------------------------------
 * SAO parameters for LCU */
typedef struct sao_t {
    sao_param_t planes[IMG_COMPONENTS];
} sao_t;


/* ---------------------------------------------------------------------------
 * ALF parameters */
typedef struct alf_param_t {
    int         num_coeff;
    int         filters_per_group;
    int         componentID;
    int         filterPattern[ALF_NUM_VARS];
    int         coeffmulti[ALF_NUM_VARS][ALF_MAX_NUM_COEF];  // 亮度分量16套，色度分量各1套
} alf_param_t;


typedef struct alf_var_t {
    alf_param_t   img_param[IMG_COMPONENTS];
    int           filterCoeffSym[ALF_NUM_VARS][ALF_MAX_NUM_COEF];
    int           tab_region_coeff_idx[ALF_NUM_VARS];   /* coefficient look-up table for 16 regions */
    uint8_t      *tab_lcu_region;                       /* region index look-up table for LCUs */
} alf_var_t;

/* ---------------------------------------------------------------------------
 * reference index */
typedef union ref_idx_t {
    struct {                          // nameless struct
        int8_t  r[2];                 // ref 1st and 2nd, 4 bit (sign integer)
    };
    uint16_t    v;                    // v = ((r2 << 8) | (r1 & 0xFF)), 16-bit
} ref_idx_t;

/* ---------------------------------------------------------------------------
 * motion vector */
typedef union mv_t {
    struct {                          // nameless struct
        int16_t x;                    // x, low  16-bit
        int16_t y;                    // y, high 16-bit
    };
    uint32_t    v;                    // v = ((y << 16) | (x & 0xFFFF)), 32-bit
} mv_t;


/* ---------------------------------------------------------------------------
 * coding block
 */
typedef union cb_t {
    struct {                          /* nameless struct */
        int8_t  x;                    /* start position (x, in pixel) within current CU */
        int8_t  y;                    /* start position (y, in pixel) within current CU */
        int8_t  w;                    /* block width  (in pixel) */
        int8_t  h;                    /* block height (in pixel) */
    };
    uint32_t    v;                    /* used for fast operation for all components */
} cb_t;


/* ---------------------------------------------------------------------------
 * motion vector */
typedef struct neighbor_inter_t {
    mv_t        mv[2];                /* motion vectors */
    int8_t      is_available;         /* is block available */
    int8_t      i_dir_pred;           /* predict direction */
    ref_idx_t   ref_idx;              /* reference indexes of 1st and 2nd frame */
} neighbor_inter_t;


/* ---------------------------------------------------------------------------
 */
typedef struct aec_t {
    ALIGN32(uint8_t *p_buffer);
    uint64_t    i_byte_buf;
    int         i_byte_pos;
    int         i_bytes;
    int8_t      i_bits_to_go;
    bool_t      b_bit_error;          /* bit error in stream */
    bool_t      b_val_bound;
    bool_t      b_val_domain;         // is value in R domain 1 is R domain 0 is LG domain
    uint32_t    i_s1;
    uint32_t    i_t1;
    uint32_t    i_value_s;
    uint32_t    i_value_t;

    /* context */
    context_set_t   syn_ctx;              // pointer to struct of context models
#if AVS2_TRACE
    /* ---------------------------------------------------------------------------
     * syntax element */
#define         TRACESTRING_SIZE 128  // size of trace string
    char        tracestring[TRACESTRING_SIZE]; // trace string
#endif // AVS2_TRACE
} aec_t;


/* ---------------------------------------------------------------------------
 * reference picture set (RPS) */
typedef struct rps_t {
    int     ref_pic[AVS2_MAX_REFS]; /* delta COI of ref pic */
    int     remove_pic[8];          /* delta COI of removed pic */
    int     num_of_ref;             /* number of reference picture */
    int     num_to_remove;          /* number of removed picture */
    int     refered_by_others;      /* referenced by others */
    int     reserved;               /* reserved 4 bytes */
} rps_t;


/* ---------------------------------------------------------------------------
 * sequence set information */
typedef struct davs2_seq_t {
    int     valid_flag;               /* is this sequence header valid ? */
    davs2_seq_info_t head;         /* sequence header information (output) */
    int     sample_precision;         /* sample precision */
    int     encoding_precision;       /* encoding precision */
    int     bit_rate_lower;           /* bitrate (lower) */
    int     bit_rate_upper;           /* bitrate (upper) */
    int     i_enc_width;              /* sequence encoding width */
    int     i_enc_height;             /* sequence encoding height */
    int     log2_lcu_size;            /* largest coding block size */
    bool_t  b_field_coding;           /* field coded sequence? */
    bool_t  b_temporal_id_exist;      /* temporal id exist flag */
    bool_t  enable_weighted_quant;    /* weight quant enable */
    bool_t  enable_background_picture;/* background picture enabled? */
    bool_t  enable_mhp_skip;          /* mhpskip enabled? */
    bool_t  enable_dhp;               /* dhp enabled? */
    bool_t  enable_wsm;               /* wsm enabled? */
    bool_t  enable_amp;               /* AMP(asymmetric motion partitions) enabled? */
    bool_t  enable_nsqt;              /* use NSQT? */
    bool_t  enable_sdip;              /* use SDIP? */
    bool_t  enable_2nd_transform;     /* secondary transform enabled? */
    bool_t  enable_sao;               /* SAO enabled? */
    bool_t  enable_alf;               /* ALF enabled? */
    bool_t  enable_pmvr;              /* PMVR enabled? */
    bool_t  cross_loop_filter_flag;   /* cross loop filter flag */
    int     picture_reorder_delay;    /* picture reorder delay */
    int     num_of_rps;               /* rps set number */
    rps_t   seq_rps[AVS2_GOP_NUM];    /* RPS at sequence level */
    int16_t seq_wq_matrix[2][64];     /* sequence base weighting quantization matrix */
} davs2_seq_t;


/* ---------------------------------------------------------------------------
 * davs2_frame_t */
typedef struct davs2_frame_t {
    /* properties */
    int64_t     i_pts;                /* user pts (presentation time stamp) */
    int64_t     i_dts;                /* user dts (decoding time stamp) */

    int         i_type;               /* frame type */
    int         i_qp;

    int         i_chroma_format;      /* chroma format    (for function davs2_write_a_frame) */
    int         i_output_bit_depth;   /* output bit depth (for function davs2_write_a_frame) */
    int         i_sample_bit_depth;   /* sample bit depth (for function davs2_write_a_frame) */
    int         frm_decode_error;     /* is there any decoding error in this frame? */

    int         dist_refs[AVS2_MAX_REFS];  /* distance of reference frames, used for MV scaling */
    int         dist_scale_refs[AVS2_MAX_REFS];  /* = (MULTI / dist_refs) */
    int         i_poc;                /* POC (picture order count), used for MV scaling */
    int         i_coi;                /* COI (coding order index) */
    int         b_refered_by_others;  /* referenced by others */

    /* planes */
    int         i_plane;              /* number of planes */
    int         i_width[3];           /* width  for Y/U/V */
    int         i_lines[3];           /* height for Y/U/V */
    int         i_stride[3];          /* stride for Y/U/V */

    /* parallel */
    uint32_t    i_ref_count;          /* the reference count, DO NOT move its position in this struct */

    int         i_disposable;         /* what to do with the frame when the reference count is decreased to 0? */
    /* 0: do nothing, 1: clean the frame, 2: free the frame */
    /* frames with 'i_disposable' greater than 0 should NOT be referenced. */

    int          is_self_malloc;      /* is the buffer allocated by itself */
    volatile int i_decoded_line;      /* latest lcu line that finished reconstruction */
    volatile int i_parsed_lcu_xy;     /* parsed number of LCU */
    int          i_conds;             /* number conds */
    davs2_thread_cond_t   cond_aec;   /* signal of AEC decoding */
    davs2_thread_cond_t  *conds_lcu_row;  /* [LCU lines] */
    int *num_decoded_lcu_in_row;      /* number of LCUs decoded in a row */ 
    davs2_thread_mutex_t mutex_frm;   /* the mutex */
    davs2_thread_mutex_t mutex_recon; /* mutex of reconstruction threads */

    /* buffers */
    pel_t      *planes[3];            /* pointers to Y/U/V data buffer */
    int8_t     *refbuf;               /* pointers to reference index buffer */
    mv_t       *mvbuf;                /* pointers to motion vector buffer*/
} davs2_frame_t;


/* ---------------------------------------------------------------------------
 * weighting quantization */
typedef struct weighted_quant_t {
    int         pic_wq_data_index;
    int         wq_param;
    int         wq_model;
    int16_t     quant_param_undetail[6];
    int16_t     quant_param_detail[6];
    int16_t     cur_wq_matrix[4][64]; // [matrix_id][coef]
    int16_t     wq_matrix[2][2][64];  // [matrix_id][detail/undetail][coef]
    int16_t     seq_wq_matrix[2][64];
    int16_t     pic_user_wq_matrix[2][64];
    int16_t     wquant_param[2][6];
} weighted_quant_t;


/* ---------------------------------------------------------------------------
 * Run-Level pair */
typedef struct runlevel_pair_t {
    int16_t run;
    int16_t level;
} runlevel_pair_t;

/* ---------------------------------------------------------------------------
 * Run-Level info */
typedef struct runlevel_t {
    ALIGN32(runlevel_pair_t  run_level[16]);     /* 最大变换系数块为32x32 */
    int         num_nonzero_cg;  // number of CGs with non-zero coefficients
    uint32_t    reserved;
    /* contexts pointer */
    context_t(*p_ctx_run)[NUM_MAP_CTX];
    context_t *p_ctx_level;
    context_t *p_ctx_sig_cg;
    context_t *p_ctx_last_cg;
    context_t *p_ctx_last_pos_in_cg;

    const int16_t(*avs_scan)[2];
    const int16_t(*cg_scan)[2];
    coeff_t       *p_res;
    int            i_res;
    int            b_swap_xy;
    int            num_cg;
    int            i_tu_level;
    int            w_tr;
    int            h_tr;
} runlevel_t;

/* ---------------------------------------------------------------------------
 * LCU reconstruction info */
typedef struct lcu_rec_info_t {
    ALIGN32(coeff_t     coeff_buf_y[LCU_BUF_SIZE]);
    ALIGN32(coeff_t     coeff_buf_uv[2][LCU_BUF_SIZE >> 2]);
} lcu_rec_info_t;

/* ---------------------------------------------------------------------------
 * LCU info */
typedef struct lcu_info_t {
#if CTRL_AEC_THREAD
    lcu_rec_info_t rec_info;
#endif
    sao_t      sao_param;                        /* SAO param for each LCU */
    uint8_t    enable_alf[IMG_COMPONENTS];       /* ALF enabled for each LCU */
} lcu_info_t;


/* ---------------------------------------------------------------------------
 * coding unit */
struct cu_t {
    /* -------------------------------------------------------------
     * variables needed for neighboring CU decoding */
    int8_t      i_cu_level;
    int8_t      i_cu_type;

    int8_t      i_slice_nr;

    int8_t      i_qp;
    int8_t      i_cbp;
    int8_t      i_trans_size;         /* tu_split_type_e */

    /* -------------------------------------------------------------
     */
    int8_t      i_weighted_skipmode;
    int8_t      i_md_directskip_mode;
    int8_t      c_ipred_mode;         /* chroma intra prediction mode */
    int8_t      i_dmh_mode;           /* dir_multi_hypothesis_mode */
    int8_t      num_pu;               /* number of prediction units */

    /* -------------------------------------------------------------
     * buffers */
    int8_t      b8pdir[4];
    int8_t      intra_pred_modes[4];
    int8_t      dct_pattern[6];       /* DCT pattern of each block, dct_pattern_e, 4 luma + 2 chroma blocks */
    mv_t        mv[4][2];             /* [block_idx][1st/2nd] */
    ref_idx_t   ref_idx[4];           /* [block_idx].r[1st/2nd] */

    cb_t        pu[4];                /* used to reserve the size of PUs */
};


#include "primitives.h"

/* get partition index for the given size */
extern const uint8_t g_partition_map_tab[];
#define PART_INDEX(w, h)    (g_partition_map_tab[((((w) >> 2) - 1) << 4) + ((h) >> 2) - 1])


/* ---------------------------------------------------------------------------
 * output picture
 */
struct davs2_outpic_t {
    ALIGN16(void        *magic);      /* must be the 1st member variable. do not change it */

    davs2_frame_t      *frame;       /* the source frame */
    davs2_seq_info_t *head;        /* sequence head used to decode the frame */
    davs2_picture_t  *pic;         /* the output picture */

    davs2_outpic_t     *next;        /* next node */
};


/* ---------------------------------------------------------------------------
 * output picture list
 */
typedef struct davs2_output_t {
    int               output;         /* output index of the next frame */
    int               busy;           /* whether possibly one frame is being delivered */
    int               num_output_pic; /* number of pictures to be output */
    davs2_outpic_t  *pics;           /* output pictures */
} davs2_output_t;


/* ---------------------------------------------------------------------------
 * assemble elementary stream to a complete decodable unit (e.g., one frame),
 * the complete decodable unit is called ES unit
 */
typedef struct es_unit_t {
    ALIGN16(void *magic);             /* must be the 1st member variable. do not change it */
    davs2_bs_t    bs;                 /* bit-stream reader of this es_unit */
    int64_t       pts;                /* presentation time stamp */
    int64_t       dts;                /* decoding time stamp */
    int           len;                /* length of valid data in byte stream buffer */
    int           size;               /* buffer size */
    uint8_t       data[1];            /* byte stream buffer */
} es_unit_t;

/* ---------------------------------------------------------------------------
 * decoder task
 */
typedef struct davs2_task_t {
    ALIGN32(int     task_id);         /* task id */
    int             task_status;      /* 0: free; 1, busy */
    davs2_mgr_t   *taskmgr;          /* the taskmgr */
    es_unit_t      *curr_es_unit;     /* decoding ES unit */
    davs2_thread_t  thread_decode;    /* handle of the decoding thread */
} davs2_task_t;

/* ---------------------------------------------------------------------------
 */
struct davs2_log_t {
    int         i_log_level;          /* log level */
    char        module_name[60];      /* module name */
};

/* ---------------------------------------------------------------------------
 * decoder manager
 */
struct davs2_mgr_t {
    davs2_log_t         module_log;   /* log module */

    volatile int        b_exit;       /* app signal to exit */
    volatile int        b_flushing;   /* is being flushing */

    davs2_param_t       param;        /* decoder param */
    es_unit_t          *es_unit;      /* next input ES unit pointer */
    davs2_seq_t         seq_info;     /* latest sequence head */

    int                 i_tr_wrap_cnt;/* COI wrap count */
    int                 i_prev_coi;   /* previous COI */

    /* --- decoder output --------- */
    int                 new_sps;      /* is SPS(sequence property set) changed? */
    int                 num_frames_to_output;

    /* --- decoding picture buffer (DBP) --------- */
    davs2_frame_t     **dpb;          /* decoded picture buffer array */
    int                 dpbsize;      /* size of the dpb array */

    /* --- frames to be removed before next frame decoding --------- */
    int     num_frames_to_remove;     /* number of frames to be removed */
    int     coi_remove_frame[8];      /* COI of frames to be removed */

    /* --- lists (input & output) ---------------------------------- */
    xlist_t             packets_idle; /* bit-stream: free buffers for input packets */

    xlist_t             pic_recycle;  /* output_picture: free pictures recycle bin */
    davs2_output_t      outpics;      /* output pictures */

    /* --- task ---------------------------------------------------- */
    int                 num_decoders;        /* number of decoders in total */
    int                 num_active_decoders; /* number of active decoders currently */
    davs2_t            *decoders;            /* frame decoder contexts */
    davs2_t            *h_dec;               /* decoder context for current input bitstream */
    int                 num_frames_in;       /* number of frames: input */
    int                 num_frames_out;      /* number of frames: output */

    /* --- thread control ------------------------------------------ */
    int                     num_total_thread;  /* number of decoding threads in total */
    int                     num_aec_thread;    /* number of threads for AEC coding (the others are for reconstruction) */
    int                     num_rec_thread;    /* use thread pool or not */
    davs2_thread_t          thread_output;     /* handle of the frame output thread */
    davs2_thread_mutex_t    mutex_mgr;         /* a non-recursive mutex */
    davs2_thread_mutex_t    mutex_aec;         /* a non-recursive mutex for AEC */
    void                   *thread_pool;       /* AEC encoding thread */ 
};

/* ---------------------------------------------------------------------------
 */
typedef struct davs2_row_rec_t {
    davs2_t   *h;                /* frame decoder handler */
    lcu_info_t *lcu_info;         /* LCU info for REC */
    lcu_rec_info_t *p_rec_info;   /* LCu reconstruction info */
    int         idx_cu_zscan;     /* current CU scan order */
    bool_t      b_block_avail_top;    /* availability of top  block, used in second transform */
    bool_t      b_block_avail_left;   /* availability of left block, used in second transform */

    /* LCU position */
    struct ctu_recon_t {
        int     i_pix_x;
        int     i_pix_y;
        int     i_pix_x_c;
        int     i_pix_y_c;
        int     i_scu_x;
        int     i_scu_y;
        int     i_scu_xy;
        int     i_spu_x;
        int     i_spu_y;
        int     i_ctu_w;              /* width  of CTU in luma */
        int     i_ctu_h;              /* height of CTU in luma */
        int     i_ctu_w_c;            /* width  of CTU in chroma */
        int     i_ctu_h_c;            /* height of CTU in chroma */

        /* buffer pointers to picture */
        int     i_frec[3];            /* stride of reconstruction buffer (reconstruction picture) */
        pel_t  *p_frec[3];            /* reconstruction buffer pointer (reconstruction picture) */

        /* buffer pointers to CTU cache */
        int     i_fdec[3];            /* stride of reconstruction buffer (current LCU) */
        pel_t  *p_fdec[3];            /* reconstruction buffer pointer (current LCU) */
    } ctu;   // CTU info

    /* buffers */
    ALIGN32(pel_t       buf_edge_pixels[MAX_CU_SIZE << 3]); /* intra predication buffer */
    ALIGN32(pel_t       pred_blk[LCU_BUF_SIZE]);            /* temporary buffer used for prediction */
    // ALIGN32(pel_t       fdec_buf[MAX_CU_SIZE * (MAX_CU_SIZE + (MAX_CU_SIZE >> 1))]);
    struct lcu_intra_border_t {
        ALIGN32(pel_t rec_left[MAX_CU_SIZE]);          /* Left border of current LCU */
        ALIGN32(pel_t rec_top[MAX_CU_SIZE * 2 + 32]);  /* top-left, top and top-right samples (Reconstruction) of current LCU */
    } ctu_border[IMG_COMPONENTS];                      /* Y, U, V components */
} davs2_row_rec_t;

/* ---------------------------------------------------------------------------
 */
struct davs2_t {
    davs2_log_t  module_log;         /* log module */

    /* -------------------------------------------------------------
     * task information */
    davs2_task_t task_info;          /* task information */

    /* -------------------------------------------------------------
     * sequence */
    davs2_seq_t  seq_info;           /* sequence head of this task */

    /* -------------------------------------------------------------
     * log */
    int         i_log_level;          /* log level */

    int         i_image_width;        /* decoded image width */
    int         i_image_height;       /* decoded image height */
    int         i_chroma_format;      /* chroma format(1: 4:2:0, 2: 4:2:2) */
    int         i_lcu_level;          /* LCU size in bit */
    int         i_lcu_size;           /* LCU size = 1 << i_lcu_level */
    int         i_lcu_size_sub1;      /* LCU size = (1 << i_lcu_level) - 1 */
    int         i_display_delay;      /* picture display delay */
    int         sample_bit_depth;     /* sample bit depth */
    int         output_bit_depth;     /* output bit depth (assuming: output_bit_depth <= sample_bit_depth) */
    bool_t      b_bkgnd_picture;      /* background picture enabled? */
    bool_t      b_ra_decodable;       /* random access decodable flag */
    bool_t      b_video_edit_code;    /* video edit code */

    /* -------------------------------------------------------------
     * coding tools enabled */
    bool_t      b_roi;
    bool_t      b_DQP;                /* using DQP? */
    bool_t      b_sao;
    bool_t      b_alf;
    // int         b_dmh;

    /* -------------------------------------------------------------
     * decoding */
    davs2_bs_t   *p_bs;               /* input bitstream pointer */
    aec_t         aec;                /* arithmetic entropy decoder */
    int           decoding_error;     /* 非零值表示遇到了解码错误 */

    /* -------------------------------------------------------------
     * field */
    bool_t      b_top_field_first;
    bool_t      b_repeat_first_field;
    bool_t      b_top_field;

    /* -------------------------------------------------------------
     * picture coding type */
    int8_t      i_frame_type;
    int8_t      i_pic_coding_type;
    int8_t      i_pic_struct;         /* frame or field coding */

    /* -------------------------------------------------------------
     * picture properties */
    int         i_width;              /* picture width  in pixel (luma) */
    int         i_height;             /* picture height in pixel (luma) */
    int         i_width_in_scu;       /* width  in SCU */
    int         i_height_in_scu;      /* height in SCU */
    int         i_size_in_scu;        /* number of SCU */
    int         i_width_in_spu;       /* width  in SPU */
    int         i_height_in_spu;      /* height in SPU */
    int         i_width_in_lcu;       /* width  in LCU */
    int         i_height_in_lcu;      /* height in LCU */

    int         i_picture_qp;
    int         i_qp;                 /* quant for the current frame */

    int         i_poc;                /* POC (picture order count) of current frame, 8 bit */
    int         i_coi;                /* COI (coding order index) */

    int         i_cur_layer;

    int         chroma_quant_param_delta_u;
    int         chroma_quant_param_delta_v;
    bool_t      b_fixed_picture_qp;
    bool_t      b_bkgnd_reference;    /* AVS2-S: background reference enabled? */
    bool_t      enable_chroma_quant_param;


    /* -------------------------------------------------------------
     * slice */
    bool_t      b_slice_checked;      /* is slice checked? */
    bool_t      b_fixed_slice_qp;
    int         i_slice_index;        /* current slice index */
    int         i_slice_qp;
    int         i_last_dquant;
    pel_t      *intra_border[3];      /* buffer for store decoded bottom pixels of the top lcu row (before filter) */

    /* -------------------------------------------------------------
     * reference frame */
    int         num_of_references;
    rps_t       rps;

    davs2_frame_t *fref[AVS2_MAX_REFS];
    davs2_frame_t *fdec;
    davs2_frame_t *f_background_cur; /* background reference frame, used for reconstruction */
    davs2_frame_t *f_background_ref; /* background_frame, used for reference */
    davs2_frame_t *p_frame_sao;      /* used for SAO */
    davs2_frame_t *p_frame_alf;      /* used for ALF */
    lcu_info_t *lcu_infos;            /* LCU level info */

    /* -------------------------------------------------------------
     * post processing */

    /* deblock */
    int         b_loop_filter;        /* loop filter enabled? */
    int         i_alpha_offset;
    int         i_beta_offset;
    int         alpha;
    int         alpha_c;
    int         beta;
    int         beta_c;

    /* ALF */
    alf_var_t  *p_alf;
    bool_t      pic_alf_on[IMG_COMPONENTS];

    /* SAO */
    bool_t      slice_sao_on[IMG_COMPONENTS];

    /* -------------------------------------------------------------
     * buffers */
    uint8_t    *p_integral;           /* holder: base pointer for all allocated memory */

    /* intra mode */
    int         i_ipredmode;          /* stride */
    int8_t     *p_ipredmode;          /* intra prediction mode buffer */

    /* scu */
    cu_t       *scu_data;

    /* ref & mv & inter prediction direction */
    int8_t     *p_dirpred;            /* inter prediction direction */
    ref_idx_t  *p_ref_idx;            /* reference index */
    mv_t       *p_tmv_1st;            /* motion vector of 4x4 block (1st reference) */
    mv_t       *p_tmv_2nd;            /* motion vector of 4x4 block (2nd reference) */

    /* loop filter */
    uint8_t    *p_deblock_flag[2];    /* [v/h][b8_x, b8_y] */

    /* -------------------------------------------------------------
     * block availability */
    const int8_t *p_tab_TR_avail;
    const int8_t *p_tab_DL_avail;

    /* -------------------------------------------------------------
     * LCU-based cache */
    struct lcu_t {
        /* geometrical properties */
        ALIGN32(int i_pix_width);     /* actual width  (in pixel) for current lcu */
        int     i_pix_height;         /* actual height (in pixel) for current lcu */
        int     i_scu_x;              /* horizontal position for the first SCU in lcu */
        int     i_scu_y;              /* vertical   position for the first SCU in lcu */
        int     i_scu_xy;             /*            position for the first SCU in lcu */
        int     i_spu_x;              /* horizontal position for the first SPU in lcu */
        int     i_spu_y;              /* vertical   position for the first SPU in lcu */
        int     i_pix_x;              /* horizontal position (in pixel) of lcu (luma) */
        int     i_pix_y;              /* vertical   position (in pixel) of lcu (luma) */
        int     i_pix_c_x;            /* horizontal position (in pixel) of lcu (chroma) */
        int     i_pix_c_y;            /* vertical   position (in pixel) of lcu (chroma) */
        int     idx_cu_zscan_aec;     /* Z-scan index of current AEC CU within LCU (in 8x8 unit) */

        /* buffer pointers */
        lcu_info_t *lcu_aec;          /* LCU info for AEC */

        int8_t  i_left_cu_qp;         /* QP of left CU (for current CU decoding) */
        int8_t  c_ipred_mode_ctx;     /* context of chroma intra prediction mode (for current CU decoding) */

        neighbor_inter_t neighbor_inter[NUM_INTER_NEIGHBOR];        /* neighboring inter modes of 4x4 blocks*/

        int8_t  ref_skip_1st[DS_MAX_NUM];
        int8_t  ref_skip_2nd[DS_MAX_NUM];
        mv_t    mv_tskip_1st[DS_MAX_NUM];
        mv_t    mv_tskip_2nd[DS_MAX_NUM];

#if !CTRL_AEC_THREAD
        lcu_rec_info_t      rec_info;
#endif
        ALIGN32(runlevel_t  cg_info);

    } lcu;

    /* -------------------------------------------------------------
     * adaptive frequency weighting quantization */
    weighted_quant_t   wq;                // weight quant parameters
};

/**
 * ===========================================================================
 * global variables
 * ===========================================================================
 */
#if HIGH_BIT_DEPTH
extern int max_pel_value;
extern int g_bit_depth;
extern int g_dc_value;
#else
static const int g_bit_depth   = BIT_DEPTH;
static const int max_pel_value = (1 << BIT_DEPTH) - 1;
static const pel_t g_dc_value    = 128;
#endif

/**
 * ===========================================================================
 * common function declares
 * ===========================================================================
 */

/**
 * ---------------------------------------------------------------------------
 * Function   : output information
 * Parameters :
 *       [in] : decoder - decoder handle
 * Return     : none
 * ---------------------------------------------------------------------------
 */
void davs2_log(void *h, int level, const char *format, ...);

/* ---------------------------------------------------------------------------
 * trace */
#if AVS2_TRACE
int  avs2_trace_init(char *psz_trace_file);
void avs2_trace_destroy(void);
int  avs2_trace(const char *psz_fmt, ...);
void avs2_trace_string(char *trace_string, int value, int len);
void avs2_trace_string2(char *trace_string, int bit_pattern, int value, int len);
#endif

/* ---------------------------------------------------------------------------
 * memory alloc
 */

static ALWAYS_INLINE void *davs2_malloc(size_t i_size)
{
    intptr_t mask = CACHE_LINE_SIZE - 1;
    uint8_t *align_buf = NULL;
    uint8_t *buf = (uint8_t *)malloc(i_size + mask + sizeof(void **));

    if (buf != NULL) {
        align_buf = buf + mask + sizeof(void **);
        align_buf -= (intptr_t)align_buf & mask;
        *(((void **)align_buf) - 1) = buf;
    } else {
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
        davs2_log(NULL, DAVS2_LOG_ERROR, "malloc of size %zu failed\n", i_size);
#else
        davs2_log(NULL, DAVS2_LOG_ERROR, "malloc of size %lu failed\n", i_size);
#endif
    }

    return align_buf;
}

static ALWAYS_INLINE void *davs2_calloc(size_t count, size_t size)
{
    void *p = davs2_malloc(count * size);
    if (p != NULL) {
        memset(p, 0, size * sizeof(uint8_t));
    }
    return p;
}

static ALWAYS_INLINE void davs2_free(void *ptr)
{
    if (ptr != NULL) {
        free(*(((void **)ptr) - 1));
    }
}

#if SYS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <time.h>

/* ---------------------------------------------------------------------------
 * get timestamp in us
 */
static ALWAYS_INLINE
int64_t davs2_get_us(void)
{
#if SYS_WINDOWS
    LARGE_INTEGER nFreq;
    if (QueryPerformanceFrequency(&nFreq)) { // 返回非零表示硬件支持高精度计数器
        LARGE_INTEGER t1;
        QueryPerformanceCounter(&t1);
        return (int64_t)(1000000 * t1.QuadPart / (double)nFreq.QuadPart);
    } else {  // 硬件不支持情况下，使用毫秒级系统时间
        int64_t tm = clock();
        return (tm * (1000000 / CLOCKS_PER_SEC));
    }
#else
    int64_t tm = clock();
    return (tm * (1000000 / CLOCKS_PER_SEC));
#endif
}

/**
 * ===========================================================================
 * inline function defines
 * ===========================================================================
 */

#if defined(__GNUC__) && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 3)
#define davs2_clz(x)      __builtin_clz(x)
#define davs2_ctz(x)      __builtin_ctz(x)
#elif defined(_MSC_VER) && defined(_WIN32)
static int ALWAYS_INLINE davs2_clz(const uint32_t x)
{
    DWORD r;
    _BitScanReverse(&r, (DWORD)x);
    return (r ^ 31);
}

static int ALWAYS_INLINE davs2_ctz(const uint32_t x)
{
    DWORD r;
    _BitScanForward(&r, (DWORD)x);
    return r;
}

#else
static int ALWAYS_INLINE davs2_clz(uint32_t x)
{
    static uint8_t lut[16] = { 4, 3, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 };
    int y, z = (((x >> 16) - 1) >> 27) & 16;
    x >>= z ^ 16;
    z += y = ((x - 0x100) >> 28) & 8;
    x >>= y ^ 8;
    z += y = ((x - 0x10) >> 29) & 4;
    x >>= y ^ 4;
    return z + lut[x];
}

static int ALWAYS_INLINE davs2_ctz(uint32_t x)
{
    static uint8_t lut[16] = { 4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0 };
    int y, z = (((x & 0xffff) - 1) >> 27) & 16;
    x >>= z;
    z += y = (((x & 0xff) - 1) >> 28) & 8;
    x >>= y;
    z += y = (((x & 0xf) - 1) >> 29) & 4;
    x >>= y;
    return z + lut[x & 0xf];
}
#endif

static ALWAYS_INLINE pel_t davs2_clip_pixel(int x)
{
    return (pel_t)((x & ~max_pel_value) ? (-x) >> 31 & max_pel_value : x);
}

static ALWAYS_INLINE int davs2_clip3(int v, int i_min, int i_max)
{
    return ((v < i_min) ? i_min : (v > i_max) ? i_max : v);
}

static ALWAYS_INLINE int davs2_median(int a, int b, int c)
{
    int t = (a - b) & ((a - b) >> 31);

    a -= t;
    b += t;
    b -= (b - c) & ((b - c) >> 31);
    b += (a - b) & ((a - b) >> 31);

    return b;
}

// 返回数值的符号位，负数返回-1，否则返回1
static ALWAYS_INLINE int davs2_sign2(int val)
{
    return ((val >> 31) << 1) + 1;
}

// 返回数值的符号位，负数返回-1，0值返回0，正数返回1
static ALWAYS_INLINE int davs2_sign3(int val)
{
    return (val >> 31) | (int)(((uint32_t)-val) >> 31u);
}

// 计算正整数的log2值，0和1时返回0，其他返回log2(val)
#define davs2_log2u(val)  davs2_ctz(val)


/* ---------------------------------------------------------------------------
 * unions for type-punning.
 * Mn: load or store n bits, aligned, native-endian
 * CPn: copy n bits, aligned, native-endian
 * we don't use memcpy for CPn because memcpy's args aren't assumed
 * to be aligned */
typedef union {
    uint16_t    i;
    uint8_t     c[2];
} MAY_ALIAS davs2_union16_t;

typedef union {
    uint32_t    i;
    uint16_t    b[2];
    uint8_t     c[4];
} MAY_ALIAS davs2_union32_t;

typedef union {
    uint64_t    i;
    uint32_t    a[2];
    uint16_t    b[4];
    uint8_t     c[8];
} MAY_ALIAS davs2_union64_t;

#define M16(src)                (((davs2_union16_t *)(src))->i)
#define M32(src)                (((davs2_union32_t *)(src))->i)
#define M64(src)                (((davs2_union64_t *)(src))->i)
#define CP16(dst,src)           M16(dst)  = M16(src)
#define CP32(dst,src)           M32(dst)  = M32(src)
#define CP64(dst,src)           M64(dst)  = M64(src)

/* ---------------------------------------------------------------------------
 * assert
 */
#define DAVS2_ASSERT(expression, ...)   if (!(expression)) { davs2_log(NULL, DAVS2_LOG_ERROR, __VA_ARGS__); }

/* ---------------------------------------------------------------------------
 * list
 */
#define xl_init         FPFX(xl_init)
int   xl_init          (xlist_t *const xlist);
#define xl_destroy      FPFX(xl_destroy)
void  xl_destroy       (xlist_t *const xlist);
#define xl_append       FPFX(xl_append)
void  xl_append        (xlist_t *const xlist, void *node);
#define xl_remove_head  FPFX(xl_remove_head)
void *xl_remove_head   (xlist_t *const xlist, const int wait);
#define xl_remove_head_ex FPFX(xl_remove_head_ex)
void *xl_remove_head_ex(xlist_t *const xlist);

#ifdef __cplusplus
}
#endif
#endif // DAVS2_COMMON_H
