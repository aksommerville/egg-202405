# common.mk
# Shared bits that load after host config but before any real rules.

SRCFILES:=$(shell find src -type f)
CFILES:=$(filter %.c %.cxx %.m %.s,$(SRCFILES))
