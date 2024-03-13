# etc/target/macos.mk
# Build for MacOS (>=10).

#TODO This Makefile, and all the Mac opt units, copied blindly from flybynight. Will require some touch-up at least.

macos_MIDDIR:=mid/macos
macos_OUTDIR:=out/macos

macos_OPT_ENABLE+=macaudio machid macos macwm \
  rawimg bmp gif ico qoi rlead png \
  midi wav fs serial \
  hostio simplifio http

macos_CC:=$(macos_TOOLCHAIN)gcc -c -MMD -O3 -Isrc -I$(macos_MIDDIR) \
  -Werror -Wimplicit -Wno-parentheses -Wno-comment \
  -DINCBIN_SILENCE_BITCODE_WARNING \
  $(patsubst %,-DUSE_%=1,$(macos_OPT_ENABLE)) $(macos_CC_EXTRA)
macos_OBJC:=$(macos_CC) -xobjective-c
macos_LD:=$(macos_TOOLCHAIN)gcc $(macos_LD_EXTRA)
macos_LDPOST:=-lm -lz -framework IOKit -framework Cocoa -framework OpenGL \
  -framework Quartz -framework CoreGraphics -framework AudioUnit $(macos_LDPOST_EXTRA)

# TODO Icons, XIB, and Plist: Can we generate these from shared metadata?
# They're stubbed with neutral content initially, but would be easy to forget to fill those in.
macos_ICONS_DIR:=src/opt/macos/appicon.iconset
macos_XIB:=src/opt/macos/Main.xib
macos_PLIST_SRC:=src/opt/macos/Info.plist
macos_ICONS:=$(wildcard $(macos_ICONS_DIR)/*)

macos_BUNDLE:=$(macos_OUTDIR)/$(APPNAME_CAMEL).app
macos_PLIST:=$(macos_BUNDLE)/Contents/Info.plist
macos_NIB:=$(macos_BUNDLE)/Contents/Resources/Main.nib
macos_EXE:=$(macos_BUNDLE)/Contents/MacOS/$(APPNAME_LOWER)
macos_ICON:=$(macos_BUNDLE)/Contents/Resources/appicon.icns

$(macos_NIB):$(macos_XIB);$(PRECMD) ibtool --compile $@ $<
$(macos_PLIST):$(macos_PLIST_SRC);$(PRECMD) cp $< $@
$(macos_ICON):$(macos_ICONS);$(PRECMD) iconutil -c icns -o $@ $(macos_ICONS_DIR)

macos_CFILES:=$(filter \
  src/main/% \
  $(addprefix src/opt/,$(addsuffix /%,$(macos_OPT_ENABLE))) \
,$(CFILES)) \

macos_OFILES:=$(patsubst src/%,$(macos_MIDDIR)/%.o,$(basename $(macos_CFILES)))
-include $(macos_OFILES:.o=.d)

$(macos_MIDDIR)/%.o:src/%.c;$(PRECMD) $(macos_CC) -o$@ $<
#$(macos_MIDDIR)/%.o:src/%.s;$(PRECMD) $(macos_AS) -o$@ $<
#$(macos_MIDDIR)/%.o:src/%.cxx;$(PRECMD) $(macos_CXX) -o$@ $<
$(macos_MIDDIR)/%.o:src/%.m;$(PRECMD) $(macos_OBJC) -o$@ $<

all:$(macos_EXE)
$(macos_EXE):$(macos_OFILES);$(PRECMD) $(macos_LD) -o$@ $(macos_OFILES) $(macos_LDPOST)

macos_OUTPUTS:=$(macos_EXE) $(macos_ICON) $(macos_NIB) $(macos_PLIST)
macos-all:$(macos_OUTPUTS)
all:$(macos_OUTPUTS)

ifneq (,$(strip $(macos_CODESIGN_NAME)))
  macos-all:macos-codesign
  macos-codesign:macos-outputs;$(PRECMD) \
    codesign -s '$(macos_CODESIGN_NAME)' -f $(macos_BUNDLE)
endif

macos-run:macos-all;open -W $(macos_BUNDLE) --args --reopen-tty=$$(tty) --chdir=$$(pwd) $(macos_RUN_ARGS)
