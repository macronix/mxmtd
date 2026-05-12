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
 * 16-bit data bus width
 */

#include "mxmtd_config.h"
#ifdef CONF_RAWNAND

#include <malloc.h>
#include "platform_print.h"
#include "platform_itf.h"
#include "rawnand_cmd.h"
#include "rawnand_err.h"
#include "mxmtd.h"
#include "mxmtd_config.h"
#include "onfi_para_page.h"
#include "lib.h"

#define BIT(x) (1U << (x))

#define MAX_LEN_ID		8
#define ONFI_ID			"ONFI"
#define ADDR_ID 		0x00
#define ADDR_ONFI		0x20
#define ADDR_FEAT_BP_OP	0xa0

#define SR_CHIP_STATUS		BIT(0)
#define SR_CACHE_PGM_STATUS	BIT(1)
#define SR_WP				BIT(7)

#define ID4_INTERNAL_ECC	BIT(7)

#define DIR_RD	0
#define DIR_PGM	1

#define LEN_ONFI_PP 256

#define NUM_PARAMTER_PAGE 3

static inline uint64_t rawnand_chg_addr(mxmtd_t *mxmtd, uint64_t addr, uint32_t ofs_oob)
{
	rawnand_t *rnand = &mxmtd->rawnand;

	return ofs_oob | (addr & (rnand->memorg.mask_page)) |
			(addr >> fmsb32(rnand->memorg.size_page)) << (8 * rnand->memorg.len_col);
}

static int rawnand_get_id(mxmtd_t *mxmtd)
{
	int ret;
	uint8_t *id = mxmtd->rawnand.buf_shared;

	ret = cmd_rawnand_rdid(mxmtd->priv, ADDR_ID, id, 5);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	mxic_pr_dbg("RAWNAND ID: %02X%02X%02X%02X%02X\r\n", id[0], id[1], id[2], id[3], id[4]);

	if (id[4] & ID4_INTERNAL_ECC) {
		mxmtd->rawnand.ecc.type = ECC_INTERNAL_RAW;
		mxic_pr_inf("Internal ECC is supported, so the ECC type is forced to internal ECC\r\n");
	}

	ret = cmd_rawnand_rdid(mxmtd->priv, ADDR_ONFI, id, 4);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	mxic_pr_dbg("RAWNAND ONFI ID: %02X%02X%02X%02X\r\n", id[0], id[1], id[2], id[3]);

	mxmtd->rawnand.cmds_supl.onfi_param_page = !memcmp(id, "ONFI", 4);
	mxic_pr_dbg("ONFI parameter Page is%s supported\r\n",
			mxmtd->rawnand.cmds_supl.onfi_param_page ? "" : " not");

	return ret;
}

static int rawnand_setup_parameters(mxmtd_t *mxmtd)
{
	int ret, n = 0;
	rawnand_t *rnand = &mxmtd->rawnand;
	uint8_t buf_pp[LEN_ONFI_PP * NUM_PARAMTER_PAGE];

	/* for ONFI parameter page */
	ret = cmd_rawnand_param_page_read(mxmtd->priv, buf_pp, sizeof(buf_pp));
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	while (n < 3) {
		ret = onfi_parse_param_page(mxmtd, &buf_pp[n * LEN_ONFI_PP]);
		if (MXST_ERR_ONFI_PP_SIGNATURE == ret || MXST_ERR_ONFI_PP_CRC == ret) {
			mxic_pr_dbg("Parse parameter page %d failed\r\n", n);
			n++;
			continue;
		}
		break;
	}
	if (MXST_SUCCESS != ret) {
		return MXST_ERR_ONFI_PP_PARSE;
	}

	/* for data & oob buffer */
	if (ECC_SW_MASK & rnand->ecc.type) {
		rnand->buf_data = malloc(rnand->memorg.size_page + rnand->memorg.size_oob);
		if (0 == rnand->buf_data) {
			mxic_pr_err("Allocate for buf_data failed\r\n");
			return MXST_ERR_ALLOC;
		}
		rnand->buf_oob = rnand->buf_data + rnand->memorg.size_page;

	} else {
		rnand->buf_oob = malloc(rnand->memorg.size_oob);
		if (0 == rnand->buf_oob) {
			mxic_pr_err("Allocate for buf_oob failed\r\n");
			return MXST_ERR_ALLOC;
		}
	}

	/* for bad block management */
	rnand->len_bbt = (rnand->memorg.nblks_per_chip + 7) / 8;
	rnand->bbt = calloc(1, rnand->len_bbt);
	if (0 == rnand->bbt) {
		mxic_pr_err("Allocate for bbt failed, size: %Xh\r\n", rnand->len_bbt);
		rnand->len_bbt = 0;
		return MXST_ERR_ALLOC;
	}

	/* for sub-page */
	rnand->memorg.size_subpage = rnand->memorg.size_page >> ((rnand->ecc.nsteps <= 2) ? 1 : 2);

	return MXST_SUCCESS;
}

static int rawnand_unlock_bp(mxmtd_t *mxmtd)
{
	int ret;
	uint8_t bp[4] = {0};

	ret = cmd_rawnand_get_feature(mxmtd->priv, ADDR_FEAT_BP_OP, bp);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	mxic_pr_dbg("Default bp: %02X\r\n", bp[0]);

	if (0x38 & bp[0]) {
		bp[0] &= ~0x38;
		ret = cmd_rawnand_set_feature(mxmtd->priv, ADDR_FEAT_BP_OP, bp);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		ret = cmd_rawnand_get_feature(mxmtd->priv, ADDR_FEAT_BP_OP, bp);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		mxic_pr_dbg("Set feature to bp: %02X for unlocking all blocks\r\n", bp[0]);
	}
	return ret;
}

static int prev_ecc(mxmtd_t *mxmtd, uint8_t *buf_data, uint8_t *buf_oob, uint8_t dir)
{
	int ret = MXST_SUCCESS;
	rawnand_t *rnand = &mxmtd->rawnand;

	if (DIR_RD == dir) {
		return MXST_SUCCESS;
	}

	switch (rnand->ecc.type) {
	case ECC_SW_BCH:
		ret = nand_sw_ecc_prev_pgm(&rnand->ecc, buf_data, buf_oob);
		break;
	default:
		break;
	}

	return ret;
}

static int post_ecc(mxmtd_t *mxmtd, uint8_t *buf_data, uint8_t *buf_oob, uint8_t dir)
{
	int ret = MXST_SUCCESS;
	rawnand_t *rnand = &mxmtd->rawnand;

	if (DIR_PGM == dir) {
		return MXST_SUCCESS;
	}

	switch (rnand->ecc.type) {
	case ECC_SW_BCH:
		ret = nand_sw_ecc_post_rd(&rnand->ecc, buf_data, buf_oob);
		break;
	}

	return ret;
}

static inline int rawnand_internal_ecc_init(ecc_t *ecc)
{
	return MXST_SUCCESS;
}

static int rawnand_ecc_init(mxmtd_t *mxmtd)
{
	rawnand_t *rnand = &mxmtd->rawnand;

	mxic_pr_dbg("ECC type is %Xh\r\n", rnand->ecc.type);

	rnand->ecc.step_s = 0;
	rnand->ecc.step_e = rnand->ecc.nsteps - 1;

	switch (rnand->ecc.type) {
#if CONF_RAWNAND_ECC_TYPE == ECC_SW_BCH
	case ECC_SW_BCH:
		if (0 == rnand->ecc.strength || 0 == rnand->ecc.size_step) {
			mxic_pr_err("ECC strength or step size is 0, please add from id table or parameter page\r\n");
			return MXST_ERR_NOT_SUP;
		}
		return nand_sw_ecc_init(&rnand->ecc);
#endif
	case ECC_INTERNAL_RAW:
		return rawnand_internal_ecc_init(&rnand->ecc);

	default:
		mxic_pr_war("Force ECC Off\r\n");
		rnand->ecc.type = ECC_NONE;
		return MXST_SUCCESS;
	}

	return MXST_SUCCESS;
}

static inline int rawnand_oob_layout_init(mxmtd_t *mxmtd)
{

	if (ECC_NONE == mxmtd->rawnand.ecc.type) {
		return MXST_SUCCESS;
	}

	return nand_oob_layout_init(&mxmtd->rawnand.layout,
			mxmtd->rawnand.ecc.nbytes * mxmtd->rawnand.ecc.nsteps,
			mxmtd->rawnand.memorg.size_oob);
}

static uint64_t rawnand_get_chip_size(mxmtd_t *mxmtd)
{
	return mxmtd->rawnand.memorg.size_chip;
}

static uint32_t rawnand_get_blk_size(mxmtd_t *mxmtd)
{
	return mxmtd->rawnand.memorg.size_blk;
}

static uint32_t rawnand_get_page_size(mxmtd_t *mxmtd)
{
	return mxmtd->rawnand.memorg.size_page;
}

static uint32_t rawnand_get_ers_size(mxmtd_t *mxmtd)
{
	return mxmtd->rawnand.memorg.size_blk;
}

static uint32_t rawnand_get_subpage_size(mxmtd_t *mxmtd)
{
	return mxmtd->rawnand.memorg.size_subpage;
}

static int rawnand_get_ecc_state(mxmtd_t *mxmtd)
{
	if (mxmtd->rawnand.ecc.uncorrectable) {
		return -1;
	}

	return mxmtd->rawnand.ecc.max_err_bits_per_step;
}

static void rawnand_set_ecc_en(mxmtd_t *mxmtd, uint8_t en)
{
	if (en) {
		if (ECC_NONE != mxmtd->rawnand.ecc.type) {
			return;
		}
		mxmtd->rawnand.ecc.type = mxmtd->rawnand.ecc.type_old;
		return;
	}

	mxmtd->rawnand.ecc.type_old = mxmtd->rawnand.ecc.type;
	mxmtd->rawnand.ecc.type = ECC_NONE;
}

static uint8_t rawnand_get_ecc_strength(mxmtd_t *mxmtd)
{
	return mxmtd->rawnand.ecc.strength;
}

static uint8_t rawnand_get_ecc_nsteps(mxmtd_t *mxmtd)
{
	return mxmtd->rawnand.ecc.nsteps;
}

static uint16_t rawnand_get_ecc_step_size(mxmtd_t *mxmtd)
{
	return mxmtd->rawnand.ecc.size_step;
}

static int rawnand_read_oob(mxmtd_t *mxmtd, uint64_t addr, uint32_t ofs_oob, uint32_t nbytes)
{
	int ret;
	rawnand_t *rnand = &mxmtd->rawnand;

	if ((ofs_oob + nbytes) > rnand->memorg.size_oob) {
		mxic_pr_err("ofs_oob(%Xh) and len_oob(%Xh) exceed oob size(%Xh)\r\n",
				ofs_oob, nbytes, rnand->memorg.size_oob);
	}

	ret = cmd_rawnand_read_start(mxmtd->priv,
			rawnand_chg_addr(mxmtd, addr, rnand->memorg.size_page | ofs_oob),
			rnand->memorg.len_addr, rnand->timing.rd_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return cmd_rawnand_page_read(mxmtd->priv, &rnand->buf_oob[ofs_oob], nbytes, 1);
}

static int rawnand_unalign_read(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	rawnand_t *rnand = &mxmtd->rawnand;
	uint8_t req_oob = !!(ECC_SW_MASK & rnand->ecc.type);
	uint8_t *buf_data = req_oob ? rnand->buf_data : buf;
	uint8_t *buf_oob = 0;
	uint32_t col = (uint32_t)(addr & rnand->memorg.mask_page);
	uint32_t nsteps_rd;
	uint64_t remain = nbytes;

	if (req_oob) {
		rnand->ecc.step_s = col / rnand->ecc.size_step;
		rnand->ecc.step_e = (col + remain - 1) / rnand->ecc.size_step;
		nsteps_rd = rnand->ecc.step_e - rnand->ecc.step_s + 1;

		addr &= ~rnand->memorg.mask_page;
		addr |= rnand->ecc.step_s * rnand->ecc.size_step;
		remain = nsteps_rd * rnand->ecc.size_step;
	}

	ret = prev_ecc(mxmtd, 0, 0, DIR_RD);
	if (MXST_SUCCESS != ret) {
		goto release_rawnand_unalign_read;
	}

	ret = cmd_rawnand_read_start(mxmtd->priv, rawnand_chg_addr(mxmtd, addr, 0), rnand->memorg.len_addr,
			rnand->timing.rd_us);
	if (MXST_SUCCESS != ret) {
		goto release_rawnand_unalign_read;
	}

	ret = cmd_rawnand_page_read(mxmtd->priv, buf_data, remain, !req_oob);
	if (MXST_SUCCESS != ret) {
		goto release_rawnand_unalign_read;
	}

	if (req_oob) {
		uint32_t col_oob;
		uint32_t ofs_oob = rnand->layout.ecc.ofs + rnand->ecc.step_s * rnand->ecc.nbytes;

		buf_oob = ofs_oob + rnand->buf_oob;
		col_oob = ofs_oob + rnand->memorg.size_page;
		remain = nsteps_rd * rnand->ecc.nbytes;

		ret = cmd_rawnand_page_read_rnd(mxmtd->priv, col_oob, rnand->memorg.len_col, buf_oob, remain);
		if (MXST_SUCCESS != ret) {
			goto release_rawnand_unalign_read;
		}
	}

	ret = post_ecc(mxmtd, buf_data, buf_oob, DIR_RD);
	if (MXST_SUCCESS != ret) {
		goto release_rawnand_unalign_read;
	}

	if (req_oob) {
		memcpy(buf, &buf_data[col % rnand->ecc.size_step], nbytes);
	}

release_rawnand_unalign_read:
	if (req_oob) {
		rnand->ecc.step_s = 0;
		rnand->ecc.step_e = rnand->ecc.nsteps - 1;
	}

	return ret;
}

static int rawnand_page_read(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint32_t nbytes)
{
	int ret;
	rawnand_t *rnand = &mxmtd->rawnand;
	uint32_t nbytes_oob = (ECC_SW_MASK & rnand->ecc.type) ? rnand->memorg.size_oob : 0;
	uint8_t *buf_data = nbytes_oob ? rnand->buf_data : buf;
	uint8_t *buf_oob = 0;

	ret = cmd_rawnand_read_start(mxmtd->priv, rawnand_chg_addr(mxmtd, addr, 0),
			rnand->memorg.len_addr, rnand->timing.rd_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = prev_ecc(mxmtd, 0, 0, DIR_RD);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_rawnand_page_read(mxmtd->priv, buf_data, nbytes + nbytes_oob, 1);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	if (nbytes_oob) {
		memcpy(buf, buf_data, nbytes);
		buf_oob = rnand->buf_oob + rnand->layout.ecc.ofs;
	}
	ret = post_ecc(mxmtd, buf, buf_oob, DIR_RD);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return MXST_SUCCESS;
}

static int rawnand_cache_read_seq(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	rawnand_t *rnand = &mxmtd->rawnand;
	uint32_t nbytes_oob = (ECC_SW_MASK & rnand->ecc.type) ? rnand->memorg.size_oob : 0;
	uint8_t *buf_data = nbytes_oob ? rnand->buf_data : buf;
	uint8_t *buf_oob = 0;
	uint32_t size_page = rnand->memorg.size_page;

	ret = cmd_rawnand_read_start(mxmtd->priv, rawnand_chg_addr(mxmtd, addr, 0),
			rnand->memorg.len_addr, rnand->timing.rd_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	while (nbytes) {
		uint32_t nbytes_rd = (nbytes >= size_page) ? size_page : (uint32_t)nbytes;

		ret = prev_ecc(mxmtd, 0, 0, DIR_RD);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		ret = cmd_rawnand_cache_read_seq(mxmtd->priv, buf_data, nbytes_rd + nbytes_oob,
				nbytes_rd == nbytes, rnand->timing.rd_us);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (nbytes_oob) {
			memcpy(buf, buf_data, nbytes_rd);
			buf_oob = rnand->buf_oob + rnand->layout.ecc.ofs;
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

	return ret;
}

static int rawnand_read_entry(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	uint32_t size_page = mxmtd->rawnand.memorg.size_page;
	uint32_t nbytes_rd = size_page - (addr % size_page);
	uint32_t remain;

	/* for small data */
	if (size_page != nbytes_rd) {
		nbytes_rd = nbytes > nbytes_rd ? nbytes_rd : (uint32_t)nbytes;

		ret = rawnand_unalign_read(mxmtd, addr, buf, nbytes_rd);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		addr += nbytes_rd;
		buf += nbytes_rd;
		nbytes -= nbytes_rd;
	}

	remain = (uint32_t)(nbytes % size_page);
	nbytes -= remain;

	if (nbytes) {
		/* for one page */
		if (size_page == nbytes) {
			ret = rawnand_page_read(mxmtd, addr, buf, nbytes);
		/* for two or more pages */
		} else {
			ret = rawnand_cache_read_seq(mxmtd, addr, buf, nbytes);
		}
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		addr += nbytes;
		buf += nbytes;
	}

	/* for small data */
	if (remain) {
		ret = rawnand_unalign_read(mxmtd, addr, buf, remain);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
	}

	return MXST_SUCCESS;
}

static int rawnand_read(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	if (0 == nbytes) {
		mxic_pr_war("Read bytes = 0, do nothing!\r\n");
		return MXST_SUCCESS;
	}

	mxmtd->rawnand.ecc.ecc_bits_corrected = 0;
	mxmtd->rawnand.ecc.uncorrectable = 0;
	mxmtd->rawnand.ecc.max_err_bits_per_step = 0;
	mxmtd->rawnand.ecc.threthold = 0;

	switch (mxmtd->rawnand.ecc.type) {
//	case ECC_INTERNAL_RAW:
//		mxic_pr_dbg("Internal ECC\r\n");
//		break;
	case ECC_SW_BCH:
		mxic_pr_dbg("S/W BCH ECC\r\n");
		break;
	case ECC_NONE:
		mxic_pr_dbg("ECC Off\r\n");
		break;
	default:
		mxic_pr_err("Read ECC type(%d) is illegal\r\n", mxmtd->rawnand.ecc.type);
		return MXST_ERR_NOT_SUP;
	}

	mxic_pr_dbg("Read    : "
				"%08X-%08Xh ~ "
				"%08X: %08Xh, "
				"Block: %d, "
				"nbytes: %08X-%08Xh\r\n",
				(uint32_t )(addr >> 32), (uint32_t)(addr),
				(uint32_t )((addr + nbytes - 1) >> 32), (uint32_t)(addr + nbytes - 1),
				(uint32_t)(addr >> fmsb32(mxmtd->rawnand.memorg.size_blk)),
				(uint32_t )(nbytes >> 32), (uint32_t)(nbytes));

	return rawnand_read_entry(mxmtd, addr, buf, nbytes);
}

static int rawnand_pgm_oob(mxmtd_t *mxmtd, uint64_t addr, uint32_t ofs_oob, uint32_t nbytes)
{
	int ret;
	rawnand_t *rnand = &mxmtd->rawnand;
	uint8_t sr;

	if ((ofs_oob + nbytes) > rnand->memorg.size_oob) {
		mxic_pr_err("ofs_oob(%Xh) and len_oob(%Xh) exceed oob size(%Xh)\r\n",
				ofs_oob, nbytes, rnand->memorg.size_oob);
	}

	ret = cmd_rawnand_page_pgm(mxmtd->priv,
			rawnand_chg_addr(mxmtd, addr, ofs_oob | rnand->memorg.size_page),
			rnand->memorg.len_addr, &rnand->buf_oob[ofs_oob], nbytes, &sr, 1,
			rnand->timing.prog_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	if (SR_CHIP_STATUS & sr) {
		mxic_pr_err("Page program oob failed! SR: %02Xh\r\n", sr);
		return MXST_ERR_PROGRAM;
	}

	return MXST_SUCCESS;
}

static int rawnand_unalign_pgm(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint32_t nbytes)
{
	int ret;
	rawnand_t *rnand = &mxmtd->rawnand;
	uint8_t sr;
	uint8_t req_oob = !!(ECC_SW_MASK & rnand->ecc.type);
	uint8_t *buf_data = req_oob ? rnand->buf_data : buf;
	uint8_t *buf_oob = 0;
	uint32_t col = (uint32_t)(addr & rnand->memorg.mask_page);
	uint32_t nsteps_pgm;

	if (req_oob) {
		memset(buf_data, 0xff, rnand->memorg.size_page + rnand->memorg.size_oob);

		rnand->ecc.step_s = col / rnand->ecc.size_step;
		rnand->ecc.step_e = (col + nbytes - 1) / rnand->ecc.size_step;
		nsteps_pgm = rnand->ecc.step_e - rnand->ecc.step_s + 1;

		buf_data += rnand->ecc.step_s * rnand->ecc.size_step;
		buf_oob = rnand->buf_oob + rnand->layout.ecc.ofs + rnand->ecc.step_s * rnand->ecc.nbytes;

		memcpy(&buf_data[col % rnand->ecc.size_step], buf, nbytes);
	}

	ret = prev_ecc(mxmtd, buf_data, buf_oob, DIR_PGM);
	if (MXST_SUCCESS != ret) {
		goto release_rawnand_unalign_pgm;
	}

	ret = cmd_rawnand_page_pgm(mxmtd->priv, rawnand_chg_addr(mxmtd, addr, 0),
			rnand->memorg.len_addr, buf, nbytes, &sr, !req_oob, rnand->timing.prog_us);
	if (MXST_SUCCESS != ret) {
		goto release_rawnand_unalign_pgm;
	}

	if (req_oob) {
		col = rnand->memorg.size_page + rnand->layout.ecc.ofs + rnand->ecc.step_s * rnand->ecc.nbytes;
		nbytes = nsteps_pgm * rnand->ecc.nbytes;

		ret = cmd_rawnand_page_pgm_rnd(mxmtd->priv, col, rnand->memorg.len_col, buf_oob, nbytes, &sr,
				rnand->timing.prog_us);
	}

	if (SR_CHIP_STATUS & sr) {
		mxic_pr_err("Page program failed!\r\n");
		ret = MXST_ERR_PROGRAM;
		goto release_rawnand_unalign_pgm;
	}

	ret = post_ecc(mxmtd, 0, 0, DIR_PGM);

release_rawnand_unalign_pgm:
	if (req_oob) {
		rnand->ecc.step_s = 0;
		rnand->ecc.step_e = rnand->ecc.nsteps - 1;
	}

	return ret;
}

static int rawnand_page_pgm(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint32_t nbytes)
{
	int ret;
	rawnand_t *rnand = &mxmtd->rawnand;
	uint8_t sr;
	uint32_t nbytes_oob = (ECC_SW_MASK & rnand->ecc.type) ? rnand->memorg.size_oob : 0;
	uint8_t *buf_data = nbytes_oob ? rnand->buf_data : buf;
	uint8_t *buf_oob = 0;

	if (nbytes_oob) {
		memcpy(buf_data, buf, nbytes);
		buf_oob = rnand->buf_oob + rnand->layout.ecc.ofs;
	}
	ret = prev_ecc(mxmtd, buf_data, buf_oob, DIR_PGM);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = cmd_rawnand_page_pgm(mxmtd->priv, rawnand_chg_addr(mxmtd, addr, 0),
			rnand->memorg.len_addr, buf_data, nbytes + nbytes_oob, &sr, 1,
			rnand->timing.prog_us);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
	if (SR_CHIP_STATUS & sr) {
		mxic_pr_err("Page program failed! SR=0x%02X\r\n", sr);
		return MXST_ERR_PROGRAM;
	}

	return post_ecc(mxmtd, 0, 0, DIR_PGM);
}

static int rawnand_cache_pgm(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	rawnand_t *rnand = &mxmtd->rawnand;
	uint32_t nbytes_oob = (ECC_SW_MASK & rnand->ecc.type) ? rnand->memorg.size_oob : 0;
	uint8_t *buf_data = nbytes_oob ? rnand->buf_data : buf;
	uint8_t *buf_oob = 0;
	uint32_t size_page = rnand->memorg.size_page;
	uint64_t remain = nbytes;

	if (remain <= size_page) {
		return rawnand_page_pgm(mxmtd, addr, buf, nbytes);
	}

	while (remain) {
		uint8_t sr;
		uint32_t nbytes_pgm = (remain > size_page) ? size_page : (uint32_t)remain;

		if (nbytes_oob) {
			memcpy(buf_data, buf, nbytes_pgm);
			buf_oob = rnand->buf_oob + rnand->layout.ecc.ofs;
		}
		ret = prev_ecc(mxmtd, buf_data, buf_oob, DIR_PGM);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (remain == nbytes) {
			ret = cmd_rawnand_cache_pgm_start(mxmtd->priv, rawnand_chg_addr(mxmtd, addr, 0),
					rnand->memorg.len_addr, buf_data, nbytes_pgm + nbytes_oob, &sr,
					rnand->timing.prog_us);
		} else if (remain <= size_page) {
			ret = cmd_rawnand_cache_pgm_end(mxmtd->priv, rawnand_chg_addr(mxmtd, addr, 0),
					rnand->memorg.len_addr, buf_data, nbytes_pgm + nbytes_oob, &sr,
					rnand->timing.prog_us);
		} else {
			ret = cmd_rawnand_cache_pgm(mxmtd->priv, rawnand_chg_addr(mxmtd, addr, 0),
					rnand->memorg.len_addr, buf_data, nbytes_pgm + nbytes_oob, &sr,
					rnand->timing.prog_us);
		}
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		if (SR_CACHE_PGM_STATUS & sr) {
			mxic_pr_err("Cache program failed! SR=0x%02X\r\n", sr);
			return ret;
		}

		ret = post_ecc(mxmtd, 0, 0, DIR_PGM);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		addr += nbytes_pgm;
		if (nbytes_oob) {
			buf  += nbytes_pgm;
		} else {
			buf_data  += nbytes_pgm;
		}
		remain -= nbytes_pgm;
	}

	return MXST_SUCCESS;
}

static int rawnand_program_entry(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	int ret;
	uint32_t size_page = mxmtd->rawnand.memorg.size_page;
	uint32_t nbytes_pgm = size_page - (addr % size_page);
	uint32_t remain;

	/* for small data */
	if (size_page != nbytes_pgm) {
		nbytes_pgm  = nbytes > nbytes_pgm ? nbytes_pgm : (uint32_t)nbytes;

		ret = rawnand_unalign_pgm(mxmtd, addr, buf, nbytes_pgm);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		addr += nbytes_pgm;
		buf += nbytes_pgm;
		nbytes -= nbytes_pgm;
	}

	remain = (uint32_t)(nbytes % size_page);
	nbytes -= remain;

	if (nbytes) {
		if (size_page == nbytes) {
			/* for one page */
			ret = rawnand_page_pgm(mxmtd, addr, buf, nbytes);
		} else {
			/* for two or more pages */
			ret = rawnand_cache_pgm(mxmtd, addr, buf, nbytes);
		}
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		addr += nbytes;
		buf += nbytes;
	}

	/* for small data */
	if (remain) {
		ret = rawnand_unalign_pgm(mxmtd, addr, buf, remain);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
	}

	return MXST_SUCCESS;
}

static int rawnand_program(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes)
{
	if (0 == nbytes) {
		mxic_pr_war("program bytes = 0, do nothing!\r\n");
		return MXST_SUCCESS;
	}

	switch (mxmtd->rawnand.ecc.type) {
//	case ECC_INTERNAL_RAW:
//		mxic_pr_dbg("Internal ECC\r\n");
//		break;
	case ECC_SW_BCH:
		mxic_pr_dbg("S/W BCH ECC\r\n");
		break;
	case ECC_NONE:
		mxic_pr_dbg("ECC Off\r\n");
		break;
	default:
		mxic_pr_err("Program ECC type(%d) is illegal\r\n", mxmtd->rawnand.ecc.type);
		return MXST_ERR_NOT_SUP;
	}

	mxic_pr_dbg("Program : "
				"%08X-%08Xh ~ "
				"%08X: %08Xh, "
				"Block: %d, "
				"nbytes: %08X-%08Xh\r\n",
				(uint32_t )(addr >> 32), (uint32_t)(addr),
				(uint32_t )((addr + nbytes - 1) >> 32), (uint32_t)(addr + nbytes - 1),
				(uint32_t)(addr >> fmsb32(mxmtd->rawnand.memorg.size_blk)),
				(uint32_t )(nbytes >> 32), (uint32_t)(nbytes));

	return rawnand_program_entry(mxmtd, addr, buf, nbytes);
}

static int rawnand_erase(mxmtd_t *mxmtd, uint64_t addr, uint64_t nbytes)
{
	int ret = MXST_SUCCESS;
	uint32_t size_blk = mxmtd->rawnand.memorg.size_blk;
	uint32_t size_page = mxmtd->rawnand.memorg.size_page;

	mxic_pr_dbg("Erase, Block: %d\r\n", (uint32_t)(addr >> fmsb32(size_blk)));

	if (addr % size_blk) {
		mxic_pr_err("Address is not allowed with erase size(%08Xh)\r\n", size_blk);
		return MXST_ERR_NOT_ALIGN;
	}

	if (nbytes % size_blk) {
		mxic_pr_err("End address is not allowed with erase size(%08X)\r\n", size_blk);
		return MXST_ERR_NOT_ALIGN;
	}

	while (nbytes) {
		uint8_t sr;

		ret = cmd_rawnand_block_erase(mxmtd->priv, (addr >> fmsb32(size_page)),
				mxmtd->rawnand.memorg.len_row, &sr, mxmtd->rawnand.timing.ers_us);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		if (SR_CHIP_STATUS & sr) {
			uint8_t bpsr;

			mxic_pr_err("Erase failed!\r\n");

			ret = cmd_rawnand_bp_status_read(mxmtd->priv, addr >> fmsb32(size_page),
					mxmtd->rawnand.memorg.len_row, &bpsr);
			if (MXST_SUCCESS != ret) {
				return ret;
			}
			mxic_pr_dbg("addr: %08X-%08Xh, block %d: %s protected, device %s solid-protected"
					", bpsr(%02Xh)\r\n",
					(uint32_t)(addr >> 32), (uint32_t)(addr),
					(uint32_t)(addr >> fmsb32(size_page)),
					(bpsr & 0x04) ? "is not" : "is",
					(bpsr & 0x02) ? "is not" : "is",
					bpsr);

			return ret;
		}

		/* Clear BB if erase successfully */
		CLR_BB(mxmtd->rawnand.bbt, addr >> fmsb32(size_blk));

		addr += size_blk;
		nbytes -= size_blk;
	}

	return ret;
}

static int rawnand_set_bb(mxmtd_t *mxmtd, uint64_t addr, uint8_t pre_erase)
{
	int ret, n;
	rawnand_t *rnand = &mxmtd->rawnand;

	memset(rnand->buf_oob + rnand->layout.bb.ofs, 0x00, rnand->layout.bb.len);

	addr &= ~rnand->memorg.mask_blk;

	if (pre_erase) {
		ret = rawnand_erase(mxmtd, addr, rnand->memorg.size_blk);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
	}

	for (n = 0; n < 2; n++) {
		ret = rawnand_pgm_oob(mxmtd, addr, rnand->layout.bb.ofs, rnand->layout.bb.len);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		addr += rnand->memorg.size_page;
	}

	SET_BB(rnand->bbt, addr >> fmsb32(rnand->memorg.size_blk));

	return MXST_SUCCESS;
}

static uint8_t rawnand_chk_bb(mxmtd_t *mxmtd, uint64_t addr)
{
	return CHK_BB(mxmtd->rawnand.bbt, addr >> fmsb32(mxmtd->rawnand.memorg.size_blk));
}

static int rawnand_scan_bb(mxmtd_t *mxmtd)
{
	int ret;
	rawnand_t *rnand = &mxmtd->rawnand;
	uint32_t n, m, blk;
	uint32_t nblks = rnand->memorg.nblks_per_chip;
	uint64_t addr = 0;

	for (blk = 0; blk < nblks; blk++) {
		addr = blk * rnand->memorg.size_blk;
		for (n = 0; n < 2; n++) {
			ret = rawnand_read_oob(mxmtd, addr, rnand->layout.bb.ofs, rnand->layout.bb.len);
			if (MXST_SUCCESS != ret) {
				return ret;
			}

			for (m = 0; m < rnand->layout.bb.len; m++) {
				if (0xff != rnand->buf_oob[rnand->layout.bb.ofs + m]) {
					SET_BB(rnand->bbt, blk);
					mxic_pr_war("Page: %d, Bytes: %02X-%02X, block: %d is bad\r\n",
						n,
						rnand->buf_oob[rnand->layout.bb.ofs],
						rnand->buf_oob[rnand->layout.bb.ofs + 1],
						blk);
					n = 2;
					break;
				}
			}
			addr += rnand->memorg.size_page;
		}
	}

	return MXST_SUCCESS;
}

static int rawnand_probe(mxmtd_t *mxmtd)
{
	int ret;

#ifdef CONF_PROBE_RESET
	ret = cmd_rawnand_reset(mxmtd->priv);
	if (MXST_SUCCESS != ret) {
		return ret;
	}
#endif

	ret = rawnand_get_id(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = rawnand_setup_parameters(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = rawnand_ecc_init(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = rawnand_oob_layout_init(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = rawnand_unlock_bp(mxmtd);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return rawnand_scan_bb(mxmtd);
}

static void rawnand_release(mxmtd_t *mxmtd)
{
	if (mxmtd->priv) {
		free(mxmtd->priv);
		mxmtd->priv = 0;
	}

	if (mxmtd->rawnand.buf_data) {
		free(mxmtd->rawnand.buf_data);
		mxmtd->rawnand.buf_data = 0;
		mxmtd->rawnand.buf_oob = 0;

	} else if(mxmtd->rawnand.buf_oob) {
		free(mxmtd->rawnand.buf_oob);
		mxmtd->rawnand.buf_oob = 0;
	}

	if (mxmtd->rawnand.bbt) {
		free(mxmtd->rawnand.bbt);
		mxmtd->rawnand.bbt = 0;
		mxmtd->rawnand.len_bbt = 0;
	}

	if (mxmtd->rawnand.ecc.release) {
		mxmtd->rawnand.ecc.release(&mxmtd->rawnand.ecc);
	}
}

static int rawnand_init(mxmtd_t *mxmtd, uint8_t ecc_type, uint32_t size_step)
{
	xfer_info_t *xfer = calloc(1, sizeof(xfer_info_t));
	if (!xfer) {
		mxic_pr_err("Allocate for xfer failed\r\n");
		return MXST_ERR_ALLOC;
	}

	xfer->hc_prot_type = HC_PROT_RAWNAND;
	mxmtd->priv = xfer;
	mxmtd->rawnand.ecc.type = ecc_type;
	mxmtd->rawnand.ecc.size_step = size_step;
	mxic_pr_dbg("Default ECC type is %Xh\r\n", mxmtd->rawnand.ecc.type);

	return MXST_SUCCESS;
}

int setup_rawnand(mxmtd_t *mxmtd, uint8_t ecc_type, uint32_t size_step)
{
	int ret;

	ret = rawnand_init(mxmtd, ecc_type, size_step);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	mxmtd->ops.release = rawnand_release;
    mxmtd->ops.probe = rawnand_probe;
    mxmtd->ops.read = rawnand_read;
    mxmtd->ops.program = rawnand_program;
    mxmtd->ops.erase = rawnand_erase;

    mxmtd->ops.get_chip_size = rawnand_get_chip_size;
	mxmtd->ops.get_blk_size = rawnand_get_blk_size;
	mxmtd->ops.get_page_size = rawnand_get_page_size;
	mxmtd->ops.get_ers_size = rawnand_get_ers_size;
	mxmtd->ops.get_subpage_size = rawnand_get_subpage_size;

	mxmtd->ops.get_ecc_state = rawnand_get_ecc_state;

	mxmtd->ops.nand.read_oob = rawnand_read_oob;
	mxmtd->ops.nand.write_oob = rawnand_pgm_oob;
    mxmtd->ops.nand.set_bb = rawnand_set_bb;
    mxmtd->ops.nand.chk_bb = rawnand_chk_bb;
    mxmtd->ops.nand.set_ecc_en = rawnand_set_ecc_en;
    mxmtd->ops.nand.get_ecc_strength = rawnand_get_ecc_strength;
    mxmtd->ops.nand.get_ecc_nsteps = rawnand_get_ecc_nsteps;
    mxmtd->ops.nand.get_ecc_step_size = rawnand_get_ecc_step_size;

	return ret;
}

#endif /* #ifdef CONF_RAWNAND */
