/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2020 Marvell.
 */

#ifndef __OTX2_CPTVF_H
#define __OTX2_CPTVF_H

#include "mbox.h"
#include "otx2_cptlf.h"

#define CPT_AE_EC_ID_MAX	8

struct cpt_ae_fpm_tbl {
	uint8_t *fpm_virt;
	dma_addr_t fpm_dma_addr;
	size_t dma_len;
	uint64_t fpm_iova[CPT_AE_EC_ID_MAX];
};

struct otx2_cptvf_dev {
	void __iomem *reg_base;		/* Register start address */
	void __iomem *pfvf_mbox_base;	/* PF-VF mbox start address */
	struct pci_dev *pdev;		/* PCI device handle */
	struct otx2_cptlfs_info lfs;	/* CPT LFs attached to this VF */
	u8 vf_id;			/* Virtual function index */

	/* PF <=> VF mbox */
	struct otx2_mbox	pfvf_mbox;
	struct work_struct	pfvf_mbox_work;
	struct workqueue_struct *pfvf_mbox_wq;
	struct cpt_ae_fpm_tbl fpm_tbl;

	int blkaddr;
	void *bbuf_base;
	unsigned long cap_flag;
	u64 eng_caps[OTX2_CPT_MAX_ENG_TYPES];
};

irqreturn_t otx2_cptvf_pfvf_mbox_intr(int irq, void *arg);
void otx2_cptvf_pfvf_mbox_handler(struct work_struct *work);
int otx2_cptvf_send_eng_grp_num_msg(struct otx2_cptvf_dev *cptvf, int eng_type);
int otx2_cptvf_send_kvf_limits_msg(struct otx2_cptvf_dev *cptvf);
int otx2_cpt_mbox_bbuf_init(struct otx2_cptvf_dev *cptvf, struct pci_dev *pdev);
int otx2_cptvf_send_caps_msg(struct otx2_cptvf_dev *cptvf);

#endif /* __OTX2_CPTVF_H */
