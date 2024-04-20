# tools.mk
# Rules for building build-time tools.

tools_MIDDIR:=mid/tool
tools_OUTDIR:=out/tool

tools_OPT_ENABLE+=fs serial romr romw midi wav http process ossmidi hostio synth pcmprint pcmprintc sfg
tools_OPT_ENABLE+=qoi rlead rawimg bmp gif ico png

tools_CCWARN:=-Werror -Wimplicit
tools_CCINC:=-Isrc -I$(tools_MIDDIR)
tools_CCDEF:=$(patsubst %,-DUSE_%=1,$(tools_OPT_ENABLE))
tools_CCOPT:=-c -MMD -O3 $(tools_CCWARN) $(tools_CCINC) $(tools_CCDEF) $(tools_CC_EXTRA)
tools_LDOPT:=$(tools_LD_EXTRA)

tools_CC:=$(tools_TOOLCHAIN)gcc $(tools_CCOPT)
tools_AS:=$(tools_TOOLCHAIN)gcc -xassembler-with-cpp $(tools_CCOPT)
tools_CXX:=$(tools_TOOLCHAIN)g++ $(tools_CCOPT)
tools_OBJC:=$(tools_TOOLCHAIN)gcc -xobjective-c $(tools_CCOPT)
tools_LD:=$(tools_TOOLCHAIN)gcc $(tools_LDOPT)
tools_LDPOST:=-lm -lz -lpthread $(tools_LDPOST_EXTRA)

ifneq (,$(strip $(filter jpeg,$(tools_OPT_ENABLE))))
  tools_LDPOST+=-ljpeg
endif
ifneq (,$(strip $(filter asound,$(tools_OPT_ENABLE))))
  tools_LDPOST+=-lasound
endif
ifneq (,$(strip $(filter pulse,$(tools_OPT_ENABLE))))
  tools_LDPOST+=-lpulse -lpulse-simple
endif

$(tools_MIDDIR)/%.o:src/%.c;$(PRECMD) $(tools_CC) -o$@ $<
$(tools_MIDDIR)/%.o:src/%.cxx;$(PRECMD) $(tools_CXX) -o$@ $<
$(tools_MIDDIR)/%.o:src/%.s;$(PRECMD) $(tools_AS) -o$@ $<
$(tools_MIDDIR)/%.o:src/%.m;$(PRECMD) $(tools_OBJC) -o$@ $<

tools_SRCFILES:=$(filter src/tool/%,$(SRCFILES)) $(filter $(addprefix src/opt/,$(addsuffix /%,$(tools_OPT_ENABLE))),$(SRCFILES))

define TOOL_RULES
  tools_$1_CFILES:=$(filter %.c,$(filter src/tool/common/% src/tool/$1/% src/opt/%,$(tools_SRCFILES)))
  tools_$1_OFILES:=$$(patsubst src/%,$(tools_MIDDIR)/%.o,$$(basename $$(tools_$1_CFILES)))
  -include $$(tools_$1_OFILES:.o=.d)
  tools_$1_EXE:=$(tools_OUTDIR)/$1
  all:$$(tools_$1_EXE)
  $$(tools_$1_EXE):$$(tools_$1_OFILES);$$(PRECMD) $(tools_LD) -o$$@ $$(tools_$1_OFILES) $(tools_LDPOST)
endef

tools_TOOLS:=$(filter-out common,$(notdir $(wildcard src/tool/*)))
$(foreach T,$(tools_TOOLS),$(eval $(call TOOL_RULES,$T)))

serve:$(tools_server_EXE) demos-all src/web/js/Instruments.js;$(tools_server_EXE) --port=8080 --htdocs=src/web --makeable-dir=out/rom
serve-public:$(tools_server_EXE) demos-all src/web/js/Instruments.js;$(tools_server_EXE) --port=8080 --htdocs=src/web --makeable-dir=out/rom --listen-remote

synthwerk:$(tools_synthwerk_EXE);$(tools_synthwerk_EXE)
