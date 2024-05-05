# demos.mk
# Rules to build our demo ROM files, from src/demo/

demos_MIDDIR:=mid/demo
demos_OUTDIR:=out/rom

demos_CCWARN:=-Werror -Wimplicit -Wno-comment -Wno-parentheses
demos_CCINC:=-Isrc -I$(demos_MIDDIR)
demos_CCDEF:=$(patsubst %,-DUSE_%=1,$(demos_OPT_ENABLE))
demos_CCOPT:=-c -MMD -O3 $(demos_CCWARN) $(demos_CCINC) $(demos_CCDEF) $(demos_CC_EXTRA)
demos_LDOPT:=$(demos_LD_EXTRA) -Wl,--no-entry -Wl,--export-table -Wl,--export-all -Wl,--import-undefined -Wl,--allow-undefined -Wl,--initial-memory=10485760

demos_CC:=$(WASI_SDK)/bin/clang $(demos_CCOPT)
demos_AS:=$(WASI_SDK)/bin/clang -xassembler-with-cpp $(demos_CCOPT)
demos_CXX:=$(WASI_SDK)/bin/clang $(demos_CCOPT)
demos_OBJC:=$(WASI_SDK)/bin/clang -xobjective-c $(demos_CCOPT)
demos_LD:=$(WASI_SDK)/bin/clang $(demos_LDOPT)
demos_LDPOST:=$(demos_LDPOST_EXTRA) 

define DEMO_RULES
  demos_$1_SRCFILES:=$(filter src/demo/$1/%,$(SRCFILES))
  demos_$1_CFILES:=$$(filter %.c %.cxx %.s %.m,$$(demos_$1_SRCFILES))
  demos_$1_OFILES:=$$(patsubst src/demo/%,$(demos_MIDDIR)/%.o,$$(basename $$(demos_$1_CFILES)))
  -include $$(demos_$1_OFILES:.o=.d)
  demos_$1_ROM:=$(demos_OUTDIR)/$1.egg
  all:$$(demos_$1_ROM)
  demos-all:$$(demos_$1_ROM)
  ifneq (,$(strip $$(demos_$1_OFILES)))
    demos_$1_WASMMOD:=$(demos_MIDDIR)/$1/1.wasm
    $$(demos_$1_WASMMOD):$$(demos_$1_OFILES);$$(PRECMD) $(demos_LD) -o$$@ $$(demos_$1_OFILES) $(demos_LDPOST)
  else
    demos_$1_WASMMOD:=
  endif
  demos_$1_DATAFILES:=$$(filter src/demo/$1/data/%,$$(demos_$1_SRCFILES))
  $$(demos_$1_ROM):$$(demos_$1_WASMMOD) $$(demos_$1_DATAFILES) $(tools_eggrom_EXE);$$(PRECMD) $(tools_eggrom_EXE) -c -o$$@ $$(demos_$1_WASMMOD) src/demo/$1/data
endef

demos_DEMOS:=$(notdir $(wildcard src/demo/*))
ifeq (,$(strip $(WASI_SDK)))
  # WASI_SDK unset, don't build the demos. (We could still build the JS-only ones, but meh).
  demos_DEMOS:=
endif
$(foreach D,$(demos_DEMOS),$(eval $(call DEMO_RULES,$D)))

$(demos_MIDDIR)/%.o:src/demo/%.c;$(PRECMD) $(demos_CC) -o$@ $<
$(demos_MIDDIR)/%.o:src/demo/%.cxx;$(PRECMD) $(demos_CXX) -o$@ $<
$(demos_MIDDIR)/%.o:src/demo/%.s;$(PRECMD) $(demos_AS) -o$@ $<
$(demos_MIDDIR)/%.o:src/demo/%.m;$(PRECMD) $(demos_OBJC) -o$@ $<

#------------------------------------------------------------------------------
# Everything below this point is for building the C-only demos in true-native mode.
# Technically, we could bake QuickJS into them too, and allow JS code, but what's the point?
# Bundled executables would work the same way.

ifneq (,$(strip $(demos_NATIVE_CC)))

define DEMO_NATIVE_RULES
  demos_$1_NATIVE_ARCHIVE:=$(demos_MIDDIR)/native/$1/data.eggar
  demos_$1_NATIVE_ARCHIVE_C:=$(demos_MIDDIR)/native/$1/data.c
  demos_$1_NATIVE_SRCFILES:=$(filter src/demo/$1/%,$(SRCFILES))
  demos_$1_NATIVE_CFILES:=$$(filter %.c,$$(demos_$1_NATIVE_SRCFILES)) $$(demos_$1_NATIVE_ARCHIVE_C)
  demos_$1_NATIVE_OFILES:=$$(patsubst src/demo/%,$(demos_MIDDIR)/native/%,$$(addsuffix .o,$$(basename $$(demos_$1_NATIVE_CFILES))))
  -include $$(demos_$1_NATIVE_OFILES:.o=.d)
  demos_$1_EXE:=$(demos_OUTDIR)/$1.exe
  all:$$(demos_$1_EXE)
  demos-all:$$(demos_$1_EXE)
  demos_$1_NATIVE_DATAFILES:=$$(filter src/demo/$1/data/%,$$(demos_$1_NATIVE_SRCFILES))
  $$(demos_$1_NATIVE_ARCHIVE_C):$$(demos_$1_NATIVE_ARCHIVE);$$(PRECMD) node etc/tool/cbin.js -o$$@ $$< --name=egg_bundled_rom
  $$(demos_$1_NATIVE_ARCHIVE):$$(demos_$1_NATIVE_DATAFILES) $(tools_eggrom_EXE);$$(PRECMD) $(tools_eggrom_EXE) -c -o$$@ src/demo/$1/data
  $$(demos_$1_EXE):$$(demos_$1_NATIVE_OFILES) $(demos_NATIVE_LIBEGG) \
    ;$$(PRECMD) $(demos_NATIVE_LD) -o$$@ $$(demos_$1_NATIVE_OFILES) $(demos_NATIVE_LIBEGG) $(demos_NATIVE_LDPOST)
endef

demos_NATIVE:=$(filter lowasm,$(demos_DEMOS))
$(foreach D,$(demos_NATIVE),$(eval $(call DEMO_NATIVE_RULES,$D)))

$(demos_MIDDIR)/native/%.o:src/demo/%.c;$(PRECMD) $(demos_NATIVE_CC) -o$@ $<
$(demos_MIDDIR)/native/%.o:$(demos_MIDDIR)/native/%.c;$(PRECMD) $(demos_NATIVE_CC) -o$@ $<

endif
