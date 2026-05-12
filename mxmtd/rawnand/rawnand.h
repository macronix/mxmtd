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

#ifndef _RAWNAND_H_
#define _RAWNAND_H_

#include <stdint.h>
#include "nand.h"

typedef struct _mxmtd mxmtd_t;

#define MAX_BUF_RAWNAND 128

typedef struct {
	uint8_t buf_shared[MAX_BUF_RAWNAND];
	uint8_t *buf_oob;
	uint8_t *buf_data;
	uint8_t ver_onfi;
	ecc_t ecc;
	oob_layout_t layout;
	struct {
		uint8_t nbits_col;
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
		uint8_t len_col:2,
				len_row:2,
				len_addr:3;
	} memorg;
	struct {
		uint32_t prog_us;
		uint32_t ers_us;
		uint32_t rd_us;
		uint32_t ccs_ns;
	} timing;
	struct {
		uint8_t	cache_pgm:1,
				cach_read:1,
				set_get_feat:1,
				rdsr_enh:1,
				copy_back:1,
				rd_uniq_id:1,
				reserved:2;

		uint8_t randomizer:1,
				special_read_retry:1,
				onfi_param_page:1;

		uint8_t	nspecial_read;

	} cmds_supl;

	struct {
		int (*pre_ecc_rd);
		int (*pre_ecc_pgm);
		int (*post_ecc_rd);
		int (*post_ecc_pgm);
	} ops;

	uint8_t *bbt;
	uint32_t len_bbt;
} rawnand_t;

int setup_rawnand(mxmtd_t *mxmtd, uint8_t ecc_type, uint32_t size_step);

#endif
