// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2021 Marvell. */

#include "otx2_cpt_devlink.h"

static int otx2_cpt_dl_egrp_create(struct devlink *dl, u32 id,
				   struct devlink_param_gset_ctx *ctx)
{
	struct otx2_cpt_devlink *cpt_dl = devlink_priv(dl);
	struct otx2_cptpf_dev *cptpf = cpt_dl->cptpf;

	return otx2_cpt_dl_custom_egrp_create(cptpf, ctx);
}

static int otx2_cpt_dl_egrp_delete(struct devlink *dl, u32 id,
				   struct devlink_param_gset_ctx *ctx)
{
	struct otx2_cpt_devlink *cpt_dl = devlink_priv(dl);
	struct otx2_cptpf_dev *cptpf = cpt_dl->cptpf;

	return otx2_cpt_dl_custom_egrp_delete(cptpf, ctx);
}

static int otx2_cpt_dl_uc_info(struct devlink *dl, u32 id,
			       struct devlink_param_gset_ctx *ctx)
{
	ctx->val.vstr[0] = '\0';

	return 0;
}

static int otx2_cpt_dl_max_rxc_icb_cnt(struct devlink *dl, u32 id,
				       struct devlink_param_gset_ctx *ctx)
{
	struct otx2_cpt_devlink *cpt_dl = devlink_priv(dl);
	struct otx2_cptpf_dev *cptpf = cpt_dl->cptpf;
	struct pci_dev *pdev = cptpf->pdev;
	u64 reg_val = 0;

	otx2_cpt_read_af_reg(&cptpf->afpf_mbox, pdev, CPT_AF_RXC_CFG1, &reg_val,
			     BLKADDR_CPT0);
	ctx->val.vu16 = (reg_val >> 32) & 0x1FF;

	return 0;
}

static int otx2_cpt_dl_max_rxc_icb_cnt_set(struct devlink *dl, u32 id,
					   struct devlink_param_gset_ctx *ctx)
{
	struct otx2_cpt_devlink *cpt_dl = devlink_priv(dl);
	struct otx2_cptpf_dev *cptpf = cpt_dl->cptpf;
	struct pci_dev *pdev = cptpf->pdev;
	u64 reg_val = 0;

	if (cptpf->enabled_vfs != 0)
		return -EPERM;

	if (cpt_feature_rxc_icb_cnt(pdev)) {
		otx2_cpt_read_af_reg(&cptpf->afpf_mbox, pdev, CPT_AF_RXC_CFG1, &reg_val,
				     BLKADDR_CPT0);
		reg_val &= ~(0x1FFULL << 32);
		reg_val |= (u64)ctx->val.vu16 << 32;
		return otx2_cpt_write_af_reg(&cptpf->afpf_mbox, pdev, CPT_AF_RXC_CFG1,
					     reg_val, BLKADDR_CPT0);
	}
	return 0;
}

static int otx2_cpt_dl_t106_mode_get(struct devlink *dl, u32 id,
				     struct devlink_param_gset_ctx *ctx)
{
	struct otx2_cpt_devlink *cpt_dl = devlink_priv(dl);
	struct otx2_cptpf_dev *cptpf = cpt_dl->cptpf;
	struct pci_dev *pdev = cptpf->pdev;
	u64 reg_val = 0;

	otx2_cpt_read_af_reg(&cptpf->afpf_mbox, pdev, CPT_AF_CTL, &reg_val,
			     BLKADDR_CPT0);
	ctx->val.vu8 = (reg_val >> 18) & 0x1;

	return 0;
}

static int otx2_cpt_dl_t106_mode_set(struct devlink *dl, u32 id,
				     struct devlink_param_gset_ctx *ctx)
{
	struct otx2_cpt_devlink *cpt_dl = devlink_priv(dl);
	struct otx2_cptpf_dev *cptpf = cpt_dl->cptpf;
	struct pci_dev *pdev = cptpf->pdev;
	u64 reg_val = 0;

	if (cptpf->enabled_vfs != 0 || cptpf->eng_grps.is_grps_created)
		return -EPERM;

	if (cpt_feature_sgv2(pdev)) {
		otx2_cpt_read_af_reg(&cptpf->afpf_mbox, pdev, CPT_AF_CTL, &reg_val,
				     BLKADDR_CPT0);
		reg_val &= ~(0x1ULL << 18);
		reg_val |= ((u64)ctx->val.vu8 & 0x1) << 18;
		return otx2_cpt_write_af_reg(&cptpf->afpf_mbox, pdev, CPT_AF_CTL,
					     reg_val, BLKADDR_CPT0);
	}

	return 0;
}

enum otx2_cpt_dl_param_id {
	OTX2_CPT_DEVLINK_PARAM_ID_BASE = DEVLINK_PARAM_GENERIC_ID_MAX,
	OTX2_CPT_DEVLINK_PARAM_ID_EGRP_CREATE,
	OTX2_CPT_DEVLINK_PARAM_ID_EGRP_DELETE,
	OTX2_CPT_DEVLINK_PARAM_ID_MAX_RXC_ICB_CNT,
	OTX2_CPT_DEVLINK_PARAM_ID_T106_MODE,
};

static const struct devlink_param otx2_cpt_dl_params[] = {
	DEVLINK_PARAM_DRIVER(OTX2_CPT_DEVLINK_PARAM_ID_EGRP_CREATE,
			     "egrp_create", DEVLINK_PARAM_TYPE_STRING,
			     BIT(DEVLINK_PARAM_CMODE_RUNTIME),
			     otx2_cpt_dl_uc_info, otx2_cpt_dl_egrp_create,
			     NULL),
	DEVLINK_PARAM_DRIVER(OTX2_CPT_DEVLINK_PARAM_ID_EGRP_DELETE,
			     "egrp_delete", DEVLINK_PARAM_TYPE_STRING,
			     BIT(DEVLINK_PARAM_CMODE_RUNTIME),
			     otx2_cpt_dl_uc_info, otx2_cpt_dl_egrp_delete,
			     NULL),
	DEVLINK_PARAM_DRIVER(OTX2_CPT_DEVLINK_PARAM_ID_MAX_RXC_ICB_CNT,
			     "max_rxc_icb_cnt", DEVLINK_PARAM_TYPE_U16,
			     BIT(DEVLINK_PARAM_CMODE_RUNTIME),
			     otx2_cpt_dl_max_rxc_icb_cnt,
			     otx2_cpt_dl_max_rxc_icb_cnt_set,
			     NULL),
	DEVLINK_PARAM_DRIVER(OTX2_CPT_DEVLINK_PARAM_ID_T106_MODE,
			     "t106_mode", DEVLINK_PARAM_TYPE_U8,
			     BIT(DEVLINK_PARAM_CMODE_RUNTIME),
			     otx2_cpt_dl_t106_mode_get, otx2_cpt_dl_t106_mode_set,
			     NULL),
};

static int otx2_cpt_dl_info_firmware_version_put(struct devlink_info_req *req,
						 struct otx2_cpt_eng_grp_info grp[],
						 const char *ver_name, int eng_type)
{
	struct otx2_cpt_engs_rsvd *eng;
	int i;

	for (i = 0; i < OTX2_CPT_MAX_ENGINE_GROUPS; i++) {
		eng = find_engines_by_type(&grp[i], eng_type);
		if (eng)
			return devlink_info_version_running_put(req, ver_name,
								eng->ucode->ver_str);
	}

	return 0;
}

static int otx2_cpt_devlink_info_get(struct devlink *dl,
				     struct devlink_info_req *req,
				     struct netlink_ext_ack *extack)
{
	struct otx2_cpt_devlink *cpt_dl = devlink_priv(dl);
	struct otx2_cptpf_dev *cptpf = cpt_dl->cptpf;
	int err;

	err = devlink_info_driver_name_put(req, "rvu_cptpf");
	if (err)
		return err;

	err = otx2_cpt_dl_info_firmware_version_put(req, cptpf->eng_grps.grp,
						    "fw.ae", OTX2_CPT_AE_TYPES);
	if (err)
		return err;

	err = otx2_cpt_dl_info_firmware_version_put(req, cptpf->eng_grps.grp,
						    "fw.se", OTX2_CPT_SE_TYPES);
	if (err)
		return err;

	return otx2_cpt_dl_info_firmware_version_put(req, cptpf->eng_grps.grp,
						    "fw.ie", OTX2_CPT_IE_TYPES);
}

static const struct devlink_ops otx2_cpt_devlink_ops = {
	.info_get = otx2_cpt_devlink_info_get,
};

int otx2_cpt_register_dl(struct otx2_cptpf_dev *cptpf)
{
	struct device *dev = &cptpf->pdev->dev;
	struct otx2_cpt_devlink *cpt_dl;
	struct devlink *dl;
	int ret;

	dl = devlink_alloc(&otx2_cpt_devlink_ops,
			   sizeof(struct otx2_cpt_devlink), dev);
	if (!dl) {
		dev_warn(dev, "devlink_alloc failed\n");
		return -ENOMEM;
	}

	cpt_dl = devlink_priv(dl);
	cpt_dl->dl = dl;
	cpt_dl->cptpf = cptpf;
	cptpf->dl = dl;
	ret = devlink_params_register(dl, otx2_cpt_dl_params,
				      ARRAY_SIZE(otx2_cpt_dl_params));
	if (ret) {
		dev_err(dev, "devlink params register failed with error %d",
			ret);
		devlink_free(dl);
		return ret;
	}
	devlink_params_publish(dl);
	devlink_register(dl);

	return 0;
}

void otx2_cpt_unregister_dl(struct otx2_cptpf_dev *cptpf)
{
	struct devlink *dl = cptpf->dl;

	if (!dl)
		return;

	devlink_unregister(dl);
	devlink_params_unregister(dl, otx2_cpt_dl_params,
				  ARRAY_SIZE(otx2_cpt_dl_params));
	devlink_free(dl);
}
