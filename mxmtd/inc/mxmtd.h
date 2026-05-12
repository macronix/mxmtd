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

#ifndef _MXMTD_H_
#define _MXMTD_H_

#include "mxmtd_config.h"
#include <lib.h>

#ifdef CONF_RAWNAND
#include "../rawnand/rawnand.h"
#endif

#ifdef CONF_SPINAND
#include "../spinand/spinand.h"
#endif

#ifdef CONF_SPINOR
#include "../spinor/spinor.h"
#endif

typedef struct _mxmtd mxmtd_t;
typedef struct _cfg cfg_t;

enum FLASH_TYPE {
	FLASH_MMC,
	FLASH_SPINOR,
	FLASH_SPINAND,
	FLASH_RAWNAND,
};

/* Host controller channel type */
enum CHANNEL_TYPE {
	CH_NONE,
	CH_PAR,	/* Select parallel mode with HC */
	CH_A,	/* Select Channel A with HC */
	CH_B	/* Select Channel B with HC */
};

#ifdef CONF_ENABLE_ASP
typedef enum {
	ASP_SPB,
	ASP_DPB
} ASP_LOCK_TARGET;

typedef enum {
	ASP_UNLOCK,
	ASP_SPB_LOCK,
	ASP_DPB_LOCK,
	ASP_MIX_LOCK,
	ASP_PARTIAL_LOCK
} ASP_LOCK_MODE;
#endif

typedef enum {
	BP_MODE,
	ASP_MODE,
	ASP_PWD_MODE,
} LOCK_MODE;


struct _mxmtd{
	void *priv;
	uint8_t is_le: 1;
	
	union {
#ifdef CONF_SPINOR
		spinor_t spinor;
#endif

#ifdef CONF_RAWNAND
		rawnand_t rawnand;
#endif

#ifdef CONF_SPINAND
		spinand_t spinand;
#endif
	};

	struct {
		void (*release)(mxmtd_t *mxmtd);
		int (*probe)(mxmtd_t *mxmtd);
		int (*read)(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes);
		int (*program)(mxmtd_t *mxmtd, uint64_t addr, uint8_t *buf, uint64_t nbytes);
		int (*erase)(mxmtd_t *mxmtd, uint64_t addr, uint64_t nbytes);
		uint64_t (*get_chip_size)(mxmtd_t *mxmtd);
		uint32_t (*get_blk_size)(mxmtd_t *mxmtd);
		uint32_t (*get_page_size)(mxmtd_t *mxmtd);
		uint32_t (*get_ers_size)(mxmtd_t *mxmtd);
		uint32_t (*get_subpage_size)(mxmtd_t *mxmtd);
		int (*get_ecc_state)(mxmtd_t *mxmtd);

		int (*asp_set_wpsel)(mxmtd_t *mxmtd);
		// target, 0: dpb and spb, 1: spb, 2: dpb
		int (*asp_lock)(mxmtd_t *mxmtd, uint32_t addr, uint32_t nbytes, int target);
		int (*asp_unlock)(mxmtd_t *mxmtd, uint32_t addr, uint32_t nbytes, int target);
		// return, -1: error, 0: unlock, 1: lock
		int (*asp_is_lock)(mxmtd_t *mxmtd, uint32_t addr, uint32_t nbytes, int *lock_mode);
		// return 0: BP, 1: ASP w/o Password, 2: ASP w/ Password
		int (*query_lock_mode)(mxmtd_t *mxmtd);

		union {
			struct {
				int (*read_oob)(mxmtd_t *mxmtd, uint64_t addr, uint32_t ofs_oob, uint32_t nbytes);
				int (*write_oob)(mxmtd_t *mxmtd, uint64_t addr, uint32_t ofs_oob, uint32_t nbytes);
				int (*set_bb)(mxmtd_t *mxmtd, uint64_t addr, uint8_t pre_erase);
				uint8_t (*chk_bb)(mxmtd_t *mxmtd, uint64_t addr);

				void (*set_ecc_en)(mxmtd_t *mxmtd, uint8_t en);
				uint8_t (*get_ecc_strength)(mxmtd_t *mxmtd);
				uint8_t (*get_ecc_nsteps)(mxmtd_t *mxmtd);
				uint16_t (*get_ecc_step_size)(mxmtd_t *mxmtd);
			} nand;
		}; /* union { */
	} ops;
};

int mxmtd_setup_dev(mxmtd_t **mxmtd, enum FLASH_TYPE, enum CHANNEL_TYPE, int port);
void mxmtd_release(mxmtd_t **mxmtd);

#endif
