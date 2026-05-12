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

#ifndef _SPINOR_H_
#define _SPINOR_H_

#include <stdint.h>

typedef struct _mxmtd mxmtd_t;
typedef struct _cmd_meta cmd_meta_t;
typedef struct _rdcmds rdcmds_t;

#define MAX_BUF_SPINOR 32

typedef struct {
	char *name;
	uint8_t *id;
	uint8_t len_id;
	uint32_t size_chip;
	uint32_t size_block;
	uint16_t size_page;
	uint16_t nblocks;
	uint32_t flag_spcl;	
	struct {
		uint32_t val[5];
#define CMDS_LIST_ID_REG	0
#define CMDS_LIST_OTHERS	1
#define CMDS_LIST_ERS		2
#define CMDS_LIST_PGM		3
#define CMDS_LIST_RD		4
		rdcmds_t *rdcmds;
		uint8_t nrdcmds;
		uint16_t *dc;
		uint8_t ndc;
	} cmd_list[4];//FIX MAX_SPINOR_MODE
	struct {
		uint32_t w;
		uint32_t dp;
		uint32_t bp;
		uint32_t pp;
		uint32_t se;
		uint32_t be32;
		uint32_t be;
		uint32_t ce;
		uint32_t wreaw;
	} timing;
} snor_id_info_t;

typedef struct {
	snor_id_info_t *id_info;
	cmd_meta_t *cmd_meta;
	uint8_t buf_shared[MAX_BUF_SPINOR];
	struct {
		uint32_t size;
		uint32_t cmd_idx_3b;
		uint32_t cmd_idx_4b;
		uint32_t cmd_idx_spi;
		uint32_t cmd_idx_opi;
	} conf_ers;
	uint8_t en4b:1,
			enqe:1,
			set_dc:1,
			wpsel:1,
			asp_pwd:1;
} spinor_t;

int setup_spinor(mxmtd_t *mxmtd, uint32_t size_ers);

#endif
