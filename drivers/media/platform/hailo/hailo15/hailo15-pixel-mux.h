#ifndef __HAILO15_PIXEL_MUX_DRIVER__
#define __HAILO15_PIXEL_MUX_DRIVER__

struct device;

/* Public API for sister drivers (e.g. hailo15-dphy) that need to drive
 * pixel-mux registers as part of cross-IP bring-up sequences.
 *
 * Route the shared right-side D-PHY lanes (V6/V5 pads) to a CSI receiver.
 * csi_idx==0 => CSI2RX0 (default), csi_idx==1 => CSI2RX1 (dual mode).
 * Returns -EOPNOTSUPP on SoC variants that don't expose this register.
 */
int hailo15_pixel_mux_route_right_lane(struct device *dev,
				       unsigned int csi_idx);


#define P2A0_2_SW_DBG_P2A1_2_CSIRX0_ISP0_2_CSIRX0_ISP1_2_SW_DBG 0xc0e
#define P2A0_DIS_P2A1_DIS_ISP0_2_CSIRX0_ISP1_2_CSIRX1 0x236
#define P2A0_2_CSIRX0_P2A1_2_CSIRX1_ISP0_2_SW_DBG_ISP1_2_SW_DBG 0xd88

#define DT_RAW_10 0x2B
#define DT_RAW_12 0x2C

#define ENABLE_VC_0_DT_RAW_10 { .enable = 1, .vc = 0, .dt = DT_RAW_10 }
#define ENABLE_VC_1_DT_RAW_10 { .enable = 1, .vc = 1, .dt = DT_RAW_10 }
#define ENABLE_VC_2_DT_RAW_10 { .enable = 1, .vc = 2, .dt = DT_RAW_10 }
#define ENABLE_VC_0_DT_RAW_12 { .enable = 1, .vc = 0, .dt = DT_RAW_12 }
#define ENABLE_VC_1_DT_RAW_12 { .enable = 1, .vc = 1, .dt = DT_RAW_12 }
#define ENABLE_VC_2_DT_RAW_12 { .enable = 1, .vc = 2, .dt = DT_RAW_12 }
#define DISABLE_VC_4_DT_DISABLE { .enable = 0, .vc = 0, .dt = 0 }

#define ENABLE_ISP_VCLK_CSIRX0_VCLK_CSIRX0_XTAL_ISP_HCLK_CSIRX0_HCLK 0xd3
#define ENABLE_CSIRX0_VCLK_CSIRX1_VCLK_CSIRX0_XTAL_CSIRX0_HCLK_CSIRX1_HCLK_CSIRX1_XTAL \
	0x596

#endif // __HAILO15_PIXEL_MUX_DRIVER__
