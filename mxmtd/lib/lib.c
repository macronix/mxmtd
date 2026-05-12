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

#include <string.h>
#include "lib.h"

/* count bits */
uint8_t cbs32(uint32_t bits)
{
	uint8_t n = 0;

	while (bits) {
		bits = (bits - 1) & bits;
		n++;
	}
	return n;
}

/**
 *  find msb
 *  For example:
 *  fmsb32(0) return -1
 *  fmsb32(1) return 0
 *  fmsb32(0x80000000) return 31
 **/
int fmsb32(uint32_t bits)
{
	int n = 32, m = 16;
	uint32_t x = 0xffffffff;

	if (!bits) {
		return -1;
	}

	do {
		if (!(bits & (x << (32 - m)))) {
			bits <<= m;
			n -= m;
		}
		m >>= 1;
	} while(m);

	return n - 1;
}

/**
 *  reserve msb
 *  For example:
 *  rmsb32(0x11000) return 0x10000
 *  fmsb32(0xA0051) return 0x80000
 *  fmsb32(0x00003) return 0x00002
 **/
uint32_t rmsb32(uint32_t bits)
{
	uint32_t x = 0xffff0000U, y = 8;

	if (!bits) {
		return 0;
	}

	while (y) {
		x = (bits & x) ? (x << y) : ((x >> y) & ~x);
		if (1 == y) {
			if (!(bits & x)) {
				x >>= 1;
			}
			break;
		}
		y >>= 1;
	}
	return bits & x;
}

int swap64_nbytes(uint64_t *val, uint8_t nbytes)
{
	int n = 0;
	uint64_t tmp = 0;

	if (nbytes > 8 || nbytes < 1) {
		return -1;
	}

	while (n < nbytes) {
		tmp |= ((*val >> (n * 8)) & 0xff) << ((nbytes -n -1) * 8);
		n++;
	}

	memcpy(val, &tmp, 8);

	return 0;
}

void swap_nbytes(uint8_t *data, uint8_t nbytes)
{
    uint8_t tmp[64], n;

    for (n = 0; n < nbytes; n++) {
    	tmp[nbytes - n - 1] = data[n];
    }

    memcpy(data, tmp, nbytes);
}

void swap_half(uint8_t *data, uint8_t nbytes)
{
	swap_nbytes(data, nbytes / 2);
	swap_nbytes(data + nbytes / 2, nbytes / 2);
}

