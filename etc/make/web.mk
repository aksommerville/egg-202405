# web.mk
# Builds the skeleton web embedder.

web_MIDDIR:=mid/web
web_OUTDIR:=out/web

web_HEADLESS_TEMPLATE:=$(web_OUTDIR)/egg-headless.html
all:$(web_HEADLESS_TEMPLATE)
web_HEADLESS_INPUTS:=$(filter src/web/js/%,$(SRCFILES)) src/web/tm.html src/web/egg.js
$(web_HEADLESS_TEMPLATE):$(tools_webtm_EXE) $(web_HEADLESS_INPUTS);$(PRECMD) $(tools_webtm_EXE) -o$@ --js=src/web/egg.js --html=src/web/tm.html
