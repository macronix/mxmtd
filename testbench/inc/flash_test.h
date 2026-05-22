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

#ifndef _COMMON_TEST_H_
#define _COMMON_TEST_H_

#include <stdint.h>

typedef struct _mxmtd mxmtd_t;

uint32_t check_keyin(uint32_t from, uint32_t to);
int flash_dump_info(mxmtd_t *mxmtd);
int flash_nand_nop_test(mxmtd_t *mxmtd, uint32_t blk, uint32_t page, uint8_t npages);
int flash_nand_error_bit_test(mxmtd_t *mxmtd, uint32_t blk);
int flash_nand_bb_test(mxmtd_t *mxmtd, uint32_t blk);
int flash_performance_test(mxmtd_t *mxmtd, uint32_t blk);
int flash_random_test(mxmtd_t *mxmtd, uint32_t ntimes);
int flash_torture_test(mxmtd_t *mxmtd, uint32_t blk, uint32_t ntimes);
int flash_epr_verify_test(mxmtd_t *mxmtd);
int flash_erase_all(mxmtd_t *mxmtd);


#endif
