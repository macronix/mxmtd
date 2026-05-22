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

#ifndef _LIB_H
#define _LIB_H

#include "stdint.h"

#define ROUNDDOWN(_val, _base) (((_val) / (_base)) * (_base))
#define ROUNDUP(_val, _base) (((((_val) + (_base) - 1)) / (_base)) * (_base))

uint8_t cbs32(uint32_t bits);
int fmsb32(uint32_t bits);
uint32_t rmsb32(uint32_t bits);
void swap_nbytes(uint8_t *data, uint8_t nbytes);
void swap_half(uint8_t *data, uint8_t len);
int swap64_nbytes(uint64_t *val, uint8_t nbytes);

static inline int is_le()
{
	uint16_t n = 0x0001;

	return 1 == (*(uint8_t *)&n);
}

#define H2BE(_host, _bits)		(is_le() ? swap##_bits(_host) : (_host))
#define H2LE(_host, _bits)		(is_le() ? (_host) : swap##_bits(_host))
#define BE2H(_big, _bits)		(is_le() ? swap##_bits(_big) : (_big))
#define LE2H(_little, _bits)	(is_le() ? (_little) : swap##_bits(_little))

#endif
