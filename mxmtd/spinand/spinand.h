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

#ifndef _SPINAND_H_
#define _SPINAND_H_

#include <stdint.h>
#include "nand.h"

#define MAX_BUF_SPINAND 128

typedef struct _mxmtd mxmtd_t;

typedef struct {
	char *name;
	uint8_t *id;
	uint8_t len_id;
	uint32_t flag_spcl;
	struct {
		uint8_t strength;
		uint32_t size_step;
	} ecc;

	struct {
		uint8_t bbm_table:1,
				cont_rd:1,
				randomizer:1,
				special_read_retry:1;
		uint8_t nspecial_read;
	} cmds_supl;

} snand_id_info_t;

typedef struct {
	uint8_t buf_shared[MAX_BUF_SPINAND];
	uint8_t *buf_oob;
	uint8_t *buf_data;
	uint8_t ver_serial;
	snand_id_info_t *id_info;
	ecc_t ecc;
	oob_layout_t layout;
	struct {
		uint8_t nluns;
		uint8_t nplanes_per_lun;
		uint16_t nblks_per_plane;
		uint32_t nblks_per_chip;
		uint16_t npages_per_blk;
		uint64_t size_chip;
		uint64_t size_lun;
		uint64_t size_plane;
		uint32_t size_blk;
		uint32_t size_page;
		uint32_t size_subpage;
		uint16_t size_oob;
		uint64_t mask_blk;
		uint64_t mask_page;
		uint64_t mask_subpage;
	} memorg;
	struct {
        uint32_t t_prog_us;
        uint32_t t_ers_us;
        uint32_t t_rd_us;
	} timing;
	struct {
		uint8_t	cache_pgm:1,
				cach_read:1,
				set_get_feat:1,
				rdsr_enh:1,
				copy_back:1,
				rd_uniq_id:1,
				reserved:2;
		uint8_t	randomizer:1,
				special_read_retry:1,
				cont_rd:1,
				bbm_table:1;
		uint8_t	nspecial_read;
	} cmds_supl;

	struct {
		uint8_t ecc_thre;
		uint8_t cont_rd:1,
				internal_ecc:1,
				quad:1;
	} cache;

	uint8_t *bbt;
	uint32_t len_bbt;
} spinand_t;

int setup_spinand(mxmtd_t *mxmtd, uint8_t ecc_type, uint32_t size_step);

#endif
