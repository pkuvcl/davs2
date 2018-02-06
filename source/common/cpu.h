/*
 * cpu.h
 *
 * Description of this file:
 *    CPU-Processing functions definition of the davs2 library
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


#ifndef DAVS2_CPU_H
#define DAVS2_CPU_H
#ifdef __cplusplus
extern "C" {
#endif


uint32_t davs2_cpu_detect(void);
int  davs2_cpu_num_processors(void);
void avs_cpu_emms(void);
void avs_cpu_mask_misalign_sse(void);
void avs_cpu_sfence(void);

char *davs2_get_simd_capabilities(char *buf, uint32_t cpuid);

#if HAVE_MMX
uint32_t davs2_cpu_cpuid(uint32_t op, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);
void davs2_cpu_xgetbv(uint32_t op, uint32_t *eax, uint32_t *edx);
#define avs_emms() avs_cpu_emms()
#else
#define avs_emms()
#endif

#define avs_sfence avs_cpu_sfence

/* kluge:
 * gcc can't give variables any greater alignment than the stack frame has.
 * We need 16 byte alignment for SSE2, so here we make sure that the stack is
 * aligned to 16 bytes.
 * gcc 4.2 introduced __attribute__((force_align_arg_pointer)) to fix this
 * problem, but I don't want to require such a new version.
 * This applies only to x86_32, since other architectures that need alignment
 * also have ABIs that ensure aligned stack. */
#if ARCH_X86 && HAVE_MMX
//int xavs_stack_align(void(*func) (xavs_t *), xavs_t * arg);
//#define avs_stack_align(func,arg) avs_stack_align((void (*)(xavs_t*))func,arg)
#else
#define avs_stack_align(func,...) func(__VA_ARGS__)
#endif

void avs_cpu_restore(uint32_t cpuid);

#ifdef __cplusplus
}
#endif
#endif  // DAVS2_CPU_H
