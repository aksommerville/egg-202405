# TODO Copied blindly from Fly By Night, along with all the "ms*" units. Not yet fitted.

mswin_OPT_ENABLE+=mswin
mswin_OPT_ENABLE+=hostio simplifio fs serial midi wav http
mswin_OPT_ENABLE+=bmp gif ico jpeg png qoi rlead rawimg

mswin_CCWARN:=-Werror -Wimplicit
mswin_CCINC:=-Isrc -I$(mswin_MIDDIR)
mswin_CCDEF:=$(patsubst %,-DUSE_%=1,$(mswin_OPT_ENABLE)) -D_POSIX_THREAD_SAFE_FUNCTIONS -D_PTW32_STATIC_LIB=1 -D_WIN32_WINNT=0x0501
mswin_CCOPT:=-c -MMD -O3 -m32 -mwindows $(mswin_CCWARN) $(mswin_CCINC) $(mswin_CCDEF) $(mswin_CC_EXTRA)
mswin_LDOPT:=$(mswin_LD_EXTRA) -m32 -mwindows -Wl,-static

mswin_CC:=$(mswin_TOOLCHAIN)gcc $(mswin_CCOPT)
mswin_AS:=$(mswin_TOOLCHAIN)gcc -xassembler-with-cpp $(mswin_CCOPT)
mswin_CXX:=$(mswin_TOOLCHAIN)g++ $(mswin_CCOPT)
mswin_OBJC:=$(mswin_TOOLCHAIN)gcc -xobjective-c $(mswin_CCOPT)
mswin_LD:=$(mswin_TOOLCHAIN)gcc $(mswin_LDOPT)
mswin_LDPOST:=-lm -lz -lopengl32 -lwinmm -lhid $(mswin_LDPOST_EXTRA)

mswin_CFILES:=$(filter \
  src/main/% \
  $(addprefix src/opt/,$(addsuffix /%,$(mswin_OPT_ENABLE))) \
,$(CFILES)) \

mswin_OFILES:=$(patsubst src/%,$(mswin_MIDDIR)/%.o,$(basename $(mswin_CFILES)))
-include $(mswin_OFILES:.o=.d)

$(mswin_MIDDIR)/%.o:src/%.c;$(PRECMD) $(mswin_CC) -o$@ $<
$(mswin_MIDDIR)/%.o:src/%.s;$(PRECMD) $(mswin_AS) -o$@ $<
$(mswin_MIDDIR)/%.o:src/%.cxx;$(PRECMD) $(mswin_CXX) -o$@ $<
$(mswin_MIDDIR)/%.o:src/%.m;$(PRECMD) $(mswin_OBJC) -o$@ $<

mswin_EXE:=$(mswin_OUTDIR)/$(APPNAME_LOWER).exe
all:$(mswin_EXE)
$(mswin_EXE):$(mswin_OFILES);$(PRECMD) $(mswin_LD) -o$@ $(mswin_OFILES) $(mswin_LDPOST)

mswin-run:$(mswin_EXE);$(mswin_EXE) $(mswin_RUN_ARGS)
