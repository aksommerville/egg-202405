# web.mk

web_OPT_ENABLE+=

web_CCWARN:=-Werror -Wimplicit -Wno-comment -Wno-parentheses
web_CCINC:=-Isrc -I$(web_MIDDIR)
web_CCDEF:=$(patsubst %,-DUSE_%=1,$(web_OPT_ENABLE))
web_CCOPT:=-c -MMD -O3 $(web_CCWARN) $(web_CCINC) $(web_CCDEF) $(web_CC_EXTRA)
web_LDOPT:=$(web_LD_EXTRA) -nostdlib -Wl,--no-entry -Wl,--export-table -Wl,--export-all -Wl,--import-undefined -Wl,--allow-undefined -Wl,--initial-memory=1048576

web_CC:=$(WASI_SDK)/bin/clang $(web_CCOPT)
web_AS:=$(WASI_SDK)/bin/clang -xassembler-with-cpp $(web_CCOPT)
web_CXX:=$(WASI_SDK)/bin/clang $(web_CCOPT)
web_OBJC:=$(WASI_SDK)/bin/clang -xobjective-c $(web_CCOPT)
web_LD:=$(WASI_SDK)/bin/clang $(web_LDOPT)
web_LDPOST:=$(web_LDPOST_EXTRA)

web_CFILES:=$(filter \
  src/wasm/% \
  $(addprefix src/opt/,$(addsuffix /%,$(web_OPT_ENABLE))) \
,$(CFILES)) \

web_OFILES:=$(patsubst src/%,$(web_MIDDIR)/%.o,$(basename $(web_CFILES)))
-include $(web_OFILES:.o=.d)

$(web_MIDDIR)/%.o:src/%.c;$(PRECMD) $(web_CC) -o$@ $<
$(web_MIDDIR)/%.o:src/%.s;$(PRECMD) $(web_AS) -o$@ $<
$(web_MIDDIR)/%.o:src/%.cxx;$(PRECMD) $(web_CXX) -o$@ $<
$(web_MIDDIR)/%.o:src/%.m;$(PRECMD) $(web_OBJC) -o$@ $<

web_MODS:=$(notdir $(wildcard src/wasm/*))
define web_MOD_RULES
  web_MOD_$1:=$(web_OUTDIR)/$1.wasm
  all:$$(web_MOD_$1)
  web_MOD_$1_OFILES:=$$(filter $(web_MIDDIR)/opt/% $(web_MIDDIR)/wasm/$1/%,$(web_OFILES))
  $$(web_MOD_$1):$$(web_MOD_$1_OFILES);$$(PRECMD) $(web_LD) -o$$@ $$(web_MOD_$1_OFILES) $(web_LDPOST)
endef
$(foreach M,$(web_MODS),$(eval $(call web_MOD_RULES,$M)))

web_INSTRUMENTS:=src/web/js/Instruments.js
all:$(web_INSTRUMENTS)
$(web_INSTRUMENTS):src/opt/synth/synth_builtin.c etc/tool/mkinstruments.js;$(PRECMD) node etc/tool/mkinstruments.js src/opt/synth/synth_builtin.c $@

web-run:$(web_MODS_OUT) $(web_INSTRUMENTS);echo "TODO HTTP server?" ; exit 1

test:$(web_MODS_OUT) $(web_INSTRUMENTS)
test-%:$(web_MODS_OUT) $(web_INSTRUMENTS)
