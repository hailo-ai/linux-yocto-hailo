# SPDX-License-Identifier: GPL-2.0
# Generic Memory Tracker Framework
obj-$(CONFIG_CMA_TRACKER) += mem_tracker.o cma_tracker.o

# Add include path for internal CMA header (mm/cma.h)
# $(srctree) is set by top-level Makefile to point to source tree root
# Add srctree itself so we can use #include <mm/cma.h>
ccflags-y += -I$(srctree)

