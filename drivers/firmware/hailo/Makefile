# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (c) 2017 Cadence Design Systems Inc.
# Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
#

obj-${CONFIG_HAILO_XRP} += xvp_main.o xrp_alloc.o xrp_io.o xrp_cma_alloc.o \
    xrp_boot.o xrp_exec.o xrp_firmware.o xrp_hw.o xrp_log.o xrp_share.o xrp_fs.o

ccflags-$(CONFIG_HAILO_XRP_DEBUG) += -DDEBUG

obj-$(CONFIG_HAILO_SCU_LOG) += scu_log.o
obj-$(CONFIG_HAILO_SCMI_PROTOCOL) += scmi_hailo.o scmi_hailo_ops.o
