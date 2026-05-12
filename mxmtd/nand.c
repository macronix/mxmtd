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
#if defined(CONF_SPINAND) || defined(CONF_RAWNAND)

#include <stdint.h>
#include <malloc.h>
#include "lib.h"
#include "ecc/bch.h"
#include "nand.h"
#include "mxmtd_err.h"
#include "platform_print.h"

#define ROUNDUP_DIV(_val, _base) (((_base) - 1 + (_val)) / (_base))

static int nand_bch_calc(ecc_t *ecc, uint8_t *buf_data, uint8_t *buf_ecc)
{
	int n;

	bch_encode(ecc->priv, buf_data, buf_ecc);
	for (n = 0; n < ecc->nbytes; n++) {
		buf_ecc[n] ^= ecc->mask_ff[n];
	}

	return MXST_SUCCESS;
}

static int nand_bch_corr(ecc_t *ecc, uint8_t *buf_data, uint8_t *buf_ecc)
{
	int n;

	for (n = 0; n < ecc->nbytes; n++) {
		buf_ecc[n] ^= ecc->mask_ff[n];
	}

	ecc->state = bch_decode(ecc->priv, buf_data, buf_ecc);

	return MXST_SUCCESS;
}

static inline void nand_bch_release(ecc_t *ecc)
{
	bch_free(ecc->priv);
	if (ecc->mask_ff) {
		free(ecc->mask_ff);
		ecc->mask_ff = 0;
	}
	ecc->priv = 0;
}

static int nand_bch_init(ecc_t *ecc)
{
	int ret, n;
	uint32_t m = fmsb32(8 * ecc->size_step) + 1;
	bch_t *bch = 0;
	uint8_t *data;

	if (!ecc->size_step || !ecc->strength) {
		mxic_pr_err("ECC step size(%d) or ECC strength(%d) is 0\r\n",
				ecc->size_step, ecc->strength);
		return MXST_ERR_ECC;
	}

	ecc->nbytes = ROUNDUP_DIV(ecc->strength * m, 8);

	ret = bch_init(m, ecc->strength, ecc->size_step, &bch);
	if (MXST_SUCCESS != ret) {
		nand_bch_release(ecc);
		mxic_pr_err("SW ECC BCH initialization failed\r\n");
		return MXST_ERR_ECC;
	}

	if (bch->ecc_bytes != ecc->nbytes) {
		nand_bch_release(ecc);
		mxic_pr_err("ECC bytes is invalid: %u, expected: %u\r\n", bch->ecc_bytes, ecc->nbytes);
		return MXST_ERR_ECC;
	}

	ecc->mask_ff = malloc(ecc->nbytes);
	if (!ecc->mask_ff) {
		nand_bch_release(ecc);
		return MXST_ERR_ALLOC;
	}

	data = malloc(ecc->size_step);
	if (!data) {
		nand_bch_release(ecc);
		return MXST_ERR_ALLOC;
	}

	memset(data, 0xff, ecc->size_step);
	bch_encode(bch, data, ecc->mask_ff);
	free(data);

	for (n = 0; n < ecc->nbytes; n++) {
		ecc->mask_ff[n] ^= 0xff;
	}

	ecc->priv = bch;
	ecc->calc = nand_bch_calc;
	ecc->corr = nand_bch_corr;
	ecc->release = nand_bch_release;

	return MXST_SUCCESS;
}

int nand_sw_ecc_init(ecc_t *ecc)
{
	int ret = MXST_SUCCESS;

	switch (ecc->type) {
	case ECC_SW_BCH:
		ret = nand_bch_init(ecc);
		break;
	default:
		ret = MXST_ERR_NOT_SUP;
		break;
	}

	return ret;
}

int nand_sw_ecc_prev_rd(ecc_t *ecc)
{
	return MXST_SUCCESS;
}

int nand_sw_ecc_prev_pgm(ecc_t *ecc, uint8_t *buf_data, uint8_t *buf_oob)
{
	int ret;
	uint32_t n;

	for (n = ecc->step_s; n <= ecc->step_e; n++) {
		ret = ecc->calc(ecc, buf_data, buf_oob);
		if (MXST_SUCCESS != ret) {
			return ret;
		}
		buf_data += ecc->size_step;
		buf_oob += ecc->nbytes;
	}

	return MXST_SUCCESS;
}

int nand_sw_ecc_post_rd(ecc_t *ecc, uint8_t *buf_data, uint8_t *buf_oob)
{
	int ret;
	uint32_t n;

	for (n = ecc->step_s; n <= ecc->step_e; n++) {
		ret = ecc->corr(ecc, buf_data, buf_oob);
		if (MXST_SUCCESS != ret) {
			return ret;
		}

		if (ecc->state < 0) {
			ecc->uncorrectable = 1;
			mxic_pr_dbg("Step %d, ECC correction failed, may exceed ECC strength(%d)\r\n",
					n, ecc->strength);

		} else if (ecc->state) {
			if (ecc->max_err_bits_per_step < ecc->state) {
				ecc->max_err_bits_per_step = ecc->state;
			}
			ecc->ecc_bits_corrected += ecc->state;
			mxic_pr_dbg("Step %d, ECC corrected %d bits\r\n",
					n, ecc->state);

		}
		buf_data += ecc->size_step;
		buf_oob += ecc->nbytes;
	}

	return MXST_SUCCESS;
}

int nand_oob_layout_init(oob_layout_t *layout, uint32_t nbytes_ecc_per_page, uint32_t size_oob)
{
	if (0 == layout || 0 == size_oob) {
		mxic_pr_err("Layout(%08Xh) or size_oob(%d) should not be 0\r\n",
				layout, size_oob);
		return MXST_ERR_PARAM;
	}

	layout->bb.ofs = 0;
	layout->bb.len = 2;

	layout->ecc.ofs = nbytes_ecc_per_page ? (size_oob - nbytes_ecc_per_page) : 0;
	layout->ecc.len = nbytes_ecc_per_page ? nbytes_ecc_per_page : 0;

	layout->free.ofs = layout->bb.len;
	layout->free.len = size_oob - layout->bb.len - layout->ecc.len;

	return MXST_SUCCESS;
}

#endif /* #if defined(CONF_SPINAND) || defined(CONF_RAWNAND) */
