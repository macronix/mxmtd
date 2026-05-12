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
#ifdef CONF_RAWNAND

#include <stdint.h>
#include "onfi_para_page.h"
#include "platform_print.h"
#include "rawnand_err.h"
#include "mxmtd.h"
#include "lib.h"

// TODO: feature supported

/* ONFI Parameter Page v1.0 */

/* Revision Information and Features Block (32B) */
typedef struct __attribute__((packed)) {
    uint8_t signature[4];
    uint16_t revision_num;
    uint16_t feat_supl;
    uint16_t opt_cmd_supl;
    uint8_t reserved[22];
} onfi_rifb_t;

/* Manufacturer Information Block (48B) */
typedef struct __attribute__((packed)) {
	uint8_t dev_manufacturer[12];
	uint8_t dev_model[20];
	uint8_t jedec_id;
	uint16_t date_code;
	uint8_t reserved[13];
} onfi_mib_t;

/* Memory Organization Block (48B) */
typedef struct __attribute__((packed)) {
	uint32_t data_nbytes_per_page;
	uint16_t spare_nbytes_per_page;
	uint32_t data_nbytes_per_ppage; /* ppage: partial page */
	uint16_t spare_nbytes_per_ppage; /* ppage: partial page */
	uint32_t pages_per_blk;
	uint32_t blocks_per_lun;
	uint8_t nluns;
	uint8_t addr_cycles;
	uint8_t nbits_per_cell;
	uint16_t bb_per_lun;
	uint16_t block_endurance;
	uint8_t guaranteed_valid_blks_begin;
	uint16_t guaranteed_valid_blks_endurance;
	uint8_t pgms_per_page;
	uint8_t ppage_attr;
	uint8_t ecc_bits;
	uint8_t interleaved_bits;
	uint8_t interleaved_ops;
	uint8_t reserved[13];
} onfi_mob_t;

/* Electrical Parameters Block (36B) */
typedef struct __attribute__((packed)) {
	uint8_t io_pin_capacitance;
	uint16_t timing_mode;
	uint16_t pgm_cache_timing_mode;
	uint16_t t_prog_us;
	uint16_t t_ers_us;
	uint16_t t_rd_us;
	uint16_t t_ccs_ns;
	uint8_t reserved[23];
} onfi_epb_t;

/* Vendor Blocks (92B) */
typedef struct __attribute__((packed)) {
	uint16_t vendor_revision;
	uint8_t reserved0;
	uint8_t special_read_retry:1,
			randomizer:1,
			reserved:6;
	uint8_t reserved1;
	uint8_t nspecial_read;
	uint8_t vendor[84];
	uint16_t crc;
} onfi_vb_t;

typedef struct __attribute__((packed)) {
	onfi_rifb_t rifb;	/* [031:000] */
	onfi_mib_t mib;		/* [079:032] */
	onfi_mob_t mob;		/* [127:080] */
	onfi_epb_t epb;		/* [163:128] */
	onfi_vb_t vb;		/* [255:164] */
} onfi_param_page_t;

#define BIT(x) (1U << (x))

#define ONFI_V1_0			BIT(1)

static int onfi_check_sign_crc(onfi_param_page_t *param_page)
{
	int n = 0;
	uint8_t *data = (uint8_t *)param_page;
	uint16_t crc = 0x4F4E;
	uint8_t len = sizeof(onfi_param_page_t) - 2; //exclusive of CRC (2 bytes)

	if (memcmp(param_page->rifb.signature, (uint8_t *)("ONFI"), 4)) {
		mxic_pr_err("Signature in ONFI parameter page is not matched\r\n");
		return MXST_ERR_ONFI_PP_SIGNATURE;
	}
	mxic_pr_dbg("Signature in ONFI parameter page is matched!\r\n");

	while (len) {
		crc ^= *data << 8;
		while (8 > n) {
			crc = (crc << 1) ^ ((crc & 0x8000) ? 0x8005 : 0);
			n++;
		}
		len--;
		data++;
		n = 0;
	}

	if (crc != param_page->vb.crc) {
		mxic_pr_err("CRC in ONFI parameter page is not matched, get(%04X): cal(%04X)\r\n",
				crc, param_page->vb.crc);
		return MXST_ERR_ONFI_PP_CRC;
	}

	mxic_pr_dbg("CRC in ONFI parameter page is matched!\r\n");
	return MXST_SUCCESS;
}

static int onfi_get_ver(mxmtd_t *mxmtd, onfi_param_page_t *param_page)
{
	if (ONFI_V1_0 & param_page->rifb.revision_num) {
		mxmtd->rawnand.ver_onfi = 10;
		mxic_pr_dbg("Support for ONFI version is v1.0\r\n");
		return MXST_SUCCESS;
	}
	mxic_pr_err("Unsupported ONFI version (%d)\r\n", param_page->rifb.revision_num);
	return MXST_ERR_ONFI_PP_VER;
}

static int onfi_get_info(mxmtd_t *mxmtd, onfi_param_page_t *param_page)
{
	int n = sizeof(param_page->mib.dev_model);
	rawnand_t *rnand = &mxmtd->rawnand;

	while (n--) {
		if (0x20 == param_page->mib.dev_model[n]) {
			param_page->mib.dev_model[n] = 0x00;
		} else {
			break;
		}
	}
	mxic_pr_inf("[ONFI PARAMETER PAGE] %s is found from ONFI parameter page\r\n", param_page->mib.dev_model);

	rnand->memorg.npages_per_blk = param_page->mob.pages_per_blk;
	rnand->memorg.nplanes_per_lun = 1 << param_page->mob.interleaved_bits;
	rnand->memorg.nblks_per_plane = param_page->mob.blocks_per_lun / rnand->memorg.nplanes_per_lun;
	rnand->memorg.nluns = param_page->mob.nluns;

	mxic_pr_dbg("\r\n"
			"pages per blk : %d,\t %04Xh\r\n"
			"blks per plane: %d,\t %04Xh\r\n"
			"planes per lun: %d,\t %02Xh\r\n"
			"num of lun    : %d,\t %02Xh\r\n",
			rnand->memorg.npages_per_blk, rnand->memorg.npages_per_blk,
			rnand->memorg.nblks_per_plane, rnand->memorg.nblks_per_plane,
			rnand->memorg.nplanes_per_lun, rnand->memorg.nplanes_per_lun,
			rnand->memorg.nluns, rnand->memorg.nluns);

	rnand->memorg.size_oob = param_page->mob.spare_nbytes_per_page;
	rnand->memorg.size_page = param_page->mob.data_nbytes_per_page;
	rnand->memorg.size_blk = rnand->memorg.size_page * rnand->memorg.npages_per_blk;
	rnand->memorg.size_plane = rnand->memorg.size_blk * rnand->memorg.nblks_per_plane;
	rnand->memorg.size_lun = rnand->memorg.size_plane * rnand->memorg.nplanes_per_lun;
	rnand->memorg.size_chip = rnand->memorg.size_lun * rnand->memorg.nluns;
	rnand->memorg.nblks_per_chip = rnand->memorg.size_chip / rnand->memorg.size_blk;

	mxic_pr_dbg("\r\n"
				"size of oob  : %d,\t %04Xh\r\n"
				"size of page : %d,\t %04Xh\r\n"
				"size of blk  : %d,\t %08Xh\r\n"
				"size of plane: %d,\t %08Xh\r\n"
				"size of lun  : %d,\t %08Xh\r\n"
				"size of chip : %d,\t %08Xh\r\n",
				rnand->memorg.size_oob, rnand->memorg.size_oob,
				rnand->memorg.size_page, rnand->memorg.size_page,
				rnand->memorg.size_blk, rnand->memorg.size_blk,
				(uint32_t)rnand->memorg.size_plane, (uint32_t)(rnand->memorg.size_plane),
				(uint32_t)rnand->memorg.size_lun, (uint32_t)(rnand->memorg.size_lun),
				(uint32_t)rnand->memorg.size_chip, (uint32_t)(rnand->memorg.size_chip));

	rnand->memorg.len_col = (param_page->mob.addr_cycles & 0xf0) >> 4;
	rnand->memorg.len_row = param_page->mob.addr_cycles & 0xf;
	rnand->memorg.len_addr = rnand->memorg.len_col + rnand->memorg.len_row;
	rnand->memorg.nbits_col = fmsb32(rnand->memorg.size_page) + 1;

	mxic_pr_dbg("\r\n"
				"length of column: %d,\t %02Xh\r\n"
				"length of row   : %d,\t %02Xh\r\n"
				"nbits of column : %d,\t %02Xh\r\n",
				rnand->memorg.len_col, rnand->memorg.len_col,
				rnand->memorg.len_row, rnand->memorg.len_row,
				rnand->memorg.nbits_col, rnand->memorg.nbits_col);

	rnand->ecc.size_pp = param_page->mob.data_nbytes_per_ppage; /* ECC data block size */
	rnand->ecc.nsteps = rnand->memorg.size_page / rnand->ecc.size_step;
	rnand->ecc.strength = param_page->mob.ecc_bits; /* Number of bits ECC correctability */

	mxic_pr_dbg("\r\n"
				"ecc step size: %d,\t %02Xh\r\n"
				"ecc step     : %d,\t %02Xh\r\n"
				"ecc strength : %d,\t %02Xh\r\n",
				rnand->ecc.size_step, rnand->ecc.size_step,
				rnand->ecc.nsteps, rnand->ecc.nsteps,
				rnand->ecc.strength, rnand->ecc.strength);

	rnand->timing.prog_us = param_page->epb.t_prog_us;
	rnand->timing.ers_us = param_page->epb.t_ers_us;
	rnand->timing.rd_us = param_page->epb.t_rd_us;
	rnand->timing.ccs_ns = param_page->epb.t_ccs_ns;

	memcpy(&rnand->cmds_supl, &param_page->rifb.opt_cmd_supl, 1);
	rnand->cmds_supl.copy_back = 0; /* Not support */
	rnand->cmds_supl.randomizer = param_page->vb.randomizer;
	rnand->cmds_supl.special_read_retry = param_page->vb.special_read_retry;
	rnand->cmds_supl.nspecial_read = param_page->vb.nspecial_read;

	mxic_pr_dbg("\r\n"
				"support cache pgm                  : %d\r\n"
				"support cache read                 : %d\r\n"
				"support set/get feature            : %d\r\n"
				"support rdsr enhance               : %d\r\n"
				"support copy back                  : %d\r\n"
				"support read unique ID             : %d\r\n"
				"support randomizer                 : %d\r\n"
				"support read retries               : %d\r\n"
				"support read retries times         : %d\r\n",
				rnand->cmds_supl.cache_pgm,
				rnand->cmds_supl.cach_read,
				rnand->cmds_supl.set_get_feat,
				rnand->cmds_supl.rdsr_enh,
				rnand->cmds_supl.copy_back,
				rnand->cmds_supl.rd_uniq_id,
				rnand->cmds_supl.randomizer,
				rnand->cmds_supl.special_read_retry,
				rnand->cmds_supl.nspecial_read);

	rnand->memorg.size_subpage = rnand->memorg.size_page >> ((rnand->ecc.nsteps <= 2) ? 1 : 2);
	rnand->memorg.mask_subpage = rnand->memorg.size_subpage - 1;
	rnand->memorg.mask_page = rnand->memorg.size_page - 1;
	rnand->memorg.mask_blk = rnand->memorg.size_blk - 1;

	return MXST_SUCCESS;
}

int onfi_parse_param_page(mxmtd_t *mxmtd, uint8_t *buf_pp)
{
	int ret;
	onfi_param_page_t *param_page = (onfi_param_page_t *)buf_pp;

	ret = onfi_check_sign_crc(param_page);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = onfi_get_ver(mxmtd, param_page);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return onfi_get_info(mxmtd, param_page);
}

#endif /* #ifdef CONF_RAWNAND */
