/*
 * defines.h
 *
 * Description of this file:
 *    const variable definition of the davs2 library
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

#ifndef DAVS2_DEFINES_H
#define DAVS2_DEFINES_H


/**
 * ===========================================================================
 * build switch
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * build */
#define RELEASE_BUILD           1     /* 1: release build */

#define CTRL_AEC_THREAD         0     /* AEC and reconstruct conducted in different threads */
#define CTRL_AEC_CONVERSION     0     /* AEC result conversion */


/* ---------------------------------------------------------------------------
 * debug */
#if RELEASE_BUILD
#define AVS2_TRACE              0     /* write trace file,    1: ON, 0: OFF */
#else
#define AVS2_TRACE              0     /* write trace file,    1: ON, 0: OFF */
#endif

#define DAVS2_TRACE_API        0     /* API calling trace */

#define USE_NEW_INTPL           0     /* use new interpolation functions */

#define BUGFIX_PREDICTION_INTRA 1     /* align to latest intra prediction */



/**
 * ===========================================================================
 * define of const variables
 * ===========================================================================
 */

/* ---------------------------------------------------------------------------
 * profile */
#define MAIN_PICTURE_PROFILE    0x12
#define MAIN_PROFILE            0x20
#define MAIN10_PROFILE          0x22

enum chroma_format_e {
    CHROMA_400 = 0,
    CHROMA_420 = 1,
    CHROMA_422 = 2,
    CHROMA_444 = 3
};

/* ---------------------------------------------------------------------------
 * prediction techniques */
#define DMH_MODE_NUM            5     /* number of DMH mode */


/* ---------------------------------------------------------------------------
 * SAO */
#define MAX_NUM_SAO_CLASSES         32
#define NUM_SAO_BO_CLASSES_LOG2     5
#define NUM_SAO_BO_CLASSES_IN_BIT   5
#define NUM_SAO_EO_TYPES_LOG2       2
#define SAO_SHIFT_PIX_NUM           4


/* ---------------------------------------------------------------------------
 * ALF parameters */
#define ALF_NUM_VARS            16
#define ALF_MAX_NUM_COEF        9
#define LOG2_VAR_SIZE_H         2
#define LOG2_VAR_SIZE_W         2
#define ALF_FOOTPRINT_SIZE      7
#define DF_CHANGED_SIZE         3
#define ALF_NUM_BIT_SHIFT       6


/* ---------------------------------------------------------------------------
 * Quantization parameter range */
#define MIN_QP                  0
#if HIGH_BIT_DEPTH
#define MAX_QP                  79    /* max QP */
#else
#define MAX_QP                  63    /* max QP */
#endif
#define SHIFT_QP                11


/* ---------------------------------------------------------------------------
 * block sizes */
#define MAX_CU_SIZE             64    /* 64x64 */
#define MAX_CU_SIZE_IN_BIT      6
#define MIN_CU_SIZE             8     /* 8x8 */
#define MIN_CU_SIZE_IN_BIT      3
#define MIN_PU_SIZE             4     /* 4x4 */
#define MIN_PU_SIZE_IN_BIT      2
#define BLOCK_MULTIPLE          (MIN_CU_SIZE/MIN_PU_SIZE)

#define B4X4_IN_BIT             2
#define B8X8_IN_BIT             3
#define B16X16_IN_BIT           4
#define B32X32_IN_BIT           5
#define B64X64_IN_BIT           6


/* ---------------------------------------------------------------------------
 * luma intra prediction modes
 */
enum intra_pred_mode_e {
    /* non-angular mode */
    DC_PRED         = 0 ,                /* prediction mode: DC */
    PLANE_PRED      = 1 ,                /* prediction mode: PLANE */
    BI_PRED         = 2 ,                /* prediction mode: BI */

    /* vertical angular mode */
    INTRA_ANG_X_3   =  3, INTRA_ANG_X_4   =  4, INTRA_ANG_X_5   =  5,
    INTRA_ANG_X_6   =  6, INTRA_ANG_X_7   =  7, INTRA_ANG_X_8   =  8,
    INTRA_ANG_X_9   =  9, INTRA_ANG_X_10  = 10, INTRA_ANG_X_11  = 11,
    INTRA_ANG_X_12  = 12,
    VERT_PRED       = INTRA_ANG_X_12,    /* prediction mode: VERT */

    /* vertical + horizontal angular mode */
    INTRA_ANG_XY_13 = 13, INTRA_ANG_XY_14 = 14, INTRA_ANG_XY_15 = 15,
    INTRA_ANG_XY_16 = 16, INTRA_ANG_XY_17 = 17, INTRA_ANG_XY_18 = 18,
    INTRA_ANG_XY_19 = 19, INTRA_ANG_XY_20 = 20, INTRA_ANG_XY_21 = 21,
    INTRA_ANG_XY_22 = 22, INTRA_ANG_XY_23 = 23,

    /* horizontal angular mode */
    INTRA_ANG_Y_24  = 24, INTRA_ANG_Y_25  = 25, INTRA_ANG_Y_26 = 26,
    INTRA_ANG_Y_27  = 27, INTRA_ANG_Y_28  = 28, INTRA_ANG_Y_29 = 29,
    INTRA_ANG_Y_30  = 30, INTRA_ANG_Y_31  = 31, INTRA_ANG_Y_32 = 32,
    HOR_PRED        = INTRA_ANG_Y_24,    /* prediction mode: HOR */
    NUM_INTRA_MODE  = 33,                /* number of luma intra prediction modes */
};

/* ---------------------------------------------------------------------------
 * chroma intra prediction modes
 */
enum intra_chroma_pred_mode_e {
    /* chroma intra prediction modes */
    DM_PRED_C             = 0,     /* prediction mode: DM */
    DC_PRED_C             = 1,     /* prediction mode: DC */
    HOR_PRED_C            = 2,     /* prediction mode: HOR */
    VERT_PRED_C           = 3,     /* prediction mode: VERT */
    BI_PRED_C             = 4,     /* prediction mode: BI */
    NUM_INTRA_MODE_CHROMA = 5,     /* number of chroma intra prediction modes */
};

/* ---------------------------------------------------------------------------
 * mv predicating */
#define MVPRED_xy_MIN           0
#define MVPRED_L                1
#define MVPRED_U                2
#define MVPRED_UR               3


/* ---------------------------------------------------------------------------
 * mv predicating direction */
#define PDIR_FWD                0
#define PDIR_BWD                1
#define PDIR_SYM                2
#define PDIR_BID                3
#define PDIR_DUAL               4
#define PDIR_INVALID           -1     /* invalid predicating direction */

/* ---------------------------------------------------------------------------
 * unification of MV scaling */
#define MULTI                   16384
#define HALF_MULTI              8192
#define OFFSET                  14


/* ---------------------------------------------------------------------------
 * motion information storage compression */
#define MV_DECIMATION_FACTOR    4     /* store the middle pixel's mv in a motion information unit */
#define MV_FACTOR_IN_BIT        2


/* ---------------------------------------------------------------------------
 * for 16-BITS transform */
#define LIMIT_BIT               16


/* ---------------------------------------------------------------------------
 * max value */
#define AVS2_THREAD_MAX        16     /* max number of threads */
#define DAVS2_WORK_MAX        128     /* max number of works (thread queue) */
#define AVS2_MAX_REFS           4     /* max reference frame number */
#define AVS2_GOP_NUM           32     /* max GOP number */
#define AVS2_COI_CYCLE        256     /* COI ranges from [0, 255] */

#define MAX_POC_DISTANCE      128     /* max POC distance */
#define INVALID_FRAME          -1     /* invalid value for COI & POC */

#define CG_SIZE                16     /* size of an coefficient group, 4x4 */

#define TEMPORAL_MAXLEVEL_BIT   3     /* bit number of temporal_id */
#define THRESHOLD_PMVR          2     /* threshold for pmvr */

#define MAX_ES_FRAME_SIZE 4000000     /* default max es frame size: 4MB */
#define MAX_ES_FRAME_NUM       64     /* default number of es frames */

#define AVS2_PAD        (64 + 16)     /* number of pixels padded around the reference frame */

#define DAVS2_MAX_LCU_ROWS   256      /* maximum number of LCU rows of one frame */ 


/* ---------------------------------------------------------------------------
 * aec
 */
#define SE_CHROMA               1     /* context for read (run, level) */
#define SE_LUMA_8x8             2     /* context for read (run, level) */


/* ---------------------------------------------------------------------------
 * transform */
#define SEC_TR_SIZE             4     /* block size of 2nd transform */


/* ---------------------------------------------------------------------------
 * CPU flags
 */

/* x86 */
#define DAVS2_CPU_CMOV            0x0000001
#define DAVS2_CPU_MMX             0x0000002
#define DAVS2_CPU_MMX2            0x0000004   /* MMX2 aka MMXEXT aka ISSE */
#define DAVS2_CPU_MMXEXT          DAVS2_CPU_MMX2
#define DAVS2_CPU_SSE             0x0000008
#define DAVS2_CPU_SSE2            0x0000010
#define DAVS2_CPU_SSE3            0x0000020
#define DAVS2_CPU_SSSE3           0x0000040
#define DAVS2_CPU_SSE4            0x0000080   /* SSE4.1 */
#define DAVS2_CPU_SSE42           0x0000100   /* SSE4.2 */
#define DAVS2_CPU_LZCNT           0x0000200   /* Phenom support for "leading zero count" instruction. */
#define DAVS2_CPU_AVX             0x0000400   /* AVX support: requires OS support even if YMM registers aren't used. */
#define DAVS2_CPU_XOP             0x0000800   /* AMD XOP */
#define DAVS2_CPU_FMA4            0x0001000   /* AMD FMA4 */
#define DAVS2_CPU_AVX2            0x0002000   /* AVX2 */
#define DAVS2_CPU_FMA3            0x0004000   /* Intel FMA3 */
#define DAVS2_CPU_BMI1            0x0008000   /* BMI1 */
#define DAVS2_CPU_BMI2            0x0010000   /* BMI2 */
/* x86 modifiers */
#define DAVS2_CPU_CACHELINE_32    0x0020000   /* avoid memory loads that span the border between two cachelines */
#define DAVS2_CPU_CACHELINE_64    0x0040000   /* 32/64 is the size of a cacheline in bytes */
#define DAVS2_CPU_SSE2_IS_SLOW    0x0080000   /* avoid most SSE2 functions on Athlon64 */
#define DAVS2_CPU_SSE2_IS_FAST    0x0100000   /* a few functions are only faster on Core2 and Phenom */
#define DAVS2_CPU_SLOW_SHUFFLE    0x0200000   /* The Conroe has a slow shuffle unit (relative to overall SSE performance) */
#define DAVS2_CPU_STACK_MOD4      0x0400000   /* if stack is only mod4 and not mod16 */
#define DAVS2_CPU_SLOW_CTZ        0x0800000   /* BSR/BSF x86 instructions are really slow on some CPUs */
#define DAVS2_CPU_SLOW_ATOM       0x1000000   /* The Atom is terrible: slow SSE unaligned loads, slow
                                                 * SIMD multiplies, slow SIMD variable shifts, slow pshufb,
                                                 * cacheline split penalties -- gather everything here that
                                                 * isn't shared by other CPUs to avoid making half a dozen
                                                 * new SLOW flags. */
#define DAVS2_CPU_SLOW_PSHUFB     0x2000000   /* such as on the Intel Atom */
#define DAVS2_CPU_SLOW_PALIGNR    0x4000000   /* such as on the AMD Bobcat */

/* ARM */
#define DAVS2_CPU_ARMV6           0x0000001
#define DAVS2_CPU_NEON            0x0000002   /* ARM NEON */
#define DAVS2_CPU_FAST_NEON_MRC   0x0000004   /* Transfer from NEON to ARM register is fast (Cortex-A9) */


/* ---------------------------------------------------------------------------
 * others */
#ifndef FALSE
#define FALSE                   0
#endif
#ifndef TRUE
#define TRUE                    1
#endif

#define FAST_GET_SPS            1     /* get SPS as soon as possible */


/* ---------------------------------------------------------------------------
 * all assembly and related C functions are prefixed with 'staravs_' default
 */
#define PFXB(prefix, name)  prefix ## _ ## name
#define PFXA(prefix, name)  PFXB(prefix,   name)
#define FPFX(name)          PFXA(davs2,  name)

/* ---------------------------------------------------------------------------
 * flag
 */
#define AVS2_EXIT_THREAD     (-1)  /* flag to terminate thread */

/* ---------------------------------------------------------------------------
* if hdr chroma qp open
*/
#define HDR_CHROMA_DELTA_QP     0

#endif  // DAVS2_DEFINES_H
