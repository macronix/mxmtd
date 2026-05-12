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

#ifndef _SPINAND_CMD_H_
#define _SPINAND_CMD_H_

#include <stdint.h>

typedef struct _xfer_info xfer_info_t;

int cmd_spinand_param_page_read(xfer_info_t *xfer, uint8_t *buf, uint32_t nbytes);

int cmd_spinand_page_read(xfer_info_t *xfer, uint32_t row, uint8_t len_row, uint32_t timeout_us);
int cmd_spinand_read_cache(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes);
int cmd_spinand_read_cache_seq(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes, uint8_t end, uint32_t timeout_us);

int cmd_spinand_write_enable(xfer_info_t *xfer);
int cmd_spinand_write_disable(xfer_info_t *xfer);
int cmd_spinand_program_load(xfer_info_t *xfer, uint32_t col, uint8_t len_col, uint8_t *buf,
		uint32_t nbytes);
int cmd_spinand_program_load_rnd(xfer_info_t *xfer, uint32_t col, uint8_t len_col,
		uint8_t *buf, uint32_t nbytes);
int cmd_spinand_program_execute(xfer_info_t *xfer, uint32_t row, uint8_t len_row, uint8_t *sr,
		uint32_t timeout_us);
int cmd_spinand_block_erase(xfer_info_t *xfer, uint32_t addr, uint8_t len_addr, uint8_t *sr,
		uint32_t timeout_us);
int cmd_spinand_rdid(xfer_info_t *xfer, uint8_t *id, uint8_t len_id);
int cmd_spinand_reset(xfer_info_t *xfer, uint8_t *sr);
int cmd_spinand_get_feature(xfer_info_t *xfer, uint8_t addr, uint8_t *buf);
int cmd_spinand_set_feature(xfer_info_t *xfer, uint8_t addr, uint8_t *buf);
int cmd_spinand_rd_sr(xfer_info_t *xfer, uint8_t *sr);
int cmd_spinand_rd_eccsr(xfer_info_t *xfer, uint8_t *eccsr);

#endif
