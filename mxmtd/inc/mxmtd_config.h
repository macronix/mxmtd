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

#ifndef _MXMTD_CONFIG_H_
#define _MXMTD_CONFIG_H_

#define MXMTD_VER 0x2000

//#define CONF_SPINAND
//#define CONF_RAWNAND
#define CONF_SPINOR
//#define CONF_BCH_SMALL_HEAP

/* Send reset commands at the beginning of probe function */
#define CONF_PROBE_RESET

/* ------ configuration for SPINOR ------ */
#ifdef CONF_SPINOR

/* ---------------------------------------- */

	#define ERS_SIZE_64KB   (1U << 16)
	#define ERS_SIZE_32KB   (1U << 15)
	#define ERS_SIZE_4KB    (1U << 12)
	/* Specified erase size of spi nor */
	#define CONF_ERS_SIZE ERS_SIZE_64KB

	/* Write WPSEL in Security Register. Please note that this is an OTP bit!
	 * Keep ASP disabled by default; only enable for workflows that explicitly intend permanent device changes.
	 */
	// #define CONF_ENABLE_ASP
#endif /* #ifdef CONF_SPINOR */

/* ------ SPINAND Configuration------ */
#ifdef CONF_SPINAND
//	#define CONF_SPINAND_ECC_TYPE ECC_NONE
	#define CONF_SPINAND_ECC_TYPE ECC_SW_BCH
//	#define CONF_SPINAND_ECC_TYPE ECC_INTERNAL_SPI

	#define CONF_SPINAND_ECC_THRE	8
	#define CONF_SPINAND_ECC_STEP_SIZE_DEFAULT 512
//	#define CONF_SPINAND_CONT_RD_DIS

#if defined(CONF_BCH_SMALL_HEAP) && (CONF_SPINAND_ECC_TYPE == ECC_SW_BCH)
	#define CONF_BCH_ECC_SIZE	CONF_SPINAND_ECC_STEP_SIZE_DEFAULT
#endif

#endif /* #if defined(CONF_SPINAND) */

/* ------ RAWNAND Configuration ------ */
#ifdef CONF_RAWNAND
//	#define CONF_RAWNAND_ECC_TYPE ECC_NONE
	#define CONF_RAWNAND_ECC_TYPE ECC_SW_BCH
//	#define CONF_RAWNAND_ECC_TYPE ECC_INTERNAL_RAW

	#define CONF_RAWNAND_ECC_STEP_SIZE_DEFAULT 512

#if defined(CONF_BCH_SMALL_HEAP) && (CONF_RAWNAND_ECC_TYPE == ECC_SW_BCH)
	#define CONF_BCH_ECC_SIZE	CONF_RAWNAND_ECC_STEP_SIZE_DEFAULT
#endif

#endif /* #if defined(CONF_SPINAND) */


#endif
