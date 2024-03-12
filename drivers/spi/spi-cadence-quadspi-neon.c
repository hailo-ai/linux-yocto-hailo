// SPDX-License-Identifier: GPL-2.0-only
//
// Neon helper functions
// Must be a separate compilation unit so NEON operation are only enabled
// for these functions and not the entire driver.
//

#include <linux/types.h>
#include <linux/kernel.h>
#include <asm/neon-intrinsics.h>

void neon_write_16B(const u8 *buf, volatile void __iomem *addr)
{
	vst2_u8((u8 *)addr, vld2_u8(buf));
}
EXPORT_SYMBOL_GPL(neon_write_16B);

void neon_write_32B(const u8 *buf, volatile void __iomem *addr)
{
	vst4_u8((u8 *)addr, vld4_u8(buf));
}
EXPORT_SYMBOL_GPL(neon_write_32B);
