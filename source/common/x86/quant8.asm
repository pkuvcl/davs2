;*****************************************************************************
;* quant8.asm: x86 quantization functions
;*****************************************************************************
;*    xavs2 - video encoder of AVS2/IEEE1857.4 video coding standard
;*    Copyright (C) 2018~ VCL, NELVT, Peking University
;*
;*    Authors: Falei LUO <falei.luo@gmail.com>
;*             Jiaqi Zhang <zhangjiaqi.cs@gmail.com>
;*
;*    Homepage1: http://vcl.idm.pku.edu.cn/xavs2
;*    Homepage2: https://github.com/pkuvcl/xavs2
;*    Homepage3: https://gitee.com/pkuvcl/xavs2
;*
;*    This program is free software; you can redistribute it and/or modify
;*    it under the terms of the GNU General Public License as published by
;*    the Free Software Foundation; either version 2 of the License, or
;*    (at your option) any later version.
;*
;*    This program is distributed in the hope that it will be useful,
;*    but WITHOUT ANY WARRANTY; without even the implied warranty of
;*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;*    GNU General Public License for more details.
;*
;*    You should have received a copy of the GNU General Public License
;*    along with this program; if not, write to the Free Software
;*    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
;*
;*    This program is also available under a commercial proprietary license.
;*    For more information, contact us at sswang @ pku.edu.cn.
;*****************************************************************************

%include "x86inc.asm"
%include "x86util.asm"


SECTION .text

; ----------------------------------------------------------------------------
; void dequant(coeff_t *coef, const int i_coef, const int scale, const int shift);
; ----------------------------------------------------------------------------

; ----------------------------------------------------------------------------
; dequant_sse4
INIT_XMM sse4
cglobal dequant, 2,2,7
;{
    mov         r3, r3mp              ; r3  <-- shift
    movq        m4, r2mp              ; m4[0] = scale
    movq        m6, r3                ; m6[0] = shift
    dec         r3                    ; r3d <-- shift - 1
    xor         r2, r2                ; r2 <-- 0
    shr         r1, 4                 ; r1    = i_coef/16
    bts         r2, r3                ; r2 <-- add = 1 < (shift - 1)
    movq        m5, r2                ; m5[0] = add
    pshufd      m4, m4, 0             ; m4[3210] = scale
    pshufd      m5, m5, 0             ; m5[3210] = add
                                      ;
.loop:                                ;
    pmovsxwd    m0, [r0     ]         ; load 4 coeff
    pmovsxwd    m1, [r0 +  8]         ;
    pmovsxwd    m2, [r0 + 16]         ;
    pmovsxwd    m3, [r0 + 24]         ;
                                      ;
    pmulld      m0, m4                ; coef[i] * scale
    pmulld      m1, m4                ;
    pmulld      m2, m4                ;
    pmulld      m3, m4                ;
    paddd       m0, m5                ; coef[i] * scale + add
    paddd       m1, m5                ;
    paddd       m2, m5                ;
    paddd       m3, m5                ;
    psrad       m0, m6                ; (coef[i] * scale + add) >> shift
    psrad       m1, m6                ;
    psrad       m2, m6                ;
    psrad       m3, m6                ;
                                      ;
    packssdw    m0, m1                ; pack to 8 coeff
    packssdw    m2, m3                ;
                                      ;
    mova   [r0   ], m0                ; store
    mova   [r0+16], m2                ;
    add         r0, 32                ;
    dec         r1                    ;
    jnz        .loop                  ;
                                      ;
    RET                               ; return
;}
