/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __HAILO15_RXWRAPPER_DRIVER__
#define __HAILO15_RXWRAPPER_DRIVER__

#include <linux/types.h>

struct device;

/* Public API for sister drivers (e.g. hailo15-dphy) that need to
 * coordinate with the rxwrapper for shared-DPHY 2-clock-lane bring-up.
 * All helpers take the rxwrapper's struct device * (resolved by the
 * caller via DT phandle + of_find_device_by_node).
 */

/* True iff IPCONFIG_CMN==DUAL and LANE_RSTB_CMN==1, i.e. this rxwrapper
 * is already configured for dual-link. Used by dphy to skip 2cl bring-up
 * on a driver reload while a stream is live.
 */
bool hailo15_rxwrapper_is_dual_link_active(struct device *dev);

/* Phase 1 of dual-link entry: clear LANE_RSTB_CMN and set IPCONFIG_CMN
 * to DUAL. Caller must drive the matching DPHY-side CMN sequence
 * (TBIT2 disable, udelay, TBIT2 re-enable) around paired calls on both
 * rxwrappers per the Cadence DPHY Rx user guide.
 */
int hailo15_rxwrapper_dual_link_assert_cmn_reset(struct device *dev);

/* Phase 2 of dual-link entry: release LANE_RSTB_CMN. */
int hailo15_rxwrapper_dual_link_release_cmn_reset(struct device *dev);

/* Rollback path: clear IPCONFIG_CMN back to single-link, set LANE_RSTB_CMN. */
int hailo15_rxwrapper_restore_single_link(struct device *dev);

#endif /* __HAILO15_RXWRAPPER_DRIVER__ */
