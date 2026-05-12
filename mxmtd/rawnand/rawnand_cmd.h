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

#ifndef _RAWNAND_CMD_H_
#define _RAWNAND_CMD_H_

#include <stdint.h>

typedef struct _xfer_info xfer_info_t;

int cmd_rawnand_param_page_read(xfer_info_t *xfer, uint8_t *buf, uint32_t nbytes);
int cmd_rawnand_read_start(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint32_t timeout_us);
int cmd_rawnand_page_read(xfer_info_t *xfer, uint8_t *buf, uint32_t nbytes, uint8_t last);
int cmd_rawnand_page_read_rnd(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes);
int cmd_rawnand_cache_read_seq(xfer_info_t *xfer, uint8_t *buf, uint32_t nbytes, uint8_t last,
		uint32_t timeout_us);
int cmd_rawnand_cache_read_rnd(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr,
		uint8_t *buf, uint32_t nbytes, uint8_t last);

int cmd_rawnand_page_pgm(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint8_t last, uint32_t timeout_us);
int cmd_rawnand_page_pgm_rnd(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint32_t timeout_us);
int cmd_rawnand_cache_pgm_start(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint32_t timeout_us);
int cmd_rawnand_cache_pgm(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint32_t timeout_us);
int cmd_rawnand_cache_pgm_end(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *buf,
		uint32_t nbytes, uint8_t *sr, uint32_t timeout_us);
int cmd_rawnand_block_erase(xfer_info_t *xfer, uint64_t addr, uint8_t len_addr, uint8_t *sr,
		uint32_t timeout_us);
int cmd_rawnand_bp_status_read(xfer_info_t *xfer, uint32_t addr, uint8_t len_addr, uint8_t *bpsr);
int cmd_rawnand_get_feature(xfer_info_t *xfer, uint32_t addr, uint8_t *buf);
int cmd_rawnand_set_feature(xfer_info_t *xfer, uint32_t addr, uint8_t *buf);
int cmd_rawnand_rdid(xfer_info_t *xfer, uint32_t addr, uint8_t *id, uint8_t len_id);
int cmd_rawnand_reset(xfer_info_t *xfer);

#endif
