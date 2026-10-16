# Syntax highlighting test for Makefiles: variables, functions, conditionals,
# rules, pattern rules, recipes, defines and includes.

SHELL := /bin/sh
.DEFAULT_GOAL := all
.PHONY: all clean test install
.SUFFIXES:

PREFIX     ?= /usr/local
BUILD_DIR  = build
CC         := gcc
CFLAGS    += -O2 -Wall -Wextra
LDFLAGS    =
SOURCES    := $(wildcard src/*.c) $(patsubst lib/%.c,build/%.c,$(wildcard lib/*.c))
OBJECTS    := $(SOURCES:%.c=$(BUILD_DIR)/%.o)
VERSION    != git describe --always
export CC CFLAGS
unexport TMPDIR
override DEBUG := 0
vpath %.h include

ifeq ($(DEBUG),1)
  CFLAGS += -g -DDEBUG
else ifneq ($(strip $(CC)),)
  CFLAGS += -DNDEBUG
else
  $(error no compiler configured)
endif

ifdef PREFIX
  INSTALL_DIR := $(PREFIX)/bin
endif

ifndef NO_TESTS
  TESTS := $(shell ls tests/*.sh 2>/dev/null)
endif

define compile_rule
$(1): $(2)
	@echo "building $$@"
	$(CC) $(CFLAGS) -c $$< -o $$@
endef

$(eval $(call compile_rule,$(BUILD_DIR)/main.o,src/main.c))

include config.mk
-include local.mk

all: app $(OBJECTS)
	@echo "Built $(words $(OBJECTS)) objects, version $(VERSION)"

app: $(OBJECTS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ -lm

$(BUILD_DIR)/%.o: %.c config.h
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	-mkdir $@

%.html: %.md
	pandoc --from markdown --to html $< > $@

test: app
	@for t in $(TESTS); do sh $$t || exit 1; done
	@echo "all tests passed" # trailing comment

install: all
	install -d $(INSTALL_DIR)
	install -m 755 app $(INSTALL_DIR)/app
	@echo "installed to \
	      $(INSTALL_DIR)"

clean:
	rm -rf $(BUILD_DIR) app *.o
	: nothing else to clean
