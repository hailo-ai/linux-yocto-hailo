#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include <drm/drm_of.h>
#include <drm/drm_drv.h>
#include <drm/drm_vblank.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>

#define MIPI_TX_WRAPPER_CFG (0x0)
#define BURST_LENGTH(x) ((x) << 8)
#define DSI_HEADER(x) ((x) << 16)

#define DPM_0 (0xe0)
#define CFG_PITCHES(x) (x)
#define CFG_BURST(x) ((x) << 13)
#define CFG_MAX_OUTSTANDING_CONFIG(x) ((x) << 19)

#define DPM_1 (0xe4)
#define CFG_FRAME_SIZE_PULSES(x) (x)
#define CFG_IS_CONTINUOUS_SCANOUT_MODE BIT(23)

#define DPM_4 (0xfc)
#define CFG_MAX_CYCLE_COUNT(x) (x)

#define DPM_5 (0x100)
#define CFG_LINE_SIZE_PULSES(x) (x)

#define DPM_6 (0x108)
#define CFG_BUFFER_THRESHOLD(x) (x)

#define DPM_7 (0x10c)
#define SW_UPDATE_TIME_RANGE(x) (x)

#define DPM_9 (0x114)
#define FRAME_CYCLES(x) (x)

#define DPM_11 (0x11c)
#define CFG_LINE_SIZE_PULSES_THRESHOLD(x) (x)

#define RFMT_0 (0x120)
#define SWAP_MODE(x) ((x) << 6)
#define BYTE_SWAP_RGB_TO_BGR (1)

#define HEADER_2 (0x148)
#define NO_LINES(x) (x)
#define WRITE_CHUNK_EOL(x) ((x) << 14)
#define DATA_FLUSH_CHICKEN_BIT BIT(22)

#define HEADER_4 (0x158)

#define HEADER_6 (0x188)
#define PACKET_LENGTH(x) (x)
#define LINE_SIZE(x) ((x) << 16)

#define HA_STREAM_START_EXT_DATA_OUT (0xf8)
#define CFG_HA_STREAM_START BIT(0)

#define PLANE_0_PHYSICAL_BASE_ADD_NEXT_EXT_DATA_OUT (0xf0)
#define CFG_PLANE_0_PHYSICAL_BASE_ADD_NEXT(x) (x)

#define GO_SCANOUT_FRAMEBUFFER_EXT_DATA_OUT (0xf4)
#define CFG_GO_SCANOUT_FRAMEBUFFER BIT(0)

#define SUPPORTED_BYTES_PER_PIXEL (3)

struct hailo_driver_device {
	void __iomem *regs;
	struct clk *dsi_sys_clk;
	struct clk *dsi_p_clk;
	struct drm_device drm;
	struct drm_simple_display_pipe pipe;
};

DEFINE_DRM_GEM_CMA_FOPS(fops);

static const struct drm_driver driver_drm_driver = {
	.driver_features = DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.fops = &fops,
	DRM_GEM_CMA_DRIVER_OPS,
	.name = "hailo-drm",
	.desc = "HAILO DRM",
	.date = "20240319",
	.major = 1,
	.minor = 0,
};

static const u32 hailo_supported_formats[] = {
	DRM_FORMAT_RGB888,
};

static void hailo_pipe_enable(struct drm_simple_display_pipe *pipe,
			      struct drm_crtc_state *crtc_state,
			      struct drm_plane_state *plane_state)
{
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_device *drm = crtc->dev;
	struct hailo_driver_device *priv = drm->dev_private;
	const struct drm_display_mode *mode = &crtc_state->mode;
	uint32_t frame_size_in_8B_pulses, line_size_in_bytes,
		line_size_in_8B_pulses, frame_duration_ns, frame_cycles;
	unsigned long dsi_sys_clk;
	int refresh;

	dsi_sys_clk = clk_get_rate(priv->dsi_sys_clk);
	refresh = drm_mode_vrefresh(mode);
	line_size_in_bytes = mode->hdisplay * SUPPORTED_BYTES_PER_PIXEL;
	line_size_in_8B_pulses = line_size_in_bytes / 8;
	frame_size_in_8B_pulses = (line_size_in_bytes * mode->vdisplay) / 8;
	frame_duration_ns = ktime_divns(NSEC_PER_SEC, refresh);
	frame_cycles = dsi_sys_clk / refresh;

	/* burst_length is AXI Master maximum burst length – 1. i.e. For bursts of 8 beats; set burst_length = 7 */
	writel(BURST_LENGTH(7) | DSI_HEADER(line_size_in_bytes),
	       priv->regs + MIPI_TX_WRAPPER_CFG);

	writel(CFG_PITCHES(line_size_in_8B_pulses) | CFG_BURST(63) |
		       CFG_MAX_OUTSTANDING_CONFIG(12),
	       priv->regs + DPM_0);

	writel(CFG_FRAME_SIZE_PULSES(frame_size_in_8B_pulses) |
		       CFG_IS_CONTINUOUS_SCANOUT_MODE,
	       priv->regs + DPM_1);

	writel(CFG_MAX_CYCLE_COUNT(line_size_in_8B_pulses), priv->regs + DPM_4);

	writel(CFG_LINE_SIZE_PULSES(line_size_in_8B_pulses),
	       priv->regs + DPM_5);

	writel(PACKET_LENGTH(line_size_in_bytes) |
		       LINE_SIZE(line_size_in_bytes),
	       priv->regs + HEADER_6);

	writel(CFG_BUFFER_THRESHOLD(2048), priv->regs + DPM_6);

	writel(CFG_LINE_SIZE_PULSES_THRESHOLD(768), priv->regs + DPM_11);

	writel(SWAP_MODE(BYTE_SWAP_RGB_TO_BGR), priv->regs + RFMT_0);

	writel(NO_LINES(mode->vdisplay) | WRITE_CHUNK_EOL(4) |
		       DATA_FLUSH_CHICKEN_BIT,
	       priv->regs + HEADER_2);

	writel(SW_UPDATE_TIME_RANGE((frame_duration_ns * 2) / 3),
	       priv->regs + DPM_7);

	// frame_cycles = dsi_sys_clk / refresh = 600 MHz / 60 = 10M
	writel(10000000, priv->regs + DPM_9);
	writel(10000000, priv->regs + HEADER_4);

	writel(CFG_HA_STREAM_START, priv->regs + HA_STREAM_START_EXT_DATA_OUT);

	writel(CFG_GO_SCANOUT_FRAMEBUFFER,
	       priv->regs + GO_SCANOUT_FRAMEBUFFER_EXT_DATA_OUT);
}

static void hailo_pipe_update(struct drm_simple_display_pipe *pipe,
			      struct drm_plane_state *old_state)
{
	struct drm_plane_state *new_state = pipe->plane.state;
	struct drm_framebuffer *fb = new_state->fb;
	struct drm_gem_cma_object *cma_obj;
	dma_addr_t dma_addr;
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_device *drm = crtc->dev;
	struct hailo_driver_device *priv = drm->dev_private;

	if (!fb) {
		return;
	}

	cma_obj = drm_fb_cma_get_gem_obj(fb, 0);
	if (!cma_obj)
		return;

	dma_addr = cma_obj->paddr;

	writel(CFG_PLANE_0_PHYSICAL_BASE_ADD_NEXT(dma_addr),
	       priv->regs + PLANE_0_PHYSICAL_BASE_ADD_NEXT_EXT_DATA_OUT);
}

static const struct drm_simple_display_pipe_funcs hailo_pipe_funcs = {
	.update = hailo_pipe_update,
	.enable = hailo_pipe_enable,
	DRM_GEM_SIMPLE_DISPLAY_PIPE_SHADOW_PLANE_FUNCS,
};

static const struct drm_mode_config_funcs hailo_drm_modecfg_funcs = {
	.fb_create = drm_gem_fb_create,
	.atomic_check = drm_atomic_helper_check,
	.atomic_commit = drm_atomic_helper_commit,
};

static int driver_probe(struct platform_device *pdev)
{
	struct hailo_driver_device *priv;
	struct drm_device *drm;
	int ret;
	struct drm_panel *panel;
	struct drm_bridge *bridge;

	priv = devm_drm_dev_alloc(&pdev->dev, &driver_drm_driver,
				  struct hailo_driver_device, drm);
	if (IS_ERR(priv)) {
		dev_err(&pdev->dev, "failed to allocate drm device\n");
		return PTR_ERR(priv);
	}
	drm = &priv->drm;

	drm->dev_private = priv;

	priv->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(priv->regs))
		return PTR_ERR(priv->regs);

	ret = drmm_mode_config_init(drm);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize mode config\n");
		return ret;
	}

	drm->mode_config.min_width = 0;
	drm->mode_config.min_height = 0;
	drm->mode_config.max_width = 8096;
	drm->mode_config.max_height = 8096;
	drm->mode_config.preferred_depth = SUPPORTED_BYTES_PER_PIXEL * 8;
	drm->mode_config.funcs = &hailo_drm_modecfg_funcs;

	ret = drm_vblank_init(drm, 1);
	if (ret) {
		dev_err(&pdev->dev, "failed to initialize vblank\n");
		return ret;
	}

	ret = drm_of_find_panel_or_bridge(drm->dev->of_node, 0, 0, &panel,
					  &bridge);
	if (ret || !bridge) {
		dev_err(&pdev->dev, "Didn't find panel or bridge\n");
		return ret;
	}

	if (panel) {
		dev_err(&pdev->dev, "in hailo driver_probe - FIND PANEL\n");
		return -ENODEV;
	}

	priv->dsi_sys_clk = devm_clk_get(&pdev->dev, "dsi_sys_clk");
	if (IS_ERR(priv->dsi_sys_clk))
		return PTR_ERR(priv->dsi_sys_clk);
	priv->dsi_p_clk = devm_clk_get(&pdev->dev, "dsi_p_clk");
	if (IS_ERR(priv->dsi_p_clk))
		return PTR_ERR(priv->dsi_p_clk);

	pm_runtime_get_sync(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);

	ret = clk_prepare_enable(priv->dsi_sys_clk);
	if (ret)
		return ret;
	ret = clk_prepare_enable(priv->dsi_p_clk);
	if (ret)
		goto err_disable_dsi_sys_clk;
	ret = drm_simple_display_pipe_init(drm, &priv->pipe, &hailo_pipe_funcs,
					   hailo_supported_formats,
					   ARRAY_SIZE(hailo_supported_formats),
					   NULL, NULL);

	if (ret) {
		dev_err(&pdev->dev,
			"drm_simple_display_pipe_init return err\n");
		goto err_disable_clks;
	}

	ret = drm_simple_display_pipe_attach_bridge(&priv->pipe, bridge);
	if (ret) {
		dev_err(&pdev->dev,
			"drm_simple_display_pipe_attach_bridge return err\n");
		goto err_disable_clks;
	}

	drm_mode_config_reset(drm);

	platform_set_drvdata(pdev, drm);

	ret = drm_dev_register(drm, 0);
	if (ret) {
		dev_err(&pdev->dev, "failed to register drm device\n");
		goto err_disable_clks;
	}

	drm_fbdev_generic_setup(drm, SUPPORTED_BYTES_PER_PIXEL * 8);

	return 0;

err_disable_clks:
	clk_disable_unprepare(priv->dsi_p_clk);
err_disable_dsi_sys_clk:
	clk_disable_unprepare(priv->dsi_sys_clk);
	return ret;
}

// This function is called before the devm_ resources are released
static int driver_remove(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);
	drm_dev_unregister(drm);
	drm_atomic_helper_shutdown(drm);
	pm_runtime_put_sync(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	pm_runtime_disable(&pdev->dev);

	return 0;
}

// This function is called on kernel restart and shutdown
static void driver_shutdown(struct platform_device *pdev)
{
	drm_atomic_helper_shutdown(platform_get_drvdata(pdev));
}

static int __maybe_unused driver_pm_suspend(struct device *dev)
{
	return drm_mode_config_helper_suspend(dev_get_drvdata(dev));
}

static int __maybe_unused driver_pm_resume(struct device *dev)
{
	drm_mode_config_helper_resume(dev_get_drvdata(dev));

	return 0;
}

static const struct of_device_id hailo_dt_ids[] = { { .compatible =
							      "hailo,dsi" },
						    { /* sentinel */ } };
MODULE_DEVICE_TABLE(of, hailo_dt_ids);

static const struct dev_pm_ops driver_pm_ops = { SET_SYSTEM_SLEEP_PM_OPS(
	driver_pm_suspend, driver_pm_resume) };

static struct platform_driver hailo_drm_driver = {
 		.driver = {
			.name           = "hailo-dsi",
 			.of_match_table	= hailo_dt_ids,
 			.pm = &driver_pm_ops,
 		},
 		.probe = driver_probe,
 		.remove = driver_remove,
 		.shutdown = driver_shutdown,
 	};

static int __init hailo_drm_init(void)
{
	return platform_driver_register(&hailo_drm_driver);
}
module_init(hailo_drm_init);

static void __exit hailo_drm_exit(void)
{
	platform_driver_unregister(&hailo_drm_driver);
}
module_exit(hailo_drm_exit);

MODULE_AUTHOR("Hailo Technologies Ltd.");
MODULE_DESCRIPTION("Hailo DRM Driver");
MODULE_LICENSE("GPL v2");
