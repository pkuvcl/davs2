# Makefile

include config.mak

vpath %.cc $(SRCPATH)
vpath %.c $(SRCPATH)
vpath %.h $(SRCPATH)
vpath %.S $(SRCPATH)
vpath %.asm $(SRCPATH)
vpath %.rc $(SRCPATH)
CFLAGS += -I$(SRCPATH) -I$(SRCPATH)/.. \
			-I$(SRCPATH)/x86 \
			-I$(SRCPATH)/vec

GENERATED =

all: default
default:

SRCS  = common/aec.cc    common/alf.cc \
		common/bitstream.cc common/block_info.cc \
		common/common.cc common/davs2.cc common/cpu.cc common/cu.cc \
		common/deblock.cc common/decoder.cc \
		common/frame.cc  common/header.cc \
		common/intra.cc common/mc.cc \
		common/memory.cc \
		common/pixel.cc common/predict.cc \
		common/quant.cc \
		common/sao.cc common/transform.cc \
		common/primitives.cc \
		common/threadpool.cc common/win32thread.cc

SRCCLI = test/test.c

SRCSO =
OBJS =
OBJAVX =
OBJSO =
OBJCLI =

#OBJCHK = tools/checkasm.o

## CONFIG: $(shell cat config.h)
## 
## ifneq ($(findstring HAVE_THREAD 1, $(CONFIG)),)
## SRCS    += common/threadpool.cc
## endif
## ifneq ($(findstring HAVE_WIN32THREAD 1, $(CONFIG)),)
## SRCS    += common/win32thread.cc
## endif

# MMX/SSE optims
ifneq ($(AS),)
# asm --------------------------------------------------------------
X86SRC = common/x86/const-a.asm \
				common/x86/blockcopy8.asm \
				common/x86/cpu-a.asm \
				common/x86/dct8.asm \
				common/x86/mc-a2.asm \
				common/x86/pixeladd8.asm \
				common/x86/quant8.asm

ifeq ($(SYS_ARCH),X86)
ARCH_X86 = yes
ASMSRC   = $(X86SRC) 
endif

## Until now, we do not have 64-bit asm
ifeq ($(SYS_ARCH),X86_64)
ARCH_X86 = yes
SRCS     += common/vec/intrinsic.cc \
			common/vec/intrinsic_alf.cc \
			common/vec/intrinsic_sao.cc \
			common/vec/intrinsic_deblock.cc \
			common/vec/intrinsic_intra-filledge.cc \
			common/vec/intrinsic_intra-pred.cc \
			common/vec/intrinsic_inter_pred.cc \
			common/vec/intrinsic_idct.cc \
			common/vec/intrinsic_pixel.cc

SRCSAVX = common/vec/intrinsic_sao_avx2.cc \
		  common/vec/intrinsic_deblock_avx2.cc \
		  common/vec/intrinsic_intra-pred_avx2.cc \
		  common/vec/intrinsic_inter_pred_avx2.cc \
		  common/vec/intrinsic_pixel_avx.cc \
		  common/vec/intrinsic_idct_avx2.cc

CFLAGS += -mmmx -msse -msse2 -msse3 -msse4 -msse4.1 -msse4.2 -msse4a -mssse3 -mavx
# ASMSRC   = $(X86SRC:-32.asm=-64.asm)
ASMSRC   = $(X86SRC)
ASFLAGS += -DARCH_X86_64=1
OBJASM  = $(ASMSRC:%.asm=%.o)
$(OBJASM): common/x86/x86inc.asm common/x86/x86util.asm
endif

ifdef ARCH_X86
ASFLAGS += -I$(SRCPATH)/x86/
#SRCS    += x86/mc-c.cc x86/predict-c.cc
OBJASM  = $(ASMSRC:%.asm=%.o)
$(OBJASM): common/x86/x86inc.asm common/x86/x86util.asm
endif

# AltiVec optims
ifeq ($(SYS_ARCH),PPC)
SRCS += common/ppc/mc.cc common/ppc/pixel.cc common/ppc/dct.cc \
        common/ppc/quant.cc common/ppc/deblock.cc \
        common/ppc/predict.cc
endif

# NEON optims
ifeq ($(SYS_ARCH),ARM)
# x264 ARM asm sources
# ASMSRC += common/arm/cpu-a.S common/arm/pixel-a.S common/arm/mc-a.S \
#           common/arm/dct-a.S common/arm/quant-a.S common/arm/deblock-a.S \
#           common/arm/predict-a.S common/arm/bitstream-a.S
# SRCS   += common/arm/mc-c.cc common/arm/predict-c.cc
# x265 ARM asm sources
ASMSRC += common/arm/blockcopy8.S common/arm/cpu-a.S common/arm/dct-a.S \
          common/arm/ipfilter8.S common/arm/mc-a.S common/arm/pixel-util.S \
          common/arm/sad-a.S common/arm/ssd-a.S
OBJASM  = $(ASMSRC:%.S=%.o)
endif

# AArch64 NEON optims
ifeq ($(SYS_ARCH),AARCH64)
ASMSRC += common/aarch64/bitstream-a.S \
          common/aarch64/cabac-a.S     \
          common/aarch64/dct-a.S     \
          common/aarch64/deblock-a.S \
          common/aarch64/mc-a.S      \
          common/aarch64/pixel-a.S   \
          common/aarch64/predict-a.S \
          common/aarch64/quant-a.S
SRCS   += common/aarch64/asm-offsets.cc \
          common/aarch64/mc-c.cc        \
          common/aarch64/predict-c.cc
OBJASM  = $(ASMSRC:%.S=%.o)
OBJCHK += tools/checkasm-aarch64.o
endif

# MSA optims
ifeq ($(SYS_ARCH),MIPS)
ifneq ($(findstring HAVE_MSA 1, $(CONFIG)),)
SRCS += common/mips/mc-c.cc common/mips/dct-c.cc \
        common/mips/deblock-c.cc common/mips/pixel-c.cc \
        common/mips/predict-c.cc common/mips/quant-c.cc
endif
endif

# asm --------------------------------------------------------------
endif 
# here ends ifneq ($(AS),)

ifneq ($(HAVE_GETOPT_LONG),1)
SRCS += compat/getopt/getopt.cc
endif

## Windows Dll
## ifeq ($(SYS), WINDOWS)
## # OBJCLI += $(if $(RC), davs2res.o)
## ifneq ($(SONAME),)
## SRCSO   += davs2dll.cc
## OBJSO   += $(if $(RC), davs2res.dll.o)
## endif
## endif

OBJS   += $(SRCS:%.cc=%.o)
OBJAVX += $(SRCSAVX:%.cc=%.o)
OBJCLI += $(SRCCLI:%.c=%.o)
OBJSO  += $(SRCSO:%.cc=%.o)

.PHONY: all default fprofiled clean distclean install install-* uninstall cli lib-* etags

cli: davs2$(EXE)
lib-static: $(LIBDAVS2)
lib-shared: $(SONAME)

$(LIBDAVS2): $(GENERATED) .depend $(OBJS) $(OBJAVX) $(OBJASM)
	@echo "\033[33m [linking static] $(LIBDAVS2) \033[0m"
	rm -f $(LIBDAVS2)
	$(AR)$@ $(OBJS) $(OBJAVX) $(OBJASM)
	$(if $(RANLIB), $(RANLIB) $@)

$(SONAME): $(GENERATED) .depend $(OBJS) $(OBJAVX) $(OBJASM) $(OBJSO)
	@echo "\033[33m [linking shared] $(SONAME) \033[0m"
	$(LD)$@ $(OBJS) $(OBJAVX) $(OBJASM) $(OBJSO) $(SOFLAGS) $(LDFLAGS)

ifneq ($(EXE),)
.PHONY: davs2 checkasm
davs2: davs2$(EXE)
checkasm: checkasm$(EXE)
endif

davs2$(EXE): $(GENERATED) .depend $(OBJCLI) $(CLI_LIBDAVS2)
	@echo "\033[33m [linking execution] davs2$(EXE) \033[0m"
	$(LD)$@ $(OBJCLI) $(CLI_LIBDAVS2) $(LDFLAGSCLI) $(LDFLAGS)

checkasm$(EXE): $(GENERATED) .depend $(OBJCHK) $(LIBDAVS2)
	@echo "\033[33m [linking checkasm] checkasm$(EXE) \033[0m"
	$(LD)$@ $(OBJCHK) $(LIBDAVS2) $(LDFLAGS)

$(OBJS) $(OBJAVX) $(OBJASM) $(OBJSO) $(OBJCLI) $(OBJCHK): .depend

%.o: %.asm common/x86/x86inc.asm common/x86/x86util.asm
	@echo "\033[33m [Compiling asm]: $< \033[0m"
	$(AS) $(ASFLAGS) -o $@ $<
	-@ $(if $(STRIP), $(STRIP) -x $@) # delete local/anonymous symbols, so they don't show up in oprofile

%.o: %.S
	@echo "\033[33m [Compiling asm]: $< \033[0m"
	$(AS) $(ASFLAGS) -o $@ $<
	-@ $(if $(STRIP), $(STRIP) -x $@) # delete local/anonymous symbols, so they don't show up in oprofile

%.dll.o: %.rc davs2.h
	@echo "\033[33m [Compiling dll]: $< \033[0m"
	$(RC) $(RCFLAGS)$@ -DDLL $<

%.o: %.rc davs2.h
	@echo "\033[33m [Compiling rc]: $< \033[0m"
	$(RC) $(RCFLAGS)$@ $<

$(OBJAVX):
	@echo "\033[33m [Compiling]: $(@:.o=.cc) \033[0m"
	$(CC) $(CFLAGS) -mavx2 -c -o $@ $(SRCPATH)/$(@:.o=.cc)

%.o: %.cc
	@echo "\033[33m [Compiling]: $< \033[0m"
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.c
	@echo "\033[33m [Compiling]: $< \033[0m"
	$(CC) $(CFLAGS) -c -o $@ $<

.depend: config.mak
	@rm -f .depend
	@echo "\033[33m dependency file generation... \033[0m"
ifeq ($(COMPILER),CL)
	@$(foreach SRC, $(addprefix $(SRCPATH)/, $(SRCS) $(SRCCLI) $(SRCSO)), $(SRCPATH)/tools/msvsdepend.sh "$(CC)" "$(CFLAGS)" "$(SRC)" "$(SRC:$(SRCPATH)/%.cc=%.o)" 1>> .depend;)
	@$(foreach SRC, $(addprefix $(SRCPATH)/, $(SRCSAVX)), $(SRCPATH)/tools/msvsdepend.sh "$(CC)" "$(CFLAGS)" "$(SRC)" "$(SRC:$(SRCPATH)/%.cc=%.o)" 1>> .depend;)
else
	@$(foreach SRC, $(addprefix $(SRCPATH)/, $(SRCS) $(SRCCLI) $(SRCSO)), $(CC) $(CFLAGS) $(SRC) $(DEPMT) $(SRC:$(SRCPATH)/%.cc=%.o) $(DEPMM) 1>> .depend;)
	@$(foreach SRC, $(addprefix $(SRCPATH)/, $(SRCSAVX)), $(CC) $(CFLAGS) $(SRC) $(DEPMT) $(SRC:$(SRCPATH)/%.cc=%.o) $(DEPMM) 1>> .depend;)
endif

config.mak:
	./configure

depend: .depend
ifneq ($(wildcard .depend),)
include .depend
endif

SRC2 = $(SRCS) $(SRCCLI)
# These should cover most of the important codepaths
OPT0 = --crf 30 -b1 -m1 -r1 --me dia --no-cabac --direct temporal --ssim --no-weightb
OPT1 = --crf 16 -b2 -m3 -r3 --me hex --no-8x8dct --direct spatial --no-dct-decimate -t0  --slice-max-mbs 50
OPT2 = --crf 26 -b4 -m5 -r2 --me hex --cqm jvt --nr 100 --psnr --no-mixed-refs --b-adapt 2 --slice-max-size 1500
OPT3 = --crf 18 -b3 -m9 -r5 --me umh -t1 -A all --b-pyramid normal --direct auto --no-fast-pskip --no-mbtree
OPT4 = --crf 22 -b3 -m7 -r4 --me esa -t2 -A all --psy-rd 1.0:1.0 --slices 4
OPT5 = --frames 50 --crf 24 -b3 -m10 -r3 --me tesa -t2
OPT6 = --frames 50 -q0 -m9 -r2 --me hex -Aall
OPT7 = --frames 50 -q0 -m2 -r1 --me hex --no-cabac

ifeq (,$(VIDS))
fprofiled:
	@echo 'usage: make fprofiled VIDS="infile1 infile2 ..."'
	@echo 'where infiles are anything that davs2 understands,'
	@echo 'i.e. YUV with resolution in the filename, y4m, or avisynth.'
else
fprofiled:
	$(MAKE) clean
	$(MAKE) davs2$(EXE) CFLAGS="$(CFLAGS) $(PROF_GEN_CC)" LDFLAGS="$(LDFLAGS) $(PROF_GEN_LD)"
	$(foreach V, $(VIDS), $(foreach I, 0 1 2 3 4 5 6 7, ./davs2$(EXE) $(OPT$I) --threads 1 $(V) -o $(DEVNULL) ;))
ifeq ($(COMPILER),CL)
# Because Visual Studio timestamps the object files within the PGD, it fails to build if they change - only the executable should be deleted
	rm -f davs2$(EXE)
else
	rm -f $(SRC2:%.cc=%.o)
endif
	$(MAKE) CFLAGS="$(CFLAGS) $(PROF_USE_CC)" LDFLAGS="$(LDFLAGS) $(PROF_USE_LD)"
	rm -f $(SRC2:%.cc=%.gcda) $(SRC2:%.cc=%.gcno) *.dyn pgopti.dpi pgopti.dpi.lock *.pgd *.pgc
endif

clean:
	rm -f $(OBJS) $(OBJASM) $(OBJCLI) $(OBJSO) $(SONAME) 
	rm -f *.a *.lib *.exp *.pdb libdavs2.so* davs2 davs2.exe .depend TAGS
	rm -f checkasm checkasm.exe $(OBJCHK) $(GENERATED) davs2_lookahead.cclbin
	rm -f example example.exe $(OBJEXAMPLE)
	rm -f $(SRC2:%.cc=%.gcda) $(SRC2:%.cc=%.gcno) *.dyn pgopti.dpi pgopti.dpi.lock *.pgd *.pgc

distclean: clean
	rm -f config.mak davs2_config.h config.h config.log davs2.pc davs2.def conftest*

install-cli: cli
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) davs2$(EXE) $(DESTDIR)$(bindir)

install-lib-dev:
	$(INSTALL) -d $(DESTDIR)$(includedir)
	$(INSTALL) -d $(DESTDIR)$(libdir)
	$(INSTALL) -d $(DESTDIR)$(libdir)/pkgconfig
	$(INSTALL) -m 644 $(SRCPATH)/davs2.h $(DESTDIR)$(includedir)
	$(INSTALL) -m 644 davs2_config.h $(DESTDIR)$(includedir)
	$(INSTALL) -m 644 davs2.pc $(DESTDIR)$(libdir)/pkgconfig

install-lib-static: lib-static install-lib-dev
	$(INSTALL) -m 644 $(LIBDAVS2) $(DESTDIR)$(libdir)
	$(if $(RANLIB), $(RANLIB) $(DESTDIR)$(libdir)/$(LIBDAVS2))

install-lib-shared: lib-shared install-lib-dev
ifneq ($(IMPLIBNAME),)
	$(INSTALL) -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 $(SONAME) $(DESTDIR)$(bindir)
	$(INSTALL) -m 644 $(IMPLIBNAME) $(DESTDIR)$(libdir)
else ifneq ($(SONAME),)
	ln -f -s $(SONAME) $(DESTDIR)$(libdir)/libdavs2.$(SOSUFFIX)
	$(INSTALL) -m 755 $(SONAME) $(DESTDIR)$(libdir)
endif

uninstall:
	rm -f $(DESTDIR)$(includedir)/davs2.h $(DESTDIR)$(includedir)/davs2_config.h $(DESTDIR)$(libdir)/libdavs2.a
	rm -f $(DESTDIR)$(bindir)/davs2$(EXE) $(DESTDIR)$(libdir)/pkgconfig/davs2.pc
ifneq ($(IMPLIBNAME),)
	rm -f $(DESTDIR)$(bindir)/$(SONAME) $(DESTDIR)$(libdir)/$(IMPLIBNAME)
else ifneq ($(SONAME),)
	rm -f $(DESTDIR)$(libdir)/$(SONAME) $(DESTDIR)$(libdir)/libdavs2.$(SOSUFFIX)
endif

etags: TAGS

TAGS:
	etags $(SRCS)
