#ifndef _HAILO15L_AUDIO_H
#define _HAILO15L_AUDIO_H

/*!
 * Auxilary configuration macros
 * Node: I2S contrroller (audio-controller-0@10d000, audio-controller-1@10d400)
 * - I2S Master mode:
 *   - I2S_CTRL_SRC_CLK__PCLK(ctrl_id): pclk = 200 MHz
 *   - I2S_CTRL_SRC_CLK__FAST_ACLK(ctrl_id): fast-aclk = 600 MHz
 *   - I2S_CTRL_SRC_CLK__XTAL(ctrl_id): xtal = 25 MHz
 *   - I2S_CTRL_SRC_CLK__EXTERNAL_MCLK(ctrl_id, <OSC Hz>): usually 12288000 Hz
 * - I2S Slave mode:
 *   - I2S_CTRL_SRC_CLK__SLAVE_CLK(ctrl_id, <OSC Hz>): usually 12288000 Hz
 * ctrl-id: 0(Controller-0), 1(Controller-1)
 */
#define I2S_CTRL_SRC_CLK__PCLK(ctrl_id, ...) \
	source-clock-id = <HAILO15_SCMI_I2S_SOURCE_CLK_MASTER_PCLK>; \
    /delete-property/ source-clock-frequency; \
	clocks = <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_CONTROLLER_MODE_MUX>, \
	         <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_FRAC_DIV>; \
	clock-names = "i2sclk", "i2sclk-rate"

#define I2S_CTRL_SRC_CLK__FAST_ACLK(ctrl_id, ...) \
	source-clock-id = <HAILO15_SCMI_I2S_SOURCE_CLK_MASTER_FAST_ACLK>; \
    /delete-property/ source-clock-frequency; \
	clocks = <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_CONTROLLER_MODE_MUX>, \
	         <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_FRAC_DIV>; \
	clock-names = "i2sclk", "i2sclk-rate"

#define I2S_CTRL_SRC_CLK__XTAL(ctrl_id, ...) \
	source-clock-id = <HAILO15_SCMI_I2S_SOURCE_CLK_MASTER_XTAL>; \
    /delete-property/ source-clock-frequency; \
	clocks = <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_CONTROLLER_MODE_MUX>, \
	         <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_FRAC_DIV>; \
	clock-names = "i2sclk", "i2sclk-rate"

#define I2S_CTRL_SRC_CLK__EXTERNAL_MCLK(ctrl_id, freq) \
	source-clock-id = <HAILO15_SCMI_I2S_SOURCE_CLK_MASTER_EXTERNAL_CLK>; \
    source-clock-frequency = <freq>; \
	clocks = <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_CONTROLLER_MODE_MUX>, \
	         <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_MCLK_IN_PAD>; \
	clock-names = "i2sclk", "i2s-ext-mclk"

#define I2S_CTRL_SRC_CLK__SLAVE_CLK(ctrl_id, ...) \
	source-clock-id = <HAILO15_SCMI_I2S_SOURCE_CLK_SLAVE_CLK>; \
    /delete-property/ source-clock-frequency; \
    /delete-property/ clocks; \
    /delete-property/ clock-names

/*!
 * Helper configuration macro
 * Node: I2S contrroller (audio-controller-0@10d000, audio-controller-1@10d400)
 * - Slave mode:
 *   - set: HAILO_I2S_CTRL_CFG(<ctrl-id>, SLAVE_CLK);
 * - Master mode:
 *   - set one of the following:
 *     - HAILO_I2S_CTRL_CFG(<ctrl-id>, XTAL);
 *     - HAILO_I2S_CTRL_CFG(<ctrl-id>, PCLK);
 *     - HAILO_I2S_CTRL_CFG(<ctrl-id>, FAST_ACLK);
 *     - HAILO_I2S_CTRL_CFG(<ctrl-id>, EXTERNAL_MCLK, <OSC Hz>); - E.g: OSC Hz: 12288000
 * ctrl-id: 0(Controller-0), 1(Controller-1)
 */

#define HAILO_I2S_CTRL_CFG(ctrl_id, sclk, ...) I2S_CTRL_SRC_CLK__##sclk(ctrl_id, __VA_ARGS__)



/*!
 * Auxilary configuration macros
 * Node: Audio card subnode (simple-audio-card,cpu)
 * - I2S Master mode: CPU_DAI_CFG__I2S_CTRL_MASTER_MODE(ctrl_id)
 * - I2S Slave mode: CPU_DAI_CFG__I2S_CTRL_SLAVE_MODE(ctrl_id)
 * ctrl-id: 0(Controller-0), 1(Controller-1)
 */
#define CPU_DAI_CFG__I2S_CTRL_MASTER_MODE(ctrl_id, ...) \
    system-clock-direction-out; \
    /delete-property/ system-clock-frequency

#define CPU_DAI_CFG__I2S_CTRL_SLAVE_MODE(ctrl_id, freq) \
    system-clock-frequency = <freq>; \
    /delete-property/ system-clock-direction-out

// #define CPU_DAI_CFG__I2S_CTRL_MASTER_MODE(ctrl_id) \
//     system-clock-direction-out; \
//     /delete-property/ clocks

// #define CPU_DAI_CFG__I2S_CTRL_SLAVE_MODE(ctrl_id) \
//     clocks = <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_SCLK_IN_PAD>; \
//     /delete-property/ system-clock-direction-out

/*!
 * Helper configuration macro
 * Node: Audio card subnode (simple-audio-card,cpu)
 * - I2S Slave mode: set CPU_DAI_CFG(<ctrl-id>, SLAVE);
 * - I2S Master mode: set CPU_DAI_CFG(<ctrl-id>, MASTER)
 * ctrl-id: 0(Controller-0), 1(Controller-1)
 */
#define CPU_DAI_CFG(ctrl_id, mode, ...) \
    CPU_DAI_CFG__I2S_CTRL_##mode##_MODE(ctrl_id, __VA_ARGS__)



/*!
 * Auxilary configuration macros
 * Node: Audio card subnode (simple-audio-card,codec)
 * - Master: CODEC_DAI_CFG__I2S_CTRL_MASTER_MODE(ctrl_id)
 * - Slave: CODEC_DAI_CFG__I2S_CTRL_SLAVE_MODE(ctrl_id, freq).
 * ctrl-id: 0(Controller-0), 1(Controller-1)
 */
#define CODEC_DAI_CFG__I2S_CTRL_MASTER_MODE(ctrl_id, ...) \
    clocks = <&scmi_clk HAILO15_SCMI_CLOCK_IDX_I2S##ctrl_id##_BCLK_OUT_PAD>; \
    /delete-property/ system-clock-frequency

#define CODEC_DAI_CFG__I2S_CTRL_SLAVE_MODE(ctrl_id, freq) \
    system-clock-frequency = <freq>; \
    /delete-property/ clocks

/*!
 * Helper configuration macro
 * Node: Audio card subnode (simple-audio-card,codec)
 * - Slave mode: set CODEC_DAI_CFG(<ctrl-id>, SLAVE, <OSC Hz>); - E.g: OSC Hz: 12288000
 * - Master mode: set CODEC_DAI_CFG(<ctrl-id>, MASTER)
 * ctrl-id: 0(Controller-0), 1(Controller-1)
 */
#define CODEC_DAI_CFG(ctrl_id, mode, ...) \
    CODEC_DAI_CFG__I2S_CTRL_##mode##_MODE(ctrl_id, __VA_ARGS__)



/*!
 * Auxilary configuration macros
 * Node: Audio card (simple-audio-card)
 * - I2S Master mode: CPU_DAI_CFG__I2S_CTRL_MASTER_MODE(ctrl_id)
 * - I2S Slave mode: CPU_DAI_CFG__I2S_CTRL_SLAVE_MODE(ctrl_id).
 * ctrl-id: 0(Controller-0), 1(Controller-1)
 */
#define _STR(x) #x

#define SIMPLE_AUDIO_CARD__I2S_CTRL_MASTER_MODE(ctrl_id) \
    simple-audio-card,name = _STR(Hailo15l-Audio-master-##ctrl_id); \
    simple-audio-card,bitclock-master = <&cpu_dai_##ctrl_id>; \
    simple-audio-card,frame-master = <&cpu_dai_##ctrl_id>; \
    simple-audio-card,mclk-fs = <32>

#define SIMPLE_AUDIO_CARD__I2S_CTRL_SLAVE_MODE(ctrl_id) \
    simple-audio-card,name = _STR(Hailo15l-Audio-slave-##ctrl_id); \
    simple-audio-card,bitclock-master = <&codec_dai_##ctrl_id>; \
    simple-audio-card,frame-master = <&codec_dai_##ctrl_id>; \
    /delete-property/ simple-audio-card,mclk-fs

/*!
 * Helper configuration macro
 * Node: Audio card (simple-audio-card)
 * - Slave mode: set SIMPLE_AUDIO_CARD(<ctrl-id>, SLAVE);
 * - Master mode: set SIMPLE_AUDIO_CARD(<ctrl-id>, MASTER)
 * ctrl-id: 0(Controller-0), 1(Controller-1)
 */
#define SIMPLE_AUDIO_CARD_CFG(ctrl_id, mode) \
    SIMPLE_AUDIO_CARD__I2S_CTRL_##mode##_MODE(ctrl_id)

#endif /* _HAILO15L_AUDIO_H */