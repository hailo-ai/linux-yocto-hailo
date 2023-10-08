/* SPDX-License-Identifier: GPL-2.0 */
/**
* Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.
**/

/*
 * DDR ECC Driver for Cadence LPDDR4
 */

#include <linux/edac.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include "edac_module.h"
#include "hailo15_cdns_mc_edac.h"

/* Number of cs_rows needed per memory controller */
#define CDNS_EDAC_NR_CSROWS 2
/* Number of channels per memory controller */
#define CDNS_EDAC_NR_CHANS 2
/* Granularity of reported error in bytes */
#define CDNS_EDAC_ERR_GRAIN 8
/* Buffer size for framing the event specific info */
#define CDNS_EDAC_MSG_SIZE 256
/* Module string */
#define CDNS_EDAC_MOD_STRING "cdns_edac"
/* Module version */
#define CDNS_EDAC_MOD_VER "1"

/**
 * struct ecc_error_info - ECC error log information.
 * @address:	Holds the address of the read data that caused a single-bit correctable ECC event.
 * @data:		Holds the pre-corrected data associated with a single-bit correctable ECC event.
 * @id:			Holds the source ID associated with a single-bit correctable ECC event.
 * 				For AXI ports, the source ID is comprised of the:
 * 				- Port ID (upper bit/s)
 * 				- Requestor ID, where the Requestor ID is the axiY_AWID for write commands or the axiY_ARID for read commands.
 * @syndrom:	Holds the syndrome value associated with a single-bit correctable ECC error event.
 */
struct ecc_error_info {
	u64 address;
	u64 data;
	u32 id;
	u32 syndrom;
};

/**
 * struct cdns_ecc_status - ECC status information to report.
 * @ce_cnt:	Correctable error count.
 * @ue_cnt:	Uncorrectable error count.
 * @ceinfo:	Correctable error log information.
 * @ueinfo:	Uncorrectable error log information.
 */
struct cdns_ecc_status {
	u32 ce_cnt;
	u32 ue_cnt;
	u32 ce_multi_cnt;
	u32 ue_multi_cnt;
	struct ecc_error_info ceinfo;
	struct ecc_error_info ueinfo;
};

enum ecc_type {
	ECC_TYPE_CE,
	ECC_TYPE_MULTI_CE,
	ECC_TYPE_UE,
	ECC_TYPE_MULTI_UE,
	ECC_TYPE_MAX,
};

enum ddr_ctrl_ecc_mode {
	DDR_CTRL_ECC_MODE_DISABLE, /*!< ECC disabled */
	DDR_CTRL_ECC_MODE_EN, /*!< ECC enabled, detection(no),  correction(no). */
	DDR_CTRL_ECC_MODE_EN_DETECT, /*!< ECC enabled, detection(yes), correction(no). */
	DDR_CTRL_ECC_MODE_EN_DETECT_CORRECT, /*!< ECC enabled, detection(yes), correction(yes). */
};

/**
 * struct cdns_edac_priv - DDR memory controller private instance data.
 * @baseaddr:		Base address of the DDR controller.
 * @message:		Buffer for framing the event specific info.
 * @stat:		ECC status information.
 * @p_data:		Platform data.
 * @ce_cnt:		Correctable Error count.
 * @ue_cnt:		Uncorrectable Error count.
 */
struct cdns_edac_priv {
	void __iomem *ddr_ctrl_base;
	char message[CDNS_EDAC_MSG_SIZE];
	struct cdns_ecc_status stat;
	u32 ce_cnt;
	u32 ue_cnt;
	u32 ce_multi_cnt;
	u32 ue_multi_cnt;
};

static int ecc_intr_status(struct cdns_edac_priv *priv, enum ecc_type ecc_type,
			   u32 *status)
{
	switch (ecc_type) {
	case ECC_TYPE_CE:
		*status = ddr_ctrl_reg32_rd(priv->ddr_ctrl_base,
					    GLOBAL_ERROR_INFO_C_ECC);
		break;
	case ECC_TYPE_UE:
		*status = ddr_ctrl_reg32_rd(priv->ddr_ctrl_base,
					    GLOBAL_ERROR_INFO_U_ECC);
		break;
	case ECC_TYPE_MULTI_CE:
		*status = ddr_ctrl_reg32_rd(
			priv->ddr_ctrl_base,
			INT_STATUS_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED);
		break;
	case ECC_TYPE_MULTI_UE:
		*status = ddr_ctrl_reg32_rd(
			priv->ddr_ctrl_base,
			INT_STATUS_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED);
		break;
	default:
		edac_printk(KERN_ERR, EDAC_MC, "Invalid ECC Type arg.\n");
		return -EINVAL;
	}

	return 0;
}

static int ecc_intr_ack(struct cdns_edac_priv *priv, enum ecc_type ecc_type)
{
	switch (ecc_type) {
	case ECC_TYPE_CE:
		ddr_ctrl_reg32_set(priv->ddr_ctrl_base,
				   GLOBAL_ERROR_INFO_C_ECC);
		break;
	case ECC_TYPE_UE:
		ddr_ctrl_reg32_set(priv->ddr_ctrl_base,
				   GLOBAL_ERROR_INFO_U_ECC);
		break;
	case ECC_TYPE_MULTI_CE:
		ddr_ctrl_reg32_set(
			priv->ddr_ctrl_base,
			INT_ACK_LSB_MULTI_CORRECTABLE_ECC_EVENTS_DETECTED);
		break;
	case ECC_TYPE_MULTI_UE:
		ddr_ctrl_reg32_set(
			priv->ddr_ctrl_base,
			INT_ACK_LSB_MULTI_UNCORRECTABLE_ECC_EVENTS_DETECTED);
		break;
	default:
		edac_printk(KERN_ERR, EDAC_MC, "Invalid ECC Type arg.\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * cdns_get_error_info - Get the current ECC error info.
 *
 * @param priv:	DDR memory controller private instance data.
 *
 * Return: 0(success).
 */
static int cdns_get_error_info(struct cdns_edac_priv *priv,
			       enum ecc_type ecc_type)
{
	struct cdns_ecc_status *p;
	u64 address_msb;
	u64 address_lsb;
	u64 data_msb;
	u64 data_lsb;

	void __iomem *base = priv->ddr_ctrl_base;
	p = &priv->stat;

	switch (ecc_type) {
	case ECC_TYPE_CE:
		p->ce_cnt = 1;
		address_msb = ddr_ctrl_reg32_rd(base, ECC_C_ADDR_MSB);
		address_lsb = ddr_ctrl_reg32_rd(base, ECC_C_ADDR_LSB);
		data_msb = ddr_ctrl_reg32_rd(base, ECC_C_DATA_MSB);
		data_lsb = ddr_ctrl_reg32_rd(base, ECC_C_DATA_LSB);
		p->ceinfo.address = ((address_msb << 32) | address_lsb);
		p->ceinfo.data = ((data_msb << 32) | data_lsb);
		p->ceinfo.id = ddr_ctrl_reg32_rd(base, ECC_C_ID);
		p->ceinfo.syndrom = ddr_ctrl_reg32_rd(base, ECC_C_SYND);
		break;
	case ECC_TYPE_UE:
		p->ue_cnt = 1;
		address_msb = ddr_ctrl_reg32_rd(base, ECC_U_ADDR_MSB);
		address_lsb = ddr_ctrl_reg32_rd(base, ECC_U_ADDR_LSB);
		data_msb = ddr_ctrl_reg32_rd(base, ECC_U_DATA_MSB);
		data_lsb = ddr_ctrl_reg32_rd(base, ECC_U_DATA_LSB);
		p->ceinfo.address = ((address_msb << 32) | address_lsb);
		p->ceinfo.data = ((data_msb << 32) | data_lsb);
		p->ceinfo.id = ddr_ctrl_reg32_rd(base, ECC_U_ID);
		p->ceinfo.syndrom = ddr_ctrl_reg32_rd(base, ECC_U_SYND);
		break;
	case ECC_TYPE_MULTI_CE:
		/* No error signature is recorded if multiple ECC error detected, only the first ECC info is recorded. */
		p->ce_multi_cnt = 1;
		break;
	case ECC_TYPE_MULTI_UE:
		/* No error signature is recorded if multiple ECC error detected, only the first ECC info is recorded. */
		p->ue_multi_cnt = 1;
		break;
	default:
		edac_printk(KERN_ERR, EDAC_MC, "Invalid ECC Type arg.\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * handle_error - Handle Correctable and Uncorrectable ECC errors.
 *
 * @param mci:	EDAC memory controller instance.
 * @param p:    cdns ECC status structure.
 *
 */
static void handle_error(struct mem_ctl_info *mci, struct cdns_ecc_status *p,
			 enum ecc_type ecc_type)
{
	struct cdns_edac_priv *priv = mci->pvt_info;
	struct ecc_error_info *pinf;
	u32 cnt;
	char *ev_err_type_str[ECC_TYPE_MAX] = {
		[ECC_TYPE_CE] = "",
		[ECC_TYPE_MULTI_CE] = "-multi",
		[ECC_TYPE_UE] = "",
		[ECC_TYPE_MULTI_UE] = "-multi",
	};
	const enum hw_event_mc_err_type ev_err_type[ECC_TYPE_MAX] = {
		[ECC_TYPE_CE] = HW_EVENT_ERR_CORRECTED,
		[ECC_TYPE_MULTI_CE] = HW_EVENT_ERR_CORRECTED,
		[ECC_TYPE_UE] = HW_EVENT_ERR_UNCORRECTED,
		[ECC_TYPE_MULTI_UE] = HW_EVENT_ERR_UNCORRECTED,
	};

	switch (ecc_type) {
	case ECC_TYPE_CE:
		pinf = &p->ceinfo;
		cnt = p->ce_cnt;
		break;
	case ECC_TYPE_UE:
		pinf = &p->ueinfo;
		cnt = p->ue_cnt;
		break;
	case ECC_TYPE_MULTI_CE:
	case ECC_TYPE_MULTI_UE:
		/* Nothing to report: no error signature is recorded. */
		memset(p, 0, sizeof(*p));
		return;
	default:
		edac_printk(KERN_ERR, EDAC_MC, "Invalid ECC Type arg.\n");
		return;
	}

	snprintf(priv->message, CDNS_EDAC_MSG_SIZE, "%s id(%x) data(%llx) ",
		 ev_err_type_str[ecc_type], pinf->id, pinf->data);

	edac_mc_handle_error(ev_err_type[ecc_type], mci, cnt,
			     pinf->address >> PAGE_SHIFT,
			     pinf->address & ~PAGE_MASK, pinf->syndrom, 0, 0,
			     -1, priv->message, "");

	memset(p, 0, sizeof(*p));
}

/**
 * @brief Check if ECC error occurred.
 *
 * @param mci:		EDAC memory controller instance..
 * @param ecc_type: enum ecc_type.
 *
 */
static void check_errors_per_ecc_type(struct mem_ctl_info *mci,
				      enum ecc_type ecc_type)
{
	struct cdns_edac_priv *priv;
	u32 int_status;
	int rc;

	priv = mci->pvt_info;

	rc = ecc_intr_status(priv, ecc_type, &int_status);
	if (rc || !int_status) {
		return;
	}

	cdns_get_error_info(priv, ecc_type);
	switch (ecc_type) {
	case ECC_TYPE_CE:
		priv->ce_cnt += priv->stat.ce_cnt;
		break;
	case ECC_TYPE_UE:
		priv->ce_cnt += priv->stat.ue_cnt;
		break;
	case ECC_TYPE_MULTI_CE:
		priv->ce_multi_cnt += priv->stat.ce_multi_cnt;
		break;
	case ECC_TYPE_MULTI_UE:
		priv->ue_multi_cnt += priv->stat.ue_multi_cnt;
		break;
	default:
		edac_printk(KERN_ERR, EDAC_MC, "Invalid ECC Type arg.\n");
		return;
	}
	handle_error(mci, &priv->stat, ecc_type);
	ecc_intr_ack(priv, ecc_type);
}

/**
 * check_errors - Check controller for ECC errors.
 * @param mci:	EDAC memory controller instance.
 *
 * Check and post ECC errors. Called by the polling thread.
 */
static void check_errors(struct mem_ctl_info *mci)
{
	struct cdns_edac_priv *priv;
	priv = mci->pvt_info;

	check_errors_per_ecc_type(mci, ECC_TYPE_MULTI_CE);
	check_errors_per_ecc_type(mci, ECC_TYPE_CE);
	edac_dbg(3,
		 "Total error count CE[%d] UE[%d] multi-CE[%d] multi-UE[%d]\n",
		 priv->ce_cnt, priv->ue_cnt, priv->ce_multi_cnt,
		 priv->ue_multi_cnt);
}

/**
 * cdns_get_dtype - Return the controller memory used chips.
 *
 * @param priv:	private instance data.
 *
 * Return: enum dev_type.
 */
static enum dev_type cdns_get_dtype(struct cdns_edac_priv *priv)
{
	return DEV_X16;
}

/**
 * cdns_get_ecc_state - Return the controller ECC enable/disable status.
 *
 * @param priv:	private instance data.
 *
 * Return: true(ECC enabled), false(ECC disabled).
 */
static bool cdns_get_ecc_state(struct cdns_edac_priv *priv)
{
	enum dev_type dt;
	enum ddr_ctrl_ecc_mode ecc_mode;

	dt = cdns_get_dtype(priv);
	if (dt == DEV_UNKNOWN) {
		return false;
	}

	ecc_mode = ddr_ctrl_reg32_rd(priv->ddr_ctrl_base, ECC_ENABLE);
	switch (ecc_mode) {
	case DDR_CTRL_ECC_MODE_EN_DETECT:
	case DDR_CTRL_ECC_MODE_EN_DETECT_CORRECT:
		return true;
	default:
		return false;
	}

	return false;
}

/**
 * get_memsize - Read the size of the attached memory device.
 *
 * Return: the memory size in bytes.
 */
static u32 get_memsize(void)
{
	struct sysinfo inf;

	si_meminfo(&inf);

	return inf.totalram * inf.mem_unit;
}

/**
 * cdns_get_mtype - Returns controller memory type.
 * @base:	Candence ECC status structure.
 *
 * Get the EDAC memory type appropriate for the current controller
 * configuration.
 *
 * Return: a memory type enumeration.
 */
static enum mem_type cdns_get_mtype(struct cdns_edac_priv *priv)
{
	u32 ctrl_id = ddr_ctrl_reg32_rd(priv->ddr_ctrl_base, CONTROLLER_ID);

	if (ctrl_id != 0x1046) {
		edac_printk(KERN_ERR, EDAC_MC,
			    "Unsupported memory controller.\n");
		return MEM_UNKNOWN;
	}
	return MEM_LPDDR4;
}

/**
 * init_csrows - Initialize the csrow data.
 * @mci:	EDAC memory controller instance.
 *
 * Initialize the chip select rows associated with the EDAC memory
 * controller instance.
 */
static void init_csrows(struct mem_ctl_info *mci)
{
	struct cdns_edac_priv *priv = mci->pvt_info;
	struct csrow_info *csi;
	struct dimm_info *dimm;
	u32 size, row;
	int j;

	for (row = 0; row < mci->nr_csrows; row++) {
		csi = mci->csrows[row];
		size = get_memsize();

		for (j = 0; j < csi->nr_channels; j++) {
			dimm = csi->channels[j]->dimm;
			dimm->edac_mode = EDAC_SECDED;
			dimm->mtype = cdns_get_mtype(priv);
			dimm->nr_pages =
				(size >> PAGE_SHIFT) / csi->nr_channels;
			dimm->grain = CDNS_EDAC_ERR_GRAIN;
			dimm->dtype = cdns_get_dtype(priv);
		}
	}
}

/**
 * mc_init - Initialize one driver instance.
 * @mci:	EDAC memory controller instance.
 * @pdev:	platform device.
 *
 * Perform initialization of the EDAC memory controller instance and
 * related driver-private data associated with the memory controller the
 * instance is bound to.
 */
static int mc_init(struct mem_ctl_info *mci, struct platform_device *pdev)
{
	struct cdns_edac_priv *priv;

	mci->pdev = &pdev->dev;
	priv = mci->pvt_info;

	//platform_set_drvdata(pdev, mci);

	if (cdns_get_mtype(priv) == MEM_UNKNOWN) {
		return -ENODEV;
	}

	if (!cdns_get_ecc_state(priv)) {
		edac_printk(KERN_INFO, EDAC_MC, "ECC not enabled\n");
		return -ENODEV;
	}

	/* Initialize controller capabilities and configuration */
	mci->mtype_cap = MEM_FLAG_LPDDR4;
	mci->edac_ctl_cap = EDAC_FLAG_NONE | EDAC_FLAG_SECDED;
	mci->scrub_cap = SCRUB_HW_PROG | SCRUB_HW_TUNABLE | SCRUB_HW_SRC;
	mci->scrub_mode = SCRUB_NONE;

	mci->edac_cap = EDAC_FLAG_SECDED;
	mci->ctl_name = "cdns_ddr_controller";
	mci->dev_name = CDNS_EDAC_MOD_STRING;
	mci->mod_name = CDNS_EDAC_MOD_VER;

	edac_op_state = EDAC_OPSTATE_POLL;
	mci->edac_check = check_errors;

	mci->ctl_page_to_phys = NULL;

	init_csrows(mci);

	return 0;
}

static const struct of_device_id cdns_edac_match[] = {
	{ .compatible = "hailo15,cdns-mc-edac" },
	{ /* end of table */ }
};

MODULE_DEVICE_TABLE(of, cdns_edac_match);

/**
 * mc_probe - Check controller and bind driver.
 * @pdev:	platform device.
 *
 * Probe a specific controller instance for binding with the driver.
 *
 * Return: 0 if the controller instance was successfully bound to the
 * driver; otherwise, < 0 on error.
 */
static int mc_probe(struct platform_device *pdev)
{
	struct edac_mc_layer layers[2];
	struct cdns_edac_priv *priv;
	struct mem_ctl_info *mci;
	void __iomem *ddr_ctrl_base;
	struct resource *res;
	int rc;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ddr-ctrl");
	ddr_ctrl_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ddr_ctrl_base)) {
		return PTR_ERR(ddr_ctrl_base);
	}

	layers[0].type = EDAC_MC_LAYER_CHIP_SELECT;
	layers[0].size = CDNS_EDAC_NR_CSROWS;
	layers[0].is_virt_csrow = false;
	layers[1].type = EDAC_MC_LAYER_CHANNEL;
	layers[1].size = CDNS_EDAC_NR_CHANS;
	layers[1].is_virt_csrow = false;

	mci = edac_mc_alloc(0, ARRAY_SIZE(layers), layers,
			    sizeof(struct cdns_edac_priv));
	if (!mci) {
		edac_printk(KERN_ERR, EDAC_MC,
			    "Failed memory allocation for mc instance\n");
		return -ENOMEM;
	}

	priv = mci->pvt_info;
	priv->ddr_ctrl_base = ddr_ctrl_base;

	rc = mc_init(mci, pdev);
	if (rc) {
		edac_printk(KERN_ERR, EDAC_MC,
			    "Failed to Init EDAC memory controller instance\n");
		goto free_edac_mc;
	}

	rc = edac_mc_add_mc(mci);
	if (rc) {
		edac_printk(KERN_ERR, EDAC_MC,
			    "Failed to register with EDAC core\n");
		goto free_edac_mc;
	}

	return rc;

free_edac_mc:
	edac_mc_free(mci);

	return rc;
}

/**
 * mc_remove - Unbind driver from controller.
 * @pdev:	Platform device.
 *
 * Return: Unconditionally 0
 */
static int mc_remove(struct platform_device *pdev)
{
	struct mem_ctl_info *mci = platform_get_drvdata(pdev);

	edac_mc_del_mc(&pdev->dev);
	edac_mc_free(mci);

	return 0;
}

static struct platform_driver cdns_edac_mc_driver = {
	.driver = {
		   .name = "Candence-edac",
		   .of_match_table = cdns_edac_match,
		   },
	.probe = mc_probe,
	.remove = mc_remove,
};

module_platform_driver(cdns_edac_mc_driver);

MODULE_AUTHOR("Copyright (c) 2019-2023 Hailo Technologies Ltd. All rights reserved.");
MODULE_DESCRIPTION("Hailo15 EDAC driver for Cadence memory controller");
MODULE_LICENSE("GPL");
