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

#ifndef _NAND_H_
#define _NAND_H_

typedef struct _ecc ecc_t;

/* 0: good block, 1: bad block */
#define SET_BB(_bbt, _pos) (_bbt[(_pos) / 8] |= 1U << ((_pos) % 8))
#define CLR_BB(_bbt, _pos) (_bbt[(_pos) / 8] &= ~(1U << ((_pos) % 8)))
#define CHK_BB(_bbt, _pos) ((_bbt[(_pos) / 8] >> ((_pos) % 8)) & 1U)

typedef struct {
		uint32_t ofs;
		uint32_t len;
} layout_t;

typedef struct {
	layout_t ecc;
	layout_t bb;
	layout_t free;
} oob_layout_t;

struct _ecc{
	void *priv;
	uint8_t strength;
	uint8_t nsteps;
	int state;
	uint8_t max_err_bits_per_step;
	uint32_t ecc_bits_corrected;
	uint8_t *mask_ff;
	uint16_t size_step;
	/* Size of partial page */
	uint32_t size_pp;
	uint32_t nbytes;
	uint8_t type;
	uint8_t type_old;
			#define ECC_NONE			0x00000000U
			/* Raw NAND flash */
			#define ECC_INTERNAL_RAW	(1U << 1)
			/* SPI NAND flash */
			#define ECC_INTERNAL_SPI	(1U << 2)
			/* Software ECC BCH */
			#define ECC_SW_BCH			(1U << 3)
			/* Mask for all software ECC */
			#define ECC_SW_MASK			ECC_SW_BCH
	uint8_t uncorrectable:1,
			threthold:1;

	uint32_t step_s;
	uint32_t step_e;

	int (*init)(ecc_t *ecc, uint32_t size_step, uint32_t strength_ecc, uint32_t *nbytes_ecc);
	void (*release)(ecc_t *ecc);
	int (*calc)(ecc_t *ecc, uint8_t *buf_data, uint8_t *buf_ecc);
	int (*corr)(ecc_t *ecc, uint8_t *buf_data, uint8_t *buf_ecc);
};

int nand_sw_ecc_init(ecc_t *ecc);
int nand_oob_layout_init(oob_layout_t *layout, uint32_t nbytes_ecc_per_page, uint32_t size_oob);
int nand_sw_ecc_prev_rd(ecc_t *ecc);
int nand_sw_ecc_prev_pgm(ecc_t *ecc, uint8_t *buf_data, uint8_t *buf_ecc);
int nand_sw_ecc_post_rd(ecc_t *ecc, uint8_t *buf_data, uint8_t *buf_ecc);

#endif
