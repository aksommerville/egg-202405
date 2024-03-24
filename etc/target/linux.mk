# The truly optional things should be set by config.mk:
#   glx x11fb xinerama drmgx drmfb alsafd asound pulse evdev
linux_OPT_ENABLE+=hostio fs serial qjs wamr timer romr romw strfmt localstore http render synth midi
linux_OPT_ENABLE+=qoi rlead rawimg gif jpeg bmp ico png

linux_CCWARN:=-Werror -Wimplicit
linux_CCINC:=-Isrc -I$(linux_MIDDIR) -I$(QJS_SDK) -I$(WAMR_SDK)/core/iwasm/include
linux_CCDEF:=$(patsubst %,-DUSE_%=1,$(linux_OPT_ENABLE))
linux_CCOPT:=-c -MMD -O3 $(linux_CCWARN) $(linux_CCINC) $(linux_CCDEF) $(linux_CC_EXTRA)
linux_LDOPT:=$(linux_LD_EXTRA)

linux_CC:=$(linux_TOOLCHAIN)gcc $(linux_CCOPT)
linux_AS:=$(linux_TOOLCHAIN)gcc -xassembler-with-cpp $(linux_CCOPT)
linux_CXX:=$(linux_TOOLCHAIN)g++ $(linux_CCOPT)
linux_OBJC:=$(linux_TOOLCHAIN)gcc -xobjective-c $(linux_CCOPT)
linux_LD:=$(linux_TOOLCHAIN)gcc $(linux_LDOPT)
linux_LDPOST:=-lm -lz -lpthread $(linux_LDPOST_EXTRA) $(QJS_SDK)/libquickjs.a $(WAMR_SDK)/build/libvmlib.a
linux_AR:=$(linux_TOOLCHAIN)ar rc

ifneq (,$(strip $(filter drmgx,$(linux_OPT_ENABLE))))
  linux_CC+=-I/usr/include/libdrm
  linux_LDPOST+=-ldrm -lgbm -lEGL
else ifneq (,$(strip $(filter drmfb,$(linux_OPT_ENABLE))))
  linux_CC+=-I/usr/include/libdrm
  linux_LDPOST+=-ldrm
endif
ifneq (,$(strip $(filter glx,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lX11 -lGL
else ifneq (,$(strip $(filter x11fb,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lX11
endif
ifneq (,$(strip $(filter xegl,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lX11 -lEGL -lGL
endif
ifneq (,$(strip $(filter xinerama,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lXinerama
endif
ifneq (,$(strip $(filter asound,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lasound
endif
ifneq (,$(strip $(filter pulse,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-lpulse-simple
endif
ifneq (,$(strip $(filter jpeg,$(linux_OPT_ENABLE))))
  linux_LDPOST+=-ljpeg
endif

linux_CFILES:=$(filter \
  src/native/% \
  $(addprefix src/opt/,$(addsuffix /%,$(linux_OPT_ENABLE))) \
,$(CFILES)) \

linux_OFILES:=$(filter-out $(linux_MIDDIR)/native/egg_native_rom.%,$(patsubst src/%,$(linux_MIDDIR)/%.o,$(basename $(linux_CFILES))))

# src/native/egg_native_rom.c is special; it gets built 3 different ways for the 3 different ROM file sources.
linux_OFILES+= \
  $(linux_MIDDIR)/native/egg_native_rom.native.o \
  $(linux_MIDDIR)/native/egg_native_rom.bundled.o \
  $(linux_MIDDIR)/native/egg_native_rom.external.o

-include $(linux_OFILES:.o=.d)

$(linux_MIDDIR)/%.o:src/%.c;$(PRECMD) $(linux_CC) -o$@ $<
$(linux_MIDDIR)/%.o:src/%.s;$(PRECMD) $(linux_AS) -o$@ $<
$(linux_MIDDIR)/%.o:src/%.cxx;$(PRECMD) $(linux_CXX) -o$@ $<
$(linux_MIDDIR)/%.o:src/%.m;$(PRECMD) $(linux_OBJC) -o$@ $<

$(linux_MIDDIR)/native/egg_native_rom.native.o:src/native/egg_native_rom.c;$(PRECMD) $(linux_CC) -o$@ $< -DEGG_ROM_SOURCE=NATIVE
$(linux_MIDDIR)/native/egg_native_rom.bundled.o:src/native/egg_native_rom.c;$(PRECMD) $(linux_CC) -o$@ $< -DEGG_ROM_SOURCE=BUNDLED
$(linux_MIDDIR)/native/egg_native_rom.external.o:src/native/egg_native_rom.c;$(PRECMD) $(linux_CC) -o$@ $< -DEGG_ROM_SOURCE=EXTERNAL

linux_EXE:=$(linux_OUTDIR)/$(APPNAME_LOWER)
all:$(linux_EXE)
linux_OFILES_EXE:=$(filter-out $(linux_MIDDIR)/native/egg_native_rom.native.o $(linux_MIDDIR)/native/egg_native_rom.bundled.o,$(linux_OFILES))
$(linux_EXE):$(linux_OFILES_EXE);$(PRECMD) $(linux_LD) -o$@ $(linux_OFILES_EXE) $(linux_LDPOST)

linux_BUNDLED_LIB:=$(linux_OUTDIR)/libegg-bundled.a
all:$(linux_BUNDLED_LIB)
linux_OFILES_BUNDLED_LIB:=$(filter-out $(linux_MIDDIR)/native/egg_native_rom.native.o $(linux_MIDDIR)/native/egg_native_rom.external.o,$(linux_OFILES))
$(linux_BUNDLED_LIB):$(linux_OFILES_BUNDLED_LIB);$(PRECMD) $(linux_AR) $@ $(linux_OFILES_BUNDLED_LIB)

linux_NATIVE_LIB:=$(linux_OUTDIR)/libegg-native.a
all:$(linux_NATIVE_LIB)
linux_OFILES_NATIVE_LIB:=$(filter-out $(linux_MIDDIR)/native/egg_native_rom.bundled.o $(linux_MIDDIR)/native/egg_native_rom.external.o,$(linux_OFILES))
$(linux_NATIVE_LIB):$(linux_OFILES_NATIVE_LIB);$(PRECMD) $(linux_AR) $@ $(linux_OFILES_NATIVE_LIB)

linux-run:$(linux_EXE) demos-all;$(linux_EXE) $(linux_RUN_ARGS)
