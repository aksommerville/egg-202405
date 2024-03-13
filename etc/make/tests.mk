# tests.mk
# Rules for building and running tests.

tests_MIDDIR:=mid/tests
tests_OUTDIR:=out/tests

# Name here the units that should be able to build for any build host.
# OS-specific things, declare in config.mk.
tests_OPT_ENABLE+=bmp fs gif hostio ico midi png qoi rawimg rlead serial wav qjs wamr romr romw

tests_CCINC:=-I$(WAMR_SDK)/core/iwasm/include -I$(QJS_SDK) -Isrc -I$(tests_MIDDIR)
tests_CCDEF:=$(patsubst %,-DUSE_%=1,$(tests_OPT_ENABLE))
tests_CC:=$(tests_TOOLCHAIN)gcc -c -MMD -O0 -Werror -Wimplicit $(tests_CCDEF) $(tests_CCINC) $(tests_CC_EXTRA)
tests_OBJC:=$(tests_TOOLCHAIN)gcc -xobjective-c -c -MMD -O0 -Werror -Wimplicit $(tests_CCDEF) $(tests_CCINC) $(tests_CC_EXTRA)
tests_INT_LD:=$(tests_TOOLCHAIN)gcc $(tests_LD_EXTRA)
tests_INT_LDPOST:=-lm -lz -lpthread $(tests_LDPOST_EXTRA)
tests_UNIT_LD:=$(tests_TOOLCHAIN)gcc $(tests_LD_EXTRA)
tests_UNIT_LDPOST:=-lm

ifneq (,$(strip $(filter jpeg,$(tests_OPT_ENABLE))))
  tests_INT_LDPOST+=-ljpeg
endif

tests_CFILES_COMMON:=$(filter src/test/common/%,$(CFILES))
tests_OFILES_COMMON:=$(patsubst src/%,$(tests_MIDDIR)/%.o,$(basename $(tests_CFILES_COMMON)))
-include $(tests_OFILES_COMMON:.o=.d)
tests_GENHEADERS:=$(tests_MIDDIR)/test/int/itest_toc.h

tests_CFILES_UNIT:=$(filter src/test/unit/%,$(CFILES))
tests_OFILES_UNIT:=$(patsubst src/%,$(tests_MIDDIR)/%.o,$(basename $(test_CFILES_UNIT)))
tests_EXES_UNIT:=$(patsubst %.o,%,$(tests_OFILES_UNIT))
-include $(tests_OFILES_UNIT:.o=.d)
all:$(tests_EXES_UNIT)

$(tests_MIDDIR)/test/unit/% \
  :$(tests_MIDDIR)/test/unit/%.o $(tests_OFILES_COMMON)|$(tests_GENHEADERS) \
  ;$(PRECMD) $(tests_UNIT_LD) -o$@ $< $(tests_OFILES_COMMON) $(tests_UNIT_LDPOST)

tests_CFILES_INT:=$(filter \
  src/test/int/% \
  $(addprefix src/opt/,$(addsuffix /%,$(tests_OPT_ENABLE))) \
,$(CFILES)) $(tests_CFILES_COMMON)

tests_OFILES_INT:=$(patsubst src/%,$(tests_MIDDIR)/%.o,$(basename $(tests_CFILES_INT)))
-include $(tests_OFILES_INT:.o=.d)
tests_EXE_INT:=$(tests_MIDDIR)/test/itest
all:$(tests_EXE_INT)

$(tests_MIDDIR)/%.o:src/%.c|$(tests_GENHEADERS);$(PRECMD) $(tests_CC) -o$@ $<
$(tests_MIDDIR)/%.o:src/%.m|$(tests_GENHEADERS);$(PRECMD) $(tests_OBJC) -o$@ $<
$(tests_EXE_INT):$(tests_OFILES_INT);$(PRECMD) $(tests_INT_LD) -o$@ $^ $(tests_INT_LDPOST)

$(tests_MIDDIR)/test/int/itest_toc.h:$(tests_CFILES_INT);$(PRECMD) etc/tool/gen-itest-toc.sh $@ $^

#TODO Automation, Javascript.
test:$(tests_EXES_UNIT) $(tests_EXE_INT);trap '' INT ; etc/tool/run-tests.sh $(tests_EXES_UNIT) $(tests_EXE_INT)
test-%:$(tests_EXES_UNIT) $(tests_EXE_INT);trap '' INT ; TEST_FILTER="$*" etc/tool/run-tests.sh $(tests_EXES_UNIT) $(tests_EXE_INT)
