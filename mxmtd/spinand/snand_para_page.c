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
#ifdef CONF_SPINAND

#include <stdint.h>
#include "snand_para_page.h"
#include "platform_print.h"
#include "spinand_err.h"
#include "mxmtd.h"
#include "lib.h"


/* Revision Information and Features Block (32B) */
typedef struct __attribute__((packed)) {
    uint8_t signature[4];
    uint16_t revision_num;
    uint16_t feat_supl;
    uint16_t opt_cmd_supl;
    uint8_t reserved[22];
} serial_rifb_t;

/* Manufacturer Information Block (48B) */
typedef struct __attribute__((packed)) {
	uint8_t dev_manufacturer[12];
	uint8_t dev_model[20];
	uint8_t jedec_id;
	uint16_t date_code;
	uint8_t reserved[13];
} serial_mib_t;

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
} serial_mob_t;

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
} serial_epb_t;

/* Vendor Blocks (92B) */
typedef struct __attribute__((packed)) {
	uint16_t vendor_revision;
	uint8_t reserved0;
	uint8_t special_read_retry:1,
			randomizer:1,
			reserved1:6;
	uint8_t bbm_table:1,
			cont_rd:1,
			reserved2:6;
	uint8_t nspecial_read;
	uint8_t vendor[84];
	uint16_t crc;
} serial_vb_t;

typedef struct __attribute__((packed)) {
	serial_rifb_t rifb;	/* [031:000] */
	serial_mib_t mib;		/* [079:032] */
	serial_mob_t mob;		/* [127:080] */
	serial_epb_t epb;		/* [163:128] */
	serial_vb_t vb;		/* [255:164] */
} snand_param_page_t;

#define BIT(x) (1U << (x))

#define SERIAL_V1_0			(0x00)

static int snand_check_sign_crc(snand_param_page_t *param_page)
{
	int n = 0;
	uint8_t *data = (uint8_t *)param_page;
	uint16_t crc = 0x4F4E;
	uint8_t len = sizeof(snand_param_page_t) - 2; //exclusive of CRC (2 bytes)

	if (memcmp(param_page->rifb.signature, (uint8_t *)("ONFI"), 4)) {
		mxic_pr_err("Signature in SPINAND parameter page is not matched\r\n");
		return MXST_ERR_S_PP_SIGNATURE;
	}
	mxic_pr_dbg("Signature in SPINAND parameter page is matched!\r\n");

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
		mxic_pr_err("CRC in SPINAND parameter page is not matched, get(%04X): cal(%04X)\r\n",
				crc, param_page->vb.crc);
		return MXST_ERR_S_PP_CRC;
	}

	mxic_pr_dbg("CRC in SPINAND parameter page is matched!\r\n");
	return MXST_SUCCESS;
}

static int snand_get_ver(mxmtd_t *mxmtd, snand_param_page_t *param_page)
{
	if (SERIAL_V1_0 == param_page->rifb.revision_num) {
		mxmtd->spinand.ver_serial = SERIAL_V1_0;
		mxic_pr_dbg("Support for SPINAND version is v0.0\r\n");
		return MXST_SUCCESS;
	}
	mxic_pr_err("Unsupported SPINAND version (%d)\r\n", param_page->rifb.revision_num);
	return MXST_ERR_S_PP_VER;
}

static int snand_get_info(mxmtd_t *mxmtd, snand_param_page_t *param_page)
{
	int n = sizeof(param_page->mib.dev_model);
	spinand_t *snand = &mxmtd->spinand;

	while (n--) {
		if (0x20 == param_page->mib.dev_model[n]) {
			param_page->mib.dev_model[n] = 0x00;
		} else {
			break;
		}
	}
	mxic_pr_inf("[SPINAND PARAMETER PAGE] %s is found\r\n", param_page->mib.dev_model);

	snand->memorg.npages_per_blk = param_page->mob.pages_per_blk;
	snand->memorg.nplanes_per_lun = 1 << param_page->mob.interleaved_bits;
	snand->memorg.nblks_per_plane = param_page->mob.blocks_per_lun / snand->memorg.nplanes_per_lun;
	snand->memorg.nluns = param_page->mob.nluns;

	mxic_pr_dbg("\r\n"
			"pages per blk : %d,\t %04Xh\r\n"
			"blks per plane: %d,\t %04Xh\r\n"
			"planes per lun: %d,\t %02Xh\r\n"
			"num of lun    : %d,\t %02Xh\r\n",
			snand->memorg.npages_per_blk, snand->memorg.npages_per_blk,
			snand->memorg.nblks_per_plane, snand->memorg.nblks_per_plane,
			snand->memorg.nplanes_per_lun, snand->memorg.nplanes_per_lun,
			snand->memorg.nluns, snand->memorg.nluns);

	snand->memorg.size_oob = param_page->mob.spare_nbytes_per_page;
	snand->memorg.size_page = param_page->mob.data_nbytes_per_page;
	snand->memorg.size_blk = snand->memorg.size_page * snand->memorg.npages_per_blk;
	snand->memorg.size_plane = snand->memorg.size_blk * snand->memorg.nblks_per_plane;
	snand->memorg.size_lun = snand->memorg.size_plane * snand->memorg.nplanes_per_lun;
	snand->memorg.size_chip = snand->memorg.size_lun * snand->memorg.nluns;
	snand->memorg.nblks_per_chip = snand->memorg.size_chip / snand->memorg.size_blk;

	mxic_pr_dbg("\r\n"
				"size of oob  : %d,\t %04Xh\r\n"
				"size of page : %d,\t %04Xh\r\n"
				"size of blk  : %d,\t %08Xh\r\n"
				"size of plane: %d,\t %08Xh\r\n"
				"size of lun  : %d,\t %08Xh\r\n"
				"size of chip : %d,\t %08Xh\r\n",
				snand->memorg.size_oob, snand->memorg.size_oob,
				snand->memorg.size_page, snand->memorg.size_page,
				snand->memorg.size_blk, snand->memorg.size_blk,
				(uint32_t)snand->memorg.size_plane, (uint32_t)(snand->memorg.size_plane),
				(uint32_t)snand->memorg.size_lun, (uint32_t)(snand->memorg.size_lun),
				(uint32_t)snand->memorg.size_chip, (uint32_t)(snand->memorg.size_chip));

	snand->ecc.size_pp = param_page->mob.data_nbytes_per_ppage; /* ECC data block size */
	snand->ecc.nsteps = snand->memorg.size_page / snand->ecc.size_step;
	snand->ecc.strength = param_page->mob.ecc_bits; /* Number of bits ECC correctability */
	if (0 == snand->ecc.strength && snand->id_info) {
		snand->ecc.strength = snand->id_info->ecc.strength;
	}

	mxic_pr_dbg("\r\n"
				"ecc step size: %d,\t %02Xh\r\n"
				"ecc step     : %d,\t %02Xh\r\n"
				"ecc strength : %d,\t %02Xh\r\n",
				snand->ecc.size_step, snand->ecc.size_step,
				snand->ecc.nsteps, snand->ecc.nsteps,
				snand->ecc.strength, snand->ecc.strength);

	snand->timing.t_prog_us = param_page->epb.t_prog_us;
	snand->timing.t_ers_us = param_page->epb.t_ers_us;
	snand->timing.t_rd_us = param_page->epb.t_rd_us;

	memcpy(&snand->cmds_supl, &param_page->rifb.opt_cmd_supl, 1);
	snand->cmds_supl.bbm_table = param_page->vb.bbm_table;
	snand->cmds_supl.cont_rd = param_page->vb.cont_rd;
	snand->cmds_supl.randomizer = param_page->vb.randomizer;
	snand->cmds_supl.special_read_retry = param_page->vb.special_read_retry;
	snand->cmds_supl.nspecial_read = param_page->vb.nspecial_read;

	mxic_pr_dbg("\r\n"
				"support cache pgm                  : %d\r\n"
				"support cache read                 : %d\r\n"
				"support set/get feature            : %d\r\n"
				"support rdsr enhance               : %d\r\n"
				"support copy back                  : %d\r\n"
				"support read unique ID             : %d\r\n"
				"support BBM table                  : %d\r\n"
				"support continuous read            : %d\r\n"
				"support randomizer                 : %d\r\n"
				"support read retries               : %d\r\n"
				"support read retries times         : %d\r\n",
				snand->cmds_supl.cache_pgm,
				snand->cmds_supl.cach_read,
				snand->cmds_supl.set_get_feat,
				snand->cmds_supl.rdsr_enh,
				snand->cmds_supl.copy_back,
				snand->cmds_supl.rd_uniq_id,
				snand->cmds_supl.bbm_table,
				snand->cmds_supl.cont_rd,
				snand->cmds_supl.randomizer,
				snand->cmds_supl.special_read_retry,
				snand->cmds_supl.nspecial_read);

	snand->memorg.size_subpage = snand->memorg.size_page >> ((snand->ecc.nsteps <= 2) ? 1 : 2);
	snand->memorg.mask_subpage = snand->memorg.size_subpage - 1;
	snand->memorg.mask_page = snand->memorg.size_page - 1;
	snand->memorg.mask_blk = snand->memorg.size_blk - 1;

	return MXST_SUCCESS;
}

int snand_parse_param_page(mxmtd_t *mxmtd, uint8_t *buf_pp)
{
	int ret;
	snand_param_page_t *param_page = (snand_param_page_t *)buf_pp;

	ret = snand_check_sign_crc(param_page);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	ret = snand_get_ver(mxmtd, param_page);
	if (MXST_SUCCESS != ret) {
		return ret;
	}

	return snand_get_info(mxmtd, param_page);
}

#endif /* #ifdef CONF_SPINAND */
