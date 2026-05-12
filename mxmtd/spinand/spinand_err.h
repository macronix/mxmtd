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

#ifndef _SIPNAND_ERR_H_
#define _SIPNAND_ERR_H_

/* Range of SPINAND specific error code : [0x00002fff: 0x00002000]*/

#include "mxmtd_err.h"

#define _SPINAND_ERR_H_

#define MXST_ERR_SPINAND(x)			(0x00002000U | x)
#define MXST_ERR_S_PP_CRC			(MXST_ERR_SPINAND(0x00000000U))
#define MXST_ERR_S_PP_SIGNATURE		(MXST_ERR_SPINAND(0x00000001U))
#define MXST_ERR_S_PP_VER			(MXST_ERR_SPINAND(0x00000002U))
#define MXST_ERR_S_PP_PARSE			(MXST_ERR_SPINAND(0x00000003U))
#define MXST_ERR_BP					(MXST_ERR_SPINAND(0x00000004U))
#define MXST_ERR_WRCR				(MXST_ERR_SPINAND(0x00000005U))
#define MXST_ERR_NOT_SUP_CONT_RD	(MXST_ERR_SPINAND(0x00000006U))

#endif
