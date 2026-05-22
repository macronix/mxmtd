/*
 * Copyright (c) 2025 Macronix International Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mxmtd_config.h"
#ifdef CONF_SPINOR

#include <malloc.h>
#include "platform_print.h"
#include "platform_itf.h"
#include "mxmtd.h"
#include "mxmtd_config.h"
#include "spinor_ids.h"
#include "spinor_err.h"
#include "lib.h"
#include "spinor_sfdp.h"

/**
 * TODO:
 * 1. Probe flash info by SFDP
 * 2. Manual selection mode
 * 3. Add more flash support in spinor_ids.h
 * 4. QPI Verification
 * 5. RwW, advanced sector protection
 * 6. Cache for register(SR, CR, CR2, etc.)
 * 7. remove xfer_into_t in spinor.c
 * FIX:
 * 1. Default DOPI mode failed
 **/

#define CMD_IDX_SKIP	0xffffffffU
#define SR_WIP BIT(0)
#define SR_WEL BIT(1)

#define SCUR_WPSEL BIT(7)

#define CR_PBE BIT(4)

#define LR_SPBLD_BIT1	BIT(1)
#define LR_SPBLD_BIT6	BIT(6)
#define LR_PWDPMLD		BIT(2)

#define NBYTES_SECTOR	0x1000
#define ASP_VAL_LOCK	0xff
#define ASP_VAL_UNLOCK	0x00

typedef struct {
	uint32_t s;
	uint32_t e;
} asp_range_t;

#define CHECK_CL_REG_CMD(_cmd, _mode) \
	((_cmd) & snor->id_info->cmd_list[_mode].val[CMDS_LIST_ID_REG])

#define CHECK_CL_OTHERS_CMD(_cmd, _mode) \
	((_cmd) & snor->id_info->cmd_list[_mode].val[CMDS_LIST_OTHERS])

#define CHECK_CL_PGM_CMD(_cmd, _mode) \
	((_cmd) & snor->id_info->cmd_list[_mode].val[CMDS_LIST_PGM])

#define CHECK_CL_ERS_CMD(_cmd, _mode) \
	((_cmd) & snor->id_info->cmd_list[_mode].val[CMDS_LIST_ERS])

#define CHECK_CL_RD_CMD(_cmd, _mode) \
	((_cmd) & snor->id_info->cmd_list[_mode].val[CMDS_LIST_RD])

#define CHECK_SPCL(_flag_spcl) \
	(_flag_spcl & snor->id_info->flag_spcl)

#define CHECK_CL_DOPI \
	(CHECK_CL_RD_CMD(MXIC_8DTRD, SPINOR_MODE_DOPI) && \
	CHECK_CL_PGM_CMD( MXIC_OPI_PGM_CMDS, SPINOR_MODE_DOPI) && \
	CHECK_CL_ERS_CMD(snor->conf_ers.cmd_idx_opi, SPINOR_MODE_DOPI) && \
	CHECK_CL_REG_CMD(MXIC_CR2, SPINOR_MODE_DOPI) && \
	CHECK_CL_REG_CMD((MXIC_CR2), SPINOR_MODE_SPI))

#define CHECK_CL_SOPI \
	(CHECK_CL_RD_CMD(MXIC_8READ, SPINOR_MODE_SOPI) && \
	CHECK_CL_PGM_CMD(MXIC_OPI_PGM_CMDS, SPINOR_MODE_SOPI) && \
	CHECK_CL_ERS_CMD(snor->conf_ers.cmd_idx_opi, SPINOR_MODE_SOPI) && \
	CHECK_CL_REG_CMD(MXIC_CR2, SPINOR_MODE_SOPI) && \
	CHECK_CL_REG_CMD(MXIC_CR2, SPINOR_MODE_SPI))

#define CHECK_CL_QPI \
	(CHECK_CL_RD_CMD(MXIC_QPI_RD_CMDS, SPINOR_MODE_QPI) && \
	CHECK_CL_PGM_CMD(MXIC_QPI_PGM_CMDS, SPINOR_MODE_QPI) && \
	CHECK_CL_ERS_CMD(snor->conf_ers.cmd_idx_spi, SPINOR_MODE_QPI) && \
	CHECK_CL_OTHERS_CMD(MXIC_RSTQIO, SPINOR_MODE_QPI) && \
	CHECK_CL_OTHERS_CMD(MXIC_EQIO, SPINOR_MODE_SPI))

#define SET_CMD_META(_mode, _idx_cmd_rd, _idx_cmd_pgm, _idx_cmd_ers)	\
	do {																\
		snor->cmd_meta->rd.idx_cmd_num = fmsb32(_idx_cmd_rd);				\
		snor->cmd_meta->pgm.idx_cmd_num = fmsb32(_idx_cmd_pgm);				\
		snor->cmd_meta->ers.idx_cmd_num = fmsb32(_idx_cmd_ers);				\
	} while (0)

static void spinor_setup_cmdlist_ers(mxmtd_t *mxmtd, enum SPINOR_MODE mode)
{
	xfer_info_t *xfer = mxmtd->spinor.cmd_meta->priv;
	spinor_t *snor = &mxmtd->spinor;
	uint32_t cmd_list = snor->id_info->cmd_list[mode].val[CMDS_LIST_ERS];

	while (cmd_list) {
		uint32_t idx_cmd = rmsb32(cmd_list);
		uint32_t idx_cmd_num = fmsb32(idx_cmd);

		if (CHECK_HC_CAPS_IO(xfer, ACCU_PROT_IO(cmd_info_ers[idx_cmd_num].prot[mode]))) {
			if (snor->en4b && ADDR_3B == cmd_info_ers[idx_cmd_num].len_addr) {
				cmd_info_ers[idx_cmd_num].len_addr = ADDR_4B;
			}
		} else {
			snor->id_info->cmd_list[mode].val[CMDS_LIST_ERS] &= ~idx_cmd;
		}
		cmd_list &= ~idx_cmd;
	}
}

static void spinor_setup_cmdlist_pgm(mxmtd_t *mxmtd, enum SPINOR_MODE mode)
{
	xfer_info_t *xfer = mxmtd->spinor.cmd_meta->priv;
	spinor_t *snor = &mxmtd->spinor;
	uint32_t cmd_list = snor->id_info->cmd_list[mode].val[CMDS_LIST_PGM];

	while (cmd_list) {
		uint32_t idx_cmd = rmsb32(cmd_list);
		uint32_t idx_cmd_num = fmsb32(idx_cmd);

		if (CHECK_HC_CAPS_IO(xfer, ACCU_PROT_IO(cmd_info_pgm[idx_cmd_num].prot[mode]))) {
			if (snor->en4b && ADDR_3B == cmd_info_pgm[idx_cmd_num].len_addr) {
				cmd_info_pgm[idx_cmd_num].len_addr = ADDR_4B;
			}
		} else {
			snor->id_info->cmd_list[mode].val[CMDS_LIST_PGM] &= ~idx_cmd;
		}
		cmd_list &= ~idx_cmd;
	}
}

static void spinor_setup_cmdlist_rd(mxmtd_t *mxmtd, enum SPINOR_MODE mode)
{
	int n;
	xfer_info_t *xfer = mxmtd->spinor.cmd_meta->priv;
	spinor_t *snor = &mxmtd->spinor;

	for (n = 0; n < snor->id_info->cmd_list[mode].nrdcmds; n++) {
		uint32_t idx_cmd = snor->id_info->cmd_list[mode].rdcmds[n].idx_cmd;
		uint32_t idx_cmd_num = fmsb32(idx_cmd);

		if (CHECK_HC_CAPS_IO(xfer, ACCU_PROT_IO(cmd_info_rd[idx_cmd_num].prot[mode]))) {
			/* Assign information of dummy cycle to id_cmd_info_rd */
			cmd_info_rd[idx_cmd_num].dc[mode] = snor->id_info->cmd_list[mode].rdcmds[n].dc;
			/*  Add valid read command to id_info */
			snor->id_info->cmd_list[mode].val[CMDS_LIST_RD] |= idx_cmd;
			if (snor->en4b && ADDR_3B == cmd_info_rd[idx_cmd_num].len_addr) {
				cmd_info_rd[idx_cmd_num].len_addr = ADDR_4B;
			}
		}
	}

	/* Check word mode support in DOPI read command */
	if (SPINOR_MODE_DOPI == mode) {
		if (CHECK_SPCL(SPCL_WORD_MODE) && !CHECK_HC_CAPS_WORD_MODE(xfer)) {
			mxic_pr_war("DOPI mode is using word mode, but the HC does not support\r\n");
			snor->id_info->cmd_list[SPINOR_MODE_DOPI].val[CMDS_LIST_RD] &= ~MXIC_8DTRD;
		} else {
			cmd_info_rd[fmsb32(MXIC_8DTRD)].word_mode = 1;
		}
	}
}

static void spinor_setup_cmdlist(mxmtd_t *mxmtd)
{
	int n;

	for (n = SPINOR_MODE_SPI; n < MAX_SPINOR_MODE; n++) {
		spinor_setup_cmdlist_ers(mxmtd, n);
		spinor_setup_cmdlist_pgm(mxmtd, n);
		spinor_setup_cmdlist_rd(mxmtd, n);
	}
}

static inline int spinor_rdsr(mxmtd_t *mxmtd, uint8_t *sr)
{
	return cmd_spinor_rdsr(mxmtd->spinor.cmd_meta, sr);
}

static int spinor_poll_sr_ready(mxmtd_t *mxmtd, uint32_t us)
{
	int ret;
	xfer_info_t *xfer = mxmtd->priv;
	uint8_t sr;

	if (!us || !xfer->ops.delay_us) {
		us = 100000;
	}

	do {
		ret = spinor_rdsr(mxmtd, &sr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		if (xfer->ops.delay_us) {
			xfer->ops.delay_us(1);
		}
		us--;
	} while (((SR_WIP | SR_WEL) & sr) && us);

	if ((SR_WIP | SR_WEL) & sr) {
		mxic_pr_err("Polling WIP & WEL timeout failed. SR: %02X\r\n", sr);
		return MXST_ERR_TIMEOUT;
	}

	return MXST_SUCCESS;
}

static inline int spinor_rdcr(mxmtd_t *mxmtd, uint8_t *cr)
{
		return cmd_spinor_rdcr(mxmtd->spinor.cmd_meta, cr);
}

static inline int spinor_rdcr2(mxmtd_t *mxmtd, uint32_t addr, uint8_t *cr2)
{
	return cmd_spinor_rdcr2(mxmtd->spinor.cmd_meta, addr, cr2);
}

static int spinor_wrsr(mxmtd_t *mxmtd, uint8_t sr)
{
	int ret;

	ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinor_wrsr(mxmtd->spinor.cmd_meta, &sr, 1);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return spinor_poll_sr_ready(mxmtd, 0);
}

static int spinor_wrcr(mxmtd_t *mxmtd, uint8_t cr)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;
	uint8_t sr[2];

	switch(snor->cmd_meta->spinor_mode) {
	case SPINOR_MODE_DOPI:
	case SPINOR_MODE_SOPI:
		ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = cmd_spinor_wrcr(mxmtd->spinor.cmd_meta, &cr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = spinor_poll_sr_ready(mxmtd, 0);
		break;
	default:
		ret = cmd_spinor_rdsr(mxmtd->spinor.cmd_meta, sr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		sr[1] = cr;
		ret = cmd_spinor_wrsr(mxmtd->spinor.cmd_meta, sr, 2);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		ret = spinor_poll_sr_ready(mxmtd, 0);
	}
	return ret;
}

static int spinor_wrcr2(mxmtd_t *mxmtd, uint32_t addr, uint8_t cr2)
{
	int ret;

	ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinor_wrcr2(mxmtd->spinor.cmd_meta, addr, &cr2);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return spinor_poll_sr_ready(mxmtd, 0);
}

static int enter_addr_4b(mxmtd_t *mxmtd)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;

	if (snor->en4b) {
		return MXST_SUCCESS;
	}

	if (!CHECK_CL_OTHERS_CMD(MXIC_4B, snor->cmd_meta->spinor_mode)) {
		mxic_pr_war("EN4B command is not supported in spinor mode: %d\r\n",
				snor->cmd_meta->spinor_mode);
		return MXST_ERR_CONT;
	}

	ret = cmd_spinor_en4b(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	snor->en4b = 1;
	mxic_pr_dbg("Enter 4 bytes address mode\r\n");

	return MXST_ERR_CMD_LIST;
}


static int match_id_with_ids(mxmtd_t *mxmtd)
{
	int ret, n = 0;
	spinor_t *snor = &mxmtd->spinor;
	uint8_t *id = snor->buf_shared;

	ret = cmd_spinor_rdid(snor->cmd_meta, id);
	mxic_pr_dbg("ID: %02X%02X%02X\r\n", id[0], id[1], id[2]);
	if (MXST_ERR_TIMEOUT == ret) {
		return MXST_ERR_CONT;
	}
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	while (ids[n].name) {
		if (memcmp(id, ids[n].id, ids[n].len_id)) {
			n++;
			continue;
		}

		snor->id_info = &ids[n];
		mxic_pr_inf("SPINOR [%s] is found, ID: %02X%02X%02X\r\n",
				snor->id_info->name, id[0], id[1], id[2]);
		return MXST_SUCCESS;
	}

	mxic_pr_err("ID is not matched, protocol(%08X), ID:%02X%02X%02X\r\n",
			snor->cmd_meta->prot_reg, id[0], id[1], id[2]);

	return MXST_ERR_CONT;
}

static int set_quad_enable(mxmtd_t *mxmtd, uint8_t en)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;
	uint8_t sr, qe_ofs;

	if (SPINOR_MODE_DOPI ==  snor->cmd_meta->spinor_mode ||
		SPINOR_MODE_SOPI ==  snor->cmd_meta->spinor_mode) {
		return MXST_SUCCESS;
	}

	if (CHECK_SPCL(SPCL_SR_QE_BIT6)) {
		qe_ofs = BIT(6);
	} else {
		mxic_pr_war("SPCL_SR_QE_BITX is not defined in id_info->flag_spcl\r\n");
		return MXST_ERR_CONT;
	}

	ret = spinor_rdsr(mxmtd, &sr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (en == !!(sr & qe_ofs)) {
		snor->enqe = en;
		return MXST_SUCCESS;
	}

	sr ^= qe_ofs;
	ret = spinor_wrsr(mxmtd, sr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_poll_sr_ready(mxmtd, 1000);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_rdsr(mxmtd, &sr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (en == !!(sr & qe_ofs)) {
		snor->enqe = en;
		return MXST_SUCCESS;
	}

	return MXST_ERR_QUAD_EN;
}

static int setup_dc_opi(mxmtd_t *mxmtd)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;
	uint8_t cr2;

	snor->cmd_meta->cmd_info = &cmd_info_rd[snor->cmd_meta->rd.idx_cmd_num];
	cr2 = (snor->cmd_meta->cmd_info->dc[snor->cmd_meta->spinor_mode] >> 8) & 0xff;

	if (0xff != cr2) {
		ret = spinor_wrcr2(mxmtd, ADDR_CR2_DC, cr2);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = spinor_poll_sr_ready(mxmtd, 1000);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
	}

	snor->set_dc = 1;

	return MXST_SUCCESS;
}

static int setup_dc_spi_qpi(mxmtd_t *mxmtd)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;
	uint8_t reg_cr, cr;

	snor->cmd_meta->cmd_info = &cmd_info_rd[snor->cmd_meta->rd.idx_cmd_num];
	cr = (snor->cmd_meta->cmd_info->dc[snor->cmd_meta->spinor_mode] >> 8) & 0xff;

	if (!CHECK_CL_REG_CMD(MXIC_RDCR, snor->cmd_meta->spinor_mode)) {
		mxic_pr_err("RDCR command is not found in id_info->cmd_list\r\n");
		return MXST_ERR_CMD_LIST;
	}

	if (0xff != cr) {
		uint8_t dc_mask, dc_ofs;

		if (CHECK_SPCL(SPCL_CR_DC_BIT6_7)) {
			dc_mask = 0xc0;
			dc_ofs = 6;
		} else {
			mxic_pr_err("SPCL_CR_DC_BITX_Y is not defined in id_info->flag_spcl");
			return MXST_ERR_NOT_SUP;
		}

		ret = spinor_rdcr(mxmtd, &reg_cr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		reg_cr = (reg_cr & ~dc_mask) | (cr << dc_ofs);
		ret = spinor_wrcr(mxmtd, reg_cr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = spinor_poll_sr_ready(mxmtd, 1000);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
	}
	snor->set_dc = 1;
	return MXST_SUCCESS;
}

static inline int setup_dc(mxmtd_t *mxmtd)
{
	spinor_t *snor = &mxmtd->spinor;

	if (SPINOR_MODE_DOPI == snor->cmd_meta->spinor_mode ||
		SPINOR_MODE_SOPI == snor->cmd_meta->spinor_mode) {
		return setup_dc_opi(mxmtd);
	}

	return setup_dc_spi_qpi(mxmtd);
}

static int scan_mode(mxmtd_t *mxmtd)
{
	int ret;
	xfer_info_t *xfer = mxmtd->priv;
	spinor_t *snor = &mxmtd->spinor;

	mxic_pr_dbg("Scan SPI mode...\r\n");
	snor->cmd_meta->prot_reg = PROT_1S_1S_1S;
	ret = match_id_with_ids(mxmtd);
	if (MXST_SUCCESS == ret) {
		mxic_pr_dbg("The initial protocol is 1S-1S-1S\r\n");
		snor->cmd_meta->spinor_mode = SPINOR_MODE_SPI;
		return MXST_SUCCESS;

	} else if (MXST_ERR_CONT != ret) {
		return ret;
	}

	if (!CHECK_HC_CAPS_IO(xfer, 8)) {
		mxic_pr_err("Host controller is not supported in 8IO mode\r\n");
		return MXST_ERR_SCAN;
	}

	if (!CHECK_HC_CAPS_DTR(xfer)) {
		mxic_pr_war("Host controller is not supported in DTR mode\r\n");

	} else {
		mxic_pr_dbg("Scan DOPI mode...\r\n");
		snor->cmd_meta->prot_reg = PROT_8D_8D_8D;
		ret = match_id_with_ids(mxmtd);
		if (MXST_SUCCESS == ret) {
			mxic_pr_dbg("The initial protocol is DOPI(8D-8D-8D)\r\n");
			snor->cmd_meta->spinor_mode = SPINOR_MODE_DOPI;
			return MXST_SUCCESS;

		} else if (MXST_ERR_CONT != ret) {
			return ret;
		}
	}

	mxic_pr_dbg("Scan SOPI mode...\r\n");
	snor->cmd_meta->prot_reg = PROT_8S_8S_8S;
	ret = match_id_with_ids(mxmtd);
	if (MXST_SUCCESS == ret) {
		mxic_pr_dbg("The initial protocol is SOPI(8S-8S-8S)\r\n");
		snor->cmd_meta->spinor_mode = SPINOR_MODE_SOPI;
		return MXST_SUCCESS;

	} else if (MXST_ERR_CONT != ret) {
		return ret;
	}
//#endif
	return MXST_ERR_SCAN;
}

static inline int set_conf_ers_cmd(mxmtd_t *mxmtd)
{
	spinor_t *snor = &mxmtd->spinor;

	if (!CHECK_CL_ERS_CMD(snor->conf_ers.cmd_idx_3b, SPINOR_MODE_SPI)) {
		mxic_pr_err("Erase command index (BIT%d) is not found in id_info!\r\n",
				fmsb32(snor->conf_ers.cmd_idx_3b));
		return MXST_ERR_CONFIG;
	}
	snor->conf_ers.cmd_idx_spi = (CHECK_CL_ERS_CMD(snor->conf_ers.cmd_idx_4b, SPINOR_MODE_SPI) ?
		snor->conf_ers.cmd_idx_4b : snor->conf_ers.cmd_idx_3b);

	snor->conf_ers.cmd_idx_opi = snor->conf_ers.cmd_idx_4b;
	return MXST_SUCCESS;
}

static int check_rdid(mxmtd_t *mxmtd)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;
	uint8_t *id = snor->buf_shared;

	ret = cmd_spinor_rdid(snor->cmd_meta, id);
	mxic_pr_dbg("ID: %02X%02X%02X\r\n", id[0], id[1], id[2]);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (memcmp(snor->id_info->id, id, snor->id_info->len_id)) {
		mxic_pr_err("ID is not matched, %02X%02X%02X\r\n", id[0], id[1], id[2]);
		return MXST_ERR_CONT;
	}
	return MXST_SUCCESS;
}

static int setup_sopi_dqs(mxmtd_t *mxmtd)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;
	uint8_t cr2;

	/* Check dqs in SOPI mode */
	if (CHECK_CL_SOPI) {
		ret = spinor_rdcr2(mxmtd, ADDR_CR2_DQS, &cr2);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		cmd_info_rd[fmsb32(MXIC_8READ)].dqs = !!(DATA_CR2_DQS_STR_EN & cr2);
		mxic_pr_dbg(" DQS in SPOI mode is %s\r\n", cmd_info_rd[fmsb32(MXIC_8READ)].dqs ? "enabled" : "disabled");
	}

	return MXST_SUCCESS;
}

static int spinor_preamble_calibration(mxmtd_t *mxmtd)
{
	int ret;
	uint8_t cr;
	spinor_t *snor = &mxmtd->spinor;

	if (!CHECK_SPCL(SPCL_PREAMBLE)) {
		mxic_pr_dbg("SPCL_PREAMBLE is not supported, calibration stop\r\n");
		return MXST_SUCCESS;
	}

	mxic_pr_dbg("OctaFlash DOPI mode calibration start\r\n");

	ret = spinor_rdcr(mxmtd, &cr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_wrcr(mxmtd, cr | CR_PBE);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_rdcr(mxmtd, &cr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (!(cr & CR_PBE)) {
		mxic_pr_war("Preamble mode is not supported\r\n");
		return MXST_SUCCESS;
	}

	snor->cmd_meta->cmd_info = &cmd_info_rd[snor->cmd_meta->rd.idx_cmd_num];
	ret = cmd_spinor_preamble_calibration(snor->cmd_meta);
	if (MXST_SUCCESS != ret) {
		mxic_pr_err("DOPI calibration FAIL\r\n");
		return ret;
	}

	cr ^= CR_PBE;
	return spinor_wrcr(mxmtd, cr);
}

static int switch_mode_opi(mxmtd_t *mxmtd, enum SPINOR_MODE mode_target)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;
	uint8_t cr2;

	if (!CHECK_CL_DOPI && SPINOR_MODE_DOPI == mode_target) {
		mxic_pr_war("DOPI: DOPI/CR2 are not defined in cmd_list\r\n");
		ret = MXST_ERR_CONT;
	}
	if (!CHECK_CL_SOPI && SPINOR_MODE_SOPI == mode_target) {
		mxic_pr_war("SOPI: SOPI/CR2 are not defined in cmd_list\r\n");
		ret = MXST_ERR_CONT;
	}
	mxic_pr_dbg("[ID_REG]: %08X, [OTHERS]: %08X, [READ]: %08X, [PGM]: %08X, [ERS]: %08X\r\n",
			snor->id_info->cmd_list[SPINOR_MODE_DOPI].val[CMDS_LIST_ID_REG],
			snor->id_info->cmd_list[SPINOR_MODE_DOPI].val[CMDS_LIST_OTHERS],
			snor->id_info->cmd_list[SPINOR_MODE_DOPI].val[CMDS_LIST_RD],
			snor->id_info->cmd_list[SPINOR_MODE_DOPI].val[CMDS_LIST_PGM],
			snor->id_info->cmd_list[SPINOR_MODE_DOPI].val[CMDS_LIST_ERS]
			);
	if (MXST_ERR_CONT == ret) {
		return ret;
	}

	if (snor->cmd_meta->spinor_mode == mode_target && snor->set_dc) {
		return MXST_SUCCESS;
	}

	if (SPINOR_MODE_SPI != snor->cmd_meta->spinor_mode) {
		cr2 = DATA_CR2_OPI_SPI_EN;
		ret = spinor_wrcr2(mxmtd, ADDR_CR2_OPI, cr2);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		snor->cmd_meta->spinor_mode = SPINOR_MODE_SPI;
		snor->cmd_meta->prot_reg = PROT_1S_1S_1S;

		ret = spinor_poll_sr_ready(mxmtd, 1000);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = check_rdid(mxmtd);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (SPINOR_MODE_SPI == mode_target) {
			cmd_info_pgm[fmsb32(MXIC_PP4B)].word_mode = 0;
			SET_CMD_META(SPINOR_MODE_SPI,
					snor->id_info->cmd_list[SPINOR_MODE_SPI].val[CMDS_LIST_RD],
					snor->id_info->cmd_list[SPINOR_MODE_SPI].val[CMDS_LIST_PGM],
					snor->conf_ers.cmd_idx_spi);

			return setup_dc(mxmtd);
		}
	}

	cr2 = (SPINOR_MODE_DOPI == mode_target) ? DATA_CR2_OPI_DTR_EN : DATA_CR2_OPI_STR_EN;
	ret = spinor_wrcr2(mxmtd, ADDR_CR2_OPI, cr2);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	snor->cmd_meta->spinor_mode = mode_target;
	snor->cmd_meta->prot_reg = (SPINOR_MODE_DOPI == mode_target) ? PROT_8D_8D_8D : PROT_8S_8S_8S;

	ret = spinor_poll_sr_ready(mxmtd, 1000);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = check_rdid(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	cmd_info_pgm[fmsb32(MXIC_PP4B)].word_mode = (SPINOR_MODE_DOPI == mode_target) ?
				!!(CHECK_SPCL(SPCL_WORD_MODE)) : 0;

	SET_CMD_META(mode_target,
			snor->id_info->cmd_list[mode_target].val[CMDS_LIST_RD],
			snor->id_info->cmd_list[mode_target].val[CMDS_LIST_PGM],
			snor->conf_ers.cmd_idx_opi);

	return setup_dc(mxmtd);
}

static int switch_mode_qpi(mxmtd_t *mxmtd, enum SPINOR_MODE mode_target)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;

	if (!CHECK_CL_QPI) {
		mxic_pr_war("QPI: EQIO/RSTQIO are not defined in cmd_list\r\n");
		mxic_pr_dbg("[ID_REG]: %08X, [OTHERS]: %08X, [READ]: %08X, [PGM]: %08X, [ERS]: %08X\r\n",
				snor->id_info->cmd_list[SPINOR_MODE_QPI].val[CMDS_LIST_ID_REG],
				snor->id_info->cmd_list[SPINOR_MODE_QPI].val[CMDS_LIST_OTHERS],
				snor->id_info->cmd_list[SPINOR_MODE_QPI].val[CMDS_LIST_RD],
				snor->id_info->cmd_list[SPINOR_MODE_QPI].val[CMDS_LIST_PGM],
				snor->id_info->cmd_list[SPINOR_MODE_QPI].val[CMDS_LIST_ERS]
					);
		return MXST_ERR_CONT;
	}

	if (snor->cmd_meta->spinor_mode == mode_target && snor->set_dc) {
		return MXST_SUCCESS;
	}

	switch (mode_target) {
	case SPINOR_MODE_QPI:
		ret = cmd_spinor_eqpi(mxmtd->spinor.cmd_meta);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		snor->cmd_meta->spinor_mode = SPINOR_MODE_QPI;
		snor->cmd_meta->prot_reg = PROT_4S_4S_4S;

		ret = check_rdid(mxmtd);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		cmd_info_pgm[fmsb32(MXIC_PP4B)].word_mode = 0;
		SET_CMD_META(mode_target,
				snor->id_info->cmd_list[SPINOR_MODE_QPI].val[CMDS_LIST_RD],
				snor->id_info->cmd_list[SPINOR_MODE_QPI].val[CMDS_LIST_PGM],
				snor->conf_ers.cmd_idx_spi);

		break;
	case SPINOR_MODE_SPI:
		ret = cmd_spinor_rstqpi(mxmtd->spinor.cmd_meta);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		snor->cmd_meta->spinor_mode = SPINOR_MODE_SPI;
		snor->cmd_meta->prot_reg = PROT_1S_1S_1S;

		ret = check_rdid(mxmtd);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		cmd_info_pgm[fmsb32(MXIC_PP4B)].word_mode = 0;
		SET_CMD_META(SPINOR_MODE_SPI,
				snor->id_info->cmd_list[SPINOR_MODE_SPI].val[CMDS_LIST_RD],
				snor->id_info->cmd_list[SPINOR_MODE_SPI].val[CMDS_LIST_PGM],
				snor->conf_ers.cmd_idx_spi);
		break;
	default:
		return MXST_ERR_PARAM;
	}

	return setup_dc(mxmtd);
}

static int switch_mode_spi(mxmtd_t *mxmtd)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;

	snor->cmd_meta->spinor_mode = SPINOR_MODE_SPI;
	snor->cmd_meta->prot_reg = PROT_1S_1S_1S;

	ret = check_rdid(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	cmd_info_pgm[fmsb32(MXIC_PP4B)].word_mode = 0;
	SET_CMD_META(SPINOR_MODE_SPI,
			snor->id_info->cmd_list[SPINOR_MODE_SPI].val[CMDS_LIST_RD],
			snor->id_info->cmd_list[SPINOR_MODE_SPI].val[CMDS_LIST_PGM],
			snor->conf_ers.cmd_idx_spi);

	return setup_dc(mxmtd);
}

static int sel_cmd_info_auto(mxmtd_t *mxmtd)
{
	int ret;

	mxic_pr_dbg("Auto select DOPI mode...\r\n");
	ret = switch_mode_opi(mxmtd, SPINOR_MODE_DOPI);
	if (MXST_SUCCESS == ret) {
		mxic_pr_inf("DOPI mode is selected\r\n");
		return MXST_SUCCESS;
	} else if (MXST_ERR_CONT != ret) {
		return ret;
	}

	mxic_pr_dbg("Auto select SOPI mode...\r\n");
	ret = switch_mode_opi(mxmtd, SPINOR_MODE_SOPI);
	if (MXST_SUCCESS == ret) {
		mxic_pr_inf("SOPI mode is selected\r\n");
		return MXST_SUCCESS;
	} else if (MXST_ERR_CONT != ret) {
		return ret;
	}

	mxic_pr_dbg("Auto select QPI mode...\r\n");
	ret = switch_mode_qpi(mxmtd, SPINOR_MODE_QPI);
	if (MXST_SUCCESS == ret) {
		mxic_pr_inf("QPI mode is selected\r\n");
		return MXST_SUCCESS;
	} else if (MXST_ERR_CONT != ret) {
		return ret;
	}

	mxic_pr_dbg("Auto select SPI mode...\r\n");
	ret = switch_mode_spi(mxmtd);
	if (MXST_SUCCESS == ret) {
		mxic_pr_inf("SPI mode is selected\r\n");
		return MXST_SUCCESS;
	}
	return ret;
}

static int setup_cmd_info(mxmtd_t *mxmtd)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;

	/* Set erase command */
	mxic_pr_dbg("set_conf_ers_cmd......\r\n");
	ret = set_conf_ers_cmd(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	/* Enable address 4 bytes mode */
	mxic_pr_dbg("enter_addr_4b......\r\n");
	ret = enter_addr_4b(mxmtd);
	if (MXST_SUCCESS == ret && MXST_ERR_CONT != ret) {
		return ret;
	}

	/*  Set up cmd_list and cmd_info for read/program/erase */
	mxic_pr_dbg("setup command list......\r\n");
	spinor_setup_cmdlist(mxmtd);

	/* Check if OPI command is CMD CMD or CMD CMD_BAR */
	snor->cmd_meta->mask_cmd = CHECK_SPCL(SPCL_OPI_CMD_CMD) ? 0x00 : 0xff;

	/* Configure dqs in SOPI mode and word mode in DOPI mode to cmd_info_pgm and cmd_info_rd */
	mxic_pr_dbg(" Set up DQS in SOPI mode\r\n");
	return setup_sopi_dqs(mxmtd);
}

static int spinor_reset(mxmtd_t *mxmtd, enum SPINOR_MODE mode)
{
	int ret;
	xfer_info_t *xfer = mxmtd->priv;

	switch (mode) {
	case SPINOR_MODE_DOPI:
		mxmtd->spinor.cmd_meta->prot_reg = PROT_8D_8D_8D;
		break;
	case SPINOR_MODE_SOPI:
		mxmtd->spinor.cmd_meta->prot_reg = PROT_8S_8S_8S;
		break;
	case SPINOR_MODE_QPI:
		mxmtd->spinor.cmd_meta->prot_reg = PROT_4S_4S_4S;
		break;
	default:
		mxmtd->spinor.cmd_meta->prot_reg = PROT_1S_1S_1S;
		break;
	}

	ret = cmd_spinor_rsten(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinor_rst(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (!xfer->ops.delay_us) {
		mxic_pr_war("Please add 40 us delay for reset command\r\n");
	} else {
		xfer->ops.delay_us(40);
	}

	return ret;
}

static int spinor_read_8d8d8d(cmd_meta_t *cmd_meta, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	uint64_t remain;

	if (addr % 2) {
		uint8_t buf_tmp[2];

		addr--;
		ret = cmd_spinor_read(cmd_meta, addr, buf_tmp, 2);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		buf[0] = buf_tmp[1];

		addr += 2;
		buf++;
		nbytes--;
	}

	remain = nbytes % 2;
	nbytes -= remain;

	if (nbytes) {
		ret = cmd_spinor_read(cmd_meta, addr, buf, nbytes);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		addr += nbytes;
		buf += nbytes;
	}

	if (remain) {
		uint8_t buf_tmp[2];

		ret = cmd_spinor_read(cmd_meta, addr, buf_tmp, 2);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		buf[0] = buf_tmp[0];
	}

	return MXST_SUCCESS;
}

static int spinor_read(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	cmd_meta_t *cmd_meta = mxmtd->spinor.cmd_meta;
	uint64_t size_chip;

	if (!nbytes) {
		mxic_pr_war("Read bytes = 0, do nothing!\r\n");
		return MXST_SUCCESS;
	}
	if (!buf) {
		mxic_pr_err("Read buffer is NULL\r\n");
		return MXST_ERR_PTR_NULL;
	}

	size_chip = mxmtd->spinor.id_info->size_chip;
	if (addr >= size_chip || nbytes > (size_chip - addr)) {
		mxic_pr_err("Read range exceeds chip size: addr=%08X%08Xh, nbytes=%08X%08Xh, chip=%08X%08Xh\r\n",
				(uint32_t)(addr >> 32), (uint32_t)addr,
				(uint32_t)(nbytes >> 32), (uint32_t)nbytes,
				(uint32_t)(size_chip >> 32), (uint32_t)size_chip);
		return MXST_ERR_PARAM;
	}

	cmd_meta->cmd_info = &cmd_info_rd[cmd_meta->rd.idx_cmd_num];

	if (PROT_8D_8D_8D == cmd_meta->cmd_info->prot[cmd_meta->spinor_mode]) {
		return spinor_read_8d8d8d(cmd_meta, addr,buf, nbytes);
	}

	return cmd_spinor_read(cmd_meta, addr, buf, nbytes);
}

static int spinor_program_8d8d8d(cmd_meta_t *cmd_meta, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	uint64_t align_s = addr;
	uint64_t remain = nbytes;
	uint8_t buf_tmp[SIZE_PAGE];

	memset(buf_tmp, 0xff, SIZE_PAGE);

	if (addr % 2) {
		align_s--;
		remain++;
	}

	if (remain % 2) {
		remain++;
	}

	memcpy(&buf_tmp[addr % 2], buf, nbytes);

	return cmd_spinor_program(cmd_meta, align_s, buf_tmp, remain);
}

static int spinor_program(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	uint64_t size_pgm;
	cmd_meta_t *cmd_meta = mxmtd->spinor.cmd_meta;
	uint64_t size_chip;

	if (!nbytes) {
		mxic_pr_war("Program bytes = 0, do nothing!\r\n");
		return MXST_SUCCESS;
	}
	if (!buf) {
		mxic_pr_err("Program buffer is NULL\r\n");
		return MXST_ERR_PTR_NULL;
	}

	size_chip = mxmtd->spinor.id_info->size_chip;
	if (addr >= size_chip || nbytes > (size_chip - addr)) {
		mxic_pr_err("Program range exceeds chip size: addr=%08X%08Xh, nbytes=%08X%08Xh, chip=%08X%08Xh\r\n",
				(uint32_t)(addr >> 32), (uint32_t)addr,
				(uint32_t)(nbytes >> 32), (uint32_t)nbytes,
				(uint32_t)(size_chip >> 32), (uint32_t)size_chip);
		return MXST_ERR_PARAM;
	}

	cmd_meta->cmd_info = &cmd_info_pgm[cmd_meta->pgm.idx_cmd_num];

	size_pgm = SIZE_PAGE - (addr % SIZE_PAGE);
	if (SIZE_PAGE != size_pgm) {
		size_pgm = (nbytes < size_pgm) ? nbytes : size_pgm;
	} else {
		size_pgm = (nbytes < SIZE_PAGE) ? nbytes : SIZE_PAGE;
	}

	while (nbytes) {
		ret = cmd_spinor_wren(cmd_meta);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (SIZE_PAGE != size_pgm && PROT_8D_8D_8D == cmd_meta->cmd_info->prot[cmd_meta->spinor_mode]) {
			ret = spinor_program_8d8d8d(cmd_meta, addr, buf, size_pgm);
		} else {
			ret = cmd_spinor_program(cmd_meta, addr, buf, size_pgm);

		}
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = spinor_poll_sr_ready(mxmtd, 1000);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		nbytes -= size_pgm;
		addr += size_pgm;
		buf += size_pgm;

		size_pgm = nbytes < SIZE_PAGE ? nbytes : SIZE_PAGE;
	}

	return MXST_SUCCESS;
}

static int spinor_erase(mxmtd_t *mxmtd, uint64_t addr, uint64_t nbytes)
{
	int ret;
	cmd_meta_t *cmd_meta = mxmtd->spinor.cmd_meta;
	uint64_t size_chip;

	if (!nbytes) {
		mxic_pr_war("Erase bytes = 0, do nothing!\r\n");
		return MXST_SUCCESS;
	}

	size_chip = mxmtd->spinor.id_info->size_chip;
	if (addr >= size_chip || nbytes > (size_chip - addr)) {
		mxic_pr_err("Erase range exceeds chip size: addr=%08X%08Xh, nbytes=%08X%08Xh, chip=%08X%08Xh\r\n",
				(uint32_t)(addr >> 32), (uint32_t)addr,
				(uint32_t)(nbytes >> 32), (uint32_t)nbytes,
				(uint32_t)(size_chip >> 32), (uint32_t)size_chip);
		return MXST_ERR_PARAM;
	}

	if (addr % mxmtd->spinor.conf_ers.size) {
		mxic_pr_err("Address is not allowed with erase size(%08X)\r\n",
				mxmtd->spinor.conf_ers.size);
		return MXST_ERR_NOT_ALIGN;
	}

	if (nbytes % mxmtd->spinor.conf_ers.size) {
		mxic_pr_err("Erase bytes is not allowed with erase size(%08X)\r\n",
				mxmtd->spinor.conf_ers.size);
		return MXST_ERR_NOT_ALIGN;
	}

	cmd_meta->cmd_info = &cmd_info_ers[cmd_meta->ers.idx_cmd_num];

	while (nbytes) {
		ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = cmd_spinor_erase(cmd_meta, addr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = spinor_poll_sr_ready(mxmtd, 20000);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		addr += mxmtd->spinor.conf_ers.size;
		nbytes -= mxmtd->spinor.conf_ers.size;
	}

	return MXST_SUCCESS;
}

static uint64_t spinor_get_chip_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinor.id_info->size_chip;
}

static uint32_t spinor_get_blk_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinor.id_info->size_block;
}

static uint32_t spinor_get_page_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinor.id_info->size_page;
}

static uint32_t spinor_get_ers_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinor.conf_ers.size;
}

static int spinor_get_ecc_state(mxmtd_t *mxmtd)
{
	return 0;
}

static int spinor_rdlr(mxmtd_t *mxmtd, uint8_t *lr)
{
	return cmd_spinor_rdlr(mxmtd->spinor.cmd_meta, lr);
}

static int spinor_wrlr(mxmtd_t *mxmtd, uint8_t *lr)
{
	int ret;

	ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return cmd_spinor_wrlr(mxmtd->spinor.cmd_meta, lr);
}

static int spinor_wrspb(mxmtd_t *mxmtd, uint32_t addr)
{
	int ret;

	ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinor_wrspb(mxmtd->spinor.cmd_meta, addr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return spinor_poll_sr_ready(mxmtd, 0);
}

static int spinor_rdspb(mxmtd_t *mxmtd, uint32_t addr, uint8_t *spb)
{
	return cmd_spinor_rdspb(mxmtd->spinor.cmd_meta, addr, spb);
}

static int spinor_ersspb(mxmtd_t *mxmtd)
{
	int ret;

	ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinor_ersspb(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return spinor_poll_sr_ready(mxmtd, 0);
}

static int spinor_wrdpb(mxmtd_t *mxmtd, uint32_t addr, uint8_t *dpb)
{
	int ret;

	ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return cmd_spinor_wrdpb(mxmtd->spinor.cmd_meta, addr, dpb);
}

static int spinor_rddpb(mxmtd_t *mxmtd, uint32_t addr, uint8_t *dpb)
{
	return cmd_spinor_rddpb(mxmtd->spinor.cmd_meta, addr, dpb);
}

static int spinor_rdscur(mxmtd_t *mxmtd, uint8_t *scur)
{
	return cmd_spinor_rdscur(mxmtd->spinor.cmd_meta, scur);
}

static int spinor_wpsel(mxmtd_t *mxmtd)
{
	int ret;

	ret = cmd_spinor_wren(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinor_wpsel(mxmtd->spinor.cmd_meta);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return spinor_poll_sr_ready(mxmtd, 0);
}

static int spinor_asp_set_wpsel(mxmtd_t *mxmtd)
{
	int ret = MXST_SUCCESS;
	uint8_t scur;

	if (mxmtd->spinor.wpsel) {
		return MXST_SUCCESS;
	}

	if (SPCL_SCUR_WPSEL & mxmtd->spinor.id_info->flag_spcl) {
		ret = spinor_wpsel(mxmtd);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = spinor_rdscur(mxmtd, &scur);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		mxmtd->spinor.wpsel = !!(SCUR_WPSEL & scur);

		if (0 == mxmtd->spinor.wpsel) {
			mxic_pr_err("Write WPSEL to scur (%02X) failed\r\n", scur);
			return MXST_ERR_WPSEL;
		}
	} else {
		mxic_pr_inf("This Flash is not supported WPSEL in ID Table\r\n");
		return MXST_ERR_NOT_SUP;
	}

	return ret;
}

static int spinor_query_lock_mode(mxmtd_t *mxmtd)
{
	if (0 == mxmtd->spinor.wpsel) {
//		mxic_pr_dbg("It is BP mode\r\n");
		return BP_MODE;
	}

	if (mxmtd->spinor.asp_pwd) {
//		mxic_pr_dbg("It is ASP w/ Password mode\r\n");
		return ASP_PWD_MODE;
	}

//	mxic_pr_dbg("It is ASP mode\r\n");
	return ASP_MODE;
}

static uint32_t spinor_lock_size(mxmtd_t *mxmtd, uint32_t addr)
{
	if (mxmtd->spinor.wpsel && SPCL_ASP_4KB_IN_BLK & mxmtd->spinor.id_info->flag_spcl) {
		uint32_t bdy_first = mxmtd->spinor.id_info->size_block;
		uint32_t bdy_last = mxmtd->spinor.id_info->size_chip - mxmtd->spinor.id_info->size_block;

		if (addr < bdy_first || addr >= bdy_last) {
			return NBYTES_SECTOR;
		}
	}

	return mxmtd->spinor.id_info->size_block;
}

static int spinor_asp_set_boundary(mxmtd_t *mxmtd, asp_range_t *range)
{
	if (range->s > range->e || range->e >= mxmtd->spinor.id_info->size_chip) {
		return MXST_ERR_PARAM;
	}

	range->s = ROUNDDOWN(range->s, spinor_lock_size(mxmtd, range->s));
	range->e = ROUNDUP(range->e, spinor_lock_size(mxmtd, range->e)) - 1;

	return MXST_SUCCESS;
}

static int spinor_asp_init(mxmtd_t *mxmtd, asp_range_t *range)
{
	int lock_mode = spinor_query_lock_mode(mxmtd);

	if (BP_MODE == lock_mode) {
		mxic_pr_err("WPSEL is not set\r\n");
		return MXST_ERR_NOT_SUP;
	}

	if (ASP_PWD_MODE == lock_mode) {
		mxic_pr_err("ASP PASSWORD MODE is not support yet\r\n");
		return MXST_ERR_NOT_SUP;
	}

	return spinor_asp_set_boundary(mxmtd, range);
}

static int spinor_asp_spb_unlock(mxmtd_t *mxmtd, asp_range_t *range)
{
	int ret, n = 0;
	spinor_t *snor = &mxmtd->spinor;
	uint32_t ofs = 0;
	uint32_t num_spb = snor->id_info->nblocks;
	uint32_t nbytes_chip = snor->id_info->size_chip;
	uint8_t *spb;

	if (0 == range->s && (nbytes_chip - 1) == range->e) {
		return spinor_ersspb(mxmtd);
	}

	if (SPCL_ASP_4KB_IN_BLK & snor->id_info->flag_spcl) {
		num_spb += ((snor->id_info->size_block / NBYTES_SECTOR) * 2) - 2;
	}

	spb = calloc(num_spb, sizeof(uint8_t));
	if (0 == spb) {
		return MXST_ERR_ALLOC;
	}

	for (; ofs < nbytes_chip; ofs += spinor_lock_size(mxmtd, ofs), n++) {
		if (ofs < range->s || ofs > range->e) {
			ret = spinor_rdspb(mxmtd, ofs, &spb[n]);
			if (MXST_SUCCESS != ret) {
				free(spb);
				return ret;
			}
		}
	}

	ret = spinor_ersspb(mxmtd);
	if (MXST_SUCCESS != ret) {
		free(spb);
		return ret;
	}

	for (ofs = 0, n = 0; ofs < nbytes_chip; ofs += spinor_lock_size(mxmtd, ofs), n++) {
		if ((ofs < range->s || ofs > range->e) && 0xff == spb[n]) {
			ret = spinor_wrspb(mxmtd, ofs);
			if (MXST_SUCCESS != ret) {
				free(spb);
				return ret;
			}
		}
	}

	free(spb);
	return ret;
}

static int spinor_asp_lock(mxmtd_t *mxmtd, uint32_t addr, uint32_t nbytes, int target)
{
	int ret;
	asp_range_t range = {.s = addr, .e = addr + nbytes - 1};
	uint8_t val = 0xff;
	uint32_t ofs;

	ret = spinor_asp_init(mxmtd, &range);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	for (ofs = range.s; ofs < range.e; ofs += spinor_lock_size(mxmtd, ofs)) {
		switch (target) {
		case ASP_SPB:
			ret = spinor_wrspb(mxmtd, ofs);
			if (MXST_SUCCESS != ret) {
				return ret;
			}
			break;
		case ASP_DPB:
			ret = spinor_wrdpb(mxmtd, ofs, &val);
			if (MXST_SUCCESS != ret) {
				return ret;
			}
			break;
		default:
			mxic_pr_err("Lock target(%d) is not supported\r\n", target);
			return MXST_ERR_NOT_SUP;
		}
	}

	return ret;
}

static int spinor_asp_unlock(mxmtd_t *mxmtd, uint32_t addr, uint32_t nbytes, int target)
{
	int ret;
	asp_range_t range = {.s = addr, .e = addr + nbytes - 1};
	uint8_t val = 0x00;
	uint32_t ofs;

	ret = spinor_asp_init(mxmtd, &range);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	switch (target) {
	case ASP_SPB:
		ret = spinor_asp_spb_unlock(mxmtd, &range);
		break;
	case ASP_DPB:
		for (ofs = range.s; ofs < range.e; ofs += spinor_lock_size(mxmtd, ofs)) {
			ret = spinor_wrdpb(mxmtd, ofs, &val);
			if (MXST_SUCCESS != ret) {
				return ret;
			}
		}
		break;
	default:
		mxic_pr_err("Lock target(%d) is not supported\r\n", target);
		return MXST_ERR_NOT_SUP;
	}

	return ret;
}

static int spinor_asp_is_lock(mxmtd_t *mxmtd, uint32_t addr, uint32_t nbytes, int *lock_mode)
{
	int ret = MXST_SUCCESS;
	asp_range_t range = {.s = addr, .e = addr + nbytes - 1};
	uint32_t ofs;
	uint8_t spb = 0, dpb = 0;
	int status = ASP_UNLOCK, status_acc = ASP_UNLOCK;

	ret = spinor_asp_init(mxmtd, &range);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	for (ofs = range.s; ofs < range.e; ofs += spinor_lock_size(mxmtd, ofs), status = ASP_UNLOCK) {

		ret = spinor_rddpb(mxmtd, ofs, &dpb);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = spinor_rdspb(mxmtd, ofs, &spb);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		mxic_pr_dbg("addr%08X, DPB Locked: %02X\r\n", ofs, dpb);
		mxic_pr_dbg("addr%08X, SPB Locked: %02X\r\n", ofs, spb);

		if (0xff == dpb) {
			status |= ASP_DPB_LOCK;
		}

		if (0xff == spb) {
			status |= ASP_SPB_LOCK;
		}

		if ((ofs != range.s) && (!!status != !!status_acc)) {
				*lock_mode = ASP_PARTIAL_LOCK;
				return MXST_SUCCESS;
		}
		status_acc |= status;
	}
	*lock_mode = status_acc;

	return MXST_SUCCESS;
}

static int spinor_probe(mxmtd_t *mxmtd)
{
	int ret;
	spinor_t *snor = &mxmtd->spinor;

#ifdef CONF_PROBE_RESET
	ret = spinor_reset(mxmtd, SPINOR_MODE_SPI);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_reset(mxmtd, SPINOR_MODE_DOPI);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinor_reset(mxmtd, SPINOR_MODE_SOPI);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
#endif

	/* Scan which mode the SPI NOR is set as default */
	mxic_pr_dbg("scan_mode ...\r\n");
	ret = scan_mode(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (SPCL_SCUR_WPSEL & snor->id_info->flag_spcl) {
		uint8_t reg;

		ret = spinor_rdscur(mxmtd, &reg);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		mxmtd->spinor.wpsel = !!(SCUR_WPSEL & reg);

		if (SPCL_LR_PWDPMLD & snor->id_info->flag_spcl) {
			ret = spinor_rdlr(mxmtd, &reg);
			if (MXST_SUCCESS != ret) {
				return ret;
			}

			mxmtd->spinor.asp_pwd = !!(LR_PWDPMLD & reg);
		}

		if (mxmtd->spinor.asp_pwd) {
			mxic_pr_dbg("ASP with Password mode\r\n");
		} else if (mxmtd->spinor.wpsel) {
			mxic_pr_dbg("ASP mode\r\n");
		} else {
			mxic_pr_dbg("BP mode\r\n");
		}
	}

	ret = sfdp_parsing(mxmtd);
	if (MXST_ERR_NOT_SUP == ret) {
		mxic_pr_inf("SFDP is not supported\r\n");
	} else if (MXST_SUCCESS != ret) {
		return ret;
	}

	/*  Set up the cmd_info */
	mxic_pr_dbg("setup_cmd_info ...\r\n");
	ret = setup_cmd_info(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	/* Auto-select the best suited read/program/erase commands  */
	mxic_pr_dbg("sel_cmd_info_auto ...\r\n");
	ret = sel_cmd_info_auto(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	/* Check QE bit */
	if (SPINOR_MODE_SPI == snor->cmd_meta->spinor_mode) {
		int mode = snor->cmd_meta->spinor_mode;
		uint32_t prot = cmd_info_pgm[fmsb32(snor->id_info->cmd_list[mode].val[CMDS_LIST_PGM])].prot[mode] |
				cmd_info_rd[fmsb32(snor->id_info->cmd_list[mode].val[CMDS_LIST_RD])].prot[mode];

		ret = set_quad_enable(mxmtd, !!(PROT_IO(0, 4, 4) & prot));
		if (MXST_SUCCESS != ret && MXST_ERR_CONT != ret) {
			return ret;
		}
	}

	mxic_pr_dbg("RD CMD: %02X, PGM CMD: %02X, ERS CMD: %02X\r\n",
			cmd_info_rd[mxmtd->spinor.cmd_meta->rd.idx_cmd_num].cmd,
			cmd_info_pgm[mxmtd->spinor.cmd_meta->pgm.idx_cmd_num].cmd,
			cmd_info_ers[mxmtd->spinor.cmd_meta->ers.idx_cmd_num].cmd);

	ret = spinor_preamble_calibration(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return ret;
}

static void spinor_release(mxmtd_t *mxmtd)
{
	if (mxmtd->priv) {
		free(mxmtd->priv);
		mxmtd->priv = 0;
	}

	if (mxmtd->spinor.cmd_meta) {
		free(mxmtd->spinor.cmd_meta);
		mxmtd->spinor.cmd_meta = 0;
	}
}

static int spinor_init(mxmtd_t *mxmtd, uint32_t size_ers)
{
	int ret;
	xfer_info_t *xfer = 0;
	cmd_meta_t *cmd_meta = 0;

	cmd_meta = calloc(1, sizeof(cmd_meta_t));
	if (!cmd_meta) {
		mxic_pr_err("Allocate cmd_meta failed\r\n");
		return MXST_ERR_ALLOC;
	}
	mxmtd->spinor.cmd_meta = cmd_meta;

	xfer = calloc(1, sizeof(xfer_info_t));
	if (!xfer) {
		ret = MXST_ERR_ALLOC;
		mxic_pr_err("Allocate xfer failed\r\n");
		goto release_spinor_init;
	}
	mxmtd->priv = xfer;
    cmd_meta->priv = xfer;

	xfer->hc_prot_type = HC_PROT_xSPI;
	mxmtd->spinor.conf_ers.size = size_ers;

	switch (size_ers) {
	case SIZE_64KB:
		mxmtd->spinor.conf_ers.cmd_idx_3b = MXIC_BE;
		mxmtd->spinor.conf_ers.cmd_idx_4b = MXIC_BE4B;
		break;
	case SIZE_32KB:
		mxmtd->spinor.conf_ers.cmd_idx_3b = MXIC_BE32K;
		mxmtd->spinor.conf_ers.cmd_idx_4b = MXIC_BE32K4B;
		break;
	case SIZE_4KB:
		mxmtd->spinor.conf_ers.cmd_idx_3b = MXIC_SE;
		mxmtd->spinor.conf_ers.cmd_idx_4b = MXIC_SE4B;
		break;
	default:
		mxic_pr_err("The erase size is not defined in mxmtd_config.h\r\n");
		ret = MXST_ERR_CONFIG;
		goto release_spinor_init;
	}

	cmd_meta->mask_cmd = 0xff;
	return MXST_SUCCESS;

release_spinor_init:
    spinor_release(mxmtd);

	return ret;
}

int setup_spinor(mxmtd_t *mxmtd, uint32_t size_ers)
{
	int ret = spinor_init(mxmtd, size_ers);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

    mxmtd->ops.release = spinor_release;
    mxmtd->ops.probe = spinor_probe;
    mxmtd->ops.read = spinor_read;
    mxmtd->ops.program = spinor_program;
    mxmtd->ops.erase = spinor_erase;

    mxmtd->ops.get_chip_size = spinor_get_chip_size;
    mxmtd->ops.get_blk_size = spinor_get_blk_size;
    mxmtd->ops.get_page_size = spinor_get_page_size;
    mxmtd->ops.get_ers_size = spinor_get_ers_size;

    mxmtd->ops.get_ecc_state = spinor_get_ecc_state;

    mxmtd->ops.asp_set_wpsel = spinor_asp_set_wpsel;
    mxmtd->ops.asp_lock = spinor_asp_lock;
    mxmtd->ops.asp_unlock = spinor_asp_unlock;
    mxmtd->ops.asp_is_lock = spinor_asp_is_lock;
    mxmtd->ops.query_lock_mode = spinor_query_lock_mode;

	return ret;
}

#endif /* #ifdef CONF_SPINOR */
