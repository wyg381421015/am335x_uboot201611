/*
 * Copyright 2015 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/arch/clock.h>
#include <asm/io.h>
#include <asm/arch/fsl_serdes.h>
#include <asm/arch/immap_ls102xa.h>
#include <asm/arch/ls102xa_soc.h>
#include <asm/arch/ls102xa_stream_id.h>
#include <fsl_csu.h>

struct liodn_id_table sec_liodn_tbl[] = {
	SET_SEC_JR_LIODN_ENTRY(0, 0x10, 0x10),
	SET_SEC_JR_LIODN_ENTRY(1, 0x10, 0x10),
	SET_SEC_JR_LIODN_ENTRY(2, 0x10, 0x10),
	SET_SEC_JR_LIODN_ENTRY(3, 0x10, 0x10),
	SET_SEC_RTIC_LIODN_ENTRY(a, 0x10),
	SET_SEC_RTIC_LIODN_ENTRY(b, 0x10),
	SET_SEC_RTIC_LIODN_ENTRY(c, 0x10),
	SET_SEC_RTIC_LIODN_ENTRY(d, 0x10),
	SET_SEC_DECO_LIODN_ENTRY(0, 0x10, 0x10),
	SET_SEC_DECO_LIODN_ENTRY(1, 0x10, 0x10),
	SET_SEC_DECO_LIODN_ENTRY(2, 0x10, 0x10),
	SET_SEC_DECO_LIODN_ENTRY(3, 0x10, 0x10),
	SET_SEC_DECO_LIODN_ENTRY(4, 0x10, 0x10),
	SET_SEC_DECO_LIODN_ENTRY(5, 0x10, 0x10),
	SET_SEC_DECO_LIODN_ENTRY(6, 0x10, 0x10),
	SET_SEC_DECO_LIODN_ENTRY(7, 0x10, 0x10),
};

struct smmu_stream_id dev_stream_id[] = {
	{ 0x100, 0x01, "ETSEC MAC1" },
	{ 0x104, 0x02, "ETSEC MAC2" },
	{ 0x108, 0x03, "ETSEC MAC3" },
	{ 0x10c, 0x04, "PEX1" },
	{ 0x110, 0x05, "PEX2" },
	{ 0x114, 0x06, "qDMA" },
	{ 0x118, 0x07, "SATA" },
	{ 0x11c, 0x08, "USB3" },
	{ 0x120, 0x09, "QE" },
	{ 0x124, 0x0a, "eSDHC" },
	{ 0x128, 0x0b, "eMA" },
	{ 0x14c, 0x0c, "2D-ACE" },
	{ 0x150, 0x0d, "USB2" },
	{ 0x18c, 0x0e, "DEBUG" },
};

unsigned int get_soc_major_rev(void)
{
	struct ccsr_gur __iomem *gur = (void *)(CONFIG_SYS_FSL_GUTS_ADDR);
	unsigned int svr, major;

	svr = in_be32(&gur->svr);
	major = SVR_MAJ(svr);

	return major;
}

void s_init(void)
{
}

#ifdef CONFIG_SYS_FSL_ERRATUM_A010315
void erratum_a010315(void)
{
	int i;

	for (i = PCIE1; i <= PCIE2; i++)
		if (!is_serdes_configured(i)) {
			debug("PCIe%d: disabled all R/W permission!\n", i);
			set_pcie_ns_access(i, 0);
		}
}
#endif

int arch_soc_init(void)
{
	struct ccsr_scfg *scfg = (struct ccsr_scfg *)CONFIG_SYS_FSL_SCFG_ADDR;
	struct ccsr_cci400 *cci = (struct ccsr_cci400 *)CONFIG_SYS_CCI400_ADDR;
	unsigned int major;

#ifdef CONFIG_LAYERSCAPE_NS_ACCESS
	enable_layerscape_ns_access();
#endif

#ifdef CONFIG_FSL_QSPI
	out_be32(&scfg->qspi_cfg, SCFG_QSPI_CLKSEL);
#endif

#ifdef CONFIG_FSL_DCU_FB
	out_be32(&scfg->pixclkcr, SCFG_PIXCLKCR_PXCKEN);
#endif

	/* Configure Little endian for SAI, ASRC and SPDIF */
	out_be32(&scfg->endiancr, SCFG_ENDIANCR_LE);

	/*
	 * Enable snoop requests and DVM message requests for
	 * All the slave insterfaces.
	 */
	out_le32(&cci->slave[0].snoop_ctrl,
		 CCI400_DVM_MESSAGE_REQ_EN | CCI400_SNOOP_REQ_EN);
	out_le32(&cci->slave[1].snoop_ctrl,
		 CCI400_DVM_MESSAGE_REQ_EN | CCI400_SNOOP_REQ_EN);
	out_le32(&cci->slave[2].snoop_ctrl,
		 CCI400_DVM_MESSAGE_REQ_EN | CCI400_SNOOP_REQ_EN);
	out_le32(&cci->slave[4].snoop_ctrl,
		 CCI400_DVM_MESSAGE_REQ_EN | CCI400_SNOOP_REQ_EN);

	major = get_soc_major_rev();
	if (major == SOC_MAJOR_VER_1_0) {
		/*
		 * Set CCI-400 Slave interface S1, S2 Shareable Override
		 * Register All transactions are treated as non-shareable
		 */
		out_le32(&cci->slave[1].sha_ord, CCI400_SHAORD_NON_SHAREABLE);
		out_le32(&cci->slave[2].sha_ord, CCI400_SHAORD_NON_SHAREABLE);

		/* Workaround for the issue that DDR could not respond to
		 * barrier transaction which is generated by executing DSB/ISB
		 * instruction. Set CCI-400 control override register to
		 * terminate the barrier transaction. After DDR is initialized,
		 * allow barrier transaction to DDR again */
		out_le32(&cci->ctrl_ord, CCI400_CTRLORD_TERM_BARRIER);
	}

	/* Enable all the snoop signal for various masters */
	out_be32(&scfg->snpcnfgcr, SCFG_SNPCNFGCR_SEC_RD_WR |
				SCFG_SNPCNFGCR_DCU_RD_WR |
				SCFG_SNPCNFGCR_SATA_RD_WR |
				SCFG_SNPCNFGCR_USB3_RD_WR |
				SCFG_SNPCNFGCR_DBG_RD_WR |
				SCFG_SNPCNFGCR_EDMA_SNP);

	/*
	 * Memory controller require a register write before being enabled.
	 * Affects: DDR
	 * Register: EDDRTQCFG
	 * Description: Memory controller performance is not optimal with
	 *		default internal target queue register values.
	 * Workaround: Write a value of 63b2_0042h to address: 157_020Ch.
	 */
	out_be32(&scfg->eddrtqcfg, 0x63b20042);

	return 0;
}

int ls102xa_smmu_stream_id_init(void)
{
	ls1021x_config_caam_stream_id(sec_liodn_tbl,
				      ARRAY_SIZE(sec_liodn_tbl));

	ls102xa_config_smmu_stream_id(dev_stream_id,
				      ARRAY_SIZE(dev_stream_id));

	return 0;
}
