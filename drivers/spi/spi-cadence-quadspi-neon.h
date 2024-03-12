/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __SPI_CADENCE_QUADSPI_NEON_H__
#define __SPI_CADENCE_QUADSPI_NEON_H__

#include <linux/types.h>

void neon_write_16B(const u8 *buf, volatile void __iomem *addr);
void neon_write_32B(const u8 *buf, volatile void __iomem *addr);

#endif /* __SPI_CADENCE_QUADSPI_NEON_H__ */
