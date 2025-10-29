# SPDX-License-Identifier: GPL-2.0
# Makefile for vm tools
#
include ../scripts/Makefile.include

TARGETS=page-types slabinfo page_owner_sort

# Default LIB_DIR if srctree is not defined
ifeq ($(srctree),)
    LIB_DIR := ../lib/api
else
    LIB_DIR := $(srctree)/tools/lib/api
endif

# Default LIBS if prefix is not defined
ifeq ($(prefix),)
    LIBS    := $(LIB_DIR)/libapi.a
else
    LIBS    := $(prefix)/libapi.a
endif

CFLAGS = -Wall -Wextra -I../lib/
LDFLAGS = $(LIBS)

all: $(TARGETS)

$(TARGETS): $(LIBS)

$(LIBS):
	make -C $(LIB_DIR)

%: %.c
	$(CC) $(CFLAGS) -o $(prefix)$@ $< $(LDFLAGS)

clean:
	$(RM) page-types slabinfo page_owner_sort
	make -C $(LIB_DIR) clean

sbindir ?= /usr/sbin

install: all
	install -d $(DESTDIR)$(sbindir)
	install -m 755 -p $(TARGETS) $(DESTDIR)$(sbindir)
