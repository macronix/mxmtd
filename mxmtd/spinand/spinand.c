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

/**
 * TODO:
 * special read retry
 * randomizer
 * 4 io read/program
 */

#include "mxmtd_config.h"
#ifdef CONF_SPINAND

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include "platform_print.h"
#include "mxmtd.h"
#include "platform_itf.h"
#include "spinand_err.h"
#include "mxmtd_config.h"
#include "spinand_cmd.h"
#include "spinand_ids.h"
#include "lib.h"
#include "snand_para_page.h"

/* get/set features address definition */
#define  ADDR_CR_10						0x10
#define  ADDR_CR_60						0x60
#define  ADDR_CR_70						0x70
#define  ADDR_CR_A0						0xA0
#define  ADDR_CR_B0						0xB0
#define  ADDR_CR_C0						0xC0
#define  ADDR_CR_E0						0xE0

/* status register definition */
#define  SR_ERASE_FAIL					0x04
#define  SR_PROGRAM_FAIL				0x08
#define  SR_BBMT_FAIL					0x40
#define  SR_ECC_S_MASK					0x30
#define  SR_ECC_S_CORR					0x10
#define  SR_ECC_S_FAIL					0x20
#define  SR_ECC_S_THRE					0x30

#define DIR_RD	0
#define DIR_PGM	1

#define LEN_ROW		3
#define LEN_COLUMN	2

#define LEN_ONFI 256

#define NUM_PARAMTER_PAGE 3

static int spinand_get_id(mxmtd_t *mxmtd, uint8_t *id)
{
	int ret;

	ret = cmd_spinand_rdid(mxmtd->priv, id, 3);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	mxic_pr_dbg("SPINAND ID: %02X%02X%02X\r\n", id[0], id[1], id[2]);

	return ret;
}

static int match_id_with_ids(mxmtd_t *mxmtd)
{
	int ret, n = 0;
	spinand_t *snand = &mxmtd->spinand;
	uint8_t *id = snand->buf_shared;

	ret = spinand_get_id(mxmtd, id);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	while (ids[n].name) {
		if (memcmp(id, ids[n].id, ids[n].len_id)) {
			n++;
			continue;
		}

		snand->id_info = &ids[n];
		mxic_pr_inf("[SPINAND ID Table] %s is found, ID: %02X%02X%02X\r\n",
				snand->id_info->name, id[0], id[1], id[2]);
		return MXST_SUCCESS;
	}

	mxic_pr_inf("[SPINAND ID Table] is not matched, ID: %02X%02X%02X\r\n",
			id[0], id[1], id[2]);

	return MXST_SUCCESS;
}

static int spinand_setup_parameters(mxmtd_t *mxmtd)
{
	int ret, n = 0;
	spinand_t *snand = &mxmtd->spinand;
	uint8_t buf_pp[LEN_ONFI * NUM_PARAMTER_PAGE];

	/* update ecc step size from ids */
	if (snand->id_info) {
		snand->ecc.size_step = snand->id_info->ecc.size_step;
	}

	/* for parameter page */
	ret = cmd_spinand_param_page_read(mxmtd->priv, buf_pp, sizeof(buf_pp));
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	while (n < 3) {
		ret = snand_parse_param_page(mxmtd, &buf_pp[n * LEN_ONFI]);
		if (MXST_ERR_S_PP_SIGNATURE == ret || MXST_ERR_S_PP_CRC == ret) {
			mxic_pr_dbg("Parse parameter page %d failed\r\n", n);
			n++;
			continue;
		}
		break;
	}
	if (MXST_SUCCESS != ret) {
		return MXST_ERR_S_PP_PARSE;
	}

	/* for data & oob buffer */
	if (ECC_SW_MASK & snand->ecc.type) {
		snand->buf_data = malloc(snand->memorg.size_page + snand->memorg.size_oob);
		if (0 == snand->buf_data) {
			mxic_pr_err("Allocate for buf_data failed\r\n");
			return MXST_ERR_ALLOC;
		}
		snand->buf_oob = snand->buf_data + snand->memorg.size_page;

	} else {
		snand->buf_oob = malloc(snand->memorg.size_oob);
		if (0 == snand->buf_oob) {
			mxic_pr_err("Allocate for buf_oob failed\r\n");
			return MXST_ERR_ALLOC;
		}
	}

	/* for bad block management */
	snand->len_bbt = (snand->memorg.nblks_per_chip + 7) / 8;
	snand->bbt = calloc(1, snand->len_bbt);
	if (0 == snand->bbt) {
		snand->len_bbt = 0;
		mxic_pr_err("Allocate for bbt failed\r\n");
		return MXST_ERR_ALLOC;
	}

	/* for sub-page */
	snand->memorg.size_subpage = snand->memorg.size_page >> ((snand->ecc.nsteps <= 2) ? 1 : 2);

	return ret;
}

static int spinand_unlock_bp(mxmtd_t *mxmtd)
{
	int ret;
	uint8_t bp[2];

	ret = cmd_spinand_get_feature(mxmtd->priv, ADDR_CR_A0, &bp[0]);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	mxic_pr_dbg("Default bp: %02X\r\n", bp[0]);

	if (0x38 & bp[0]) {
		bp[0] &= ~0x38;
		ret = cmd_spinand_set_feature(mxmtd->priv, ADDR_CR_A0, &bp[0]);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		ret = cmd_spinand_get_feature(mxmtd->priv, ADDR_CR_A0, &bp[1]);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (bp[0] != bp[1]) {
			mxic_pr_err("unlocking block failed\r\n");
			return MXST_ERR_BP;
		}
		mxic_pr_dbg("Set feature to bp: %02X for unlocking all blocks\r\n", bp);
	}

	return ret;
}

static int spinand_internal_ecc_post_rd(mxmtd_t *mxmtd)
{
	int ret;
	uint8_t sr, mask_sr, eccsr;
	ecc_t *ecc = &mxmtd->spinand.ecc;

	ret = cmd_spinand_rd_sr(mxmtd->priv, &sr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	mask_sr =  SR_ECC_S_MASK & sr;
	switch (mask_sr) {
	case SR_ECC_S_CORR:
		mxic_pr_dbg("bit error detected, but be corrected, bit error count is less than bit flip "
				"threshold!\r\n");
		ecc->threthold = 0;
		break;
	case SR_ECC_S_THRE:
		mxic_pr_dbg("bit error detected, but be corrected, bit error count is equal or more than "
				"bit flip threshold!\r\n");
		ecc->threthold = 1;
		break;
	case SR_ECC_S_FAIL:
		mxic_pr_dbg("bit error detected and not be corrected!\r\n");
		ecc->uncorrectable = 1;
		break;
	default:
		return MXST_SUCCESS;
	}

	ret = cmd_spinand_rd_eccsr(mxmtd->priv, &eccsr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	mxic_pr_dbg("ecc sr: %02X\r\n", eccsr);

	eccsr = mxmtd->spinand.cache.cont_rd ? (eccsr >> 4) : (eccsr & 0xf);
	ecc->max_err_bits_per_step = ecc->max_err_bits_per_step > eccsr ? ecc->max_err_bits_per_step : eccsr;
	ecc->ecc_bits_corrected += eccsr;

	return MXST_SUCCESS;
}

static int spinand_internal_ecc_en(mxmtd_t *mxmtd, uint8_t enable)
{
	int ret;
	uint8_t cr, addr_cr = ADDR_CR_B0;

	if (mxmtd->spinand.cache.internal_ecc == enable) {
		return MXST_SUCCESS;
	}

	ret = cmd_spinand_get_feature(mxmtd->priv, addr_cr, &cr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if ((!!(0x10 & cr)) != enable) {
		uint8_t cr_ret;

		cr ^= 0x10;
		ret = cmd_spinand_set_feature(mxmtd->priv, addr_cr, &cr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = cmd_spinand_get_feature(mxmtd->priv, addr_cr, &cr_ret);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (cr_ret != cr) {
			mxmtd->spinand.cache.internal_ecc = !!(cr_ret & 0x10);
			mxic_pr_err("Write cr(%02X) failed, exp(%02X), ret(%02X)\r\n", addr_cr, cr, cr_ret);
			return MXST_ERR_WRCR;
		}
	}

	mxmtd->spinand.cache.internal_ecc = enable;

	mxic_pr_dbg("Set internal ECC %s\r\n",
			mxmtd->spinand.cache.internal_ecc ? "enable" : "disable");

	return MXST_SUCCESS;
}

static int spinand_cont_rd_en(mxmtd_t *mxmtd, uint8_t enable)
{
	int ret;
	uint8_t cr, addr_cr = ADDR_CR_B0;

	if (0 == mxmtd->spinand.cmds_supl.cont_rd) {
		mxic_pr_dbg("Continuous read mode is not supported\r\n");
		mxmtd->spinand.cache.cont_rd = 0;
		return MXST_ERR_NOT_SUP_CONT_RD;
	}

	if (mxmtd->spinand.cache.cont_rd == enable) {
		return MXST_SUCCESS;
	}

	ret = cmd_spinand_get_feature(mxmtd->priv, addr_cr, &cr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (!!(0x04 & cr) != enable) {
		uint8_t cr_ret;

		cr ^= 0x04;
		ret = cmd_spinand_set_feature(mxmtd->priv, addr_cr, &cr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = cmd_spinand_get_feature(mxmtd->priv, addr_cr, &cr_ret);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (cr_ret != cr) {
			mxmtd->spinand.cache.cont_rd = !!(cr & 0x04);
			mxic_pr_err("Write cr(%02X) failed, exp(%02X), ret(%02X)\r\n", addr_cr, cr, cr_ret);
			return MXST_ERR_WRCR;
		}
	}

	mxmtd->spinand.cache.cont_rd = enable;

	mxic_pr_dbg("%s continuous read\r\n",
			mxmtd->spinand.cache.cont_rd ? "Enable" : "Disable");

	return MXST_SUCCESS;
}

static int spinand_quad_en(mxmtd_t *mxmtd, uint8_t enable)
{
	int ret;
	uint8_t cr, addr_cr = ADDR_CR_B0;

	if (mxmtd->spinand.cache.quad == enable) {
		return MXST_SUCCESS;
	}

	ret = cmd_spinand_get_feature(mxmtd->priv, addr_cr, &cr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (!!(0x01 & cr) != enable) {
		uint8_t cr_ret;

		cr ^= 0x01;
		ret = cmd_spinand_set_feature(mxmtd->priv, addr_cr, &cr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = cmd_spinand_get_feature(mxmtd->priv, addr_cr, &cr_ret);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (cr_ret != cr) {
			mxmtd->spinand.cache.quad = cr & 0x01;
			mxic_pr_err("Write cr(%02X) failed, exp(%02X), ret(%02X)\r\n", addr_cr, cr, cr_ret);
			return MXST_ERR_WRCR;
		}
	}

	mxmtd->spinand.cache.quad = enable;

	return MXST_SUCCESS;
}

static int spinand_set_ecc_thre(mxmtd_t *mxmtd, uint8_t thre)
{
	int ret;
	uint8_t cr, cr_ret, addr_cr = ADDR_CR_10;

	if (mxmtd->spinand.cache.ecc_thre == thre) {
		return MXST_SUCCESS;
	}

	ret = cmd_spinand_get_feature(mxmtd->priv, addr_cr, &cr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	cr = (cr & 0x0f) | (thre << 4);
	ret = cmd_spinand_set_feature(mxmtd->priv, addr_cr, &cr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinand_get_feature(mxmtd->priv, addr_cr, &cr_ret);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (cr_ret != cr) {
		mxmtd->spinand.cache.ecc_thre = cr_ret >> 4;
		mxic_pr_err("Write cr(%02X) failed, exp(%02X), ret(%02X)\r\n", addr_cr, cr, cr_ret);
		return MXST_ERR_WRCR;
	}

	mxic_pr_dbg("Set ecc thrshold : %d\r\n", mxmtd->spinand.cache.ecc_thre);

	mxmtd->spinand.cache.ecc_thre = thre;

	return MXST_SUCCESS;
}

static int prev_ecc(mxmtd_t *mxmtd, uint8_t *buf_data, uint8_t *buf_oob, uint8_t dir)
{
	int ret = MXST_SUCCESS;
	spinand_t *snand = &mxmtd->spinand;

	if (DIR_RD == dir) {
		return MXST_SUCCESS;
	}

	switch (snand->ecc.type) {
	case ECC_SW_BCH:
		ret = nand_sw_ecc_prev_pgm(&snand->ecc, buf_data, buf_oob);
		break;
	default:
		break;
	}

	return ret;
}

static int post_ecc(mxmtd_t *mxmtd, uint8_t *buf_data, uint8_t *buf_ecc, uint8_t dir)
{
	int ret = MXST_SUCCESS;
	spinand_t *snand = &mxmtd->spinand;

	if (DIR_PGM == dir) {
		return MXST_SUCCESS;
	}

	switch (snand->ecc.type) {
	case ECC_SW_BCH:
		ret = nand_sw_ecc_post_rd(&snand->ecc, buf_data, buf_ecc);
		break;
	case ECC_INTERNAL_SPI:
		ret = spinand_internal_ecc_post_rd(mxmtd);
		break;
	default:
		break;
	}

	return ret;
}

static int spinand_internal_ecc_init(mxmtd_t *mxmtd)
{
	int ret;
	uint8_t cr;

	/* configure for internal ecc */
	ret = cmd_spinand_get_feature(mxmtd->priv, ADDR_CR_B0, &cr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	mxmtd->spinand.cache.internal_ecc = !!(0x10 & cr);

	mxic_pr_dbg("Default internal ECC %s\r\n",
			mxmtd->spinand.cache.internal_ecc ? "enable" : "disable");

	ret = spinand_internal_ecc_en(mxmtd, 1);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	/* configure for internal ecc threshold */
	ret = cmd_spinand_get_feature(mxmtd->priv, ADDR_CR_10, &cr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	mxmtd->spinand.cache.ecc_thre = cr >> 4;
	mxic_pr_dbg("Default ecc threshold : %d\r\n", mxmtd->spinand.cache.ecc_thre);

	return spinand_set_ecc_thre(mxmtd, CONF_SPINAND_ECC_THRE > mxmtd->spinand.ecc.strength ?
			mxmtd->spinand.ecc.strength : CONF_SPINAND_ECC_THRE);
}

static int spinand_cont_rd_init(mxmtd_t *mxmtd)
{
	int ret;

	if (mxmtd->spinand.cmds_supl.cont_rd) {
		uint8_t cr;
		ret = cmd_spinand_get_feature(mxmtd->priv, ADDR_CR_B0, &cr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		mxmtd->spinand.cache.cont_rd = !!(0x04 & cr);
	} else {
		return MXST_SUCCESS;
	}

	mxic_pr_dbg("Default continuous read is %s\r\n",
				mxmtd->spinand.cache.cont_rd ? "enabled" : "disabled");

#ifdef CONF_SPINAND_CONT_RD_DIS
	if (mxmtd->spinand.cmds_supl.cont_rd) {
		ret = spinand_cont_rd_en(mxmtd, 0);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		mxmtd->spinand.cmds_supl.cont_rd = 0;
	}

	return MXST_SUCCESS;
#else

	return spinand_cont_rd_en(mxmtd, 1);
#endif
}

static int spinand_ecc_init(mxmtd_t *mxmtd)
{
	spinand_t *snand = &mxmtd->spinand;

	mxic_pr_dbg("ECC type is %Xh\r\n", snand->ecc.type);

	snand->ecc.step_s = 0;
	snand->ecc.step_e = snand->ecc.nsteps - 1;

	switch (snand->ecc.type) {
#if CONF_SPINAND_ECC_TYPE == ECC_SW_BCH
	case ECC_SW_BCH:
		if (0 == snand->ecc.strength || 0 == snand->ecc.size_step) {
			mxic_pr_err("ECC strength or step size is 0, please add from id table or parameter page\r\n");
			return MXST_ERR_NOT_SUP;
		}
		return nand_sw_ecc_init(&snand->ecc);
#endif
	case ECC_INTERNAL_SPI:
		return spinand_internal_ecc_init(mxmtd);

	default:
		mxic_pr_war("Force ECC Off\r\n");
		snand->ecc.type = ECC_NONE;
		return MXST_SUCCESS;
	}

	return MXST_SUCCESS;
}

static inline int spinand_oob_layout_init(mxmtd_t *mxmtd)
{
	uint32_t nbytes_ecc_per_oob;

	switch(mxmtd->spinand.ecc.type) {
	case ECC_SW_BCH:
		nbytes_ecc_per_oob = mxmtd->spinand.ecc.nbytes * mxmtd->spinand.ecc.nsteps;
		break;
	case ECC_INTERNAL_SPI:
		nbytes_ecc_per_oob = mxmtd->spinand.memorg.size_oob / 2;
		break;
	default:
		nbytes_ecc_per_oob = 0;
		break;
	}

	return nand_oob_layout_init(&mxmtd->spinand.layout, nbytes_ecc_per_oob, mxmtd->spinand.memorg.size_oob);
}

static int spinand_read_oob(mxmtd_t *mxmtd, uint64_t addr, uint32_t ofs_oob, uint32_t nbytes)
{
	int ret;
	spinand_t *snand = &mxmtd->spinand;
	uint32_t col = snand->memorg.size_page + ofs_oob;
	uint32_t row = (uint32_t)addr >> fmsb32(snand->memorg.size_page);

	if ((ofs_oob + nbytes) > snand->memorg.size_oob) {
		mxic_pr_err("ofs_oob(%Xh) and len_oob(%Xh) exceed oob size(%Xh)\r\n",
				ofs_oob, nbytes, snand->memorg.size_oob);
	}

	ret = spinand_cont_rd_en(mxmtd, 0);
	if (MXST_ERR_NOT_SUP_CONT_RD != ret && MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinand_page_read(mxmtd->priv, row, LEN_ROW, snand->timing.t_rd_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (snand->id_info && (SPCL_PLANE_SEL_READ & snand->id_info->flag_spcl)) {
		col |= ((row >> fmsb32(snand->memorg.npages_per_blk)) & 1)
				<< (fmsb32(snand->memorg.size_page) + 1);
	}

	return cmd_spinand_read_cache(mxmtd->priv, col, LEN_COLUMN, &snand->buf_oob[ofs_oob], nbytes);
}

static int spinand_unalign_read(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	spinand_t *snand = &mxmtd->spinand;
	uint32_t req_oob = !!(ECC_SW_MASK & snand->ecc.type);
	uint8_t *buf_data = req_oob ? snand->buf_data : buf;
	uint8_t *buf_oob = 0;
	uint32_t col  = (uint32_t)(addr & snand->memorg.mask_page);
	uint32_t row = (uint32_t)(addr >> fmsb32(snand->memorg.size_page));
	uint32_t nsteps_rd;
	uint64_t remain = nbytes;

	ret = spinand_cont_rd_en(mxmtd, 0);
	if (MXST_ERR_NOT_SUP_CONT_RD != ret && MXST_SUCCESS != ret) {
		return ret;
	}

	if (req_oob) {
		snand->ecc.step_s = col / snand->ecc.size_step;
		snand->ecc.step_e = (col + remain - 1) / snand->ecc.size_step;
		nsteps_rd = snand->ecc.step_e - snand->ecc.step_s + 1;

		col = snand->ecc.step_s * snand->ecc.size_step;
		remain = nsteps_rd * snand->ecc.size_step;
	}

	ret = prev_ecc(mxmtd, 0, 0, DIR_RD);
	if (MXST_SUCCESS != ret) {
		goto release_spinand_unalign_read;
	}

	ret = cmd_spinand_page_read(mxmtd->priv, row, LEN_ROW, snand->timing.t_rd_us);
	if (MXST_SUCCESS != ret) {
		goto release_spinand_unalign_read;
	}

	if (snand->id_info && (SPCL_PLANE_SEL_READ & snand->id_info->flag_spcl)) {
		col |= ((row >> fmsb32(snand->memorg.npages_per_blk)) & 1)
				<< (fmsb32(snand->memorg.size_page) + 1);
	}

	ret = cmd_spinand_read_cache(mxmtd->priv, col, LEN_COLUMN, buf_data, remain);
	if (MXST_SUCCESS != ret) {
		goto release_spinand_unalign_read;
	}

	if (req_oob) {
		uint32_t col_oob;
		uint32_t ofs_oob = snand->layout.ecc.ofs + snand->ecc.step_s * snand->ecc.nbytes;

		buf_oob = ofs_oob + snand->buf_oob;
		col_oob = ofs_oob + snand->memorg.size_page;
		if (snand->id_info && (SPCL_PLANE_SEL_READ & snand->id_info->flag_spcl)) {
			col_oob |= ((row >> fmsb32(snand->memorg.npages_per_blk)) & 1)
					<< (fmsb32(snand->memorg.size_page) + 1);
		}
		remain = nsteps_rd * snand->ecc.nbytes;

		ret = cmd_spinand_read_cache(mxmtd->priv, col_oob, LEN_COLUMN, buf_oob, remain);
		if (MXST_SUCCESS != ret) {
			goto release_spinand_unalign_read;
		}
	}

	ret = post_ecc(mxmtd, buf_data, buf_oob, DIR_RD);
	if (MXST_SUCCESS != ret) {
		goto release_spinand_unalign_read;
	}

	if (req_oob) {
		memcpy(buf, &buf_data[addr % snand->ecc.size_step], nbytes);
	}

release_spinand_unalign_read:
	if (req_oob) {
		snand->ecc.step_s = 0;
		snand->ecc.step_e = snand->ecc.nsteps - 1;
	}

	return ret;
}

static int spinand_page_read(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	spinand_t *snand = &mxmtd->spinand;
	uint32_t nbytes_oob = (ECC_SW_MASK & snand->ecc.type) ? snand->memorg.size_oob : 0;
	uint8_t *buf_data = nbytes_oob ? snand->buf_data : buf;
	uint8_t *buf_oob = 0;
	uint32_t col  = (uint32_t)(addr & snand->memorg.mask_page);
	uint32_t row = (uint32_t)(addr >> fmsb32(snand->memorg.size_page));

	ret = spinand_cont_rd_en(mxmtd, 0);
	if (MXST_ERR_NOT_SUP_CONT_RD != ret && MXST_SUCCESS != ret) {
		return ret;
	}

	ret = prev_ecc(mxmtd, 0, 0, DIR_RD);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinand_page_read(mxmtd->priv, row, LEN_ROW, snand->timing.t_rd_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (snand->id_info && (SPCL_PLANE_SEL_READ & snand->id_info->flag_spcl)) {
		col |= ((row >> fmsb32(snand->memorg.npages_per_blk)) & 1)
				<< (fmsb32(snand->memorg.size_page) + 1);
	}

	ret = cmd_spinand_read_cache(mxmtd->priv, col, LEN_COLUMN, buf_data, nbytes + nbytes_oob);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (nbytes_oob) {
		memcpy(buf, buf_data, nbytes);
		buf_oob = snand->buf_oob + snand->layout.ecc.ofs;
	}

	ret = post_ecc(mxmtd, buf, buf_oob, DIR_RD);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return MXST_SUCCESS;
}

static int spinand_cache_read_seq(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	spinand_t *snand = &mxmtd->spinand;
	uint32_t nbytes_oob = (ECC_SW_MASK & snand->ecc.type) ? snand->memorg.size_oob : 0;
	uint8_t *buf_data = nbytes_oob ? snand->buf_data : buf;
	uint8_t *buf_oob = 0;
	uint32_t size_page = snand->memorg.size_page;
	uint32_t row = (uint32_t)(addr >> fmsb32(size_page));

	ret = spinand_cont_rd_en(mxmtd, 0);
	if (MXST_ERR_NOT_SUP_CONT_RD != ret && MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinand_page_read(mxmtd->priv, row, LEN_ROW, snand->timing.t_rd_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	while(nbytes) {
		uint32_t nbytes_rd = (nbytes > size_page) ? size_page : (uint32_t)nbytes;

		ret = prev_ecc(mxmtd, 0, 0, DIR_RD);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		/* the page read out is at the current addr, its plane may differ per iteration */
		uint32_t col = 0;

		row = (uint32_t)(addr >> fmsb32(size_page));
		if (snand->id_info && (SPCL_PLANE_SEL_READ & snand->id_info->flag_spcl)) {
			col |= ((row >> fmsb32(snand->memorg.npages_per_blk)) & 1)
					<< (fmsb32(snand->memorg.size_page) + 1);
		}
		ret = cmd_spinand_read_cache_seq(mxmtd->priv, col, LEN_COLUMN, buf_data,
				nbytes_rd + nbytes_oob, nbytes == nbytes_rd, snand->timing.t_rd_us);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (nbytes_oob) {
			memcpy(buf, buf_data, nbytes_rd);
			buf_oob = snand->buf_oob + snand->layout.ecc.ofs;
		}
		ret = post_ecc(mxmtd, buf, buf_oob, DIR_RD);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		addr += nbytes_rd;
		if (nbytes_oob) {
			buf  += nbytes_rd;
		} else {
			buf_data  += nbytes_rd;
		}
		nbytes  -= nbytes_rd;
	}

	return MXST_SUCCESS;
}

static int spinand_cont_rd(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	spinand_t *snand = &mxmtd->spinand;
	uint32_t row = (uint32_t)(addr >> fmsb32(snand->memorg.size_page));

	if (ECC_NONE != snand->ecc.type && ECC_INTERNAL_SPI != snand->ecc.type) {
		mxic_pr_dbg("Continuous read only supported for ECC off and internal ECC\r\n");
		return MXST_ERR_NOT_SUP_CONT_RD;
	}

	ret = spinand_cont_rd_en(mxmtd, 1);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = prev_ecc(mxmtd, 0, 0, DIR_RD);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinand_page_read(mxmtd->priv, row, LEN_ROW, snand->timing.t_rd_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinand_read_cache(mxmtd->priv, 0, LEN_COLUMN, buf, nbytes);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = post_ecc(mxmtd, 0, 0, DIR_RD);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return MXST_SUCCESS;
}

static int spinand_read_entry(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret = MXST_SUCCESS;
	uint32_t size_page = mxmtd->spinand.memorg.size_page;
	uint32_t nbytes_rd = size_page - (addr % size_page);
	uint64_t remain;

	/* for small data */
	if (size_page != nbytes_rd) {
		nbytes_rd = nbytes > nbytes_rd ? nbytes_rd : (uint32_t)nbytes;

		ret = spinand_unalign_read(mxmtd, addr, buf, nbytes_rd);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		addr += nbytes_rd;
		buf += nbytes_rd;
		nbytes -= nbytes_rd;
	}

	remain = nbytes % size_page;
	nbytes -= remain;

	if (nbytes) {
		/* for one page */
		if (size_page == nbytes) {
			ret = spinand_page_read(mxmtd, addr, buf, nbytes);
		/*  for two or more pages */
		} else {
			ret = spinand_cont_rd(mxmtd, addr, buf, nbytes);
			if (MXST_ERR_NOT_SUP_CONT_RD == ret) {
				ret = spinand_cache_read_seq(mxmtd, addr, buf, nbytes);
			}
		}
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		addr += nbytes;
		buf += nbytes;
	}

	/* for small data */
	if (remain) {
		ret = spinand_unalign_read(mxmtd, addr, buf, remain);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
	}

	return ret;
}

int spinand_read(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	uint64_t size_chip;

	if (!nbytes) {
		mxic_pr_war("Read bytes = 0, do nothing!\r\n");
		return MXST_SUCCESS;
	}
	if (!buf) {
		mxic_pr_err("Read buffer is NULL\r\n");
		return MXST_ERR_PTR_NULL;
	}

	size_chip = mxmtd->spinand.memorg.size_chip;
	if (addr >= size_chip || nbytes > (size_chip - addr)) {
		mxic_pr_err("Read range exceeds chip size: addr=%08X%08Xh, nbytes=%08X%08Xh, chip=%08X%08Xh\r\n",
				(uint32_t)(addr >> 32), (uint32_t)addr,
				(uint32_t)(nbytes >> 32), (uint32_t)nbytes,
				(uint32_t)(size_chip >> 32), (uint32_t)size_chip);
		return MXST_ERR_PARAM;
	}

	mxmtd->spinand.ecc.ecc_bits_corrected = 0;
	mxmtd->spinand.ecc.uncorrectable = 0;
	mxmtd->spinand.ecc.max_err_bits_per_step = 0;
	mxmtd->spinand.ecc.threthold = 0;

	switch (mxmtd->spinand.ecc.type) {
	case ECC_INTERNAL_SPI:
		mxic_pr_dbg("Internal ECC\r\n");
		ret = spinand_internal_ecc_en(mxmtd, 1);
		break;
	case ECC_SW_BCH:
		mxic_pr_dbg("S/W BCH ECC\r\n");
		ret = spinand_internal_ecc_en(mxmtd, 0);
		break;
	case ECC_NONE:
		mxic_pr_dbg("ECC Off\r\n");
		ret = spinand_internal_ecc_en(mxmtd, 0);
		break;
	default:
		mxic_pr_dbg("Read ECC type(%d) is illegal\r\n", mxmtd->spinand.ecc.type);
		ret = MXST_ERR_NOT_SUP;
		break;
	}
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	mxic_pr_dbg("Read    : "
				"%08X-%08Xh ~ "
				"%08X: %08Xh, "
				"Block: %d, "
				"nbytes: %08X-%08Xh\r\n",
				(uint32_t )(addr >> 32), (uint32_t)(addr),
				(uint32_t )((addr + nbytes - 1) >> 32), (uint32_t)(addr + nbytes - 1),
				(uint32_t)(addr >> fmsb32(mxmtd->spinand.memorg.size_blk)),
				(uint32_t )(nbytes >> 32), (uint32_t)(nbytes));

	return spinand_read_entry(mxmtd, addr, buf, nbytes);
}

static int spinand_pgm_oob(mxmtd_t *mxmtd, uint64_t addr, uint32_t ofs_oob, uint32_t nbytes)
{
	int ret;
	spinand_t *snand = &mxmtd->spinand;
	uint8_t sr;
	uint32_t col = snand->memorg.size_page + ofs_oob;
	uint32_t row = (uint32_t)addr >> fmsb32(snand->memorg.size_page);

	if ((ofs_oob + nbytes) > snand->memorg.size_oob) {
		mxic_pr_err("ofs_oob(%Xh) and len_oob(%Xh) exceed oob size(%Xh)\r\n",
				ofs_oob, nbytes, snand->memorg.size_oob);
	}

	if (snand->id_info && (SPCL_PLANE_SEL_PROG & snand->id_info->flag_spcl)) {
		col |= ((row >> fmsb32(snand->memorg.npages_per_blk)) & 1)
				<< (fmsb32(snand->memorg.size_page) + 1);
	}

	ret = cmd_spinand_program_load(mxmtd->priv, col, LEN_COLUMN, &snand->buf_oob[ofs_oob], nbytes);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinand_program_execute(mxmtd->priv, row, LEN_ROW, &sr, snand->timing.t_prog_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (SR_PROGRAM_FAIL & sr) {
		mxic_pr_err("Page program failed! SR=0x%02X\r\n", sr);
		return MXST_ERR_PROGRAM;
	}

	return MXST_SUCCESS;
}

static int spinand_unalign_pgm(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint32_t nbytes)
{
	int ret;
	spinand_t *snand = &mxmtd->spinand;
	uint8_t sr;
	uint8_t req_oob = !!(ECC_SW_MASK & snand->ecc.type);
	uint8_t *buf_data = req_oob ? snand->buf_data : buf;
	uint8_t *buf_oob = 0;
	uint32_t col = (uint32_t)(addr & snand->memorg.mask_page);
	uint32_t row = (uint32_t)(addr >> fmsb32(snand->memorg.size_page));
	uint32_t nsteps_pgm;

	if (req_oob) {
		memset(buf_data, 0xff, snand->memorg.size_page + snand->memorg.size_oob);

		snand->ecc.step_s = col / snand->ecc.size_step;
		snand->ecc.step_e = (col + nbytes - 1) / snand->ecc.size_step;
		nsteps_pgm = snand->ecc.step_e - snand->ecc.step_s + 1;

		buf_data += snand->ecc.step_s * snand->ecc.size_step;
		buf_oob = snand->buf_oob + snand->layout.ecc.ofs + snand->ecc.step_s * snand->ecc.nbytes;

		memcpy(&buf_data[col % snand->ecc.size_step], buf, nbytes);
	}

	ret = prev_ecc(mxmtd, buf_data, buf_oob, DIR_PGM);
	if (MXST_SUCCESS != ret) {
		goto release_spinand_unalign_pgm;
	}

	if (snand->id_info && (SPCL_PLANE_SEL_PROG & snand->id_info->flag_spcl)) {
		col |= ((row >> fmsb32(snand->memorg.npages_per_blk)) & 1)
				<< (fmsb32(snand->memorg.size_page) + 1);
	}

	ret = cmd_spinand_program_load(mxmtd->priv, col, LEN_COLUMN, buf, nbytes);
	if (MXST_SUCCESS != ret) {
		goto release_spinand_unalign_pgm;
	}

	if (req_oob) {
		col = snand->memorg.size_page + snand->layout.ecc.ofs + snand->ecc.step_s * snand->ecc.nbytes;
		if (snand->id_info && (SPCL_PLANE_SEL_PROG & snand->id_info->flag_spcl)) {
			col |= ((row >> fmsb32(snand->memorg.npages_per_blk)) & 1)
					<< (fmsb32(snand->memorg.size_page) + 1);
		}
		nbytes = nsteps_pgm * snand->ecc.nbytes;

		ret = cmd_spinand_program_load_rnd(mxmtd->priv, col, LEN_COLUMN, buf_oob, nbytes);
		if (MXST_SUCCESS != ret) {
			goto release_spinand_unalign_pgm;
		}
	}

	ret = cmd_spinand_program_execute(mxmtd->priv, row, LEN_ROW, &sr, snand->timing.t_prog_us);
	if (MXST_SUCCESS != ret) {
		goto release_spinand_unalign_pgm;
	}

	if (SR_PROGRAM_FAIL & sr) {
		mxic_pr_err("Page program failed! SR=0x%02X\r\n", sr);
		ret = MXST_ERR_PROGRAM;
		goto release_spinand_unalign_pgm;
	}

	ret = post_ecc(mxmtd, 0, 0, DIR_PGM);

release_spinand_unalign_pgm:
	if (req_oob) {
		snand->ecc.step_s = 0;
		snand->ecc.step_e = snand->ecc.nsteps - 1;
	}

	return ret;
}

static int spinand_page_pgm(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint32_t nbytes)
{
	int ret;
	spinand_t *snand = &mxmtd->spinand;
	uint8_t sr;
	uint32_t nbytes_oob = (ECC_SW_MASK & snand->ecc.type) ? snand->memorg.size_oob : 0;
	uint8_t *buf_data = (ECC_SW_MASK & snand->ecc.type) ? snand->buf_data : buf;
	uint8_t *buf_oob = 0;
	uint32_t col = (uint32_t)(addr & snand->memorg.mask_page);
	uint32_t row = (uint32_t)(addr >> fmsb32(snand->memorg.size_page));

	if (nbytes_oob) {
		memcpy(buf_data, buf, nbytes);
		buf_oob = snand->buf_oob + snand->layout.ecc.ofs;
	}
	ret = prev_ecc(mxmtd, buf_data, buf_oob, DIR_PGM);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (snand->id_info && (SPCL_PLANE_SEL_PROG & snand->id_info->flag_spcl)) {
		col |= ((row >> fmsb32(snand->memorg.npages_per_blk)) & 1)
				<< (fmsb32(snand->memorg.size_page) + 1);
	}

	ret = cmd_spinand_program_load(mxmtd->priv, col, LEN_COLUMN, buf_data, nbytes + nbytes_oob);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_spinand_program_execute(mxmtd->priv, row, LEN_ROW, &sr, snand->timing.t_prog_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (SR_PROGRAM_FAIL & sr) {
		mxic_pr_err("Page program failed! SR=0x%02X\r\n", sr);
		return MXST_ERR_PROGRAM;
	}

	return post_ecc(mxmtd, 0, 0, DIR_PGM);
}

static int spinand_program_entry(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	uint32_t size_page = mxmtd->spinand.memorg.size_page;
	uint32_t nbytes_pgm = size_page - (addr % size_page);
	uint64_t remain;

	/* for small data */
	if (size_page != nbytes_pgm) {
		nbytes_pgm = nbytes > nbytes_pgm ? nbytes_pgm : (uint32_t)nbytes;

		ret = spinand_unalign_pgm(mxmtd, addr, buf, nbytes_pgm);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		addr += nbytes_pgm;
		buf += nbytes_pgm;
		nbytes -= nbytes_pgm;
	}

	remain = nbytes % size_page;
	nbytes -= remain;

	/* for one or more pages */
	while (nbytes) {
		ret = spinand_page_pgm(mxmtd, addr, buf, size_page);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		addr += size_page;
		buf += size_page;
		nbytes -= size_page;
	}

	/* for small data */
	if (remain) {
		ret = spinand_unalign_pgm(mxmtd, addr, buf, remain);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
	}

	return MXST_SUCCESS;
}

static int spinand_program(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	uint64_t size_chip;

	if (!nbytes) {
		mxic_pr_war("Program bytes = 0, do nothing!\r\n");
		return MXST_SUCCESS;
	}
	if (!buf) {
		mxic_pr_err("Program buffer is NULL\r\n");
		return MXST_ERR_PTR_NULL;
	}

	size_chip = mxmtd->spinand.memorg.size_chip;
	if (addr >= size_chip || nbytes > (size_chip - addr)) {
		mxic_pr_err("Program range exceeds chip size: addr=%08X%08Xh, nbytes=%08X%08Xh, chip=%08X%08Xh\r\n",
				(uint32_t)(addr >> 32), (uint32_t)addr,
				(uint32_t)(nbytes >> 32), (uint32_t)nbytes,
				(uint32_t)(size_chip >> 32), (uint32_t)size_chip);
		return MXST_ERR_PARAM;
	}

	switch (mxmtd->spinand.ecc.type) {
	case ECC_INTERNAL_SPI:
		mxic_pr_dbg("Internal ECC\r\n");
		ret = spinand_internal_ecc_en(mxmtd, 1);
		break;
	case ECC_SW_BCH:
		mxic_pr_dbg("S/W BCH ECC\r\n");
		ret = spinand_internal_ecc_en(mxmtd, 0);
		break;
	case ECC_NONE:
		mxic_pr_dbg("ECC Off\r\n");
		ret = spinand_internal_ecc_en(mxmtd, 0);
		break;
	default:
		mxic_pr_dbg("Program ECC type(%d) is illegal\r\n", mxmtd->spinand.ecc.type);
		ret = MXST_ERR_NOT_SUP;
		break;
	}
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	mxic_pr_dbg("Program : "
				"%08X-%08Xh ~ "
				"%08X: %08Xh, "
				"Block: %d, "
				"nbytes: %08X-%08Xh\r\n",
				(uint32_t )(addr >> 32), (uint32_t)(addr),
				(uint32_t )((addr + nbytes - 1) >> 32), (uint32_t)(addr + nbytes - 1),
				(uint32_t)(addr >> fmsb32(mxmtd->spinand.memorg.size_blk)),
				(uint32_t )(nbytes >> 32), (uint32_t)(nbytes));

	return spinand_program_entry(mxmtd, addr, buf, nbytes);
}

int spinand_erase(mxmtd_t *mxmtd, uint64_t addr, uint64_t nbytes)
{
	int ret = MXST_SUCCESS;
	uint64_t size_chip;
	uint32_t size_blk = mxmtd->spinand.memorg.size_blk;
	uint32_t size_page = mxmtd->spinand.memorg.size_page;
	uint32_t row = addr >> fmsb32(size_page);

	if (!nbytes) {
		mxic_pr_war("Erase bytes = 0, do nothing!\r\n");
		return MXST_SUCCESS;
	}

	size_chip = mxmtd->spinand.memorg.size_chip;
	if (addr >= size_chip || nbytes > (size_chip - addr)) {
		mxic_pr_err("Erase range exceeds chip size: addr=%08X%08Xh, nbytes=%08X%08Xh, chip=%08X%08Xh\r\n",
				(uint32_t)(addr >> 32), (uint32_t)addr,
				(uint32_t)(nbytes >> 32), (uint32_t)nbytes,
				(uint32_t)(size_chip >> 32), (uint32_t)size_chip);
		return MXST_ERR_PARAM;
	}

	mxic_pr_dbg("Erase, Block: %d\r\n", (uint32_t)addr >> fmsb32(size_blk));

	if (addr % size_blk) {
		mxic_pr_err("Address is not allowed with erase size(%08Xh)\r\n", size_blk);
		return MXST_ERR_NOT_ALIGN;
	}

	if (nbytes % size_blk) {
		mxic_pr_err("End address is not allowed with erase size(%08X)\r\n", size_blk);
		return MXST_ERR_NOT_ALIGN;
	}

	while(nbytes) {
		uint8_t sr;

		ret = cmd_spinand_block_erase(mxmtd->priv, row, LEN_ROW, &sr, mxmtd->spinand.timing.t_ers_us);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (SR_ERASE_FAIL & sr) {
			mxic_pr_err("Erase failed!\r\n");
			return MXST_ERR_ERASE;
		}

		/* Clear BB if erase successfully */
		CLR_BB(mxmtd->spinand.bbt, row >> fmsb32(mxmtd->spinand.memorg.npages_per_blk));

		row++;
		nbytes -= size_blk;
	}

	return ret;
}

static uint64_t spinand_get_chip_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinand.memorg.size_chip;
}

static uint32_t spinand_get_blk_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinand.memorg.size_blk;
}

static uint32_t spinand_get_page_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinand.memorg.size_page;
}

static uint32_t spinand_get_ers_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinand.memorg.size_blk;
}

static uint32_t spinand_get_subpage_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinand.memorg.size_subpage;
}

static int spinand_get_ecc_state(mxmtd_t *mxmtd)
{
	if (mxmtd->spinand.ecc.uncorrectable) {
		return -1;
	}

	return mxmtd->spinand.ecc.max_err_bits_per_step;
}

static void spinand_set_ecc_en(mxmtd_t *mxmtd, uint8_t en)
{
	if (en) {
		if (ECC_NONE != mxmtd->spinand.ecc.type) {
			return;
		}
		mxmtd->spinand.ecc.type = mxmtd->spinand.ecc.type_old;
		return;
	}

	mxmtd->spinand.ecc.type_old = mxmtd->spinand.ecc.type;
	mxmtd->spinand.ecc.type = ECC_NONE;
}

static uint8_t spinand_get_ecc_strength(mxmtd_t *mxmtd)
{
	return mxmtd->spinand.ecc.strength;
}

static uint8_t spinand_get_ecc_nsteps(mxmtd_t *mxmtd)
{
	return mxmtd->spinand.ecc.nsteps;
}

static uint16_t spinand_get_ecc_step_size(mxmtd_t *mxmtd)
{
	return mxmtd->spinand.ecc.size_step;
}

static int spinand_set_bb(mxmtd_t *mxmtd, uint64_t addr, uint8_t pre_erase)
{
	int ret, n;
	spinand_t *snand = &mxmtd->spinand;
	uint8_t cont_rd = snand->cache.cont_rd;

	ret = spinand_cont_rd_en(mxmtd, 0);
	if (MXST_ERR_NOT_SUP_CONT_RD != ret && MXST_SUCCESS != ret) {
		return ret;
	}

	memset(&snand->buf_oob[snand->layout.bb.ofs], 0x00, snand->layout.bb.len);

	addr &= ~snand->memorg.mask_blk;

	if (pre_erase) {
		ret = spinand_erase(mxmtd, addr, snand->memorg.size_blk);
		if (MXST_SUCCESS != ret) {
			spinand_cont_rd_en(mxmtd, cont_rd);
			return ret;
		}
	}

	for (n = 0; n < 2; n++) {
		ret = spinand_pgm_oob(mxmtd, addr, snand->layout.bb.ofs, snand->layout.bb.len);
		if (MXST_SUCCESS != ret) {
			spinand_cont_rd_en(mxmtd, cont_rd);
			return ret;
		}
		addr += snand->memorg.size_page;
	}

	SET_BB(snand->bbt, addr >> fmsb32(snand->memorg.size_blk));

	ret = spinand_cont_rd_en(mxmtd, cont_rd);
	if (MXST_ERR_NOT_SUP_CONT_RD != ret && MXST_SUCCESS != ret) {
		return ret;
	}

	return MXST_SUCCESS;
}

static uint8_t spinand_chk_bb(mxmtd_t *mxmtd, uint64_t addr)
{
	return CHK_BB(mxmtd->spinand.bbt, addr >> fmsb32(mxmtd->spinand.memorg.size_blk));
}

static int spinand_scan_bb(mxmtd_t *mxmtd)
{
	int ret;
	spinand_t *snand = &mxmtd->spinand;
	uint8_t cont_rd = snand->cache.cont_rd;
	uint32_t n, m, blk;
	uint32_t nblks = snand->memorg.nblks_per_chip;
	uint64_t addr = 0;

	ret = spinand_cont_rd_en(mxmtd, 0);
	if (MXST_ERR_NOT_SUP_CONT_RD != ret && MXST_SUCCESS != ret) {
		goto release_spinand_scan_bb;
	}

	for (blk = 0; blk < nblks; blk++) {
		addr = blk * snand->memorg.size_blk;
		for (n = 0; n < 2; n++) {
			addr += n * snand->memorg.size_page;
			ret = spinand_read_oob(mxmtd, addr, snand->layout.bb.ofs, snand->layout.bb.len);
			if (MXST_SUCCESS != ret) {
				goto release_spinand_scan_bb;
			}
			for (m = 0; m < snand->layout.bb.len; m++) {
				if (0xff != snand->buf_oob[snand->layout.bb.ofs + m]) {
					SET_BB(snand->bbt, blk);
					mxic_pr_war("Page: %d, Bytes: %02X-%02X, block: %d is bad\r\n",
							n,
							snand->buf_oob[snand->layout.bb.ofs],
							snand->buf_oob[snand->layout.bb.ofs + 1],
							blk);
					n = 2;
					break;
				}
			}
		}
	}

release_spinand_scan_bb:
	spinand_cont_rd_en(mxmtd, cont_rd);
	return ret;
}

static int spinand_get_all_cfg(mxmtd_t *mxmtd)
{
	int ret, n;
	uint8_t addr[] = {ADDR_CR_10, ADDR_CR_60, ADDR_CR_70, ADDR_CR_A0, ADDR_CR_B0,
			ADDR_CR_C0, ADDR_CR_E0};
	uint8_t cr;


	for (n = 0; n < sizeof(addr); n++) {
		ret = cmd_spinand_get_feature(mxmtd->priv, addr[n], &cr);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		mxic_pr_dbg("%02X: %02X\r\n", addr[n], cr);
	}
	return MXST_SUCCESS;
}

int spinand_probe(mxmtd_t *mxmtd)
{
	int ret;

#ifdef CONF_PROBE_RESET
	uint8_t sr;
	ret = cmd_spinand_reset(mxmtd->priv, &sr);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
#endif

	ret = match_id_with_ids(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinand_setup_parameters(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinand_ecc_init(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinand_oob_layout_init(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinand_cont_rd_init(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = spinand_unlock_bp(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return spinand_scan_bb(mxmtd);
}

static void spinand_release(mxmtd_t *mxmtd)
{
	if (mxmtd->priv) {
		free(mxmtd->priv);
		mxmtd->priv = 0;
	}

	if (mxmtd->spinand.buf_data) {
		free(mxmtd->spinand.buf_data);
		mxmtd->spinand.buf_data = 0;
		mxmtd->spinand.buf_oob = 0;

	} else if(mxmtd->spinand.buf_oob) {
		free(mxmtd->spinand.buf_oob);
		mxmtd->spinand.buf_oob = 0;
	}

	if (mxmtd->spinand.bbt) {
		free(mxmtd->spinand.bbt);
		mxmtd->spinand.bbt = 0;
		mxmtd->spinand.len_bbt = 0;
	}

	if (mxmtd->spinand.ecc.release) {
		mxmtd->spinand.ecc.release(&mxmtd->spinand.ecc);
	}
}

int spinand_init(mxmtd_t *mxmtd, uint8_t ecc_type, uint32_t size_step)
{
	xfer_info_t *xfer = calloc(1, sizeof(xfer_info_t));
	if (!xfer) {
		mxic_pr_err("Allocate for xfer failed\r\n");
		return MXST_ERR_ALLOC;
	}

	xfer->hc_prot_type = HC_PROT_xSPI;
	mxmtd->priv = xfer;
	mxmtd->spinand.ecc.type = ecc_type;
	mxmtd->spinand.ecc.size_step = size_step;
	mxic_pr_dbg("Default ECC type is %Xh\r\n", mxmtd->spinand.ecc.type);

	return MXST_SUCCESS;
}

int setup_spinand(mxmtd_t *mxmtd, uint8_t ecc_type, uint32_t size_step)
{
	int ret;

	ret = spinand_init(mxmtd, ecc_type, size_step);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	mxmtd->ops.release = spinand_release;
    mxmtd->ops.probe = spinand_probe;
    mxmtd->ops.read = spinand_read;
    mxmtd->ops.program = spinand_program;
    mxmtd->ops.erase = spinand_erase;

    mxmtd->ops.get_chip_size = spinand_get_chip_size;
	mxmtd->ops.get_blk_size = spinand_get_blk_size;
	mxmtd->ops.get_page_size = spinand_get_page_size;
	mxmtd->ops.get_ers_size = spinand_get_ers_size;
	mxmtd->ops.get_subpage_size = spinand_get_subpage_size;

	mxmtd->ops.get_ecc_state = spinand_get_ecc_state;

	mxmtd->ops.nand.set_bb = spinand_set_bb;
	mxmtd->ops.nand.chk_bb = spinand_chk_bb;
	mxmtd->ops.nand.set_ecc_en = spinand_set_ecc_en;
	mxmtd->ops.nand.get_ecc_strength = spinand_get_ecc_strength;
	mxmtd->ops.nand.get_ecc_nsteps = spinand_get_ecc_nsteps;
	mxmtd->ops.nand.get_ecc_step_size = spinand_get_ecc_step_size;

	return ret;
}

#endif /* #ifdef CONF_SPINAND */
